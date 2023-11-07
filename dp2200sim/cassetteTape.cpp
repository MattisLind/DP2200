#include <cstdio>
#include <stdlib.h>
#include "cassetteTape.h"

void printLog(const char *level, const char *fmt, ...);
void removeTimerCallback(class callbackRecord * c);

class callbackRecord * addToTimerQueue(std::function<int(class callbackRecord *)>, struct timespec);

void timeoutInNanosecs (struct timespec *, long);

CassetteTape::CassetteTape(std::function<void(bool)> cb) {
  state=TAPE_GAP;
  tapeGapCb = cb;
}

bool CassetteTape::openFile(std::string fileName) {
  file = fopen(fileName.c_str(), "r");
  return file != NULL;
}

void CassetteTape::closeFile() { fclose(file); }

std::string CassetteTape::getFileName() { return fileName; }


void  CassetteTape::readBlock (unsigned char * buffer, int * size) {
  int maxSize = *size;
  fread(size, 4, 1, file);
  if (*size>maxSize) {
    fread(buffer, maxSize, 1, file);
  } else {
    fread(buffer, *size, 1, file);
  }
  fread(size, 4, 1, file);
  state=TAPE_GAP;
}

unsigned char * CassetteTape::readBlock (int * size) {
  unsigned char * buffer;
  fread(size, 4, 1, file);
  buffer = (unsigned char *) malloc(*size);
  fread(buffer, *size, 1, file);
  fread(size, 4, 1, file);
  return buffer;
}

void CassetteTape::rewind() {
  state=TAPE_GAP;
  ::rewind(file);
}

void CassetteTape::loadBoot(unsigned char *  address) {
  int size=16384;
  rewind();
  readBlock(address, &size);
}

int CassetteTape::isFileHeader(unsigned char * buffer) {
  if ((buffer[0]==0201) && (buffer[1]==0176)) {
    return 1;
  } 
  return 0;
}

void CassetteTape::stopTapeMotion() {
  printLog("INFO", "Number of outstanding timers to clear = %d \n", outStandingCallbacks.size());
  for (auto it=outStandingCallbacks.begin(); it < outStandingCallbacks.end(); it++) {
    removeTimerCallback(*it);
  }
}

int CassetteTape::isNumericRecord(unsigned char * buffer) {
  if ((buffer[0]==0303) && (buffer[1]==0074)) {
    return 1;
  } 
  return 0;
}

int CassetteTape::isSymbolicRecord(unsigned char * buffer) {
  if ((buffer[0]==0347) && (buffer[1]==0030)) {
    return 1;
  } 
  return 0;
}

int CassetteTape::isChecksumOK(unsigned char * buffer, int size) {
  unsigned char xorChecksum;
  unsigned char circulatedChecksum;
  int i;
  xorChecksum = buffer[2];
  circulatedChecksum = buffer[3];
  for (i=4;i<size;i++) {
    int lowestBit;
    xorChecksum ^= buffer[i];
    circulatedChecksum ^=buffer[i];
    lowestBit = 0x01 & circulatedChecksum;
    circulatedChecksum >>= 1;
    circulatedChecksum |= (0x80 & (lowestBit<<7));
  }
  //printf("xorChecksum: %02X circulatedChecksum: %02X .", xorChecksum, circulatedChecksum);
  if ((xorChecksum == 0) && (circulatedChecksum == 0)) return 1;
  return 0;
}

void CassetteTape::removeFromOutstandCallbacks(class callbackRecord * c) {
  auto it = std::find(outStandingCallbacks.begin(), outStandingCallbacks.end(), c);
  if (it != outStandingCallbacks.end()) {
    outStandingCallbacks.erase(it);
  }
}



void CassetteTape::readByte(std::function<void(unsigned char)> cb) {
  long timeout;
  struct timespec then;
  if (state == TAPE_GAP) {
    fread(&currentBlockSize, 4, 1, file);  
    printLog("INFO", "readByte next block is %d bytes long\n", currentBlockSize);
    state = TAPE_DATA;
    //tapeGapCb(false);
    readBytes=0;
    //tapeGapCb(true);
    timeout = 700000000;
    timeoutInNanosecs(&then, timeout);
    printLog("INFO", "Adding a readByteHandler to handle read in %d nanoseconds\n", timeout);
    outStandingCallbacks.push_back( addToTimerQueue([ct=this, tcb = tapeGapCb](class callbackRecord * c)->int {printLog("INFO", "70ms tapeGapCb(true) timeout ENTRY\n");ct->removeFromOutstandCallbacks(c); tcb(false);ct->timeoutReadByteHandler(); printLog("INFO", "70ms tapeGapCb(true) timeout EXIT\n");return 0;}, then));
  } else {
    timeout = 28000000;
    timeoutInNanosecs(&then, timeout);
    printLog("INFO", "Adding a readByteHandler to handle read in %d nanoseconds\n", timeout);
    outStandingCallbacks.push_back( addToTimerQueue([ct=this](class callbackRecord * c)->int {ct->removeFromOutstandCallbacks(c);ct->timeoutReadByteHandler(); return 0;}, then));
  }
  readCb = cb;
  //addToTimerQueue(std::bind(&CassetteTape::timeoutReadByteHandler, this), then);
  //addToTimerQueue([ct=this, tcb = tapeGapCb]()->int {tcb(false);ct->timeoutReadByteHandler(); }, then);
}

