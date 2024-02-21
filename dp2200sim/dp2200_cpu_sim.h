#ifndef _DP2200_CPU_
#define _DP2200_CPU_
#include "cassetteTape.h"
#include "dp2200_io_sim.h"
#include <cstdio>
#include <string>

class dp2200_cpu {
  int blockTransfer(bool);
public:
  // Define Registerset
  enum Reg { A, B, C, D, E, H, L, X };
  // unsigned char regs[sizeof(Reg),2];

  // accumulator and scratch registers
  struct r {
    unsigned char regA;
    unsigned char regB;
    unsigned char regC;
    unsigned char regD;
    unsigned char regE;
    unsigned char regH;
    unsigned char regL;
    unsigned char regX;
  };

  

  int setSel;

  union regs {
    unsigned char regs[8];
    struct r r;
  };

  union regs regSets[2];

  struct address {
    unsigned char regH;
    unsigned char regL;
  };

  // PC and stack
  union stack {
    struct address stack[16];
    unsigned short stk[16];
  } stack;

  unsigned char stackptr;

  struct doubleLoadStoreRegisterTable {
    int dstH;
    int dstL;
    int indexH;
    int indexL;
  };

  #define REG_A 0
  #define REG_B 1
  #define REG_C 2
  #define REG_D 3
  #define REG_E 4
  #define REG_H 5
  #define REG_L 6
  #define REG_X 7

  struct doubleLoadStoreRegisterTable t [10] = {{-1, -1, -1, -1},
                                                { REG_D, REG_E, REG_H, REG_L},
                                                { REG_B, REG_C, REG_H, REG_L},
                                                { REG_B, REG_C, REG_B, REG_C},
                                                { REG_B, REG_C, REG_D, REG_E},
                                                { REG_D, REG_E, REG_B, REG_C}, 
                                                { REG_D, REG_E, REG_D, REG_E},
                                                { REG_H, REG_L, REG_B, REG_C},
                                                { REG_H, REG_L, REG_D, REG_E},
                                                { REG_H, REG_L, REG_H, REG_L}};

  // flags
  unsigned char flagParity[2];
  unsigned char flagSign[2];
  unsigned char flagCarry[2];
  unsigned char flagZero[2];
  bool userMode;
  bool accessViolation;
  bool writeViolation;
  int interruptPending;
  int interruptEnabled;

  unsigned short P;
  unsigned short previousP;

  class Memory {
    struct SectorEntry {
      bool writeEnable;
      bool accessEnable;
      unsigned char physicalPage;
    };
    bool * is5500;
    bool * accessViolation;
    bool * writeViolation;
    bool * userMode;
    bool memoryWatch[65536];

    unsigned char memory[65536];
    public:
    struct SectorEntry sectorTable[16];
    unsigned char baseRegister;
    unsigned char physicalMemoryRead(int address);
    int size();
    void physicalMemoryWrite(int address, unsigned char data);
    bool addWatch(unsigned short address);
    bool removeWatch (unsigned short address);
    unsigned char read(unsigned short address, bool performChecks=true, bool fetch=false, int from=0);
    void write(unsigned short address, unsigned char data, int from=0);
    Memory(bool * is5500, bool * accessViolation, bool * writeViolation, bool * userMode); 
  };

  // 64K memory - works with 5500 as well.
  // unsigned char memory[0x10000];
  class Memory *  memory;
  unsigned long instructions = 0;
  unsigned long fetches = 0;
  unsigned int outbitcnt = 0;
  unsigned int inbitcnt = 0;
  int timeForInstruction;
  struct timespec totalInstructionTime, startedTime;
  // bool running;

  struct inst {
    unsigned char data[5];
    unsigned short address;
  };

  std::vector<struct inst> instructionTrace;
  std::vector<unsigned short> breakpoints;

  class IOController * ioCtrl;
  bool traceEnabled = false;
  unsigned short startAddress;
  bool keyboardLightStatus=false;
  bool displayLightStatus=false;
  bool keyboardButtonStatus=false;
  bool displayButtonStatus=false;


  bool octal;
  bool is5500;
  bool is2200;
  unsigned short pMask = 0x3fff;
  unsigned char hMask = 0x3f;
  int memorySize;
  unsigned char implicit;
  // rather than having ioAddress and ioStatus in CPU class a ioController class is handling all IO.
  // This class handles ioStatus and ioAddress.
  // It has methods to handle read and all the other commands.
  // The controller instantiates 16 different device classes which inherits from a base class. 
  // Each device instance handles all the peculiarities of the device as well as dispatching tasks to be run on the main thread
  // when delays are needed.

  #include "datapoint_instruction_set.h"

