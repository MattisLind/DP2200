// Prerelease version, 8/23/05
// Author: Jim Battle

// Updated / changed to run on 64bit systems Jos Dreesen 2023

// Bug in statemachine fixed by Mattis Lind 2023
// Added a peak detector which is much better in handling magnetic media 
// than a zero-crossing detector.

// This program reads a .WAV file and attempts to decode the data stream
// off that recording.  The audio format must be in RIFF WAV format,
// non-compressed, one or two channels.  It allows for mono/stereo input,
// any input sampling rate (lower sampling rates increase the decoding
// error rate), and 8b or 16b samples.

// The Datapoint 2200 records data bits using a FSK method, namely one
// half cycle at frequency X means a one bit and one full cycle at
// frequency 2X means a zero bit.  This is stated this way because
// the samples processed here are off a tape recorder running at 1 7/8 ips
// while the datapoint runs its tape at 7.5 ips, a factor of four times
// faster.  To be specific, at this reduced speed, a one bit is a half cycle
// of 481.25 Hz and a zero bit is a full cycle of 962.5 Hz.  The original
// 7.5 ips frequencies are 1925 Hz and 3850 Hz.  The preamble while the
// tape is coming up to speed is a train of 1 bits.
//
// The structure of the data on a tape is as follows:
//
//    (1) bytes are eight bits
//
//    (2) a sync pattern of 010 brackets each byte, so for example
//        two bytes would be (sync)(byte0)(sync)(byte1)(sync).  thus
//        the character rate is 350 cps at 7.5 ips.
//
//    (3) in the forward direction, bits of a byte are recorded
//        msb first, lsb last.
//
//    (4) a record is a sequence of bytes with valid sync patterns
//        followed by an all 1's byte and an invalid sync (of 111)
//
//    (5) the inter-record gap time is about 280 ms, or 98 character times
//
//    (6) trains of one bits are recorded before and after a record to
//        allow a PLL to track the signal and to form the end of record
//        marker at both sides of a byte of 1 bits and a sync of 111.

// There isn't a whole lot of high-powered thinking going on here.
// It is just a lot of guess work on my part as to what seems to work
// and what doesn't.  Start simple and add complexity as required.
//
// The program is structured to operate on a rolling window through
// the file, rather than doing the easier thing of sucking in the
// whole file and then operating on it.  This is because the files
// can last a few minutes, which would consume a prohibitive amount
// of memory.

#include <assert.h>
#include <stdarg.h> // for varargs
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <cmath>

// support dumping records as intel hex files
#define SUPPORT_HEX 1

// at least this many "1" bits must be s seen before a sync pattern in order
// to be taken seriously.  this is required because as the tape speed slows
// at the end of a record, sometimes false syncs are detected.
#define SYNC_THRESHOLD 32

// ========================================================================
// type definitions

typedef unsigned long uint32;
typedef long int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char uint8;
typedef char int8;

#define true 1
#define false 0

typedef int16 sample_t;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ABS(a) (((a) < (0)) ? (-a) : (a))

// ========================================================================
// global variables

// command line options
int opt_v;     // verbosity level
bool opt_x;    // explode mode
int opt_x_num; // chunk number
char *opt_ofn; // output filename
char *opt_ifn; // input filename
enum { OFMT_TAP = 0, OFMT_BIN, OFMT_HEX } opt_ofmt;

// .wav file attributes
bool inmono;             // input  file is mono (1) or stereo (0)
int sample_bytes;        // number of bytes per sample
uint32 expected_samples; // number of samples in file
uint32 sample_rate;      // samples/second

FILE *fIn; // input audio file handle

// period of a zero bit, in Hz, at 1 7/8 ips
//const float zero_freq = 1084.0f;
const float zero_freq = 1037.64f;
const float sampleRate = 44100.0f;
// pll lock range = nominal +/- 25%
const float lock_range = 0.25f;
float min_samples_per_bit;
float max_samples_per_bit;

float samples_per_bit; // bit period, in samples
float pll_period;      // current PLL estimate of bit period


uint32 nSamp = 0;

// ========================================================================
// filtered reporting

void tfprintf(int verbosity, FILE *fp, char *fmt, ...) {
  char buff[1000];
  va_list args;

  if (verbosity <= opt_v) {
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    fputs(buff, fp);
  }
}

void tprintf(int verbosity, const char *fmt, ...) {
  char buff[1000];
  va_list args;

  if (verbosity <= opt_v) {
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    fputs(buff, stdout);
  }
}

// ========================================================================
// WAV file parsing

// RIFF WAV file format
//  __________________________
// | RIFF WAVE Chunk          |
// |   groupID  = 'RIFF'      |
// |   riffType = 'WAVE'      |
// |    __________________    |
// |   | Format Chunk     |   |
// |   |   ckID = 'fmt '  |   |
// |   |__________________|   |
// |    __________________    |
// |   | Sound Data Chunk |   |
// |   |   ckID = 'data'  |   |
// |   |__________________|   |
// |__________________________|
//
// although it is legal to have more than one data chunk, this
// program assumes there is only one.

const uint32 RiffID = ('R' << 0) | ('I' << 8) | ('F' << 16) | ('F' << 24);
const uint32 WaveID = ('W' << 0) | ('A' << 8) | ('V' << 16) | ('E' << 24);
const uint32 FmtID = ('f' << 0) | ('m' << 8) | ('t' << 16) | (' ' << 24);
const uint32 DataID = ('d' << 0) | ('a' << 8) | ('t' << 16) | ('a' << 24);

void errex(const char *msg) {
  fprintf(stderr, "Error: %s\n", msg);
  exit(-1);
}

