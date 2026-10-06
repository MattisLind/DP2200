#include <cstdio>
#include <cstdint>
#include <stdlib.h>
#include <unistd.h>
#include "cassetteTape.h"

void printLog(const char *level, const char *fmt, ...);



CassetteTape::CassetteTape() {
  state=TAPE_GAP;
}

bool CassetteTape::isOpen() {
  return (file != NULL);
}

bool CassetteTape::openFile(std::string fileName) {
  if (file != NULL) {
    fclose(file);
  }
  file = fopen(fileName.c_str(), "r");
  return file != NULL;
}

bool CassetteTape::createFile(std::string name) {
  FILE *created=fopen(name.c_str(), "w+bx");
  if (!created) return false;
  if (file) fclose(file);
  file=created; fileName=name; writeProtect=false; state=TAPE_GAP;
  return true;
}

bool CassetteTape::writeBlock(const std::vector<unsigned char> &data) {
  if (!file || writeProtect || data.empty() || data.size()>65536) return false;
  // Synchronize an update stream when changing from reading to writing.
  if (fseek(file, 0, SEEK_CUR) != 0) return false;
  const uint32_t count=static_cast<uint32_t>(data.size());
  unsigned char size[4]={static_cast<unsigned char>(count),static_cast<unsigned char>(count>>8),
                         static_cast<unsigned char>(count>>16),static_cast<unsigned char>(count>>24)};
  const bool ok=fwrite(size,1,4,file)==4 && fwrite(data.data(),1,count,file)==count
    && fwrite(size,1,4,file)==4 && fflush(file)==0;
  state=TAPE_GAP;
  // Rewriting after rewind/backspace replaces the tape's remaining record
  // sequence. A shorter record must not leave an old trailer/data suffix.
  const long end = ftell(file);
  return ok && end >= 0 && ftruncate(fileno(file), end) == 0;
}

void CassetteTape::closeFile() { 
  fflush(file);
  fclose(file);
  file=NULL; 
}

std::string CassetteTape::getFileName() { return fileName; }

void CassetteTape::setWriteProtected(bool wp) { 
  writeProtect=wp;
}


bool  CassetteTape::readBlock (unsigned char * buffer, int * size) {
  int maxSize = *size;
  int count;
  if (file==NULL) return false;
  count = fread(size, 4, 1, file);
  if (count != 1) return false;
  if (*size>maxSize) {
    count = fread(buffer, 1, maxSize, file);
    if (count != maxSize) return false;
  } else {
    count = fread(buffer, 1, *size, file);
    if (count != *size) return false;
  }
  count = fread(size, 4, 1, file);
  if (count != 1) return false;
  state=TAPE_GAP;
  return true;
}

bool  CassetteTape::readBlock (int address, std::function<void(int address, unsigned char)> writeMem, int * size) {
  int maxSize = *size;
  unsigned char tmp;
  int count;
  if (file==NULL) return false;
  count = fread(size, 4, 1, file);
  if (count != 1) return false;
  if (*size>maxSize) {
    for (int i=0; i < maxSize; i++) {
      if (fread(&tmp, 1, 1, file)!=1) {
        return false;
      }
      writeMem(address+i, tmp);
    }
  } else {
    for (int i=0; i < *size; i++) {
      if (fread(&tmp, 1, 1, file)!=1) {
        return false;
      }
      writeMem(address+i, tmp);
    }
  }
  count = fread(size, 4, 1, file);
  if (count != 1) return false;
  state=TAPE_GAP;
  return true;
}



void CassetteTape::rewind() {
  state=TAPE_GAP;
  if (file==NULL) return;
  ::rewind(file);
}

bool CassetteTape::loadBoot(std::function<void(int address, unsigned char)> writeMem) {
  int size=16384;
  if (file==NULL) return false;
  rewind();
  return readBlock(0, writeMem, &size);
}

int CassetteTape::isFileHeader(unsigned char * buffer) {
  if ((buffer[0]==0201) && (buffer[1]==0176)) {
    return 1;
  } 
  return 0;
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


bool CassetteTape::isTapeOverGap() { 
  return state ==  TAPE_GAP; 
}

// 0: byte, 1: byte at tape endpoint, 2: no cassette, 3: endpoint/no byte.
int CassetteTape::readByte(bool forward, unsigned char *data) {
  if (!file) return 2;
  if (state == TAPE_GAP) {
    if (forward) {
      if (fread(&currentBlockSize, 4, 1, file) != 1) return 3;
    } else {
      if (ftell(file) < 8 || fseek(file, -4, SEEK_CUR) != 0) return 3;
      if (fread(&currentBlockSize, 4, 1, file) != 1) return 3;
      if (fseek(file, -4, SEEK_CUR) != 0) return 3;
    }
    if (currentBlockSize <= 0 || currentBlockSize > 65536) return 3;
    readBytes = forward ? 0 : currentBlockSize;
    state = TAPE_DATA;
  }
  if (!forward && fseek(file, -1, SEEK_CUR) != 0) return 3;
  if (fread(data, 1, 1, file) != 1) return 3;
  if (!forward && fseek(file, -1, SEEK_CUR) != 0) return 3;
  readBytes += forward ? 1 : -1;
  bool endpoint = false;
  if ((forward && readBytes == currentBlockSize) || (!forward && readBytes == 0)) {
    int length;
    if (!forward && fseek(file, -4, SEEK_CUR) != 0) return 3;
    if (fread(&length, 4, 1, file) != 1 || length != currentBlockSize) return 3;
    if (!forward && fseek(file, -4, SEEK_CUR) != 0) return 3;
    state = TAPE_GAP;
    if (forward) {
      // Reading the trailer successfully does not set feof. Probe the next
      // byte without consuming it, including after a one-byte record.
      int next = fgetc(file);
      endpoint = next == EOF;
      if (!endpoint) ungetc(next, file);
      clearerr(file);
    } else {
      endpoint = ftell(file) == 0;
    }
  }
  if (!forward) {
    unsigned char byte = *data;
    *data = (byte & 0x80) >> 7 | (byte & 0x40) >> 5 | (byte & 0x20) >> 3
          | (byte & 0x10) >> 1 | (byte & 8) << 1 | (byte & 4) << 3
          | (byte & 2) << 5 | (byte & 1) << 7;
  }
  return endpoint ? 1 : 0;
}