//void CassetteTape::timeoutReadByteHandler(std::function<int(void))> cb) {
int CassetteTape::timeoutReadByteHandler() {
  unsigned char data;
  struct timespec then;
  long timeout=1000000;
  fread(&data, 1, 1, file);
  readBytes++;
  printLog("INFO", "Read one byte = %02X. Now we have read %d bytes out of %d bytes\n", data, readBytes, currentBlockSize);
  if (readBytes >= currentBlockSize) {
    int dummy;
    state = TAPE_GAP;
    //tapeGapCb(true); // Need to set tapeGap after some time. Wait a ms and then set it!
    timeoutInNanosecs(&then, timeout);
    printLog("INFO", "Adding a tapeGapCb to set tape gap  in %d nanoseconds\n", timeout);
    outStandingCallbacks.push_back(addToTimerQueue([ct=this,cb=tapeGapCb](class callbackRecord * c)->int {printLog("INFO", "1ms tapeGapCb(true) timeout ENTRY\n"); ct->removeFromOutstandCallbacks(c); cb(true); printLog("INFO", "1ms tapeGapCb(true) timeout EXIT\n");return 0; }, then));
    fread(&dummy, 4, 1, file); // read end of record size marker 
    printLog("INFO", "timeoutReadByteHandler read end of record size marker = %d \n", dummy);
    if (stopAtTapeGap) {
      stopTapeMotion();
    } else {
      readByte(readCb);
    }
  } else {
    readByte(readCb);
  }
  readCb(data);
  return 0;
}

void CassetteTape::readByteBackwards(std::function<void(unsigned char)> cb) {
  long timeout;
  struct timespec then;
  if (state == TAPE_GAP) {
    fseek(file, -4, SEEK_CUR);
    fread(&currentBlockSize, 4, 1, file); 
    fseek(file, -4, SEEK_CUR);
    printLog("INFO", "readByteBackwards next block is %d bytes long\n", currentBlockSize);
    state = TAPE_DATA;
    //tapeGapCb(false);
    readBytes=currentBlockSize;
    //tapeGapCb(true);
    timeout = 70000000;
    timeoutInNanosecs(&then, timeout);
    printLog("INFO", "Adding a readByteHandler to handle read in %d nanoseconds\n", timeout);
    outStandingCallbacks.push_back( addToTimerQueue([ct=this, tcb = tapeGapCb](class callbackRecord * c)->int {ct->removeFromOutstandCallbacks(c); tcb(false);ct->timeoutReadByteBackwardsHandler(); return 0;}, then));
  } else {
    timeout = 2800000;
    timeoutInNanosecs(&then, timeout);
    printLog("INFO", "Adding a readByteHandler to handle read in %d nanoseconds\n", timeout);
    outStandingCallbacks.push_back( addToTimerQueue([ct=this](class callbackRecord * c)->int {ct->removeFromOutstandCallbacks(c);ct->timeoutReadByteBackwardsHandler(); return 0;}, then));
  }
  readCb = cb;
  //addToTimerQueue(std::bind(&CassetteTape::timeoutReadByteHandler, this), then);
  //addToTimerQueue([ct=this, tcb = tapeGapCb]()->int {tcb(false);ct->timeoutReadByteHandler(); }, then);
}


int CassetteTape::timeoutReadByteBackwardsHandler() {
  unsigned char data;
  struct timespec then;
  long timeout=1000000;
  fseek(file, -1, SEEK_CUR);
  fread(&data, 1, 1, file);
  fseek(file, -1, SEEK_CUR);
  readBytes--;
  printLog("INFO", "Read one byte = %02X. Now we have read %d bytes out of %d bytes\n", data, readBytes, currentBlockSize);
  if (readBytes <= 0) {
    int dummy;
    state = TAPE_GAP;
    //tapeGapCb(true); // Need to set tapeGap after some time. Wait a ms and then set it!
    // TODO! Make sure we stop tape and set tape deck ready at the same time. if this was a BSP command which stops when it got to a tape gape. Have flag that says stopAtTapeGap...
    timeoutInNanosecs(&then, timeout);
    printLog("INFO", "Adding a tapeGapCb to set tape gap  in %d nanoseconds\n", timeout);
    outStandingCallbacks.push_back(addToTimerQueue([ct=this,cb=tapeGapCb](class callbackRecord * c)->int {ct->removeFromOutstandCallbacks(c); cb(true); return 0; }, then));
    fseek(file, -4, SEEK_CUR);
    fread(&dummy, 4, 1, file); // read end of record size marker 
    fseek(file, -4, SEEK_CUR);
    printLog("INFO", "timeoutReadByteHandler read end of record size marker = %d \n", dummy);
    if (stopAtTapeGap) {
      stopTapeMotion();
    } else {
      readByte(readCb);
    }
  } else {
    readByte(readCb);
  }
  data = (data & 0x80) >> 7 | (data & 0x40) >> 5 | (data & 0x20) >> 3 | (data & 0x10) >> 1 | (data & 0x8) << 1 | (data & 0x4) << 3 | (data & 0x2) << 5 | (data & 0x1) <<7;
  readCb(data); // data has to be reversed bit order for backwards reading!
  return 0;
}

void CassetteTape::setTapeDirectionForward (bool forward) {
  tapeMovesForward=forward;
}

void CassetteTape::setStopAttBlockGap (bool stop) {
  stopAtTapeGap=stop;
}