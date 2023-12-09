#include "FloppyDrive.h"

void printLog(const char *level, const char *fmt, ...);

FloppyDrive::FloppyDrive() {

}


int FloppyDrive::validateTrack(int track) {
  int sectorMap[26];
  int mode = fgetc(file);
  if (feof(file)) {
    return FILE_PREMATURE_EOF;
  }
  if (mode != 0) {
    return FILE_WRONG_MODE;
  }
  int cylinder = fgetc(file);
  if (feof(file)) {
    return FILE_PREMATURE_EOF;
  }
  if (cylinder >76) {
    return FILE_TO_BIG_CYLINDER;
  } 
  if (cylinder != track) {
    return FILE_WRONG_CYLINDER;
  }  
  int head = fgetc(file);
  if (feof(file)) {
    return FILE_PREMATURE_EOF;
  }
  if ((0x3f & head) != 0) {
    return FILE_NOT_SINGLE_SIDED;
  } 
  int numSectors = fgetc(file);
  if (feof(file)) {
    return FILE_PREMATURE_EOF;
  }
  if (numSectors != 26) {
    return FILE_WRONG_NUM_SECTORS;
  }    
  int sectorSize = fgetc(file);
  if (feof(file)) {
    return FILE_PREMATURE_EOF;
  }
  if (sectorSize!=0) {
    return FILE_WRONG_SECTORSIZE;
  }
  for (int i=0; i<26; i++) {
    int sectorId = fgetc(file);
    if (feof(file)) {
      return FILE_PREMATURE_EOF;
    }
    sectorMap[i]=sectorId;
  }
  if (head & 0x80) {
    // read the sector cylinder map
    for (int i=0; i<26; i++) {
      fgetc(file);
      if (feof(file)) {
        return FILE_PREMATURE_EOF;
      }
    } 
  }
  if (head & 0x40) {
    // read the sector cylinder map
    for (int i=0; i<26; i++) {
      fgetc(file);
      if (feof(file)) {
        return FILE_PREMATURE_EOF;
      }
    } 
  }
  for (int i=0; i<26; i++) {
    sectorPointers[track][sectorMap[i]] = ftell(file);
    int sectorHeader = fgetc(file);
    if (feof(file)) {
      return FILE_PREMATURE_EOF;
    }
    switch (sectorHeader) {
      case 0:
        break;
      case 1:
      case 3:
      case 5:
      case 7:
        for (int j=0; j< 128; j++) {
          fgetc(file);
          if (feof(file)) {
            return FILE_PREMATURE_EOF;
          }
        }
        break;
      case 2:
      case 4:
      case 6:
      case 8:
        fgetc(file);
        if (feof(file)) {
          return FILE_PREMATURE_EOF;
        }
        break;
    }
  }
  return FILE_OK;
}

  // open a file and store pointers intenally to each track
  // has to be an IMD with 26 sectors, FM 500 kbps, 128 bytes/sector 77 tracks.
int FloppyDrive::openFile (std::string fileName) {
  file = fopen(fileName.c_str(), "r");
  if (file==NULL)  {
    return FILE_NOT_FOUND;
  }
  while (!feof(file)) {
    int ch;
    ch = fgetc(file);
    if (ch==0x1a) {
      break;
    }
    iMDDescription += ch;
  }
  if (feof(file)) { 
    return FILE_PREMATURE_EOF;
  }
  for (int track=0; track < 77; track++) {
    int ret;
    ret = validateTrack(track); 
    if (ret != FILE_OK) {
      return ret;
    }
  }
  status = true;
  return FILE_OK;
}

void  FloppyDrive::closeFile () {
  status = false;
}
std::string  FloppyDrive::getFileName () {
  return fileName;
}

int FloppyDrive::readSectorLowlevel(char * buffer, int track, int sector) {
  int sectorType, data;
  fseek(file, sectorPointers[track][sector], SEEK_SET);
  sectorType = fgetc(file);
  printLog("INFO", "Seek to pos=%06X Got sectorType=%d \n",sectorPointers[track][sector], sectorType);
  switch (sectorType) {
    case 0:
      return FLOPPY_SECTOR_NOT_FOUND;
      break;
    case 1:
    case 3:
    case 5:
    case 7:
      for (int j=0; j< 128; j++) {
        data = fgetc(file);
        printLog("INFO", "Got data = %02X\n", data);
        *buffer++ = data;
      }
      break;
    case 2:
    case 4:
    case 6:
    case 8:
      data = fgetc(file);
      printLog("INFO", "Got data = %02X\n", data);
      for (int j=0; j< 128; j++) {
        *buffer++ = data;
      }
      break;
  }
  switch (sectorType) {
    case 1:
    case 2:
      return FLOPPY_OK;
    case 3:
    case 4:
      return FLOPPY_DELETED_DATA;
    case 5:
    case 6:
    case 7:
    case 8:
      return FLOPPY_CRC_ERROR;
  }
  return FLOPPY_OK;
}
   // sectornumber is multiplied by two and then both the odd and even 128 byte sector is read
   // read from track given
   // returns a status OK or deleted data
int  FloppyDrive::readSector(char * buffer) {
  int ret;
  int sector = (selectedSector << 1)+1;
  ret = readSectorLowlevel(buffer, selectedTrack, sector);
  if (ret!=FLOPPY_OK) {
    return ret;
  }
  ret = readSectorLowlevel(buffer+128, selectedTrack, sector+1);
  if (ret!=FLOPPY_OK) {
    return ret;
  }
  return FLOPPY_OK;
}

void FloppyDrive::setTrack(int t) {
  selectedTrack = t;
}

void FloppyDrive::setSector(int s) {
  selectedSector = s;
}

bool FloppyDrive::online() {
  return status;
}