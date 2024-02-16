#include <stdio.h>
#include <cstdio>
#include <string.h>
#include <cstdlib>
int main () {
  struct instruction {
    char mnemonic[16];
    int type;
  };
  struct instruction instructions [2304];
  char buffer [1024];
  char model[128], implicit[128], instructionType[128], opCode[128], mnemonic[128], instructionTime2200_1[128], instructionTime2200_2[128], instructionTime5500[128];
  int cnt=0;
  int set;
  for (int i=0;i < 2304; i++) {
    instructions[i].type=0;
    strcpy(instructions[i].mnemonic, "---");
  }
  while (fgets(buffer, 1024, stdin)) {
    if (cnt!=0) {
      int len = strlen(buffer);
      buffer[len-1]=0;
      sscanf(buffer, "%[^\011] %[^\011] %[^\011] %[^\011] %[^\011] %[^\011] %[^\011] %[^\011]", model, implicit, instructionType, opCode, mnemonic, instructionTime2200_1, instructionTime2200_2, instructionTime5500);
      //printf("Read: Length %d  %6s %3s %1s %3s %12s %10s %10s %10s  ",len,  model, implicit, instructionType, opCode, mnemonic, instructionTime2200_1, instructionTime2200_2, instructionTime5500);
      if (strcmp(model, "2200-1")==0) {
        //printf("Model is 2200 I");
      }
      if (strcmp(model, "2200-2")==0) {
        //printf("Model is 2200 II");
      }
      if (strcmp(model, "5500")==0) {
        //printf("Model is 5500");
      }  
      long implicitNum = strtol(implicit, NULL, 8);
      //printf("implcit=%03lo ", implicitNum);
      switch (implicitNum) {
        case 0:
          set=0;
          break;
        case 022:
          set=1;
          break;
        case 062:
          set=2;
          break;
        case 0111:
          set=3;
          break;
        case 0113:
          set=4;
          break;
        case 0115:
          set=5;
          break;
        case 0117:
          set=6;
          break;
        case 0174:
          set=7;
          break;
        case 0176:
          set=8;
          break;
        default:
          set=0;
          break;
      }
      //printf("set=%d ", set);
      long instructionTypeNum = strtol(instructionType, NULL, 10);
      //printf("instructionType=%ld ", instructionTypeNum);   
      long opCodeNum = strtol(opCode, NULL, 8);
      //printf("opCodeNum=%03lo ", opCodeNum); 
      //printf("mnemonic=%s ", mnemonic); 
      instructions[set*256+(0xff&opCodeNum)].type = instructionTypeNum;
      strcpy(instructions[set*256+(0xff&opCodeNum)].mnemonic, mnemonic);    
      //printf ("\n");  
    }
    cnt++;
  }
  printf("struct instruction {\n");
  printf("  int type;\n");
  printf("  const char * mnemonic;\n");
  printf("};\n");
  printf("struct instruction instructionSet[2304] = {\n    ");
  for (int i=0;i < 2304; i++) {
    printf ("{%d, \"%s\"}", instructions[i].type, instructions[i].mnemonic);
    if (i!=2303) printf(",");
    if ((i%8)==7) printf("\n    ");
  }
  printf("};");
  return 0;
}