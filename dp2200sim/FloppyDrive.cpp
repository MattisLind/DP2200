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
    printLog("INFO", "Got numSectors=%d on track %d file pos=%ld",numSectors, track, ftell(file));
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
          int data;
          data = fgetc(file);
          diskImage[track][sectorMap[i]-1][j]=data;
          if (feof(file)) {
            return FILE_PREMATURE_EOF;
          }
        }
        break;
      case 2:
      case 4:
      case 6:
      case 8: {
        int compressedData;
        compressedData = fgetc(file);
        for (int j=0; j<128; j++) {
          diskImage[track][sectorMap[i]-1][j] = compressedData;
        }
        if (feof(file)) {
          return FILE_PREMATURE_EOF;
        }
        break;
      }
    }
  }
  return FILE_OK;
}

  // open a file and store pointers intenally to each track
  // has to be an IMD with 26 sectors, FM 500 kbps, 128 bytes/sector 77 tracks.
int FloppyDrive::openFile (std::string fileName) {
  if (file!=NULL) {
    fclose(file);
  }
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
      printLog("INFO", "Validate track returned not OK for track %d filepos=%ld\n", track, ftell(file));
      return ret;
    }
  }
  status = true;
  return FILE_OK;
}

void  FloppyDrive::closeFile () {
  status = false;
  fclose(file);
}
std::string  FloppyDrive::getFileName () {
  return fileName;
}

   // sectornumber is multiplied by two and then both the odd and even 128 byte sector is read
   // read from track given
   // returns a status OK or deleted data
int  FloppyDrive::readSector(char * buffer) {
  int i;
  int sector = (selectedSector << 1);
  for (i=0;i<128;i++) {
    buffer[i]=diskImage[selectedTrack][sector][i];
  }
  for (i=0;i<128;i++) {
    buffer[i+128]=diskImage[selectedTrack][sector+1][i];
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

int FloppyDrive::writeSector(char * buffer) {
  int i;
  int sector = (selectedSector << 1);
  for (i=0;i<128;i++) {
    diskImage[selectedTrack][sector][i]=buffer[i];
  }
  for (;i<256;i++) {
    diskImage[selectedTrack][sector+1][i]=buffer[i];
  }
  return FLOPPY_OK;
}