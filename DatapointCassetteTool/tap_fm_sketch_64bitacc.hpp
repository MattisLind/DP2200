/* 
 * tap_fm_tx_sketch.hpp (refactored with conditional debug + 64-bit accumulator-only buffering)
 * -------------------------------------------------------------------------------------------
 * Behavior / protocol is unchanged.
 *
 * Design recap:
 *  - Main loop:
 *      * Receives ASCII hex from host.
 *      * Converts to raw TAP bytes (leader + payload + trailer).
 *      * Streams raw TAP bytes into two 512-byte ping-pong buffers (A, B).
 *      * On 'S': starts record, sends initial 'A' once.
 *      * On 'T': marks any partially-filled buffer READY and stops.
 *      * Does NOT parse TAP, does NOT do framing, does NOT FM encode inside ISR.
 *
 *  - FM producer (produceFmBits, called from loop()):
 *      * Pulls raw TAP bytes from ping-pong buffers (through TapParser).
 *      * Parses TAP leader/payload/trailer.
 *      * Generates framing:
 *          - 200 SYNC bytes of '1's before record.
 *          - 0,1,0 before first payload byte and between payload bytes.
 *          - 11 ones + 200 SYNC bytes after record.
 *      * FM-encodes logical bits into FM bits.
 *      * Packs FM bits into a 64-bit bitAccumulator (LSB-first).
 *
 *  - ISR (onSpiTimerISR):
 *      * Called periodically (1 ms in Linux shim, or timer on MCU).
 *      * Looks at bitAccumulator / bitsInAcc.
 *      * If bitsInAcc >= 8:
 *          - Sends one byte via SPI_DEV.transfer().
 *          - Shifts bitAccumulator >> 8 and reduces bitsInAcc by 8.
 *      * Does nothing else (no TAP parsing, no framing, no FM encoding).
 *
 *  - Flow control:
 *      * On 'S', main loop sends one initial 'A' so host can send first 512 bytes.
 *      * Each ping-pong buffer is 512 TAP BYTES.
 *      * When consumer finishes a buffer (A or B), it:
 *          - Marks that buffer FREE.
 *          - Sends 'A' to host, meaning "one 512-byte chunk slot is free".
 *      * Host sends next 512 TAP bytes only after 'A'.
 *
 *  -> Result: clean 512-byte ping-pong between host and encoder, minimal ISR,
 *     with extra slack from a 64-bit accumulator instead of a FIFO.
 */

#ifdef ARDUINO
  #include <Arduino.h>  // Arduino API (pinMode, digitalWrite, etc.)
  #include <SPI.h>      // SPI library for MCU
#endif
#include <stdint.h>     // Standard integer types
#include <stdio.h>      // printf (used only in non-Arduino env)

// ========================================================
// Debug logging control
// Enabled automatically when NOT running on Arduino
// ========================================================
#ifndef ARDUINO
  // In non-Arduino environment (e.g., Linux shim), log via printf
  #define DEBUG_LOG(...)   do { printf(__VA_ARGS__); } while(0)
#else
  // On Arduino/MCU, compile all debug logging out (no overhead)
  #define DEBUG_LOG(...)   do {} while(0)
#endif

// ================= Configuration =================
#define PIN_LED        PB1
#define USB_SERIAL     SerialUSB
#define SPI_DEV        SPI
#define SPI_CS_PIN     PB12

#define SYNC_BYTE      0xFF       // 8 logical '1' bits
#define SYNC_LEN_BYTES 200        // 200 SYNC bytes before/after each record
#define BUF_SIZE       512        // Ping-pong buffer size in TAP bytes
#define TAP_LITTLE_ENDIAN 1       // TAP length is little-endian

// =============== Ping-Pong Buffer Model ===============

// Buffer state
enum class BufState : uint8_t {
    FREE,   // buffer available for writer to fill
    READY,  // buffer filled with TAP bytes, ready for consumer
    BUSY    // buffer currently being read
};

