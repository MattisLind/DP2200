#ifndef _FLOPPY_DRIVE_
#define _FLOPPY_DRIVE_
#include <string>
#include <functional>
#include <vector>

#define FILE_OK 0
#define FILE_NOT_FOUND -1
#define FILE_PREMATURE_EOF -2
#define FILE_WRONG_MODE -3
#define FILE_WRONG_SECTORSIZE -4
#define FILE_NOT_SINGLE_SIDED -5
#define FILE_TO_BIG_CYLINDER -6
#define FILE_WRONG_CYLINDER -7
#define FILE_WRONG_NUM_SECTORS -8
#define FILE_WRONG_INTERLEAVE -9

#define FLOPPY_OK 0
#define FLOPPY_SECTOR_NOT_FOUND -1
#define FLOPPY_DELETED_DATA -2
#define FLOPPY_CRC_ERROR -3


class FloppyDrive {

  FILE * file;
  std::string fileName;
  bool status;
  std::string iMDDescription; 
  long sectorPointers[77][26];
  int validateTrack(int);
  int readSectorLowlevel(char * buffer, int track, int sector);
  int selectedTrack;
  int selectedSector;
  public:

  FloppyDrive();
  // open a file and store pointers intenally to each track
  // has to be an IMD with 26 sectors, FM 500 kbps, 128 bytes/sector 77 tracks.
  int openFile (std::string fileName);
  void closeFile ();
  std::string getFileName ();
   // sectornumber is multiplied by two and then both the odd and even 128 byte sector is read
   // read from track given
   // returns a status OK or deleted data
  int readSector(char * buffer); 
  void setTrack(int t);
  void setSector(int s);
  bool online();
};

#endif