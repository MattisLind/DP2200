#include <cstdio>
#include <stdlib.h>
#include "cassetteTape.h"

void printLog(const char *level, const char *fmt, ...);



CassetteTape::CassetteTape() {
  state=TAPE_GAP;
}

bool CassetteTape::openFile(std::string fileName) {
  file = fopen(fileName.c_str(), "r");
  return file != NULL;
}

void CassetteTape::closeFile() { fclose(file); }

std::string CassetteTape::getFileName() { return fileName; }


bool  CassetteTape::readBlock (unsigned char * buffer, int * size) {
  int maxSize = *size;
  int count;
  count = fread(size, 4, 1, file);
  if (count != 4) return false;
  if (*size>maxSize) {
    count = fread(buffer, maxSize, 1, file);
    if (count != maxSize) return false;
  } else {
    count = fread(buffer, *size, 1, file);
    if (count != *size) return false;
  }
  count = fread(size, 4, 1, file);
  if (count != 4) return false;
  state=TAPE_GAP;
  return true;
}



void CassetteTape::rewind() {
  state=TAPE_GAP;
  ::rewind(file);
}

bool CassetteTape::loadBoot(unsigned char *  address) {
  int size=16384;
  if (file==NULL) return false;
  rewind();
  readBlock(address, &size);
  return true;
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

void CassetteTape::readByte(bool forward, unsigned char * data) {
  if (forward) {
    if (state == TAPE_GAP) {
      fread(&currentBlockSize, 4, 1, file);  
      printLog("INFO", "readByte direction=%s next block is %d bytes long. state is now=%s\n", forward?"forward":"backwards", currentBlockSize, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA");
      state = TAPE_DATA;
      fread(data, 1, 1, file);  
      readBytes=1;
      printLog("INFO", "readByte %02X\n # bytes read= %d state is now=%s\n", *data, readBytes, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA");  
    } else {
      fread(data, 1, 1, file); 
      readBytes++;
      printLog("INFO", "readByte %02X\n # bytes read= %d state is now=%s\n", *data, readBytes, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA");
      if (readBytes >= currentBlockSize) {
        int dummy;
        state = TAPE_GAP;
        fread(&dummy, 4, 1, file); // read end of record size marker 
        printLog("INFO", "readByte read the end of record blockSizemarker=%d state is now=%s\n", dummy, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA");
      }
    }
  } else {
    if (state == TAPE_GAP) {
      fseek(file, -4, SEEK_CUR);
      fread(&currentBlockSize, 4, 1, file); 
      fseek(file, -4, SEEK_CUR);
      printLog("INFO", "readByte direction=%s next block is %d bytes long state is now=%s \n",forward?"forward":"backwards", currentBlockSize, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA");
      state = TAPE_DATA;
      fseek(file, -1, SEEK_CUR);
      fread(data, 1, 1, file);
      fseek(file, -1, SEEK_CUR);
      readBytes=currentBlockSize-1;
      printLog("INFO", "readByte %02X\n # bytes read= %d state is now=%s \n", *data, readBytes, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA"); 
    } else {
      fseek(file, -1, SEEK_CUR);
      fread(data, 1, 1, file);
      fseek(file, -1, SEEK_CUR);
      readBytes--;
      printLog("INFO", "readByte %02X\n # bytes read= %d state is now=%s \n", *data, readBytes, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA"); 
      if (readBytes <= 0) {
        int dummy;
        state = TAPE_GAP;
        fseek(file, -4, SEEK_CUR);
        fread(&dummy, 4, 1, file); // read end of record size marker 
        fseek(file, -4, SEEK_CUR);
        printLog("INFO", "readByte read the end of record blockSizemarker=%d state is now=%s \n", dummy, state==TAPE_GAP?"TAPE_GAP":"TAPE_DATA");
      }
    } 
    *data = (*data & 0x80) >> 7 | (*data & 0x40) >> 5 | (*data & 0x20) >> 3 | (*data & 0x10) >> 1 | (*data & 0x8) << 1 | (*data & 0x4) << 3 | (*data & 0x2) << 5 | (*data & 0x1) <<7;
  }  
}
