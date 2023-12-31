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

  public:

  CassetteTape();

  bool openFile (std::string fileName);
  void closeFile ();

  void rewind();

  std::string getFileName ();

  bool loadBoot (unsigned char * address);

  void stopTapeMotion();

  bool readBlock (unsigned char * address,  int * size);

  unsigned char * readBlock (int * size); 

  int isFileHeader(unsigned char * buffer);

  int isNumericRecord(unsigned char * buffer);

  int isSymbolicRecord(unsigned char * buffer);

  int isChecksumOK(unsigned char * buffer, int size);

  void readByte(bool direction, unsigned char * data);

  bool isTapeOverGap();
};

#endif