#ifndef _CASSETTE_TAPE_
#define _CASSETTE_TAPE_
#include <string>
#include <functional>
#include <vector>




class CassetteTape {

  enum { TAPE_GAP, TAPE_DATA } state;
  
  FILE * file;
  std::string fileName;
  int currentBlockSize;
  int readBytes;
  bool stopAtTapeGap;
  bool writeProtect;
  public:
  void setWriteProtected(bool);
  CassetteTape();

  bool openFile (std::string fileName);
  void closeFile ();

  void rewind();

  std::string getFileName ();

  bool loadBoot (std::function<void(int address, unsigned char)> writeMem);

  void stopTapeMotion();

  bool readBlock (unsigned char * address,  int * size);
  bool readBlock (int address, std::function<void(int address, unsigned char)> writeMem,  int * size);
  unsigned char * readBlock (int * size); 

  int isFileHeader(unsigned char * buffer);

  int isNumericRecord(unsigned char * buffer);

  int isSymbolicRecord(unsigned char * buffer);

  int isChecksumOK(unsigned char * buffer, int size);

  bool readByte(bool direction, unsigned char * data);

  bool isTapeOverGap();
};

#endif