// make sure the file just opened is a WAV file, and if so,
// read some of the critical parameters
void CheckHeader(void) {
  uint32 groupID = 0, riffBytes = 0, riffType = 0, chunkID = 0;
  int32 ChunkSize = 0;
  int16 FormatTag = 0;  // 1=uncompressed
  uint16 Channels = 0;  // number of audio channels
  uint32 Frequency = 0; // sample frequency
  uint32 AvgBPS;        // we'll ignore this
  uint16 BlockAlign;    // we'll ignore this
  uint16 BitsPerSample;

  //  RIFF Header
  if (fread(&groupID, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (groupID != RiffID)
    errex("Error: input file not a RIFF file");
  if (fread(&riffBytes, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (fread(&riffType, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (riffType != WaveID)
    errex("input file not a WAV file");

  // Format definition
  if (fread(&chunkID, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (chunkID != FmtID)
    errex("Missing format definition");
  if (fread(&ChunkSize, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (fread(&FormatTag, 2, 1, fIn) != 1)
    errex("Cant' read file");
  if (FormatTag != 1)
    errex("Error: can't deal with compressed data\n");
  if (fread(&Channels, 2, 1, fIn) != 1)
    errex("Cant' read file");
  if (Channels != 1 && Channels != 2)
    errex("Error: can't handle too many channels");
  inmono = (Channels == 1);
  if (fread(&Frequency, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (fread(&AvgBPS, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (fread(&BlockAlign, 2, 1, fIn) != 1)
    errex("Cant' read file");
  if (fread(&BitsPerSample, 2, 1, fIn) != 1)
    errex("Cant' read file");

  if (BitsPerSample == 8)
    sample_bytes = 1;
  else if (BitsPerSample == 16)
    sample_bytes = 2;
  else
    errex("samples must be either 8b or 16b\n");

  sample_rate = Frequency;
  if (Frequency < 11000)
    errex("Warning: the sample rate is low -- it might hurt conversion");

  // Data section
  if (fread(&chunkID, 4, 1, fIn) != 1)
    errex("Cant' read file");
  if (chunkID != DataID)
    errex("file didn't contain a DATA header");
  if (fread(&ChunkSize, 4, 1, fIn) != 1)
    errex("Cant' read file");

  // compute dependent parameters
  expected_samples = ChunkSize / (sample_bytes * Channels);

  // period, in samples
  samples_per_bit = (float)sample_rate / zero_freq;

  // pll lock range = nominal +/- 25%
  pll_period = samples_per_bit; // start at nominal
  min_samples_per_bit = samples_per_bit * (1.0f - lock_range);
  max_samples_per_bit = samples_per_bit * (1.0f + lock_range);

  float sec = (float)expected_samples / sample_rate;
  tprintf(1, "File: '%s'\n", opt_ifn);
  tprintf(1, "WAV format: %d samples/sec, %db %s\n", sample_rate,
          8 * sample_bytes, inmono ? "mono" : "stereo");
  tprintf(1, "Expected # of samples: %ld", expected_samples);
  tprintf(1, " (%.2f seconds)\n", sec);
}

// return a mono sample from the input file.
// if the input file is in stereo, it averages the two channels.
// this routine reads blocks for efficiency, but doles out one sample
// per request.
sample_t GetMonoSample(void) {
  sample_t newsamp;

  if (inmono && (sample_bytes == 1)) {
    // mono 8b
    int8 b0 = (int8)fgetc(fIn) - 128;
    newsamp = (sample_t)(b0 << 8);
  } else if (inmono && (sample_bytes == 2)) {
    uint8 b0 = (uint8)fgetc(fIn);
    int8 b1 = (int8)fgetc(fIn);
    newsamp = (sample_t)((b1 << 8) + b0);
  } else if (!inmono && (sample_bytes == 1)) {
    // 8b stereo
    int8 b0 = fgetc(fIn) - 128;
    int8 b1 = fgetc(fIn) - 128;
    newsamp = (sample_t)((b0 + b1) << 7);
  } else if (!inmono && (sample_bytes == 2)) {
    // 16b stereo
    uint8 b0 = (uint8)fgetc(fIn);
    int8 b1 = (int8)fgetc(fIn);
    uint8 b2 = (uint8)fgetc(fIn);
    int8 b3 = (int8)fgetc(fIn);
    sample_t left = (sample_t)((b1 << 8) + b0);
    sample_t right = (sample_t)((b3 << 8) + b2);
    newsamp = (left + right + 1) >> 1;
  }

  return newsamp;
}

// =========================================================================
// we maintain in memory a WORKBUFSIZE window of samples.
// the tricky part is that the start of the buffer might be anywhere
// in the buffer, and all other addressing is modulo the buffer size.
//
// if any access is made to the last BUMP samples, we roll the window
// over by QTRWORKBUF samples.

// number of samples we are holding in memory
#define WORKBUFSIZE 16384             // make this a power of two
#define WORKBUFMASK (WORKBUFSIZE - 1) // for modulo wrapping
#define HALFWORKBUF (WORKBUFSIZE / 2)
#define QTRWORKBUF (WORKBUFSIZE / 4)
#define BUMP 1024

int32 windowstart;  // the oldest sample in the buffer
int32 windowoffset; // where in the window the oldest sample lives

// this holds the samples we are working on
sample_t inbuf[WORKBUFSIZE];

int sample_addr(int32 n) {
#if 1
  if (n < windowstart || n >= windowstart + WORKBUFSIZE) {
    fprintf(stderr, "Error: sample %ld, workbuf[] access out of range\n", n);
    exit(-1);
  }
#endif

  // see if we are bumping into the end of the buffer
  if (n >= windowstart + WORKBUFSIZE - BUMP) {
    // move the input buffer window forward 'n' samples.
    // we do this through modulo address arithmetic;
    // we don't actually move the samples around.

    // adjust pointers
    windowstart += QTRWORKBUF;
    windowoffset = (QTRWORKBUF + windowoffset) & WORKBUFMASK;

    // fill up newly exposed portion of buffer
    int off = (windowoffset + 3 * QTRWORKBUF) & WORKBUFMASK;
    for (int32 t = off; t < off + QTRWORKBUF; t++)
      inbuf[t] = GetMonoSample();
  }

  return (n - windowstart + windowoffset) & WORKBUFMASK;
}

#define GETIN(n) (inbuf[sample_addr(n)])

// at time zero, we initialize the first half of the buffer
// to whatever value the first sample has, then fill the rest
// of the buffer from samples from the file.
void FillInbuffer(void) {
  sample_t s;
  int t;

  // at time zero, we put the first sample right in the middle
  windowstart = -HALFWORKBUF;
  windowoffset = 0;

  // fill the first half of the buffer with the first file sample
  s = GetMonoSample();
  for (t = 0; t <= HALFWORKBUF; t++)
    inbuf[t] = s;

  // now fill up the rest
  for (t = HALFWORKBUF + 1; t < WORKBUFSIZE; t++)
    inbuf[t] = GetMonoSample();
}
#define OBUFFER_SIZE 16384
uint8 obuff_buffer[OBUFFER_SIZE];
int obuff_bytecnt; // put pointer into obuff_buffer[]

int isFileHeader(unsigned char * buffer) {
  if ((obuff_buffer[0]==0201) && (obuff_buffer[1]==0176)) {
    return 1;
  } 
  return 0;
}

int isNumericRecord(unsigned char * buffer) {
  if ((obuff_buffer[0]==0303) && (obuff_buffer[1]==0074)) {
    return 1;
  } 
  return 0;
}

int isSymbolicRecord(unsigned char * buffer) {
  if ((obuff_buffer[0]==0347) && (obuff_buffer[1]==0030)) {
    return 1;
  } 
  return 0;
}

int isChecksumOK(unsigned char * buffer, int size) {
  unsigned char xorChecksum;
  unsigned char circulatedChecksum;
  int i;
  xorChecksum = obuff_buffer[2];
  circulatedChecksum = obuff_buffer[3];
  for (i=4;i<size;i++) {
    int lowestBit;
    xorChecksum ^= obuff_buffer[i];
    circulatedChecksum ^=buffer[i];
    lowestBit = 0x01 & circulatedChecksum;
    circulatedChecksum >>= 1;
    circulatedChecksum |= (0x80 & (lowestBit<<7));
  }
  //printf("xorChecksum: %02X circulatedChecksum: %02X .", xorChecksum, circulatedChecksum);
  if ((xorChecksum == 0) && (circulatedChecksum == 0)) return 1;
  return 0;
}


void parseDPFormat() {
  int startingAddress;
  if (isFileHeader(obuff_buffer)) {
  tprintf(1, "This is a FileHeader. ");
  if (obuff_bytecnt != 4) {
    tprintf(1, "The size of this block is incorrect. It is %d bytes rather than 4 bytes. ", obuff_bytecnt);
  }
  tprintf(1, "The filenumber is %d. ", 0xff&obuff_buffer[2]);
  if ((0xff&obuff_buffer[2])!=(0xff&(~obuff_buffer[3]))) {
    tprintf(1, "The inverted filenumber is incorrect: %d should have been %d.", 0xff&obuff_buffer[3], 0xff&(~obuff_buffer[2]));
  }
  if (obuff_buffer[2]==127) {
    tprintf(1,"This is the end of tape marker. Files beyond this point is probably damaged.");
  }
  tprintf(1,"\n");
} else if (isNumericRecord(obuff_buffer)) {
  tprintf(1,"This is a numeric record with %d bytes. ", obuff_bytecnt);
  if (!isChecksumOK(obuff_buffer, obuff_bytecnt)) {
    tprintf(1, "The checksum is NOT OK.");
  }
  startingAddress = (obuff_buffer[4] << 8) | obuff_buffer[5];
  tprintf (1, "Load address for this block is %05o. ", startingAddress);
  if ((obuff_buffer[4] != (0xFF&~obuff_buffer[6])) || (obuff_buffer[5] != (0xFF&~obuff_buffer[7]))) {
    tprintf(1, "Loading address corrupted.");
  }
  tprintf(1, "\n");

} else if (isSymbolicRecord(obuff_buffer)) {
  tprintf(1, "This is a symbolic record with %d bytes. ", obuff_bytecnt);
  tprintf(1, "\n");
} else {
  // something else - should be the first block which is the boot block - 
  tprintf(1, "This is something else. Only the first boot block should be like this. ");
  tprintf(1, "\n");
}
}


// =========================================================================
// file output routines
//
// these routines accept notification of
//    (1) start of a block
//    (2) next received byte
//    (3) error
//    (4) end of block
// based on those events and the global option settings, they
// emit one or more files in the specified format, which may be
//    (a) .tap format
//    (b) .bin format
#if SUPPORT_HEX
//    (c) .hex format
#endif

// i'm not sure what the maximum sized block might be.
// from a small sample of tapes, the empirical maximum is 512 bytes.
// this is much larger than that of course, and is the same size as
// the maximum size memory in a 2200.



FILE *obuff_fp = NULL;

void StreamStart(void) { obuff_bytecnt = 0; }

void StreamByte(uint8 byte) {
  assert(obuff_bytecnt < OBUFFER_SIZE);
  obuff_buffer[obuff_bytecnt++] = byte;
}

void StreamWriteBlkLen(uint32 v) {
  fputc(((v >> 0) & 0xFF), obuff_fp);
  fputc(((v >> 8) & 0xFF), obuff_fp);
  fputc(((v >> 16) & 0xFF), obuff_fp);
  fputc(((v >> 24) & 0xFF), obuff_fp);
}

void StreamEnd(void) {
  int len = strlen(opt_ofn) + 3; // +3 for safety
  char *ofn = (char *)malloc(len);
  assert(ofn != NULL);
  int n;

  // when to open the file
  bool open_it = opt_x ||                  // explode requested
                 (opt_ofmt != OFMT_TAP) || // bin or hex
                 (opt_x_num == 0);         // first

  // when to close the file
  bool close_it = opt_x ||                // explode requested
                  (opt_ofmt != OFMT_TAP); // bin or hex

  if (opt_x) {
    snprintf(ofn, len, opt_ofn, opt_x_num); // serialize filename
  } else {
    strcpy(ofn, opt_ofn);
  }
  opt_x_num++;

  if (open_it) {
    // open a file
    obuff_fp = fopen(ofn, "wb");
    if (obuff_fp == NULL) {
      fprintf(stderr, "Error: couldn't open file '%s'\n", ofn);
      exit(-1);
    }
  }
  parseDPFormat();
  switch (opt_ofmt) {

  case OFMT_BIN: // binary
    for (n = 0; n < obuff_bytecnt; n++) {
      int r = fputc(obuff_buffer[n], obuff_fp);
      assert(r != EOF);
    }
    break;

  case OFMT_TAP: // simh format
    // from somewhere on the web:
    //   In this format each tape record is preceded and followed by
    //   a 4-byte count of the number of bytes in the record. Thus,
    //   between two records there will be the count for the previous
    //   record and the count for the next record.
    // another source confirms this, but with the caveat that zero
    // length blocks don't repeat the length twice.
    StreamWriteBlkLen(obuff_bytecnt);
    for (n = 0; n < obuff_bytecnt; n++) {
      int r = fputc(obuff_buffer[n], obuff_fp);
      assert(r != EOF);
    }
    if (obuff_bytecnt != 0)
      StreamWriteBlkLen(obuff_bytecnt);
    break;

#if SUPPORT_HEX
  case OFMT_HEX: // intel hex format
    for (n = 0; n < obuff_bytecnt; n += 16) {
      int bytesleft = MIN(16, obuff_bytecnt - n);
      int cksum;
      fprintf(obuff_fp, ":%02X%04X%02X", bytesleft, n, 0x00);
      cksum = bytesleft + (n >> 0) + (n >> 8) + (n >> 16) + (n >> 24) + 0x00;
      for (int nn = 0; nn < bytesleft; nn++) {
        uint8 b = obuff_buffer[n + nn];
        fprintf(obuff_fp, "%02X", b);
        cksum += b;
      }
      fprintf(obuff_fp, "%02X\n", (256 - cksum) & 0xFF);
    }
    fprintf(obuff_fp, ":00000001FF\n");
    break;
#endif

  default:
    assert(0);
  }

  if (close_it) {
    fclose(obuff_fp);
    obuff_fp = NULL;
  }

  free(ofn);
}

void StreamError(int code) {
  // we sometimes get erroneous syncs as the tape speed slows down if we
  // don't see at least one valid byte after the sync, don't save the block
  if (obuff_bytecnt > 0)
    StreamEnd();
}

void StreamDone(void) {
  if (obuff_fp != NULL)
    fclose(obuff_fp);
}

// =========================================================================
// bitstream decoder

#define BH_SIZE (128) // size of bithistory buffer
#define BH_MASK (BH_SIZE - 1)

enum {
  BS_LOST = 0,
  BS_PREAMBLE,    // in a train of 1 bits
  BS_PREAMBLE_0,  // train of 1s followed by 0
  BS_PREAMBLE_01, // train of 1s followed by 0,1
  BS_BYTE,        // decoding byte stream
  BS_GAP
};

const char *stateStrings[] = {"BS_LOST",        "BS_PREAMBLE", "BS_PREAMBLE_0",
                              "BS_PREAMBLE_01", "BS_BYTE",     "BS_GAP"};

int BSstate = BS_LOST;
int BSbyteCount;

void Bit(uint32 time, int bit) {
  static int count;
  static int bits;

  static int bithistory[BH_SIZE];
  static int bh_put = 0;
  static int bh_get = 0;
  static int bh_vld = 0;

  tprintf(3, "Bit: BSstate = %s sample %d: decoded bit %d  count=%d (%d) \n", stateStrings[BSstate], time, bit, count, SYNC_THRESHOLD);

  bithistory[bh_put] = bit;
  bh_put = (bh_put + 1) & BH_MASK;
  bh_vld = MIN(bh_vld + 1, BH_SIZE);

  switch (BSstate) {

  case BS_LOST:
    if (bit == 1) {
      BSstate = BS_PREAMBLE;
      count = 1;
    }
    break;

  case BS_PREAMBLE:
    if (bit == 1) {
      count++;
    } else if ((bit == 0) && (count > SYNC_THRESHOLD))
      BSstate = BS_PREAMBLE_0;
    else {
      pll_period = samples_per_bit;
      BSstate = BS_LOST;
    }
 
    break;

  case BS_PREAMBLE_0:
    if (bit == 1) {
      BSstate = BS_PREAMBLE_01;
    } else {
      tprintf(2, "sample %d: preamble lost after %d bits\n", time, count + 2);
      pll_period = samples_per_bit;
      BSstate = BS_LOST;
    }
    break;

  case BS_PREAMBLE_01:
    if (bit == 0) {
      tprintf(1, "sample %d: preamble and sync after %d bits\n", time,
              count + 3);
      BSstate = BS_BYTE;
      count = 0;
      BSbyteCount = 0;
      bits = 0x00;
      StreamStart();
    } else {
      tprintf(2, "sample %d: preamble lost after %d bit %d\n", time, count + 3);
      BSstate = BS_PREAMBLE;
      count = 2;
    }
    break;

  case BS_BYTE:
    bits = (bits << 1) | (bit > 0);
    count++;
    if (count == 11) {
      bits &= 0x7FF;
      if ((bits & 7) == 2) { // ... 010 sync code
        bits = (bits >> 3) & 0xFF;
        // in fwd direction, data comes off the tape lsb first
        bits = ((bits & 0x01) << 7) | ((bits & 0x02) << 5) |
               ((bits & 0x04) << 3) | ((bits & 0x08) << 1) |
               ((bits & 0x10) >> 1) | ((bits & 0x20) >> 3) |
               ((bits & 0x40) >> 5) | ((bits & 0x80) >> 7);
        tprintf(2, "sample %d: byte 0x%02X (%03o)\n", time, bits, bits);
        StreamByte(bits);
        count = 0;
        BSbyteCount++;
      } else if (bits == 0x7FF) {
        tprintf(1, "sample %d: hit valid gap after %d bytes, PLL period=%f @%ld\n",
                time, BSbyteCount, pll_period, nSamp);
        if (obuff_bytecnt > 0) StreamEnd();
        count = 0;
        BSstate = BS_GAP;
      } else {
        if (BSbyteCount == 0) {
          tprintf(1,
                  "sample %d: bad sync code %03X; apparently it was not a "
                  "valid sync @%ld\n",
                  time, bits & 7, nSamp);
        } else {
          tprintf(1, "sample %d: bad sync code %03X\n @%ld", time, bits & 7, nSamp);
        }
        tprintf(1, "Bad block @%ld\n", nSamp);
        StreamError(0);
        pll_period = samples_per_bit;
        BSstate = BS_LOST;
      }
    }
    break;

  case BS_GAP:
    count++;
    if (bit == 2) {
      tprintf(2, "sample %d: skipped %d bits to mid-gap\n", time, count);
      pll_period = samples_per_bit;
      BSstate = BS_LOST;
    }
    if (bit == 1) {
      BSstate = BS_PREAMBLE;
      count = 1;
    }
    break;

  default:
    assert(0);
    break;
  }
}

// =========================================================================
// bit decoder

// sequentially the stream of transitions and turn them into a stream of
// bits.  of course, this routine must guard against illegal transitions
// that are bound to come up.

int ClassifyTransition(uint32 time, uint32 duration) {
  tprintf(3, "ClassifyTransition: pll_period=%f duration=%d Threshold short%2.3f Long Threshold=%2.3f \n", pll_period, duration, 0.75f * pll_period, 1.50f * pll_period);
  if (duration < 0.25f * pll_period)
    return -1; // too short

  if (duration < 0.75f * pll_period)
    return 0; // a half "zero" bit

  if (duration < 1.50f * pll_period)
    return 1; // a "one" bit

  return 2; // too long
}

// update the phase lock loop

// we want the phase detector to lock in on the right phase without
// being too responsive nor too slow.  this is just a guess.
// it is the amount the pll phase is adjusted at each bit cell time
// as a fraction of the error between expected and actual.
const float pll_bump = 0.15f;

void PLL(int duration) {
  const float phaseDiff = duration - pll_period;

  pll_period += phaseDiff * pll_bump;
  pll_period = MAX(min_samples_per_bit, pll_period);
  pll_period = MIN(max_samples_per_bit, pll_period);

  tprintf(4, "pll period = %f\n", pll_period);
}

void DecodeBits(uint32 time) {
  //if (time== 1076874) time=1076876;
  //if (time==1018014) time=1018002;
  //if (time==908698)time=908691;
  //if (time==908820) time=908821;
  //if (time==908852) time=908854;
  enum XXX { DB_INIT = 0, DB_HALF_BIT, DB_READY };
  const char * stateStr[] = {"DB_INIT", "DB_HALF_BIT", "DB_READY"};
  static enum XXX DBstate = DB_INIT;
  static int prevTime = 0; // end of previous bit
  static int halfTime = 0; // end of half bit
  int type;
  tprintf(3, "DecodeBits %lu DBstate = %s prevTime=%d halfTime=%d\n", time, stateStr[DBstate], prevTime, halfTime);
  switch (DBstate) {

  case DB_INIT:
    prevTime = time;
    DBstate = DB_READY;
    break;

  case DB_HALF_BIT:
    type = ClassifyTransition(time, time - halfTime);
    tprintf(3, "DB_HALF_BIT DecodeBits: time=%lu (time-halfTime)=%lu ClassifyTransition returned %d\n", time,time - halfTime, type);
    switch (type) {

    case -1:
      if ((BSstate == BS_BYTE) && (BSbyteCount > 0))
        tprintf(1, "sample %d: Warning: runt pulse when short pulse expected\n",
                time);
      Bit(time, -1);        // completed second short period
      PLL(time - prevTime); // correct for phase error
      DBstate = DB_READY;
      break;

    case 0:
      Bit(time, 0);         // completed second short period
      PLL(time - prevTime); // correct for phase error
      DBstate = DB_READY;
      break;

    case 2:
      if ((BSstate == BS_BYTE) && (BSbyteCount > 0))
        tprintf(1, "sample %d: Warning: really long pulse\n", time);
      Bit(time, 2);
      PLL(time - prevTime); // correct for phase error
      prevTime = time;
      DBstate = DB_READY;
      break;

    case 1:
      if ((BSstate == BS_BYTE) && (BSbyteCount > 0))
        tprintf(1, "sample %d: long pulse when short pulse expected\n", time);
      Bit(time, 1);
      PLL(time - prevTime); // correct for phase error
      prevTime = time;
      DBstate = DB_READY;
      break;

    default:
      assert(0);
      break;
    }
    prevTime = time;
    break;

  case DB_READY:
    type = ClassifyTransition(time, time - prevTime);
    tprintf(3, "DB_READY DecodeBits: time=%lu (time-prevTime)=%lu ClassifyTransition returned %d\n", time,time - prevTime, type);
    switch (type) {

    case -1:
      if ((BSstate == BS_BYTE) && (BSbyteCount > 0))
        tprintf(1, "sample %d: Warning: runt pulse\n", time);
      Bit(time, -1);
      halfTime = time;
      DBstate = DB_READY;
      break;

    case 0:
      halfTime = time;
      DBstate = DB_HALF_BIT;
      break;

    case 2:
      if (BSstate == BS_BYTE)
        tprintf(1, "sample %d: Warning: long pulse\n", time);
      Bit(time, 2);
      PLL(time - prevTime); // correct for phase error
      prevTime = time;
      break;

    case 1:
      Bit(time, 1);
      PLL(time - prevTime); // correct for phase error
      prevTime = time;
      break;

    default:
      assert(0);
      break;
    }
    break;

  default:
    assert(0);
    break;
  }
}

// =========================================================================
// find the duration of each flux zone

#if 0
// detect zero crossings directly
void
FindTransitions()
{
    bool     bFirst = true;	// first transition seen
    sample_t prevSamp;		// previous sample

    prevSamp = GETIN(0);
    for(uint32 nSamp=1; nSamp<expected_samples; nSamp++) {
	sample_t samp = GETIN(nSamp);
	if ((samp < 0) ^ (prevSamp < 0)) {
	    // a sign transition has occurred
	    if (!bFirst)
		DecodeBits(nSamp);
	    else
		bFirst = false;
	    prevSamp = samp;
	}
    }

    StreamDone();
}
#else
#if 0
void FindTransitions() {
  int state = 0;
  sample_t lastSample = GETIN(0), sample;
  sample_t diff;
  for (uint32 nSamp = 1; nSamp < expected_samples; nSamp++) {
    sample = GETIN(nSamp);
    diff = sample - lastSample;
    lastSample = sample;   
    tprintf(3, "sample=%lu DIFF=%d\n", nSamp, abs(diff)); 
    if ((state == 0) && (abs(diff) < 1000)) {
      state = 1;      
      DecodeBits(nSamp);
    }
    if ((state == 1) && (abs(diff) > 1500)) {
      state = 0;
    }
  }
}
#endif
#if 0


bool withinWindow(int * windowState, int lastTrans, int currentTrans) {
  tprintf(3, "windowState=%d length=%d\n", *windowState,currentTrans-lastTrans);
  if (*windowState==0) {
    if (((currentTrans-lastTrans)>16) && ((currentTrans-lastTrans)<65)) {
      *windowState = 1;
    }
    return true;
  } else {
    if ((currentTrans-lastTrans)<=16) {
      return false;
    } else if (((currentTrans-lastTrans)>16) && ((currentTrans-lastTrans)<65)) {
      return true;
    } else {
      *windowState = 0;
      return true;
    }
  }
}

/*bool diffCompare (sample_t previousDiff, sample_t diff) {
  if ((previousDiff > 0) && (diff <= 0)) {
    return true;
  }
  if ((previousDiff > 0) && (diff <= 0)) {
    return true;
  }
}*/
void FindTransitions() {
  int state = 0; // 0 Waiting for high 1 Waiting for low
  int windowState = 0;
  int lastTransition=0;
  sample_t sample_nminus3 = GETIN(0);
  sample_t sample_nminus2 = GETIN(1);
  sample_t sample_nminus1 = GETIN(2);
  sample_t sample_n = GETIN(3);
  sample_t filteredSample= sample_nminus2/3 + sample_nminus1/3 + sample_n/3;
  sample_t filteredPreviousSample = sample_nminus3/3 + sample_nminus2/3 + sample_nminus1/3;
  sample_t diff = filteredSample-filteredPreviousSample;
  sample_t previousDiff, lowest, highest, diff2;
  double distance;
  int highIndex, lowIndex, indexDistance;
  std::vector<sample_t> buffer;
  buffer.push_back(sample_nminus3);
  buffer.push_back(sample_nminus2);
  buffer.push_back(sample_nminus1);
  buffer.push_back(sample_n);
  uint32 nSamp;
  for (nSamp = 4; nSamp < expected_samples; nSamp++) {
    sample_nminus2 = sample_nminus1;
    sample_nminus1 = sample_n;
    sample_n=GETIN(nSamp);
    buffer.push_back(sample_n);
    if (buffer.size()>50) {
      buffer.erase(buffer.begin());      
    }
    previousDiff = diff;
    filteredPreviousSample=filteredSample;
    filteredSample=sample_nminus2/3 + sample_nminus1/3 + sample_n/3;
    diff = filteredSample-filteredPreviousSample;
    tprintf(3, "sampleIndex=%lu filteredPreviousSample=%d filteredSample=%d previousDiff=%d diff=%d windowState=%d state=%d\n", nSamp, filteredPreviousSample, filteredSample, previousDiff, diff, windowState, state);
    if ((state == 0) || ( (windowState == 0) &&  filteredSample>0 )) {
      // Trying to find the high peak
      if (((previousDiff > 0) && (diff <= 0)) && withinWindow(&windowState,lastTransition, nSamp)) {
        // Got a high
        int i;
        state = 1;
        lastTransition = nSamp-1;
        highest=filteredPreviousSample;
        highIndex = nSamp-1;
        
        distance = (double) (highest-lowest);
        indexDistance = highIndex-lowIndex;
        for (i=buffer.size()-1; i>0; i-- ) {
          double d = (double) (abs(buffer[i]-buffer[i-1]));
          double relDiff = d/distance;
          tprintf(3, "Searching for knee: i=%d d=%f relDiff=%f distance=%f\n", i ,d, relDiff, distance);
          if (relDiff > 0.0125f) break;
        }
        DecodeBits(nSamp+i-buffer.size());  
        tprintf(3, "Find High: sampleIndex=%lu highIndex=%lu lowIndex=%lu distance=%f lowest=%d highest=%d width=%d report to decodebits=%d\n", nSamp, highIndex, lowIndex, distance, lowest, highest, indexDistance, nSamp-(i - buffer.size()));
      } 
    } 
    if ((state == 1) || ( (windowState == 0) && (filteredSample < 0)) ){
      // Trying to find bottom low.
      if (((previousDiff < 0) && (diff >= 0)) && withinWindow(&windowState,lastTransition, nSamp)) {
        // Got a low 
        int i;
        state = 0;
        lastTransition = nSamp-1;
        lowest=filteredPreviousSample;
        lowIndex = nSamp-1;   
        distance = (double) (highest-lowest);  
        indexDistance = lowIndex-highIndex; 
        for (i=buffer.size()-1; i>0; i-- ) {
          double d = (double) (abs(buffer[i]-buffer[i-1]));
          double relDiff = d/distance;
          if (relDiff > 0.025f) break;
        }
        DecodeBits(nSamp+i-buffer.size());         
        tprintf(3, "Find Low: sampleIndex=%lu highIndex=%lu lowIndex=%lu distance=%f lowest=%d highest=%d width=%d report to decodebits=%d\n", nSamp, highIndex, lowIndex, distance, lowest, highest, indexDistance, nSamp-(i - buffer.size()));
      } 
    }    
  }

}
#endif

#if 1

// New decoder
// Two type of decoding. 
// - If BS_LOST we search a 80 samples window for two peaks that has approximatelt 40 samples inbewteen.
//   Given by zero_freq / 44100 .
//   Peaks shall be of different type. I.e. one high peak and one low peak.
// - IF !BS_LOST we search a window from current peak and around 50 samples forward.
//   Use 1.25 * zero_freq / 44100 / 2. So 25 % longer than a long pulse.
//   Find all peaks during this interval. Give score to how close to nominal pulse they are. The closer the higher score.
//   Give higher score for peak that is longer distance from curent peak aplitude-wise
//   There are two nominal position to score. Closest to the short pulse peak or shortest to the high pulse peak.
//   Find the peak that gives the best score.
//
//   Find the absolute high and abolute low in the interval.
//   Use this absoulte scale to create a relative scale of the signal
//   Search for the knee on the front-porch of the signal where the slope is sufficently low. 
//   Compare this derivative with in relative scale to a fixed value


struct S {
  sample_t s;
  double scaled;
  uint32 index;
  bool highest;
  bool lowest;
  double distance;
  bool peak;
  bool highPeak;
};

typedef struct S Sample;

typedef std::vector<Sample> SampleBuffer;

struct HL {
  sample_t highValue;
  sample_t lowValue;
  uint32 highIndex;
  uint32 lowIndex;
  uint32 highBufferIndex;
  uint32 lowBufferIndex;
};

typedef struct HL HiLo;

HiLo findHighestLowest (SampleBuffer * sb) {
  HiLo hilo;
  hilo.highValue = (*sb)[0].s;
  hilo.lowValue = (*sb)[0].s;;
  hilo.highIndex = (*sb)[0].index;
  hilo.lowIndex = (*sb)[0].index; 
  hilo.highBufferIndex = 0;
  hilo.lowBufferIndex = 0; 
  uint32 currentBufferIndex;
  for (uint32 i=1; i< sb->size(); i++) {
    if ((*sb)[i].s > hilo.highValue) {
      hilo.highValue = (*sb)[i].s;
      hilo.highIndex = (*sb)[i].index;
      hilo.highBufferIndex = i;
    }
    if ((*sb)[i].s < hilo.lowValue) {
      hilo.lowValue = (*sb)[i].s;
      hilo.lowIndex = (*sb)[i].index;
      hilo.lowBufferIndex = i;
    }    
  } 
  tprintf(3, "Highest in buffer is %d at %ld and Lowest is %d at %ld\n", hilo.highValue, hilo.highIndex, hilo.lowValue, hilo.lowIndex);
  return hilo;
}

struct P {
  sample_t value;
  bool hiPeak;
  uint32 bufferIndex;
  uint32 index;
  double scaled;
};

typedef std::vector<struct P> Peaks;

void savePeak(SampleBuffer * sb, Peaks * ps, bool highPeak, uint32 i) {
  struct P p;
  (*sb)[i+1].peak = true;
  (*sb)[i+1].highPeak = highPeak;
  p.value = (*sb)[i+1].s;
  p.hiPeak = highPeak;
  p.index = (*sb)[i+1].index;
  p.scaled = (*sb)[i+1].scaled;
  p.bufferIndex = i+1;
  ps->push_back(p);
  tprintf(3, "%s peak found at %ld unscaled=%d scaled=%f\n", p.hiPeak?"High":"Low", p.index, p.value, p.scaled);
}

Peaks findPeaks (SampleBuffer * sb) {
  Peaks ps;
  
  sample_t diff1, diff2;
  
  for (uint32 i=1; i< sb->size()-2; i++) {
    diff1 = (*sb)[i+1].s - (*sb)[i].s;
    diff2 = (*sb)[i+2].s - (*sb)[i+1].s;
    if (diff1<0 && diff2>=0) { // Low local maxima
      savePeak(sb, &ps, false, i);
    } else if (diff1>0 && diff2 <=0) { // high local maxima
      savePeak(sb, &ps, true, i);  
    } else {
      (*sb)[i+1].peak = false;  
    }     
  } 
  return ps;
}

void fillSampleBuffer (uint32 * nSamp, SampleBuffer * sb, int count) {
  // First remove old samples
  sb->clear();
  for (int i = 0; i < count && *nSamp < expected_samples; i++, (*nSamp)++) {
    Sample t;
    t.scaled = 0.0;
    t.index = *nSamp;
    t.highest = false;
    t.lowest = false;
    t.distance = 0.0;
    t.peak = false;
    t.highPeak = false;
    sample_t s=GETIN(*nSamp);
    t.s = s;
    tprintf(3, "Getting nSamp=%ld sample=%d\n", *nSamp, s); 
    sb->push_back(t);
  }  
}

void fillSampleBuffer (uint32 * nSamp, SampleBuffer * sb, int count, uint32 startIndex) {
  // First remove old samples
  tprintf(3, "Clearing sampleBuffer from %d to %d\n", (sb->begin())->index, startIndex);
  while ((sb->begin())->index < startIndex) {
    sb->erase(sb->begin()); 
  }
  count -= sb->size();
  for (int i = 0; i < count && *nSamp < expected_samples; i++, (*nSamp)++) {
    sample_t s=GETIN(*nSamp);
    Sample t;
    t.s = s;
    t.scaled = 0.0;
    t.index = *nSamp;
    t.highest = false;
    t.lowest = false;
    t.distance = 0.0;
    t.peak = false;
    t.highPeak = false;
    tprintf(3, "Getting nSamp=%ld sample=%d\n", *nSamp, s); 
    sb->push_back(t);
  }  
}

void scaleBuffer (SampleBuffer * sb, double scalingValue, sample_t lowest) {
  for (int i=0; i<sb->size(); i++) {
    (*sb)[i].scaled = (( ((double)((*sb)[i].s))-((double)lowest)))/  scalingValue;
    tprintf(3, "Scale buffer value %d (%ld) with scalingValue %f/%d to %f\n",(*sb)[i].s, (*sb)[i].index, scalingValue, lowest, (*sb)[i].scaled );
  } 
}

struct PeakScore {
  sample_t timeDistance;
  double amplitudeDistance;
  uint32 firstBufferIndex;
  uint32 lastBufferIndex;
  uint32 firstIndex;
  uint32 lastIndex;
  double score;
};


bool compareSort (struct PeakScore a,struct PeakScore b) { return (a.score<b.score); }

typedef std::vector<struct PeakScore> PeakScores;


PeakScores calculateScores (Peaks * ps) {
  PeakScores scores;
  struct PeakScore s;
  for (int i = 0; i < ps->size() - 1; i++)
  {
    for (int j = i + 1; j < ps->size(); j++)
    {
      bool firstHiPeak = (*ps)[i].hiPeak;
      bool lastHiPeak = (*ps)[j].hiPeak;
      tprintf(3, "Processing peaks %d,%d firstHiPeak=%s lastHiPeak=%s",(*ps)[i].index, (*ps)[j].index, firstHiPeak?"TRUE":"FALSE", lastHiPeak?"TRUE":"FALSE" );
      if ((firstHiPeak && !lastHiPeak) || (!firstHiPeak && lastHiPeak)) {
        s.amplitudeDistance = fabs((*ps)[i].scaled - (*ps)[j].scaled);
        s.timeDistance = abs(((int)(*ps)[i].index) - ((int)(*ps)[j].index));
        s.firstBufferIndex = (*ps)[i].bufferIndex;
        s.lastBufferIndex = (*ps)[j].bufferIndex;
        s.firstIndex = (*ps)[i].index;
        s.lastIndex = (*ps)[j].index;
        scores.push_back(s);
        tprintf(3, " - amplitudeDistanced=%f timeDistance=%d firstBufferIndex=%ld lastBufferIndex=%ld firstIndex=%ld lastIndex=%ld\n",s.amplitudeDistance, s.timeDistance, s.firstBufferIndex, s.lastBufferIndex,s.firstIndex, s.lastIndex );
      } else {
        tprintf (3, " - not a transistion\n");
      }
    }
  }
  for (int i = 0; i < scores.size(); i++) { 
    double samplesPerPeriod = sampleRate / zero_freq;
    double timeScore = pow((((double)scores[i].timeDistance) - samplesPerPeriod)/samplesPerPeriod, 2);
    double amplitudeScore = pow((scores[i].amplitudeDistance - 1.0f), 2);
    scores[i].score = timeScore + amplitudeScore;
    tprintf(3, "amplitudeScore=%f timeScore=%f at %ld, %ld\n", amplitudeScore, timeScore, scores[i].firstIndex, scores[i].lastIndex);
  }
  std::sort(scores.begin(), scores.end(), compareSort);
  for (int i = 0; i < scores.size(); i++) {
    tprintf(3, "Sorted: score=%f at %ld, %ld\n", scores[i].score, scores[i].firstIndex,scores[i].lastIndex );
  }
  return scores;
}




PeakScores calculateScoresSingle (Peaks * ps, bool lastPeakIsHigh,double lastPeakAmplitude) {
  struct PeakScore s;
  PeakScores scores;
  for (int i=0; i < ps->size(); i++) {
    tprintf(3, "Processing peaks %d lastPeakIsHigh=%s currentPeakIsHigh=%s scaled=%f lastPeakAmplitude=%f", (*ps)[i].index, lastPeakIsHigh?"TRUE":"FALSE", (*ps)[i].hiPeak?"TRUE":"FALSE", (*ps)[i].scaled, lastPeakAmplitude);
    if ((lastPeakIsHigh && (!((*ps)[i].hiPeak))) ||((!lastPeakIsHigh) && ((*ps)[i].hiPeak))) { 
      double totalScore;
      double longPeriod = sampleRate / zero_freq;
      double shortPeriod = sampleRate / (2.0f *zero_freq);
      double longScore =  pow((fabs(((double) (*ps)[i].bufferIndex) - longPeriod)/longPeriod), 3);
      double shortScore = pow((fabs(((double) (*ps)[i].bufferIndex) - shortPeriod)/shortPeriod),3); 
      double amplitudeDistance = fabs((*ps)[i].scaled - lastPeakAmplitude);
      double amplitudeScore = 1/amplitudeDistance;
      if (longScore<shortScore) {
        totalScore = 8*longScore+0.10*amplitudeScore;
      } else {
        totalScore = 8*shortScore+0.10*amplitudeScore;
      }  
      s.firstBufferIndex = (*ps)[i].bufferIndex;
      s.firstIndex = (*ps)[i].index;
      s.score = totalScore;
      scores.push_back(s);
      tprintf(3, " - longScore=%f shortScore=%f amplitudeDistance=%f amplitudeScore=%f totalScore=%f\n",longScore, shortScore, amplitudeDistance, amplitudeScore, totalScore );
    } else {
      tprintf (3, " - not a transistion\n");
    }
  }
  std::sort(scores.begin(), scores.end(), compareSort);
  for (int i = 0; i < scores.size(); i++) {
    tprintf(3, "Sorted: score=%f at %ld\n", scores[i].score, scores[i].firstIndex );
  }  
  return scores;
}

uint32 findKnee(SampleBuffer * s, uint32 peakBufferIndex) {
  double peak = (*s)[peakBufferIndex].scaled;
  double previousPeak = (*s)[0].scaled;
  double distance = fabs(peak-previousPeak);
  bool isHighPeak = (*s)[peakBufferIndex].highPeak;
  uint32 i;
  for (i=peakBufferIndex; i > 0; i--) {
    tprintf(3, "findKnee: searching. i=%ld highPeak=%s scaled=%f distance=%f scaled/distance=%f\n", i, (*s)[i].highPeak?"TRUE":"FALSE",(*s)[i].scaled, distance, (*s)[i].scaled/distance );
    if ((fabs(((*s)[i].scaled)-previousPeak)/distance)<0.925f) break; 
  }
  tprintf(3, "findKnee: peak=%f previousPeak=%d distance=%f foundKnee=%f at %d (%d) \n", peak, previousPeak, distance, (*s)[i].scaled, (*s)[i].index, i); 
  return (*s)[i].index;  
}

void FindTransitions() {
  SampleBuffer sampleBuffer;
  sample_t low=32767, high=-32768, sample;
  long lastPeak=0;
  bool lastPeakIsHigh;
  int lowestIndex, highestIndex;

  HiLo hilo;
  while (nSamp < expected_samples) { 
    if (BSstate == BS_LOST) {
      Peaks ps;
      PeakScores scores;
      fillSampleBuffer(&nSamp, &sampleBuffer, 80);
      hilo = findHighestLowest(&sampleBuffer);
      scaleBuffer(&sampleBuffer, fabs(((double) hilo.highValue)-((double) hilo.lowValue)), hilo.lowValue);
      ps = findPeaks(&sampleBuffer);
      scores = calculateScores(&ps);
      // Take the first one which has lowest score. Lower score is better..
      if (scores.size()>0) {
        uint32 lastBufferIndex = scores[0].lastBufferIndex;
        lastPeakIsHigh = sampleBuffer[lastBufferIndex].highPeak;
        DecodeBits(scores[0].firstIndex);
        DecodeBits(scores[0].lastIndex);
        tprintf(3, "firstBufferIndex=%ld lastBufferIndex=%ld lastPeakIsHigh=%s\n",  scores[0].firstBufferIndex, scores[0].lastBufferIndex, lastPeakIsHigh?"TRUE":"FALSE");
        lastPeak =  scores[0].lastIndex;
      } else {
        tprintf(3, "Didn't find any peaks. Scores is empty.\n");
      }
    } else {
      Peaks ps;
      PeakScores scores;
      fillSampleBuffer(&nSamp, &sampleBuffer, 67, lastPeak);
      hilo = findHighestLowest(&sampleBuffer);
      scaleBuffer(&sampleBuffer, fabs(((double) hilo.highValue)-((double) hilo.lowValue)), hilo.lowValue);
      ps = findPeaks(&sampleBuffer);
      scores = calculateScoresSingle(&ps, lastPeakIsHigh, sampleBuffer[0].scaled); 
      if (scores.size()>0) {
        uint32 firstBufferIndex = scores[0].firstBufferIndex;
        uint32 kneeIndex = findKnee(&sampleBuffer, firstBufferIndex);
        lastPeakIsHigh = sampleBuffer[firstBufferIndex].highPeak;
        lastPeak =  scores[0].firstIndex;
        tprintf(3, "kneeIndex=%ld\n", kneeIndex);
        DecodeBits(kneeIndex);
      } else {
        DecodeBits(sampleBuffer[sampleBuffer.size()].index);
        tprintf(3, "Didn't find any peaks. Scores is empty.\n");
      }
    }
  }

}


#endif


#if 0
void FindTransitions() {
  int state = 0;
  sample_t low=32767, high=-32768, sample, previousHigh, previousLow;
  int lowestIndex, highestIndex, previousHighestIndex, previousLowestIndex, lastProcessedSampleIndex;
  lowestIndex = highestIndex = 0;
  previousLowestIndex = previousHighestIndex= low = high = GETIN(0);
  for (uint32 nSamp = 1; nSamp < expected_samples; nSamp++) {
    sample = GETIN(nSamp);
    if (low > sample) {
      lowestIndex = nSamp;
      low = sample;
    }
    if (high < sample) {
      highestIndex = nSamp;
      high = sample;
    }
    if (highestIndex > lowestIndex) {
      previousLowestIndex = lowestIndex;
      previousLow = low;
      //low = 32767;
    } else {
      previousHighestIndex = highestIndex;
      previousHigh = high;
      //high = -32768;
    }
    if ((previousHighestIndex > previousLowestIndex) && (previousLowestIndex == lastProcessedSampleIndex)) {
      double absDistance = (double) abs(previousHigh - previousLow);
      // Find leading edge on the pulse where the slope is approaching zero.
      for (int i=previousHighestIndex; i>(previousLowestIndex+1); i--) {
        // going backwards to find it.
        sample_t s = GETIN(i);
        sample_t t = GETIN(i-1);
        double diff = abs(s-t);
        double scaledDiff = diff/absDistance;
        if (diff > 0.0625f) {
          break;
        }
      }
      lastProcessedSampleIndex = previousHighestIndex;
    } 
    if ((previousLowestIndex > previousHighestIndex) && (previousHighestIndex == lastProcessedSampleIndex)) {
      double absDistance = (double) abs(previousHigh - previousLow);
      // Find leading edge on the pulse where the slope is approaching zero.
      for (int i=previousLowestIndex; i>(previousHighestIndex+1); i--) {
        // going backwards to find it.
        sample_t s = GETIN(i);
        sample_t t = GETIN(i-1);
        double diff = abs(s-t);
        double scaledDiff = diff/absDistance;
        if (diff > 0.0625f) {
          break;
        }
      }
      lastProcessedSampleIndex = previousLowestIndex;
    } 
    tprintf(3, "sample=%lu DIFF=%d\n", nSamp, abs(diff)); 
  }
}
#endif 


#if 0
void FindTransitions() {
  int highestIndex, lastHighestIndex = 0;
  int lowestIndex, lastLowestIndex = 0;
  std::vector<sample_t> sampleBuffer;
  for (int i = 0; i < 64; i++) {
    sampleBuffer.push_back(GETIN(i));
  }
  for (uint32 nSamp = 64; nSamp < expected_samples; nSamp++) {

    sample_t high = sampleBuffer.front();
    sample_t low = sampleBuffer.front();
    for (int i = 0; i < 64; i++) {
      if (high < sampleBuffer[i]) {
        high = sampleBuffer[i];
        highestIndex = i;
      }
      if (low > sampleBuffer[i]) {
        low = sampleBuffer[i];
        lowestIndex = i;
      }
    }

    sampleBuffer.push_back(GETIN(nSamp));
    sampleBuffer.erase(sampleBuffer.begin());
  }
}
#endif

// find the alternating local minima and maxima and estimate the zero
// crossing to be midway between them.

/* this needs a lot more sophistication -- eg, look for a bit time or three
around the current point to get an estimate of the envelope of the wave
and use that when looking for peaks -- a deviation must be at least some
percentage of the min/max envelope spread.*/

/*void
FindTransitions()
{
    bool     bFirst = true;	// first transition seen
    bool     bUp    = false;
    sample_t prevSamp;		// previous sample
    sample_t prev_min   =  32000;
    uint32   prev_min_t = 0;		// time the min occurred
    sample_t prev_max   = -32000;
    uint32   prev_max_t = 0;		// time the max occurred
    float    prevIntercept = 0.0f;	// previous zero intercept

    prevSamp = GETIN(0);
    for(uint32 nSamp=1; nSamp<expected_samples; nSamp++) {
        sample_t samp = GETIN(nSamp);
        if (samp > prev_max) {
            prev_max   = samp;
            prev_max_t = nSamp;
            bUp = true;
        } else if (samp < prev_min) {
            prev_min   = samp;
            prev_min_t = nSamp;
            bUp = false;
        } else {
            // look ahead a few samples to see if we just found the local
min/max bool atpeak = true; for(int n=1; n<5; n++) { sample_t s = GETIN(nSamp +
n); if ((s > prev_max) || (s < prev_min)) { atpeak = false; break;
                }
            }
            if (atpeak && (prev_max - prev_min > 2000)) {
                uint32 intercept = (prev_max_t + prev_min_t + 1) >> 1;
                if (bUp) {
printf("sample %lu: detected max peak at %d\n", nSamp, prev_max);
                    prev_min = prev_max;
                } else {
printf("sample %lu: detected min peak at %d\n", nSamp, prev_min);
                    prev_max = prev_min;
                }
                DecodeBits(intercept);
            }
        }
    }

    StreamDone();
}*/
#endif

// =========================================================================
// main

void usage(int code) {
  FILE *f = (code == 0) ? stdout : stderr;

  fprintf(f,
          "Usage: dpwav2tap [-v [#]] [-x] [-o <outname>.tag] <inname>.wav\n");
  fprintf(f, "-v is report verbosity level\n");
  fprintf(f, "   with no -v, reporting is at a minimum;\n");
  fprintf(f, "   with no # specified, 1 is assumed;\n");
  fprintf(f, "   -v 2 through -v 4 provide increasing detail.\n");
  fprintf(f, "-o specifies a specific output filename;\n");
  fprintf(f, "   by default it is <inname>.tap\n");
  fprintf(f, "-x says to \"explode\" each record into a separate file,\n");
  fprintf(f, "   which are <outname>-###.tap\n");
  fprintf(f, "Version: August 24, 2005\n");
  exit(code);

#if 0
    // detailed -v information
    0 = just produce the output file and errors
    1 = errors, warnings, length of each decoded block
    2 = and each byte decoded
    3 = and each bit decoded
    4 = and PLL value
#endif
}

// parse the command line arguments
void ParseArgs(int argc, char **argv) {
  // set command line defaults
  opt_v = 0;
  opt_x = false;
  opt_x_num = 0;
  opt_ofn = NULL;
  opt_ofmt = OFMT_TAP;

#if 1
  if (argc < 2) // we need at least one parameter
    usage(-1);
#endif

  for (int i = 1; i < argc - 1; i++) {

    if (strcmp(argv[i], "-v") == 0) {
      // verbose reporting
      opt_v = 1; // at least
      if (i + 1 < argc - 1) {
        // check for optional number
        opt_v = atol(argv[i + 1]);
        if (opt_v == 0) {
          // if next arg wasn't a number, atol returns 0
          // however, if the user did "-v 0", we mess up in this assumption
          opt_v = 1;
        } else
          i++; // skip numeric argument
      }
    }

    else if (strcmp(argv[i], "-x") == 0) {
      opt_x = true;
    }

    else if (strcmp(argv[i], "-o") == 0) {
      if (i + 1 < argc - 1) {
        int len = strlen(argv[i + 1]);
        opt_ofn = strdup(argv[i + 1]);
        i++;
      } else {
        fprintf(stderr, "Error: final argument is input filename, not part of "
                        "-o specification\n");
        usage(-1);
      }
    }

    else {
      fprintf(stderr, "Error: unrecognized option '%s'\n", argv[i]);
      usage(-1);
    }
  }

#if 1
  // one last parameter -- it must be the original wav file
  opt_ifn = strdup(argv[argc - 1]);
#else
  opt_v = 4;
  opt_ifn = "c:\\jim\\datapoint\\dpwav2tap\\wav\\tstdis1.1_3-75.wav";
#endif

  // if -o wasn't specified, derive the output filename from the input filename
  if (opt_ofn == NULL) {
    // we just change the suffix of the input filename
    int len = strlen(opt_ifn);
    opt_ofn = (char *)malloc(len + 4); // +4 for appending optional .tap suffix
    assert(opt_ofn != NULL);
    strcpy(opt_ofn, opt_ifn);
    if ((len >= 4) && (strcmp(&opt_ifn[len - 4], ".wav") == 0)) {
      // input filename ends in ".wav"; replace it with .tap
      strcpy(&opt_ofn[len - 3], "tap");
    } else {
      // input filename doesn't end like we'd expect
      opt_ofn = strcat(opt_ifn, ".tap");
    }
  }

  // if -x is in effect, insert "-%03d" just before .tap
  // in order to generate serialized filenames.
  if (opt_x) {
    int len = strlen(opt_ofn);
    char *tmp = (char *)malloc(len + 5);
    assert(tmp != NULL);
    strcpy(tmp, opt_ofn);
    if (strcmp(&opt_ofn[len - 4], ".tap") == 0)
      strcpy(&tmp[len - 4], "-%03d.tap");
    else if (strcmp(&opt_ofn[len - 4], ".bin") == 0)
      strcpy(&tmp[len - 4], "-%03d.bin");
#if SUPPORT_HEX
    else if (strcmp(&opt_ofn[len - 4], ".hex") == 0)
      strcpy(&tmp[len - 4], "-%03d.hex");
#endif
    else
      strcpy(&tmp[len], "-%03d");
    free(opt_ofn);
    opt_ofn = tmp;
  }

  {
    int len = strlen(opt_ofn);
    opt_ofmt = (strcmp(&opt_ofn[len - 4], ".bin") == 0) ? OFMT_BIN
#if SUPPORT_HEX
               : (strcmp(&opt_ofn[len - 4], ".hex") == 0) ? OFMT_HEX
#endif
                                                          : OFMT_TAP;
  }

#if 0
    // report how command line was parsed
    printf("opt_v   = %d\n", opt_v);
    printf("opt_x   = %d\n", (int)opt_x);
    printf("opt_ofn = '%s'\n", opt_ofn);
    printf("opt_ifn = '%s'\n", opt_ifn);
    exit(0);
#endif
}

int main(int argc, char **argv) {
  // parse command line arguments
  ParseArgs(argc, argv);

  fIn = fopen(opt_ifn, "rb");
  if (fIn == NULL) {
    fprintf(stderr, "Error: couldn't open file '%s'\n", opt_ifn);
    exit(-1);
  }

  // make sure the wav file is OK
  CheckHeader();

  // initialize our work buffer
  // NB: the whole inbuffer complication is plumbing for a more
  //     sophisticated version of this program that looks forward
  //     and back in the sample stream.
  FillInbuffer();

  // process the file
  FindTransitions();

  fclose(fIn);

  return 0;
}
