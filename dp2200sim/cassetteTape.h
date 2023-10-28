#ifndef _CASSETTE_TAPE_
#define _CASSETTE_TAPE_
#include <string>


class CassetteTape {
  FILE * file;
  std::string fileName;
  public:

  CassetteTape();

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
};

#endif