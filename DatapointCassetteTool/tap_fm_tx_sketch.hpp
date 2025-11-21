/* 
 * tap_fm_tx_sketch.hpp (refactored with conditional debug)
 * ------------------------------------------------
 * Behavior / protocol is unchanged.
 *
 * Design recap:
 *  - Main loop:
 *      * Receives ASCII hex from host.
 *      * Converts to raw TAP bytes (leader + payload + trailer).
 *      * Streams raw TAP bytes into two 512-byte ping-pong buffers (A, B).
 *      * On 'S': starts record, sends initial 'A' once.
 *      * On 'T': marks any partially-filled buffer READY and stops.
 *      * Does NOT parse TAP, does NOT do framing, does NOT FM encode.
 *
 *  - ISR (onSpiTimerISR):
 *      * Called periodically (1 ms in Linux shim, or timer on MCU).
 *      * Pulls raw TAP bytes from ping-pong buffers.
 *      * Parses TAP:
 *          - 4-byte leader (length, little-endian).
 *          - 'length' payload bytes.
 *          - 4-byte trailer (length again, must match).
 *      * For each TAP record:
 *          - Before first payload byte:
 *              * 200 bytes of SYNC (0xFF) → "1" bits.
 *              * Then bits 0,1,0 as start marker.
 *          - For each payload byte:
 *              * 8 data bits (LSB-first),
 *              * then bits 0,1,0.
 *          - After last payload byte:
 *              * 11 bits of 1 (logical ones),
 *              * then 200 bytes of SYNC again.
 *      * FM encodes logical bits:
 *              0 → 01 or 10 depending on last line bit
 *              1 → 00 or 11 depending on last line bit
 *      * Packs FM bits into bytes and calls SPI.transfer() once per ISR.
 *
 *  - Flow control:
 *      * On 'S', main loop sends one initial 'A' so host can send first 512 bytes.
 *      * Each ping-pong buffer is 512 TAP BYTES.
 *      * When ISR finishes consuming a buffer (A or B), it:
 *          - Marks that buffer FREE.
 *          - Sends 'A' to host, meaning "one 512-byte chunk slot is free".
 *      * Host sends next 512 TAP bytes only after 'A'.
 *
 *  -> Result: clean 512-byte ping-pong between host and encoder.
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

HardwareTimer pwmtimer(1);
const int pwmOutPin = PA8; // pin10
// Connect PA8 to PB13 as the clock input for SPI2
SPIClass SPI_2(2);

// ================= Configuration =================
#define PIN_LED        PB1
#define USB_SERIAL     Serial
#define SPI_DEV        SPI_2
#define SPI_CS_PIN     PB12

#define SYNC_BYTE      0xFF       // 8 logical '1' bits
#define SYNC_LEN_BYTES 200        // 200 SYNC bytes before/after each record
#define BUF_SIZE       512        // Ping-pong buffer size in TAP bytes
#define TAP_LITTLE_ENDIAN 1       // TAP length is little-endian

// =============== Ping-Pong Buffer Model ===============

// Buffer state
enum class BufState : uint8_t {
    FREE,   // buffer available for writer to fill
    READY,  // buffer filled with TAP bytes, ready for ISR
    BUSY    // buffer currently being read by ISR
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
 * - Reader side is used by the ISR to read TAP bytes.
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
                // No FREE buffer; writer must stall until ISR frees one
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

    // ISR-side function: get the next TAP byte, if any.
    // Returns true and sets 'out' if a TAP byte is available.
    // Returns false if no READY/BUSY data is available.
    bool readTapByte(uint8_t& out) {
        while (true) {
            // 1) If we have a current buffer with remaining data, read from it
            if (read && readPos < read->len) {
                out = read->data[readPos++];

                if ((readPos % 64) == 0) {
                    DEBUG_LOG("[ISR] TAP byte from buf=%c pos=%zu len=%zu\n",
                              (read == &bufA) ? 'A' : 'B',
                              readPos, (size_t)read->len);
                }
                return true;
            }

            // 2) If the current buffer is exhausted, FREE it and send 'A' to host
            if (read && readPos >= read->len) {
                DEBUG_LOG("[ISR] finished buffer %c, len=%zu -> FREE + 'A'\n",
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

// =============== TAP parsing state (ISR) ===============

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

                        DEBUG_LOG("[ISR] TAP leader parsed: expectedLen=%u\n", expectedLen);
                    }
                    break;

                // ===================== Payload =====================
                case Phase::PAYLOAD:
                    if (gotPayload >= expectedLen) {
                        // Done with payload -> go to trailer
                        phase  = Phase::NEED_TRAILER;
                        tmpPos = 0;
                        DEBUG_LOG("[ISR] TAP payload complete in ISR, expecting trailer\n");
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
                            DEBUG_LOG("[ISR] TAP trailer mismatch: %u != %u\n",
                                      L2, expectedLen);
                            errorFlag = true;
                        } else {
                            DEBUG_LOG("[ISR] TAP trailer OK: %u\n", L2);
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
 *  - Keeps original signature so other code is simpler.
 */
