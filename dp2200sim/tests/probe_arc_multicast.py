"""Bounded same-host multicast feasibility probe; independent of the simulator.

Three processes share a UDP port, each sends numbered packets, and all must
receive every packet including their own loopback. Default TTL 0 confines this
probe to the host. This is a transport check, not an ARC/RIM implementation.
"""
import argparse
import json
import multiprocessing
import platform
import socket
import struct
import sys
import time
import uuid


def payload(sequence, count):
    length = 253 if sequence == count - 1 else sequence + 1
    return bytes((sequence + index) & 255 for index in range(length))


def worker(pipe, group, port, interface, epoch, identity, count, reuseport):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            if reuseport:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 262144)
            sock.bind(('', port))
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                            socket.inet_aton(group) + socket.inet_aton(interface))
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF,
                            socket.inet_aton(interface))
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 0)
            pipe.send({'ready': identity})
            if not pipe.poll(10) or pipe.recv() != 'send':
                raise RuntimeError('Parent did not start the probe')
            for sequence in range(count):
                packet = b'ARCP' + epoch + struct.pack('!BH', identity, sequence)
                sock.sendto(packet + payload(sequence, count), (group, port))
            seen, duplicates, invalid = set(), 0, 0
            deadline = time.monotonic() + 5
            while len(seen) < 3 * count and time.monotonic() < deadline:
                sock.settimeout(max(0.001, deadline - time.monotonic()))
                try:
                    packet, _ = sock.recvfrom(2048)
                except socket.timeout:
                    break
                if len(packet) < 23 or packet[:20] != b'ARCP' + epoch:
                    continue  # Ignore unrelated traffic on the probe port.
                sender, sequence = struct.unpack('!BH', packet[20:23])
                if sender not in range(3) or sequence >= count or packet[23:] != payload(sequence, count):
                    invalid += 1
                    continue
                key = sender, sequence
                duplicates += int(key in seen)
                seen.add(key)
            pipe.send({'receiver': identity,
                       'per_sender': [sum(sender == index for sender, _ in seen)
                                      for index in range(3)],
                       'received': len(seen), 'duplicates': duplicates, 'invalid': invalid,
                       'passed': len(seen) == 3 * count and invalid == 0})
    except Exception as error:
        pipe.send({'error': f'{type(error).__name__}: {error}'})
    finally:
        pipe.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--group', default='239.255.22.0')
    parser.add_argument('--interface', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--count', type=int, default=20, choices=range(2, 101))
    parser.add_argument('--output')
    args = parser.parse_args()
    context = multiprocessing.get_context('spawn')
    reuseport = sys.platform == 'darwin'
    port = args.port
    if not port:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
            reservation.bind(('', 0))
            port = reservation.getsockname()[1]
    children, pipes, results = [], [], []
    epoch = uuid.uuid4().bytes
    try:
        for identity in range(3):
            parent, child = context.Pipe()
            process = context.Process(target=worker, args=(child, args.group, port,
                args.interface, epoch, identity, args.count, reuseport))
            process.start()
            child.close()
            children.append(process)
            pipes.append(parent)
        for pipe in pipes:
            if not pipe.poll(10):
                raise RuntimeError('Receiver setup timed out')
            ready = pipe.recv()
            if 'error' in ready:
                raise RuntimeError(ready['error'])
        for pipe in pipes:
            pipe.send('send')
        for pipe in pipes:
            if not pipe.poll(10):
                raise RuntimeError('Receiver result timed out')
            results.append(pipe.recv())
    finally:
        for process in children:
            process.join(timeout=1)
            if process.is_alive():
                process.terminate()
                process.join(timeout=1)
        for pipe in pipes:
            pipe.close()
    report = {'platform': platform.platform(), 'group': args.group, 'port': port,
              'interface': args.interface, 'ttl': 0, 'multicast_loop': True,
              'reuseaddr': True, 'reuseport': reuseport, 'processes': 3,
              'packets_per_sender': args.count, 'max_arc_payload': 253,
              'results': results, 'passed': all(result.get('passed') for result in results)}
    output = json.dumps(report, indent=2) + '\n'
    if args.output:
        with open(args.output, 'w') as file:
            file.write(output)
    print(output, end='')
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
