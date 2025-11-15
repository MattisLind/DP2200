#!/usr/bin/env python3
import sys, binascii, socket

CHUNK_SIZE = 512

class Link:
    def __init__(self, tcp=None, serial_path=None, baud=115200):
        if tcp:
            host, port = tcp.split(':')
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((host, int(port)))
            s.settimeout(50.0)
            self.s = s
            self.mode = 'tcp'
        else:
            import serial
            self.s = serial.Serial(serial_path, baudrate=baud, timeout=50.0)
            self.mode = 'serial'

    def write(self, b: bytes):
        print(f"Python sending: {b}")
        if self.mode == 'tcp':
            self.s.sendall(b)
        else:
            self.s.write(b)

    def read1(self) -> bytes:
        if self.mode == 'tcp':
            return self.s.recv(1)
        else:
            return self.s.read(1)

    def close(self):
        try: self.s.close()
        except: pass


def wait_for_ack(link):
    while True:
        b = link.read1()
        print(f"b={b}")
        if not b:
            raise RuntimeError("Timeout waiting for ACK")
        if b == b'A':
            return
        if b == b'E':
            raise RuntimeError("Device reported error")


def stream_file(link, fp):
    # Start
    link.write(b'S')
    wait_for_ack(link)

    while True:
        chunk = fp.read(CHUNK_SIZE)
        if not chunk:
            break  # EOF

        hexstr = binascii.hexlify(chunk)
        print(f"Sent {len(chunk)} bytes ({len(hexstr)} hex chars)")
        link.write(hexstr)

        if len(chunk) == CHUNK_SIZE:
            wait_for_ack(link)
        else:
            # Final partial chunk → exit without waiting
            break

    # Send EOF marker
    link.write(b'T')


def main():
    if len(sys.argv) < 3:
        print("Usage:")
        print("  TCP:    host.py --tcp host:port file.tap")
        print("  Serial: host.py --serial /dev/ttyACM0 file.tap")
        sys.exit(2)

    if sys.argv[1] == '--tcp':
        link = Link(tcp=sys.argv[2])
        tap_path = sys.argv[3]
    elif sys.argv[1] == '--serial':
        link = Link(serial_path=sys.argv[2])
        tap_path = sys.argv[3]
    else:
        print("First arg must be --tcp or --serial")
        sys.exit(2)

    try:
        with open(tap_path, 'rb') as fp:
            stream_file(link, fp)
        print("Done.")
    except Exception as e:
        print("ERROR:", e)
        sys.exit(1)
    finally:
        link.close()


if __name__ == '__main__':
    main()