// One ping-pong buffer: raw TAP bytes
struct PingPongBuffer {
    uint8_t         data[BUF_SIZE];   // raw TAP bytes (leader+payload+trailer)
    volatile size_t len;              // number of valid bytes
    volatile BufState state;          // FREE / READY / BUSY
};

// Two physical buffers
static PingPongBuffer bufA;
static PingPongBuffer bufB;

/* ---------------------------------------------------
 * PingPongBuffers : encapsulates writer/reader logic
 * ---------------------------------------------------
 * - Writer side is used by the main loop to push TAP bytes.
 * - Reader side is used by the TAP parser to read TAP bytes.
 * - Handles:
 *      * buffer ownership
 *      * READY/FREE transitions
 *      * 'A' ACK when a 512-byte chunk is freed
 */
struct PingPongBuffers {
    PingPongBuffer* write   = &bufA;   // current writer buffer
    PingPongBuffer* read    = nullptr; // current reader buffer
    size_t          readPos = 0;       // position inside current reader buffer

    // Reset both buffers and internal pointers
    void reset() {
        // Initialize buffer A
        bufA.len   = 0;
        bufA.state = BufState::FREE;

        // Initialize buffer B
        bufB.len   = 0;
        bufB.state = BufState::FREE;

        // Writer starts at A
        write   = &bufA;
        read    = nullptr;
        readPos = 0;
    }

    // Try to acquire a FREE buffer for the writer if none is assigned.
    // Returns false if no FREE buffer exists (both are READY/BUSY).
    bool ensureWriter() {
        // If we already have a writer buffer, nothing to do
        if (write) {
            return true;
        }

        // Try to find a FREE buffer
        if (bufA.state == BufState::FREE) {
            write      = &bufA;
            write->len = 0;
            return true;
        } else if (bufB.state == BufState::FREE) {
            write      = &bufB;
            write->len = 0;
            return true;
        }

        // No FREE buffer available -> writer must stall
        DEBUG_LOG("[RX] ERROR: no FREE buffer available for writing\n");
        return false;
    }

    // Push one TAP byte into the writer buffer.
    // Sets errorFlag externally if needed.
    bool pushTapByte(uint8_t b, volatile bool& errorFlag) {
        // Ensure we have a writer buffer
        if (!ensureWriter()) {
            errorFlag = true;
            return false;
        }

        // Store byte in current writer buffer
        write->data[write->len++] = b;

        if ((write->len % 64) == 0) {
            DEBUG_LOG("[RX] filled buf=%c, len=%zu\n",
                      (write == &bufA) ? 'A' : 'B',
                      (size_t)write->len);
        }

        // When buffer is full (512 bytes), mark it READY
        if (write->len == BUF_SIZE) {
            DEBUG_LOG("[RX] buffer %c FULL (512 bytes), marking READY\n",
                      (write == &bufA) ? 'A' : 'B');

            write->state = BufState::READY;

            // Try switching to the other buffer if it's FREE
            PingPongBuffer* other = (write == &bufA) ? &bufB : &bufA;
            if (other->state == BufState::FREE) {
                write      = other;
                write->len = 0;
            } else {
                // No FREE buffer; writer must stall until consumer frees one
                DEBUG_LOG("[RX] writer stalled, no FREE buffer after commit\n");
                write = nullptr;
            }
        }

        return true;
    }

    // Called when host sends 'T' (EOF):
    // If there's a partially-filled FREE buffer, mark it READY.
    void commitPartialOnEOF() {
        if (write && write->state == BufState::FREE && write->len > 0) {
            DEBUG_LOG("[EOF] committing partial buffer %c, len=%zu\n",
                      (write == &bufA) ? 'A' : 'B',
                      (size_t)write->len);
            write->state = BufState::READY;
            write        = nullptr;
        }
    }

