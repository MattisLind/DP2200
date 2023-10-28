#include <stdio.h>
#include <stdlib.h>

unsigned char * readBlock (FILE * in, int * size) {
  unsigned char * buffer;
  fread(size, 4, 1, in);
  buffer = (unsigned char *) malloc(*size);
  fread(buffer, *size, 1, in);
  fread(size, 4, 1, in);
  return buffer;
}

int isFileHeader(unsigned char * buffer) {
  if ((buffer[0]==0201) && (buffer[1]==0176)) {
    return 1;
  } 
  return 0;
}

int isNumericRecord(unsigned char * buffer) {
  if ((buffer[0]==0303) && (buffer[1]==0074)) {
    return 1;
  } 
  return 0;
}

int isSymbolicRecord(unsigned char * buffer) {
  if ((buffer[0]==0347) && (buffer[1]==0030)) {
    return 1;
  } 
  return 0;
}

int isChecksumOK(unsigned char * buffer, int size) {
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

int main (int argc, char * argv[])  {
  FILE * in;
  unsigned char * buffer;
  int startingAddress;
  int size;
  in = fopen(argv[1], "r");
  if (in == NULL) {
    exit(1);
  }
  while (!feof(in)) {
    buffer = readBlock(in, &size);
    if (isFileHeader(buffer)) {
      printf("This is a FileHeader. ");
      if (size != 4) {
        printf("The size of this block is incorrect. It is %d bytes rather than 4 bytes. ", size);
      }
      printf("The filenumber is %d. ", 0xff&buffer[2]);
      if ((0xff&buffer[2])!=(0xff&(~buffer[3]))) {
        printf("The inverted filenumber is incorrect: %d should have been %d.", 0xff&buffer[3], 0xff&(~buffer[2]));
      }
      if (buffer[2]==127) {
        printf("This is the end of tape marker. Files beyond this point is probably damaged.");
      }
      printf("\n");
    } else if (isNumericRecord(buffer)) {
      printf("This is a numeric record with %d bytes. ", size);
      if (!isChecksumOK(buffer, size)) {
        printf("The checksum is NOT OK.");
      }
      startingAddress = (buffer[4] << 8) | buffer[5];
      printf ("Load address for this block is %05o. ", startingAddress);
      if ((buffer[4] != (0xFF&~buffer[6])) || (buffer[5] != (0xFF&~buffer[7]))) {
        printf("Loading address corrupted.");
      }
      printf("\n");

    } else if (isSymbolicRecord(buffer)) {
      printf("This is a symbolic record with %d bytes. ", size);
      printf("\n");
    } else {
      // something else - should be the first block which is the boot block - 
      printf("This is something else. Only the first boot block should be like this. ");
      printf("\n");
    }
  }
  return 0;
} 