  int instTimeInNsTaken[256] = {   0,    0, 3200, 3200, 4800,    0, 3200, 3200, 
                                   0,    0, 3200, 3200, 4800,    0, 3200,    0,
                                3200,    0,    0, 3200, 4800,    0, 3200,    0, 
                                3200,    0,    0, 3200, 4800,    0, 3200,    0,  
                                3200,    0,    0, 3200, 4800,    0, 3200,    0, 
                                3200,    0,    0, 3200, 4800,    0, 3200,    0, 
                                4800,    0,    0, 3200, 4800,    0, 3200,    0, 
                                3200,    0,    0, 3200, 4800,    0,    0,    0,
                                6400, 9600, 6400,    0, 6400,    0, 6400,    0,
                                6400,    0, 6400,    0,    0,    0,    0,    0,
                                6400, 9600, 6400, 9600,    0, 9600,    0, 9600,
                                6400, 9600, 6400, 9600,    0, 9600,    0, 9600,
                                6400,    0, 6400,    0,    0,    0,    0,    0,
                                6400, 9600, 6400, 9600,    0, 9600,    0, 9600, 
                                6400, 9600, 6400, 9600,    0,    0,    0, 9600,
                                6400, 9600, 6400, 9600,    0, 9600,    0, 9600,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800, 
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200,    0, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200,    0, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200,    0, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200,    0, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200,    0, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200,    0, 4800,
                                4800, 4800, 4800, 4800, 4800, 4800, 4800,    0,
  };
  int instTimeInNsNotTkn[256] = {  0,    0, 3200, 3200, 4800,    0, 3200, 3200, 
                                   0,    0, 3200, 3200, 4800,    0, 3200,    0,
                                3200,    0,    0, 3200, 4800,    0, 3200,    0, 
                                3200,    0,    0, 3200, 4800,    0, 3200,    0,  
                                3200,    0,    0, 3200, 4800,    0, 3200,    0, 
                                3200,    0,    0, 3200, 4800,    0, 3200,    0, 
                                4800,    0,    0, 3200, 4800,    0, 3200,    0, 
                                3200,    0,    0, 3200, 4800,    0,    0,    0,
                                4800, 9600, 4800,    0, 6400,    0, 6400,    0,
                                4800,    0, 4800,    0,    0,    0,    0,    0,
                                4800, 9600, 4800, 9600,    0, 9600,    0, 9600,
                                4800, 9600, 4800, 9600,    0, 9600,    0, 9600,
                                4800,    0, 4800,    0,    0,    0,    0,    0,
                                4800, 9600, 4800, 9600,    0, 9600,    0, 9600, 
                                4800, 9600, 4800, 9600,    0,    0,    0, 9600,
                                4800, 9600, 4800, 9600,    0, 9600,    0, 9600,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800, 
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200,    0, 3200, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200,    0, 3200, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200,    0, 3200, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200,    0, 3200, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200,    0, 3200, 4800,
                                3200, 3200, 3200, 3200, 3200, 3200,    0, 4800,
                                4800, 4800, 4800, 4800, 4800, 4800, 4800,    0,
  };

  void reset();
  void setCPUtype2200 ();
  void setCPUtype5500 ();
  int execute();
  void clear();
  char *  disassembleLine(char * outputBuf, int size, bool octal, int address, std::function<unsigned char(int)> readMem, int imp);
  char *  disassembleLine(char * outputBuf, int size, bool octal, int address, std::function<unsigned char(int)> readMem);
  char *  disassembleLine(char * outputBuf, int size, bool octal, int address, int imp);
  char *  disassembleLine(char * outputBuf, int size, bool octal, unsigned char * address, int imp);
  char *  disassembleLine(char * outputBuf, int size, bool octal, int address);
  char *  disassembleLine(char * outputBuf, int size, bool octal, unsigned char * address);  
  int addBreakpoint(unsigned short address);
  int removeBreakpoint(unsigned short address);
  dp2200_cpu();
  private:
  int stackStore();
  int stackLoad();
  int doubleLoad();
  int doubleStore();
  int registerStore();
  int registerLoad();
  struct doubleLoadStoreRegisterTable getSourceAndIndex();
  int registerFromImplict(int implict); 
  void incrementRegisterPair (int highReg, int lowReg, int decrement);
  int getHighRegFromImplicit();
  int getLowRegFromImplicit();
  inline unsigned short getPagedAddress();
  int immediateplus(unsigned char inst);
  int iojmpcall(unsigned char inst);
  int mathboolean(unsigned char inst);
  int load(unsigned char inst);
  int chkconditional(unsigned char inst);
  void setflags(int result, int carryzero);
  void setflagsinc(int result);
  void setparity(int result);

};

#endif