    // Reader-side function: get the next TAP byte, if any.
    // Returns true and sets 'out' if a TAP byte is available.
    // Returns false if no READY/BUSY data is available.
    bool readTapByte(uint8_t& out) {
        while (true) {
            // 1) If we have a current buffer with remaining data, read from it
            if (read && readPos < read->len) {
                out = read->data[readPos++];

                if ((readPos % 64) == 0) {
                    DEBUG_LOG("[PP] TAP byte from buf=%c pos=%zu len=%zu\n",
                              (read == &bufA) ? 'A' : 'B',
                              readPos, (size_t)read->len);
                }
                return true;
            }

            // 2) If the current buffer is exhausted, FREE it and send 'A' to host
            if (read && readPos >= read->len) {
                DEBUG_LOG("[PP] finished buffer %c, len=%zu -> FREE + 'A'\n",
                          (read == &bufA) ? 'A' : 'B',
                          (size_t)read->len);

                read->state = BufState::FREE;
                read->len   = 0;

                // Flow control: one 512-byte slot is now free
                USB_SERIAL.write('A');

                read    = nullptr;
                readPos = 0;
            }

            // 3) Try to acquire a READY buffer
            if (bufA.state == BufState::READY) {
                read        = &bufA;
                read->state = BufState::BUSY;
                readPos     = 0;
                continue; // loop again to consume from it
            }
            if (bufB.state == BufState::READY) {
                read        = &bufB;
                read->state = BufState::BUSY;
                readPos     = 0;
                continue;
            }

            // 4) No data available in either buffer
            return false;
        }
    }
};

// Global buffer manager instance
static PingPongBuffers pp;

// =============== TAP parsing state ===============

// =============== Endian helpers ===============
static uint32_t read_u32_le(const uint8_t *p) {
    // Read 32-bit integer in little-endian format
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static uint32_t read_u32_be(const uint8_t *p) {
    // Read 32-bit integer in big-endian format
    return ((uint32_t)p[0] << 24)
         | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)
         |  (uint32_t)p[3];
}

// Forward declaration so TapParser can use isrReadTapByte
static bool isrReadTapByte(uint8_t& out);

// TAP parser encapsulated in a class for clarity
class TapParser {
public:
    enum class Phase : uint8_t { NEED_LEADER = 0, PAYLOAD, NEED_TRAILER };

    uint32_t expectedLen = 0;   // expected payload length
    uint32_t gotPayload  = 0;   // number of payload bytes already read
    uint8_t  tmp[4];            // temporary buffer for leader/trailer
    uint8_t  tmpPos      = 0;   // position in tmp[]
    Phase    phase       = Phase::NEED_LEADER;

    // Reset TAP parser to initial state
    void reset() {
        expectedLen = 0;
        gotPayload  = 0;
        tmpPos      = 0;
        phase       = Phase::NEED_LEADER;
    }

