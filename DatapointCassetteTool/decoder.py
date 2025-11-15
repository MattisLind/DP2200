#!/usr/bin/env python3
import sys

PREBITS = [0, 1, 0]

def fm_decode_pair(pair, last_line_bit):
    """
    FM decoding according to your spec:

      last line bit = 0:
        data 1 -> 11
        data 0 -> 10

      last line bit = 1:
        data 1 -> 00
        data 0 -> 01

    We return (decoded_bit, new_last_line_bit) or (None, last_line_bit) on error.
    """
    a, b = pair

    if last_line_bit == 0:
        if   (a, b) == (1, 1):
            decoded_bit = 1
        elif (a, b) == (1, 0):
            decoded_bit = 0
        else:
            return None, last_line_bit
    else:
        if   (a, b) == (0, 0):
            decoded_bit = 1
        elif (a, b) == (0, 1):
            decoded_bit = 0
        else:
            return None, last_line_bit

    # Line state after the pair is the second bit of the pair
    new_last_line_bit = b
    return decoded_bit, new_last_line_bit


def load_line_bits(path):
    with open(path, "r") as f:
        return [int(c) for c in f.read() if c in "01"]


def decode_file(path):
    # --------------------------------------------------
    # 1) Read raw line bits
    # --------------------------------------------------
    line_bits = load_line_bits(path)
    print(f"[INFO] Loaded {len(line_bits)} line bits")

    # --------------------------------------------------
    # 2) FM-decode into logical bits
    # --------------------------------------------------
    decoded_bits = []
    last_line = 0
    num_pairs = len(line_bits) // 2

    for i in range(num_pairs):
        a = line_bits[2*i]
        b = line_bits[2*i+1]
        bit, last_line_new = fm_decode_pair((a,b), last_line)
        if bit is None:
            print(f"[FM-ERROR] Invalid FM pair ({a},{b}) at pair index {i}")
            # keep line state unchanged or choose a resync strategy
            continue
        decoded_bits.append(bit)
        last_line = last_line_new

    print(f"[INFO] Decoded {len(decoded_bits)} logical bits")

    # --------------------------------------------------
    # 3) State machine on decoded bits
    # --------------------------------------------------
    STATE_SEARCH = 0
    STATE_BYTE   = 1

    state = STATE_SEARCH
    idx = 0

    while idx < len(decoded_bits):

        if state == STATE_SEARCH:
            # Look for decoded prebits 0,1,0
            if idx + 2 < len(decoded_bits) and decoded_bits[idx:idx+3] == PREBITS:
                print(f"\n[PREBITS] found at decoded index {idx}-{idx+2}")
                state = STATE_BYTE
                idx += 3   # move past the prebits
                continue
            idx += 1
            continue

        elif state == STATE_BYTE:
            # Need 11 bits to classify a frame
            if idx + 11 > len(decoded_bits):
                print("[WARN] Incomplete 11-bit frame at end of stream")
                break

            frame = decoded_bits[idx:idx+11]
            idx += 11

            # Case 1: End-of-record = 11 ones
            if all(b == 1 for b in frame):
                print("[END] End-of-record detected (11 ones).")
                state = STATE_SEARCH
                continue

            data_bits = frame[:8]
            trail     = frame[8:]

            # Case 2: Normal data byte with trailing prebits 010
            if trail == PREBITS:
                val = 0
                for i_bit, b in enumerate(data_bits):
                    val |= (b << i_bit)   # LSB-first
                print(f"[BYTE] decoded 0x{val:02X} ({val})")
                # Stay in BYTE state for next 11-bit frame
                continue

            # Case 3: Anything else => sync violation
            print(f"[SYNC-ERROR] Frame trailer {trail} is not PREBITS or 11 ones. Discarding frame.")
            state = STATE_SEARCH
            continue

    print("\n[DONE]")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: fm_decoder_sm.py /tmp/out_bits.txt")
    else:
        decode_file(sys.argv[1])