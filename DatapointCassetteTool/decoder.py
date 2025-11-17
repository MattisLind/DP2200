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
            # Invalid FM code for last_line_bit == 0
            return None, last_line_bit
    else:
        if   (a, b) == (0, 0):
            decoded_bit = 1
        elif (a, b) == (0, 1):
            decoded_bit = 0
        else:
            # Invalid FM code for last_line_bit == 1
            return None, last_line_bit

    # Line state after the pair is the second bit of the pair
    new_last_line_bit = b
    return decoded_bit, new_last_line_bit


def load_line_bits(path):
    """Load all '0' and '1' characters from the file as integers 0/1."""
    with open(path, "r") as f:
        return [int(c) for c in f.read() if c in "01"]


def write_tap_record(out_f, record_bytes):
    """
    Write a single TAP record to out_f.

    Format:
        4-byte little endian length (number of payload bytes)
        payload bytes
        4-byte little endian length (same as header)

    Zero-length records are ignored (nothing is written).
    """
    length = len(record_bytes)
    if length == 0:
        # Ignore zero-length records as requested
        return

    # Convert length to 4-byte little endian
    length_le = length.to_bytes(4, byteorder="little")

    # Write header, payload, trailer
    out_f.write(length_le)
    out_f.write(bytes(record_bytes))
    out_f.write(length_le)


def decode_file(path, out_path=None):
    """
    Decode an FM-encoded bit file.
    If out_path is given, write TAP records there:
      - Each record = bytes between PREBITS (start) and END (11 ones)
      - Only payload bytes are written, length is payload length in bytes
      - On SYNC-ERROR or incomplete frame, the partial record is written.
    """
    # --------------------------------------------------
    # 1) Read raw line bits
    # --------------------------------------------------
    line_bits = load_line_bits(path)
    print(f"[INFO] Loaded {len(line_bits)} line bits")

    # --------------------------------------------------
    # Optional TAP output file
    # --------------------------------------------------
    out_f = None          # File handle for TAP output, if any
    record_bytes = []     # Accumulated bytes for current record

    try:
        if out_path is not None:
            # Open output file in binary mode
            out_f = open(out_path, "wb")
            print(f"[INFO] Writing TAP output to {out_path}")

        # Helper to flush the current record to TAP file
        def flush_record():
            """
            If there are any accumulated payload bytes for the current record,
            write them as a TAP record and clear the buffer.
            """
            nonlocal record_bytes
            if out_f is not None and len(record_bytes) > 0:
                # Write TAP record for current payload
                write_tap_record(out_f, record_bytes)
                print(f"[TAP] Wrote record of {len(record_bytes)} bytes")
            # Clear current record buffer regardless of whether we wrote
            record_bytes = []

        # --------------------------------------------------
        # 2) FM-decode into logical bits
        # --------------------------------------------------
        decoded_bits = []
        last_line = 0
        num_pairs = len(line_bits) // 2

        for i in range(num_pairs):
            a = line_bits[2*i]
            b = line_bits[2*i+1]
            bit, last_line_new = fm_decode_pair((a, b), last_line)
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
        STATE_SEARCH = 0   # Searching for PREBITS that mark start of a record
        STATE_BYTE   = 1   # Reading bytes inside a record

        state = STATE_SEARCH
        idx = 0

        while idx < len(decoded_bits):

            if state == STATE_SEARCH:
                # Look for decoded prebits 0,1,0 that mark record start
                if idx + 2 < len(decoded_bits) and decoded_bits[idx:idx+3] == PREBITS:
                    print(f"\n[PREBITS] found at decoded index {idx}-{idx+2}")
                    state = STATE_BYTE
                    idx += 3   # move past the prebits
                    # New record starts here: record_bytes will be filled as we decode bytes
                    continue
                idx += 1
                continue

            elif state == STATE_BYTE:
                # Need 11 bits to classify a frame (8 data + 3 trailer)
                if idx + 11 > len(decoded_bits):
                    print("[WARN] Incomplete 11-bit frame at end of stream")
                    # Flush any partial record we have so far
                    flush_record()
                    break

                frame = decoded_bits[idx:idx+11]
                idx += 11

                # Case 1: End-of-record = 11 ones
                if all(b == 1 for b in frame):
                    print("[END] End-of-record detected (11 ones).")
                    # End of current record: write it out (if non-empty)
                    flush_record()
                    state = STATE_SEARCH
                    continue

                data_bits = frame[:8]
                trail     = frame[8:]

                # Case 2: Normal data byte with trailing prebits 010
                if trail == PREBITS:
                    # Decode LSB-first data bits into a byte value
                    val = 0
                    for i_bit, b in enumerate(data_bits):
                        val |= (b << i_bit)   # LSB-first
                    print(f"[BYTE] decoded 0x{val:02X} ({val})")

                    # If we have an output file, accumulate this byte in the current record
                    if out_f is not None:
                        record_bytes.append(val)

                    # Stay in BYTE state for next 11-bit frame
                    continue

                # Case 3: Anything else => sync violation
                print(f"[SYNC-ERROR] Frame trailer {trail} is not PREBITS or 11 ones. Discarding frame.")

                # On SYNC-ERROR, write the partial record we have so far
                flush_record()

                # Go back to searching for next record
                state = STATE_SEARCH
                continue

        print("\n[DONE]")

    finally:
        # Ensure output file is properly closed
        if out_f is not None:
            out_f.close()


if __name__ == "__main__":
    # Basic argument handling:
    #   decoder.py input_bits.txt [--out output.tap]
    if len(sys.argv) < 2:
        print("Usage: fm_decoder_sm.py /tmp/out_bits.txt [--out output.tap]")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = None

    # Simple manual parsing of --out flag
    # Example: decoder.py input_bits.txt --out output.tap
    args = sys.argv[2:]
    i = 0
    while i < len(args):
        if args[i] == "--out":
            # If there is a filename after --out, use it
            if i + 1 < len(args):
                output_path = args[i + 1]
                i += 2
                continue
            else:
                print("[ERROR] --out flag requires an output filename")
                sys.exit(1)
        else:
            # Unknown extra argument (you can choose to warn or ignore)
            print(f"[WARN] Ignoring unknown argument: {args[i]}")
            i += 1

    # Call decoder with or without TAP output
    decode_file(input_path, output_path)