    /* 
     * nextPayload:
     *  - Consumes TAP bytes via isrReadTapByte().
     *  - Implements TAP format:
     *      * NEED_LEADER: read 4-byte length.
     *      * PAYLOAD: stream out payload bytes.
     *      * NEED_TRAILER: read 4-byte length and verify.
     *  - Returns:
     *      * true if a payload byte is available.
     *      * false if no data can be read right now.
     *  - Out parameters:
     *      * outByte          : payload byte
     *      * outLastInRecord  : true if this is the last payload byte.
     */
    bool nextPayload(uint8_t& outByte, bool& outLastInRecord, volatile bool& errorFlag) {
        uint8_t b = 0;

        while (true) {
            switch (phase) {

                // ===================== Leader =====================
                case Phase::NEED_LEADER:
                    if (!isrReadTapByte(b)) {
                        // No data yet
                        return false;
                    }

                    tmp[tmpPos++] = b;
                    if (tmpPos == 4) {
                        expectedLen = TAP_LITTLE_ENDIAN ?
                                      read_u32_le(tmp) :
                                      read_u32_be(tmp);
                        gotPayload = 0;
                        tmpPos     = 0;
                        phase      = Phase::PAYLOAD;

                        DEBUG_LOG("[TAP] leader parsed: expectedLen=%u\n", expectedLen);
                    }
                    break;

                // ===================== Payload =====================
                case Phase::PAYLOAD:
                    if (gotPayload >= expectedLen) {
                        // Done with payload -> go to trailer
                        phase  = Phase::NEED_TRAILER;
                        tmpPos = 0;
                        DEBUG_LOG("[TAP] payload complete, expecting trailer\n");
                        break;
                    }

                    if (!isrReadTapByte(b)) {
                        // No more TAP data now
                        return false;
                    }

                    gotPayload++;
                    outByte         = b;
                    outLastInRecord = (gotPayload == expectedLen);
                    return true;  // payload byte ready

                // ===================== Trailer =====================
                case Phase::NEED_TRAILER:
                    if (!isrReadTapByte(b)) {
                        // No trailer data yet
                        return false;
                    }

                    tmp[tmpPos++] = b;
                    if (tmpPos == 4) {
                        uint32_t L2 = TAP_LITTLE_ENDIAN ?
                                      read_u32_le(tmp) :
                                      read_u32_be(tmp);
                        if (L2 != expectedLen) {
                            DEBUG_LOG("[TAP] trailer mismatch: %u != %u\n",
                                      L2, expectedLen);
                            errorFlag = true;
                        } else {
                            DEBUG_LOG("[TAP] trailer OK: %u\n", L2);
                        }

                        // Prepare for next record
                        reset();
                    }
                    break;
            } // switch
        } // while
    }
};

// Global TAP parser instance
static TapParser tap;

// =============== Global flags & FM bit state ===============

static volatile bool errorFlag   = false;
static volatile bool streamEnded = false;

// Last FM line bit (0 or 1)
static volatile uint8_t lastLineBit = 0;

// =============== isrReadTapByte wrapper ===============
/*
 * isrReadTapByte:
 *  - Thin wrapper around pp.readTapByte().
 *  - Keeps original signature so TapParser code is unchanged.
 *  - Now used by the producer running in the main loop.
 */
static bool isrReadTapByte(uint8_t &out) {
    return pp.readTapByte(out);
}

// =====================================================
//  FM Encode helpers (used by producer in main loop)
// =====================================================

/* 
 * fmPushEncodedBit:
 *  - FM-encodes one logical bit (0/1) based on lastLineBitRef.
 *  - Appends two FM bits to bitAccumulator (LSB-first).
 */
static inline void fmPushEncodedBit(
    uint8_t           srcbit,          // logical bit (0 or 1)
    uint64_t         &bitAccumulator,  // 64-bit accumulator for FM bits (LSB-first)
    uint8_t          &bitsInAcc,       // number of valid bits in bitAccumulator (0–64)
    volatile uint8_t &lastLineBitRef   // last FM line bit (volatile)
) {
    uint8_t out1, out2;

    if (srcbit) {
        // logical 1
        if (lastLineBitRef == 0) { out1 = 1; out2 = 1; }  // 11
        else                     { out1 = 0; out2 = 0; }  // 00
    } else {
        // logical 0
        if (lastLineBitRef == 0) { out1 = 1; out2 = 0; }  // 10
        else                     { out1 = 0; out2 = 1; }  // 01
    }

    // Put both FM bits into accumulator (LSB-first)
    bitAccumulator |= (uint64_t(out1) << bitsInAcc++);
    bitAccumulator |= (uint64_t(out2) << bitsInAcc++);
    lastLineBitRef = out2;  // update last line bit
}

/*
 * fmPushSyncByte:
 *  - Encodes one SYNC byte (0xFF) = 8 logical '1' bits.
 */
static inline void fmPushSyncByte(
    uint64_t         &bitAccumulator,  // 64-bit accumulator for FM bits (LSB-first)
    uint8_t          &bitsInAcc,       // number of valid bits in bitAccumulator
    volatile uint8_t &lastLineBitRef   // last FM line bit (volatile)
) {
    uint8_t b = SYNC_BYTE;
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t bit = (b >> i) & 1;  // LSB-first
        fmPushEncodedBit(bit, bitAccumulator, bitsInAcc, lastLineBitRef);
    }
}