static bool isrReadTapByte(uint8_t &out) {
    return pp.readTapByte(out);
}

// =====================================================
//  FM Encode helpers (used by ISR)
// =====================================================

/* 
 * fmPushEncodedBit:
 *  - FM-encodes one logical bit (0/1) based on lastLineBitRef.
 *  - Appends two FM bits to bitAccumulator (LSB-first).
 */
static inline void fmPushEncodedBit(
    uint8_t           srcbit,          // logical bit (0 or 1)
    uint32_t         &bitAccumulator,  // accumulator for FM bits (LSB-first)
    uint8_t          &bitsInAcc,       // number of valid bits in bitAccumulator
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
    bitAccumulator |= (uint32_t(out1) << bitsInAcc++);
    bitAccumulator |= (uint32_t(out2) << bitsInAcc++);
    lastLineBitRef = out2;  // update last line bit
}

/*
 * fmPushSyncByte:
 *  - Encodes one SYNC byte (0xFF) = 8 logical '1' bits.
 */
static inline void fmPushSyncByte(
    uint32_t         &bitAccumulator,  // accumulator for FM bits (LSB-first)
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
//  ISR: FM encoder + record framing + TAP parsing
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

void onSpiTimerISR() {
    static uint32_t isrCalls = 0;
    if (isrCalls < 20 || (isrCalls % 1000) == 0) {
        DEBUG_LOG("[ISR] tick %u bufA.state=%d lenA=%zu bufB.state=%d lenB=%zu readPos=%zu\n",
                  isrCalls,
                  (int)bufA.state, (size_t)bufA.len,
                  (int)bufB.state, (size_t)bufB.len,
                  pp.readPos);
    }
    ++isrCalls;

    // If we're in error state, stop transmitting
    if (errorFlag) {
        return;
    }

    // Accumulator for FM bits (LSB-first)
    static uint32_t bitAccumulator = 0;
    static uint8_t  bitsInAcc      = 0;

    // Current payload byte and flag if it's last in record
    static uint8_t curByte       = 0;
    static bool    curIsLastByte = false;

    // Framing state
    static uint8_t  frameBitPos   = 0;     // 0..10: 8 data bits + 3 bits "010"
    static uint8_t  onesRemaining = 0;     // remaining ones in END_ONES
    static uint16_t syncCount     = 0;     // remaining SYNC bytes

    // Record-level transmission state
    static TxState txState = TxState::IDLE;

    // Main loop: fill accumulator with at least 8 FM bits before sending one SPI byte
    while (bitsInAcc < 8) {
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
    } // while (bitsInAcc < 8)

    // Output exactly one SPI byte per ISR call
    if (bitsInAcc >= 8) {
        uint8_t outByte = (uint8_t)(bitAccumulator & 0xFF);
        SPI_DEV.transfer(outByte);
        bitAccumulator >>= 8;
        bitsInAcc      -= 8;
    }
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

// Reset buffers and TAP state
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

    // Reset TAP parser in ISR for this new record
    tap.reset();
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
}