#ifndef _CASSETTE_TAPE_
#define _CASSETTE_TAPE_
#include <string>




class CassetteTape {

  enum { TAPE_GAP, TAPE_DATA } state;
  
  FILE * file;
  std::string fileName;
  int currentBlockSize;
  //int timeoutReadByteHandler (std::function<void(unsigned char)>); 
  int timeoutReadByteHandler ();
  int readBytes;
  std::function<void(unsigned char)> readCb;
  std::function<void(bool)> tapeGapCb;
  public:

  CassetteTape(std::function<void(bool)> );

  bool openFile (std::string fileName);
  void closeFile ();

  void rewind();

  std::string getFileName ();

  void loadBoot (unsigned char * address);


  void readBlock (unsigned char * address,  int * size);

  unsigned char * readBlock (int * size); 

  int isFileHeader(unsigned char * buffer);

  int isNumericRecord(unsigned char * buffer);

  int isSymbolicRecord(unsigned char * buffer);

  int isChecksumOK(unsigned char * buffer, int size);

  void readByte(std::function<void(unsigned char)>);
};

#endif