// =====================================================
//  Overall transmitter state (records)
// =====================================================

// Overall transmitter state (records)
enum class TxState {
    IDLE,          // waiting for next TAP record
    LEAD_SYNC,     // 200 SYNC bytes before record
    PREBITS_START, // initial 0,1,0 before first payload byte
    FRAME_BITS,    // 8 data bits + 3 bits "010" per payload byte
    NEXT_BYTE,     // fetch next payload byte or end record
    END_ONES,      // 11 logical ones after last payload byte
    TRAIL_SYNC     // 200 SYNC bytes after record
};

// =====================================================
//  FM accumulator and state (shared producer/consumer)
// =====================================================

// 64-bit accumulator for FM bits (LSB-first)
static uint64_t bitAccumulator = 0ULL;

// Number of valid bits currently stored in bitAccumulator (0..64)
static uint8_t  bitsInAcc      = 0;

// Current payload byte and flag if it's last in record
static uint8_t curByte         = 0;
static bool    curIsLastByte   = false;

// Framing state
static uint8_t  frameBitPos    = 0;     // 0..10: 8 data bits + 3 bits "010"
static uint8_t  onesRemaining  = 0;     // remaining ones in END_ONES
static uint16_t syncCount      = 0;     // remaining SYNC bytes

// Record-level transmission state
static TxState txState         = TxState::IDLE;

// =====================================================
//  ISR: FM consumer only – sends bytes if available
// =====================================================
void onSpiTimerISR() {
    static uint32_t isrCalls = 0;
    if (isrCalls < 20 || (isrCalls % 1000) == 0) {
        DEBUG_LOG("[ISR] tick %u bufA.state=%d lenA=%zu bufB.state=%d lenB=%zu readPos=%zu bitsInAcc=%u\n",
                  isrCalls,
                  (int)bufA.state, (size_t)bufA.len,
                  (int)bufB.state, (size_t)bufB.len,
                  pp.readPos,
                  (unsigned)bitsInAcc);
    }
    ++isrCalls;

    // If we're in error state, stop transmitting
    if (errorFlag) {
        return;
    }

    // Consume one encoded byte from accumulator if available
    if (bitsInAcc >= 8) {
        // Take lowest 8 bits as output byte
        uint8_t outByte = (uint8_t)(bitAccumulator & 0xFF);

        // Send it over SPI
        SPI_DEV.transfer(outByte);

        // Shift accumulator to drop the bits we just sent (64-bit shift)
        bitAccumulator >>= 8;

        // Track how many bits remain valid
        bitsInAcc      -= 8;
    }

    // No parsing, framing, or FM encoding here.
    // All heavy work is done in produceFmBits() in the main loop.
}

// =============== Host input parsing (main loop) ===============
enum RxState { RX_IDLE=0, RX_RECORD_HEX };
static RxState rxState = RX_IDLE;

// Holds the first nibble while building a byte from two hex chars
static int8_t rxHalfNibble = -1;

