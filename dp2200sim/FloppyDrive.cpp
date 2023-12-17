#include "FloppyDrive.h"

void printLog(const char *level, const char *fmt, ...);

FloppyDrive::FloppyDrive() {
  file=NULL;
}

int FloppyDrive::writeTrackBackIMD(int track) {
  fputc(0, file); // mode
  fputc(track, file); // track
  fputc(0, file);  // single sided
  fputc(26, file);
  fputc(0, file); // sector size 128 bytes.
  // write the sector map - just a linear table for simplicity from sector 1 to sector 26
  for (int i=0;i<26;i++) {
    fputc(i+1, file);
  }
  for (int i=0;i<26;i++) {
    bool compressable=true;
    unsigned char firstByte = diskImage[selectedTrack][i].data[0];
    for (int j=1;j<128;j++) {
      if (firstByte!=diskImage[selectedTrack][i].data[j]) {
        compressable = false;
        break; 
      } 
    }
    switch (diskImage[selectedTrack][i].sectorType) {
      case 1:
      case 2:
        if (compressable) {
          fputc(2, file); // compresed 
        } else {
          fputc(1, file);
        }
        break;
      case 3:
      case 4:
        if (compressable) {
          fputc(4, file); // compresed 
        } else {
          fputc(3, file);
        } 
        break;
      case 5:
      case 6:
        if (compressable) {
          fputc(5, file); // compresed 
        } else {
          fputc(6, file);
        } 
        break; 
      case 7:
      case 8:
        if (compressable) {
          fputc(8, file); // compresed 
        } else {
          fputc(7, file);
        } 
        break;               
    } 
    if (compressable) {
      fputc(firstByte, file);
    } else {
      for (int j=0;j<128;j++) {
        fputc(diskImage[selectedTrack][i].data[j], file);  
      }
    }     
  }  
  return 0;
}

int FloppyDrive::writeBackIMD() {
  rewind(file);
  for (int i=0;i < iMDDescription.length(); i++) {
    fputc(iMDDescription[i], file);
  }
  fputc(0x1a, file); 
  for (int i=0; i<77; i++) {
    writeTrackBackIMD(i);
  }
  return 0;
}

int FloppyDrive::writeBackRaw() {
  rewind(file);
  for (int i=0; i<77; i++) {
    for (int j=0; j<26; j++) {
      fwrite(diskImage[i][j].data, 128, 1, file);
    }
  }
  return 0;
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
    diskImage[track][sectorMap[i]-1].sectorType = sectorHeader;
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
          diskImage[track][sectorMap[i]-1].data[j]=data;
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
          diskImage[track][sectorMap[i]-1].data[j] = compressedData;
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
  bool imdFailed=false;
  int ret;
  closeFile();
  file = fopen(fileName.c_str(), "r+");
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
    imdFailed=true;
  }
  for (int track=0; track < 77; track++) {
    ret = validateTrack(track); 
    if (ret != FILE_OK) {
      imdFailed=true;
      printLog("INFO", "Validate track returned not OK for track %d filepos=%ld\n", track, ftell(file));
      break;
    }
  }
  if (imdFailed) {
    // try to read it as a raw 256256 bytes image. 77 tracks, 26 sectors, 128 bytes each.
    fseek(file, 0, SEEK_END);
    long int size= ftell(file);
    printLog("INFO", "Read image file of size %ld \n");
    if (size == 256256) {
      rewind(file);
      imageTypeIsIMD = false;
      for (int i=0; i<77; i++) {
        for (int j=0; j<26; j++) {
          fread(diskImage[i][j].data, 128, 1, file);
          diskImage[i][j].sectorType = 1;
        }
      }
    } else {
      return ret;
    }
  } else {
    imageTypeIsIMD = true;
  }
  status = true;
  return FILE_OK;
}

void  FloppyDrive::closeFile () {
  status = false;
  if (file!=NULL) {
    if (imageTypeIsIMD) {
      writeBackIMD();
    } else {
      writeBackRaw();
    }
    fclose(file);
    file=NULL;
  }
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
  unsigned char s1t =diskImage[selectedTrack][sector].sectorType, s2t = diskImage[selectedTrack][sector+1].sectorType;
  for (i=0;i<128;i++) {
    buffer[i]=diskImage[selectedTrack][sector].data[i];
  }
  for (i=0;i<128;i++) {
    buffer[i+128]=diskImage[selectedTrack][sector+1].data[i];
  }
  if (((s1t == 1) || (s1t == 2)) && ((s2t == 1) || (s2t == 2))) {
    return FLOPPY_OK;
  } else if ((s1t==3)||(s1t==4)||(s2t==3)||(s2t==4)) {
    return FLOPPY_DELETED_DATA;
  } else if ((s1t==0) || (s2t==0)) {
    return FLOPPY_SECTOR_NOT_FOUND;
  } else {
    return  FLOPPY_CRC_ERROR;
  }
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
    diskImage[selectedTrack][sector].data[i]=buffer[i];
  }
  diskImage[selectedTrack][sector].sectorType = 1;
  for (;i<256;i++) {
    diskImage[selectedTrack][sector+1].data[i]=buffer[i];
  }
  diskImage[selectedTrack][sector+1].sectorType = 1;

  return FLOPPY_OK;
}