// Hex char to nibble (0..15) or -1 if invalid
static inline int8_t hexToNibble(int c) {
    // Convert ASCII hex character to numeric value (0–15)
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Reset buffers and TAP/FM state
void resetBuffers() {
    // Reset ping-pong buffers
    pp.reset();

    // Reset TAP parser
    tap.reset();

    // Flags
    streamEnded  = false;
    errorFlag    = false;
    lastLineBit  = 0;

    // RX state
    rxState      = RX_IDLE;
    rxHalfNibble = -1;

    // Reset FM accumulator and state
    bitAccumulator = 0ULL;
    bitsInAcc      = 0;
    curByte        = 0;
    curIsLastByte  = false;
    frameBitPos    = 0;
    onesRemaining  = 0;
    syncCount      = 0;
    txState        = TxState::IDLE;
}

// Wrapper around PingPongBuffers::pushTapByte
static void rxPushTapByte(uint8_t b) {
    if (!pp.pushTapByte(b, errorFlag)) {
        // errorFlag is set inside pushTapByte
        return;
    }
}

// Called when host sends 'S'
void startRecord() {
    // Turn LED on to indicate active record
    digitalWrite(PIN_LED, HIGH);

    // Set RX state to hex mode
    rxState      = RX_RECORD_HEX;
    rxHalfNibble = -1;

    // Reset TAP parser in consumer for this new record
    tap.reset();

    // Reset FM accumulator and state for new stream
    bitAccumulator = 0ULL;
    bitsInAcc      = 0;
    curByte        = 0;
    curIsLastByte  = false;
    frameBitPos    = 0;
    onesRemaining  = 0;
    syncCount      = 0;
    txState        = TxState::IDLE;
}

// Called when host sends 'T'
void endOfFileFromHost() {
    // If there's a partially filled buffer, mark it READY
    pp.commitPartialOnEOF();

    streamEnded = true;
    rxState     = RX_IDLE;
}

// Handle incoming hex char when in RX_RECORD_HEX
static void handleHexInput(int c) {
    DEBUG_LOG("Handling nibble\n");

    int8_t n = hexToNibble(c);
    if (n < 0) {
        // Ignore whitespace, otherwise set error
        if (!(c == ' ' || c == '\r' || c == '\n' || c == '\t')) {
            errorFlag = true;
        }
        DEBUG_LOG("errorFlag=%d\n", errorFlag);
        return;
    }

    // First nibble?
    if (rxHalfNibble < 0) {
        rxHalfNibble = n;
        return;
    }

    // Second nibble -> build full byte
    uint8_t b = (uint8_t)((rxHalfNibble << 4) | n);
    rxHalfNibble = -1;

    DEBUG_LOG("[RX] TAP byte: 0x%02X\n", b);
    rxPushTapByte(b);
}

// Handle one byte from host
static void handleHostByte(int c) {
    DEBUG_LOG("Got: %c %02X\n", (c >= 32 && c < 127) ? c : '.', c & 0xFF);

    // Control commands first
    if (c == 'S') {
        DEBUG_LOG("Got S\n");
        startRecord();
        // Initial ACK so host can send first 512-byte chunk
        USB_SERIAL.write('A');
        return;
    }
    if (c == 'T') {
        DEBUG_LOG("Got T\n");
        endOfFileFromHost();
        digitalWrite(PIN_LED, LOW);
        return;
    }
    if (c == 'E') {
        DEBUG_LOG("Got E from host\n");
        errorFlag = true;
        return;
    }

    // If we're in hex RX mode, interpret the char as hex
    if (rxState == RX_RECORD_HEX) {
        handleHexInput(c);
    }
    // Otherwise ignore the char (but we already logged it above if debug is on)
}

// ============================================================
//  FM producer: called in loop() to generate FM bits
//  - Uses the same state machine that used to live in ISR.
//  - Fills 64-bit bitAccumulator while there is room for at least
//    one more encoded byte (22 FM bits).
// ============================================================
static void produceFmBits() {
    // If in error state, do not produce any more data
    if (errorFlag) {
        return;
    }

    // One encoded payload byte = 8 data bits + 3 framing bits = 11 logical bits.
    // FM encoding = 2 FM bits per logical bit => 22 FM bits per encoded byte.
    //
    // bitAccumulator is 64 bits wide, so we need at most 22 free bits to add
    // another fully encoded byte. That means:
    //   bitsInAcc <= 64 - 22 = 42
    //
    // We'll run the state machine as long as there is room for another block
    // of up to 22 FM bits. This also bounds the amount of work per call.
    while (bitsInAcc <= 42) {
        switch (txState) {

            case TxState::IDLE: {
                // Try to get first payload byte of next TAP record
                if (!tap.nextPayload(curByte, curIsLastByte, errorFlag)) {
                    // No TAP data yet
                    return;
                }
                syncCount   = SYNC_LEN_BYTES;  // leading SYNC
                frameBitPos = 0;
                txState     = TxState::LEAD_SYNC;
            } break;

            case TxState::LEAD_SYNC:
                if (syncCount == 0) {
                    // After leading SYNC: send start marker 0,1,0
                    frameBitPos = 0;
                    txState     = TxState::PREBITS_START;
                } else {
                    // Encode one SYNC byte (0xFF) = 8 ones
                    fmPushSyncByte(bitAccumulator, bitsInAcc, lastLineBit);
                    --syncCount;
                }
                break;

            case TxState::PREBITS_START: {
                // Send 0,1,0 as start marker
                static const uint8_t pre[3] = {0,1,0};
                fmPushEncodedBit(pre[frameBitPos],
                                 bitAccumulator, bitsInAcc, lastLineBit);
                ++frameBitPos;
                if (frameBitPos >= 3) {
                    frameBitPos = 0;
                    txState     = TxState::FRAME_BITS;
                }
            } break;

            case TxState::FRAME_BITS:
                if (frameBitPos < 8) {
                    // 8 data bits LSB-first
                    uint8_t bit = (curByte >> frameBitPos) & 1;
                    fmPushEncodedBit(bit, bitAccumulator, bitsInAcc, lastLineBit);
                } else if (frameBitPos < 11) {
                    // 3 bits "010" after each payload byte
                    static const uint8_t pre[3] = {0,1,0};
                    fmPushEncodedBit(pre[frameBitPos - 8],
                                     bitAccumulator, bitsInAcc, lastLineBit);
                }
                ++frameBitPos;

                if (frameBitPos >= 11) {
                    txState = TxState::NEXT_BYTE;
                }
                break;

            case TxState::NEXT_BYTE:
                if (curIsLastByte) {
                    // End of TAP record: send 11 ones
                    onesRemaining = 11;
                    txState       = TxState::END_ONES;
                } else {
                    // Need next payload byte
                    if (!tap.nextPayload(curByte, curIsLastByte, errorFlag)) {
                        // No data yet
                        return;
                    }
                    frameBitPos = 0;
                    txState     = TxState::FRAME_BITS;
                }
                break;

            case TxState::END_ONES:
                // 11 logical ones
                fmPushEncodedBit(1, bitAccumulator, bitsInAcc, lastLineBit);
                if (--onesRemaining == 0) {
                    // After 11 ones: send trailing SYNC
                    syncCount = SYNC_LEN_BYTES;
                    txState   = TxState::TRAIL_SYNC;
                }
                break;

            case TxState::TRAIL_SYNC:
                if (syncCount == 0) {
                    // Done with this record, go back to IDLE
                    txState = TxState::IDLE;
                } else {
                    fmPushSyncByte(bitAccumulator, bitsInAcc, lastLineBit);
                    --syncCount;
                }
                break;
        } // switch
    }     // while (bitsInAcc <= 42)
}

// =============== Arduino-style setup/loop ===============
void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    USB_SERIAL.begin();
    while (!USB_SERIAL.isConnected()) {
        // Wait for serial connection to host
    }

    SPI_DEV.begin();
    pinMode(SPI_CS_PIN, OUTPUT);
    digitalWrite(SPI_CS_PIN, LOW);

    // Initialize buffers and internal state
    resetBuffers();
}

void loop() {
    if (errorFlag) {
        DEBUG_LOG("Error state, stopping. (Would send 'E' on real device)\n");
        //USB_SERIAL.write('E');
        while (1) { 
            // Stay in error state
            delay(1000); 
        }
    }

    // Read all available bytes from host
    while (USB_SERIAL.available()) {
        DEBUG_LOG("Got serial\n");
        int c = USB_SERIAL.read();
        handleHostByte(c);
    }

    // Produce FM bits into the shared 64-bit bitAccumulator while there is room
    // for at least one more encoded payload byte (22 FM bits).
    produceFmBits();
}