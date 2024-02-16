//
//
// Based on M. Willegal 8008 disassembler / simulator
//

#include <stdarg.h>
#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <termios.h>
#include "dp2200_cpu_sim.h"
#include <functional>
#include <algorithm>
#include "5500firmware.h"

extern bool running; 

void printLog(const char *level, const char *fmt, ...);

/*unsigned char & dp2200_cpu::Memory::operator[](int address) {
  //printLog("INFO", "Accessed address %05o\n", address);
  if (memoryWatch[address]) {
    printLog("INFO", "Accessed address %05o - halting\n", address);
    //running = false;
  }
  return memory[address];
}*/


unsigned char dp2200_cpu::Memory::physicalMemoryRead(int physicalAddress) {
  if (*is5500) {
    if ((physicalAddress >= 0) && ( physicalAddress< 0xC000)) {
      return memory[physicalAddress]; 
    } else if (physicalAddress >= 0xE000 && (physicalAddress < 0xF000) ) {
      return memory[physicalAddress]; 
    } else if ((physicalAddress >= 0xF000) && ( physicalAddress<= 0xFFFF)) {
      return firmware[physicalAddress & 0xFFF];
    }
  }
  return memory[physicalAddress];
} 

void dp2200_cpu::Memory::physicalMemoryWrite(int physicalAddress, unsigned char data) {

  if (*is5500) {
    if ((physicalAddress >= 0) && ( physicalAddress< 0xC000)) {
      memory[physicalAddress]= data; 
    } else if (physicalAddress >= 0xE000 && (physicalAddress < 0xF000) ) {
      memory[physicalAddress]=data; 
    } else if ((physicalAddress >= 0xF000) && ( physicalAddress<= 0xFFFF)) {
      firmware[physicalAddress & 0xFFF]=data;
    }
  } else {
    if (physicalAddress < 0x4000) {
      memory[physicalAddress] = data;
    }
  }

}

unsigned char inline dp2200_cpu::Memory::read(unsigned short virtualAddress, bool performChecks) {
  int physicalAddress, logicalPage, physicalPage;

  if (*is5500) {
    if ((virtualAddress & 0xC000) == 0x8000) {
      physicalAddress = 0xffff &  ((virtualAddress + (baseRegister<<8))  | (virtualAddress & 0xff)); 
    } else {
      physicalAddress = virtualAddress; 
    }
    logicalPage = (physicalAddress & 0xF000) >> 12;
    if (logicalPage == 017) { // Fixed mapped
      physicalPage = 017;
    } else {
      physicalPage = sectorTable[logicalPage].physicalPage;
      physicalAddress = ((0xf & physicalPage) << 12) | (physicalAddress & 0xfff);
      if (!sectorTable[logicalPage].accessEnable && *userMode && performChecks) {
        *accessViolation = true;
      } else {
        *accessViolation = false;
      }
    }
  } else {
    physicalAddress = virtualAddress;
  }
  return physicalMemoryRead(physicalAddress);
}


void inline dp2200_cpu::Memory::write(unsigned short virtualAddress, unsigned char data) {
  int physicalAddress, logicalPage, physicalPage;

  if (*is5500) {
    if ((virtualAddress & 0xC000) == 0x8000) {
      physicalAddress = 0xffff &  ((virtualAddress + (baseRegister<<8))  | (virtualAddress & 0xff)); 
    } else {
      physicalAddress = virtualAddress; 
    }
    logicalPage = (physicalAddress & 0xF000) >> 12;
    if (logicalPage == 017) { // Fixed mapped
      physicalPage = 017;
    } else {
      physicalPage = sectorTable[logicalPage].physicalPage;
      physicalAddress = ((0xf & physicalPage) << 12) | (physicalAddress & 0xfff);
      if (!sectorTable[logicalPage].accessEnable & *userMode) {
        *accessViolation = true;
      } else {
        *accessViolation = false;
      }
      if (sectorTable[logicalPage].writeEnable) {
        *writeViolation = false;
      } else {
        *writeViolation = true;
      }      
    }
  } else {
    physicalAddress = virtualAddress;
  } 
  if (memoryWatch[physicalAddress]) {
    printLog("INFO", "Writing to address %05o - halting\n", physicalAddress);
    running=false;
    return;
  }   
  physicalMemoryWrite(physicalAddress, data);
}

dp2200_cpu::Memory::Memory(bool * is, bool * av, bool * wv, bool * um) {
  // clear the entire memoryWatch array
  for (int address=0;address <=65535; address++) {
    memoryWatch[address]=false;
  }
  baseRegister = 0;
  for (int i=0; i<16; i++) { 
    sectorTable[i].physicalPage=i; 
    sectorTable[i].accessEnable=true;
    sectorTable[i].writeEnable=true;
  }
  sectorTable[017].writeEnable=false;
  accessViolation = av;
  writeViolation = wv;
  is5500 = is;
  userMode = um;
}

int dp2200_cpu::Memory::size() {
  return sizeof(memory);
}

bool dp2200_cpu::Memory::addWatch(unsigned short address) {
  memoryWatch[address]=true;
  return false;
}

bool dp2200_cpu::Memory::removeWatch(unsigned short address) {
  memoryWatch[address]=false;  
  return false;
}


  /***************************************************************************

   decoded opcodes



  MS 2 bits =  00  INC/DEC/ROTATE, RST/RET and immediate OPERATIONS
  LS 3 bits
  ---------------------------------------------------------------------
  000 increment (bits 3-5 determine reg except 0 = hlt inst)

  001 decrement (bits 3-5 determine reg except 0 = hlt inst)

  010 rotate  (bits 3-5 specify
      000 A left
      001 A right
      010 A left through carry
      011 A right through carry
      1xx ????

  011 conditional return

  100 add/subtract/and/or/eor/cmp immediate
      bits 3-5
      000 add w/o carry
      001 add w carry
      010 subtract w/o borrow
      011 subtract w borrow
      100 and
      101 xor
      110 or
      111 cmp

  101 RST -call routine in low mem (address = inst & 0x38)

  110 load immediate(bits 3-5 determine dest)

  111 unconditional return


  MS 2 bits =  01  I/O, JUMP AND CALL
  LS 3 bits
  ---------------------------------------------------------------------
  000 jump conditionally
  XX1 input/output
  010 call conditionally
  100 jump
  110 call

  MS 2 bits =  10   MATH AND BOOLEAN OPERATIONS
  bits 3-5
  ---------------------------------------------------------------------
  000 add (bit 0-2 determines source)
  001 add w/carry (bit 0-2 determines source)
  010 subtract (bit 0-2 determines source)
  011 subtract w/borrow (bit 0-2 determines source)
  100 and (bit 0-2 determines source)
  101 xor (bit 0-2 determines source)
  110 or (bit 0-2 determines source)
  111 cmp (bit 0-2 determines source)

  MS 2 bits =  11  LOAD INSTRUCTION
  ---------------------------------------------------------------------
  XXX load (bits 0-5 determine src/dest,except 0xFF = halt)

  *********************************************************************/



char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, int address, std::function<unsigned char(int)> readMem) {
    unsigned char instruction = readMem(address);
    int r=registerFromImplict(implicit); 
    switch (instr_l[r][instruction]) {
    case 0:
    case 1:
      snprintf(outputBuf, size, "%s", mnems[r][instruction]);
    break;
    case 2:
      if (octal) {
        snprintf(outputBuf, size, "%s %03o", mnems[r][instruction], readMem(address+1));
      } else {
        snprintf(outputBuf, size, "%s %02X", mnems[r][instruction], readMem(address+1));
      }
    break;
    case 3:
      if (octal) {
        snprintf(outputBuf, size, "%s %05o", mnems[r][instruction], (readMem(address+2) << 8) | readMem(address+1));
      } else {
        snprintf(outputBuf, size, "%s %04X", mnems[r][instruction], (readMem(address+2) << 8) | readMem(address+1));
      }
    break;
  }
  return outputBuf;
}

char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, int address) {
  return disassembleLine(outputBuf, size, octal,  address, [memory=memory](int address)->unsigned char { return memory->read(address, false);} );
}

char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, unsigned char * buffer) {
  return disassembleLine(outputBuf, size, octal,  0 , [buffer=buffer](int address)->unsigned char { return *(buffer+address);} );
}

int dp2200_cpu::addBreakpoint(unsigned short address) {
  if (breakpoints.size()>8) {
    return 1;
  }
  if ( std::find(breakpoints.begin(), breakpoints.end(), address) == breakpoints.end() ) {
    breakpoints.push_back(address);
  }
  return 0;
}
int dp2200_cpu::removeBreakpoint(unsigned short address) {
  auto found = std::find(breakpoints.begin(), breakpoints.end(), address);
  if ( found != breakpoints.end() ) {
    breakpoints.erase(found);
  }
  return 0; 
}

void dp2200_cpu::setCPUtype2200() {
  pMask = 0x3fff;
  hMask = 0x3f;
  is5500 = false;
  is2200 = true;
}


void dp2200_cpu::setCPUtype5500() {
  pMask = 0xffff;
  hMask = 0xff;
  is5500=true;
  is2200=false;
}



void dp2200_cpu::clear() {
  for (int i=0; i<memory->size();i++ ) memory->write(i,0);
}

void dp2200_cpu::reset() {
    unsigned char i, j;

    for (i = A; i <= L; i++)
      for (j = 0; j < 2; j++)
        regSets[j].regs[i] = 0;
    for (i = 0; i < 16; i++)
      stack.stk[i] = 0;
    P = 0;
    stackptr = 0;
    setSel = 0;
    interruptPending = 0;
    interruptEnabled = 0;
}

int dp2200_cpu::execute() {
  unsigned char inst;
  char buffer[32]; 
  int timeForInstruction;
  int halted;
  struct inst i;
  unsigned char instructionData;
  /* Handle interrupt*/

  if (interruptEnabled && interruptPending) {
    interruptPending = 0;

    /* now bump stack to use new pc and save it */
    stack.stk[stackptr] = P;
    stackptr = (stackptr + 1) & 0xf;
    P = 0;
  }
  inst = memory->read(P);

  switch (inst) {
    case 0022:
    case 0062:
    case 0111:
    case 0113:
    case 0115:
    case 0117:
    case 0174:
    case 0176:
      // 
      if (is5500) {
        implicit = inst;
        
        P++;
        fetches++;
        inst = memory->read(P);
        printLog("INFO", "Got implicit=%03o opcode=%03o\n", implicit, inst);
      } else {
        return 1;  // halt on 2200.
      }
      break;
    default:
      implicit = 0;
  }

  i.data[0] = memory->read(P);
  i.data[1] = memory->read(P+1);
  i.data[2] = memory->read(P+2);
  i.address = P;
  if (instructionTrace.size()>=16) {
    instructionTrace.erase(instructionTrace.begin());
  }

  instructionTrace.push_back(i);
  timeForInstruction = instTimeInNsNotTkn[inst];
  disassembleLine(buffer, 32, false, P);
  instructionData = memory->read(P);
  P++;
  P &= pMask;
  fetches++;

  


  /* call into four major instruction subgroups for decoding */

  switch (inst >> 6) {
  case 0:
    halted = immediateplus(inst);
    break;
  case 1:
    halted = iojmpcall(inst);
    break;
  case 2:
    halted = mathboolean(inst);
    break;
  case 3:
    halted = load(inst);
    break;
  }
  totalInstructionTime.tv_nsec+=timeForInstruction;
  if (totalInstructionTime.tv_nsec >= 1000000000) {
    totalInstructionTime.tv_nsec-=1000000000;
    totalInstructionTime.tv_sec++;
  }
  if (traceEnabled) { 
    if (octal) {
      printLog("TRACE", "%05o %03o %s -> %s | A=%03o B=%03o C=%03o D=%03o E=%03o H=%03o L=%03o C=%1d Z=%1d P=%1d S=%1d | A=%03o B=%03o C=%03o D=%03o E=%03o H=%03o L=%03o C=%1d Z=%1d P=%1d S=%1d \n", i.address, instructionData, buffer, setSel==0?"ALPHA":"BETA", regSets[0].r.regA,regSets[0].r.regB,regSets[0].r.regC,regSets[0].r.regD,regSets[0].r.regE,regSets[0].r.regH,regSets[0].r.regL,flagCarry[0], flagZero[0], flagParity[0], flagSign[0], regSets[1].r.regA,regSets[1].r.regB,regSets[1].r.regC,regSets[1].r.regD,regSets[1].r.regE,regSets[1].r.regH,regSets[1].r.regL, flagCarry[1], flagZero[1], flagParity[1], flagSign[1]);
    } else {
      printLog("TRACE", "%04X %02X %s -> %s | A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X C=%1d Z=%1d P=%1d S=%1d | A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X C=%1d Z=%1d P=%1d S=%1d \n", i.address, instructionData, buffer, setSel==0?"ALPHA":"BETA", regSets[0].r.regA,regSets[0].r.regB,regSets[0].r.regC,regSets[0].r.regD,regSets[0].r.regE,regSets[0].r.regH,regSets[0].r.regL,flagCarry[0], flagZero[0], flagParity[0], flagSign[0], regSets[1].r.regA,regSets[1].r.regB,regSets[1].r.regC,regSets[1].r.regD,regSets[1].r.regE,regSets[1].r.regH,regSets[1].r.regL, flagCarry[1], flagZero[1], flagParity[1], flagSign[1]);
    }
  }
  return halted;
}

int dp2200_cpu::registerFromImplict(int implict) {
  switch (implicit) {
    case 0:
      return 0;
    case 0022:         
      return 7;
    case 0062:        
      return 2;        
    case 0111:
      return 1;   
    case 0113: 
      return 3;         
    case 0115:
      return 5;  
    case 0117:
      return 0;               
    case 0174:
      return 4;     
    case 0176:
      return 6;    
    default:
      return 0;                   
  }  
}

int dp2200_cpu::getHighRegFromImplicit() {
  switch (implicit) {
    case 0:
      return 5;
    case 0022:         
      return 7;
    case 0062:        
      return 1;         
    case 0111:
      return 0;   
    case 0113: 
      return 0;        
    case 0115:
      return 0;  
    case 0117:
      return 0;               
    case 0174:
      return 3;     
    case 0176:
      return 0;   
    default:
      return 5;                   
  }  
}

int dp2200_cpu::getLowRegFromImplicit() {
  switch (implicit) {
    case 0:
      return 6;
    case 0022:         
      return 0;
    case 0062:        
      return 2;         
    case 0111:
      return 0;    
    case 0113: 
      return 0;        
    case 0115:
      return 0;  
    case 0117:
      return 0;               
    case 0174:
      return 4;     
    case 0176:
      return 0;   
    default:
      return 6;                   
  }  
}

struct dp2200_cpu::doubleLoadStoreRegisterTable dp2200_cpu::getSourceAndIndex() {
  switch (implicit) {
    case 0:
      return t[1];
    case 1:
      return t[9];
    case 0022:         
      return t[0];
    case 0062:        
      return t[3];        
    case 0111:
      return t[2];    
    case 0113: 
      return t[4];       
    case 0115:
      return t[6]; 
    case 0117:
      return t[8];             
    case 0174:
      return t[5];    
    case 0176:
      return t[7]; 
    default:
      return t[0];                 
  } 
}

int dp2200_cpu::doubleLoad () {
  struct doubleLoadStoreRegisterTable r = getSourceAndIndex();
  if (r.dstH == -1) return 1;
  unsigned short address = (regSets[setSel].regs[r.indexH] << 8) | (0xff & regSets[setSel].regs[r.indexL]);
  regSets[setSel].regs[r.dstL] = memory->read(address);
  address++; address &= pMask;
  regSets[setSel].regs[r.dstH] = memory->read(address);
  return 0;
}

int dp2200_cpu::doubleStore () {
  struct doubleLoadStoreRegisterTable r = getSourceAndIndex();
  if (r.dstH == -1) return 1;
  unsigned short address = (regSets[setSel].regs[r.indexH] << 8) | (0xff & regSets[setSel].regs[r.indexL]);
  printLog("INFO", "doubleStore address=%05o r.dstH = %d r.dstL = %dsrcH = %03o srcL=%03o\n", address,r.dstH, r.dstL, regSets[setSel].regs[r.dstH], regSets[setSel].regs[r.dstL] );
  memory->write(address, regSets[setSel].regs[r.dstL]);
  address++; address &= pMask;
  memory->write(address, regSets[setSel].regs[r.dstH]);
  return 0;
}

int dp2200_cpu::registerStore() {
  int address = 0xffff & stack.stk[(stackptr-1) & 0xf];
  for (int i=7; i >= 0; i-- ) {
    memory->write(address, regSets[setSel].regs[i]);
    address--; address &= pMask; 
  }  
  stack.stk[(stackptr-1) & 0xf] = 0xffff & address;
  return 0;
}

int dp2200_cpu::registerLoad() {
  int address = regSets[setSel].r.regH << 8 | (0xff & regSets[setSel].r.regL);
  for (int i=7; i >= 0; i--) {
    regSets[setSel].regs[i] = memory->read(address);
    address--; address &= pMask; 
  }
  return 0;
}

int dp2200_cpu::stackLoad() {
  unsigned int address = ((unsigned int)(regSets[setSel].r.regH) << 8) + (0xff & regSets[setSel].r.regL);
  int count = regSets[setSel].r.regC==0?16:regSets[setSel].r.regC;
  count = count>16?16:count;
  printLog("INFO", "stackLoad count=%d address=%05o stackptr=%d\n", count, address, stackptr);
  for (int i=0; i<count;i++) {
    stack.stk[stackptr] = (memory->read(address) & 0xff) << 8;
    //printLog("INFO", "Reading %03o from memort address %05o.\n",memory.read(address) & 0xff, address);
    address--;
    stack.stk[stackptr] |= memory->read(address) & 0xff;
    //printLog("INFO", "Reading %03o from memort address %05o.\n",memory.read(address) & 0xff, address);
    address-- ;
    printLog("INFO", "Storing %05o on stackLocation %d\n", stack.stk[stackptr], stackptr);
    stackptr = (stackptr + 1) & 0xf;
  }
  return 0;
}

int dp2200_cpu::stackStore() {
  unsigned int address = ((unsigned int)(regSets[setSel].r.regH) << 8) + (0xff & regSets[setSel].r.regL);
  int count = regSets[setSel].r.regC==0?16:regSets[setSel].r.regC;
  count = count>16?16:count;
  printLog("INFO", "stackStore count=%d address=%03o stackptr=%d\n", count, address, stackptr);
  for (int i=0; i<count;i++) {
    stackptr = (stackptr - 1) & 0xf;
    memory->write(address, stack.stk[stackptr] & 0xff);
    //printLog("INFO", "Storing %03o into address %05o from stackptr=%d\n", memory.read(address), address, stackptr);
    address++;
    memory->write(address, (stack.stk[stackptr] >> 8) & 0xff);
    //printLog("INFO", "Storing %03o into address %05o from stackptr=%d\n", memory[address], address, stackptr);
    address++;
  }
  return 0;
}

int dp2200_cpu::blockTransfer(bool reverse) {
  unsigned int sourceAddress = ((unsigned int)(regSets[setSel].r.regH) << 8) + regSets[setSel].r.regL;
  unsigned int destinationAddress = ((unsigned int)(regSets[setSel].r.regD << 8)) + regSets[setSel].r.regE;
  unsigned int count = (regSets[setSel].r.regC == 0)?256:regSets[setSel].r.regC;
  unsigned int i=0;
  //printLog("INFO", "sourceAddress=%05o destinationAddress=%05o count=%03o i=%d\n", sourceAddress, destinationAddress, count, i);
  while (i<count) {
    //printLog("INFO", "LOOP : sourceAddress=%05o destinationAddress=%05o count=%03o i=%d A=%03o\n", sourceAddress, destinationAddress, count, i, regSets[setSel].r.regA);
    memory->write(destinationAddress, memory->read(sourceAddress) + regSets[setSel].r.regA);
    //printLog("INFO", "B=%03o stored into memory=%03o condition=%03o \n", regSets[setSel].r.regB, memory[destinationAddress], (memory[destinationAddress] + regSets[setSel].r.regB) & 0xff);
    if (regSets[setSel].r.regB !=0 && (((memory->read(destinationAddress) + regSets[setSel].r.regB) & 0xff) == 0)) {
      //printLog("INFO", "Leave loop.\n");
      break;
    }
    if (reverse) {
      sourceAddress = (sourceAddress-1) & 0xffff;
      destinationAddress = (destinationAddress-1) & 0xffff;
    } else {
      sourceAddress = (sourceAddress+1) & 0xffff;
      destinationAddress = (destinationAddress+1) & 0xffff;
    }
    i++;
  }
  //printLog("INFO", "sourceAddress=%05o destinationAddress=%05o count=%03o i=%d\n", sourceAddress, destinationAddress, count, i);
  if (i==256) {
    regSets[setSel].r.regC = 0; 
  } else {
    regSets[setSel].r.regC = (unsigned char)(i & 0xff);
  }
  regSets[setSel].r.regH = (sourceAddress >> 8) & 0xff;
  regSets[setSel].r.regL = sourceAddress & 0xff;
  regSets[setSel].r.regD = (destinationAddress >> 8) & 0xff;
  regSets[setSel].r.regE = destinationAddress & 0xff;
  //printLog("INFO", "C=%03o D=%03o E=%03o H=%03o L=%03o\n", regSets[setSel].r.regC, regSets[setSel].r.regD, regSets[setSel].r.regE, regSets[setSel].r.regH , regSets[setSel].r.regL);
  return 0;
}

void dp2200_cpu::incrementRegisterPair (int highReg, int lowReg, int decrement) {
  int value = regSets[setSel].regs[REG_H] << 8 |  (0xff & regSets[setSel].regs[REG_L]);
  value += decrement;
  flagCarry[setSel] = 0x1 & (value >> 16);
  regSets[setSel].regs[REG_H] = 0xff & (value >> 8);
  regSets[setSel].regs[REG_L] = 0xff & value;
}

int dp2200_cpu::immediateplus(unsigned char inst) {
    int r;
    unsigned int op;
    unsigned char savedCarry;
    unsigned int reg;
    int carryzero;
    unsigned int sdata1, sdata2, result;
    unsigned int cc; /* condition code to check, if conditional operation */
    unsigned short address;
    int rl, rh;
    /* first check for halt */
    if ((inst & 0xfe) == 0x0) {
      return 1;
    }

    op = inst & 0x7;
    switch (op) {
    case 0:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* HALT */
        return 1;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* BETA */
        setSel = 1;
        break;
      case 3:
        /* ALPHA */
        setSel = 0;
        break;
      case 4:
        /* DI*/
        interruptEnabled = 0;
        break;
      case 5:
        /* EI */
        interruptEnabled = 1;
        break;
      case 6:
        /* POP */
        rh = getHighRegFromImplicit();
        rl = getLowRegFromImplicit();        
        stackptr = (stackptr - 1) & 0xf;
        regSets[setSel].regs[rh] = (stack.stk[stackptr] >> 8) & 0xff;
        regSets[setSel].regs[rl] = stack.stk[stackptr] & 0xff;
        break;
      case 7:
        /* PUSH */
        rh = getHighRegFromImplicit();
        rl = getLowRegFromImplicit();
        stack.stk[stackptr] = ((unsigned int)(regSets[setSel].regs[rh] & hMask) << 8) + regSets[setSel].regs[rl];
        stackptr = (stackptr + 1) & 0xf;
        break;
      }
      break;
    case 1:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* HALT */
        return 1;
      case 2:
        blockTransfer(false);
        break;
      case 5: // PUSH IMMEDIATE
        if (is2200) return 1;
        sdata1 = 0xff & memory->read(P);
        P++; P &= pMask; fetches++;
        sdata1 |= 0xff00 & (memory->read(P)<<8);
        P++; P &= pMask; fetches++;
        stack.stk[stackptr] = sdata1;
        stackptr = (stackptr + 1) & 0xf;
        break;
      default:
        /* Unimplemented */
        return 1;
      }
      break;
    case 2: /* rotate */
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0: /* rotate left - SLC */
        result = (int)regSets[setSel].r.regA << 1;
        regSets[setSel].r.regA =
            (unsigned char)(((result >> 8) | result) & 0xff);
        flagCarry[setSel] = regSets[setSel].r.regA & 1;
        break;
      case 1: /* rotate right - SRC */
        flagCarry[setSel] = regSets[setSel].r.regA & 1;
        result = (int)(regSets[setSel].r.regA >> 1);
        regSets[setSel].r.regA =
            (unsigned char)(((regSets[setSel].r.regA << 7) | result) & 0xff);
        break;
      case 3:
          r=registerFromImplict(implicit);
          savedCarry = flagCarry[setSel];
          flagCarry[setSel] = 0x1 & regSets[setSel].regs[r];
          regSets[setSel].regs[r] = (regSets[setSel].regs[r] >> 1) | ((savedCarry << 7) & 0x80);
        break;
      default: /*  unimplemented - handle as HALT for now. */
        return 1;
        break;
      }
      break;
    case 3: /* conditional return */
      cc = chkconditional(inst);
      if (cc) {
        stackptr = (stackptr - 1) & 0xf;
        P = stack.stk[stackptr] & pMask;
      }
      break;
    case 4: /* math with immediate operands */
      //printLog("INFO", "implicit=%d\n", implicit);
      r=registerFromImplict(implicit);
      sdata2 = (unsigned char)regSets[setSel].regs[r]; /* reg r is source and target */
      sdata1 = (unsigned char)memory->read(P); /* source is immediate */
      P++;
      P &= pMask;
      fetches++;
      
      sdata1 = sdata1 & 0xff;
      sdata2 = sdata2 & 0xff;
      //printLog("INFO", "r=%d\n", r);
      op = (inst >> 3) & 0x7;
      switch (op) {
      case 0: /* add without carry, but set carry */
        result = sdata1 + sdata2;
        //              printf ("mADD RA: %2.2x  RS: %2.2x RESULT:
        //              %2.2x",sdata2,sdata1, result);
        regSets[setSel].regs[r] = (unsigned char)result;
        carryzero = 0;
        break;
      case 1: /* add with carry, and set carry */
        result = sdata1 + sdata2 + flagCarry[setSel];
        //              printf ("mADC RA: %2.2x  RS: %2.2x CARRYIN: %d RESULT:
        //              %2.2x",sdata2,sdata1,flagCarry, result);
        regSets[setSel].regs[r] = (unsigned char)result;
        carryzero = 0;
        break;
      case 2: /* subtract with no borrow, but set borrow */
        result = sdata2 - sdata1;
        //              printf ("mSUB RA: %2.2x  RS: %2.2x RESULT:
        //              %2.2x",sdata2,sdata1, result);
        regSets[setSel].regs[r] = (unsigned char)result;
        carryzero = 0;
        break;
      case 3: /* subtract with borrow, and set borrow */
        result = sdata2 - sdata1 - flagCarry[setSel];
        //              printf ("mSBC RA: %2.2x  RS: %2.2x CARRYIN: %d RESULT:
        //              %2.2x",sdata2,sdata1,flagCarry, result);
        regSets[setSel].regs[r] = (unsigned char)result;
        carryzero = 0;
        break;
      case 4: /* logical and */
        result = sdata1 & sdata2;
        regSets[setSel].regs[r] = (unsigned char)result;
        carryzero = 1;
        break;
      case 5: /* exclusive or */
        result = sdata1 ^ sdata2;
        regSets[setSel].regs[r] = (unsigned char)result;
        carryzero = 1;
        break;
      case 6: /* or */
        result = sdata1 | sdata2;
        regSets[setSel].regs[r] = (unsigned char)result;
        carryzero = 1;
        break;
      case 7: /* compare (assume no borrow for now) */
        result = sdata2 - sdata1;
        //          printf ("mCMP RA: %2.2x  RS: %2.2x CARRYIN: %d RESULT:
        //          %2.2x",sdata2,sdata1,flagCarry[setSel][setSel][setSel][setSel],
        //          result);

        carryzero = 0;
        break;
      }
      setflags(result, carryzero);
      break;
    case 5:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        if (is2200) return 1; 
        if (implicit==0) {
          int disp = 0xff & memory->read(P);
          P++; P &= pMask; fetches++;
          int indexLsb = 0xff & memory->read(P);
          P++; P &= pMask; fetches++;          
          unsigned short address = (regSets[setSel].r.regX << 8) | indexLsb;
          int value = memory->read(address);
          value += disp;
          memory->write(address, 0xff & value);
          flagCarry[setSel] = (value >> 16) & 0x1;
        } else if (implicit == 0111) {
          int disp = 0xff & memory->read(P);
          P++; P &= pMask; fetches++;
          disp |= (0xff & memory->read(P)) << 8;
          P++; P &= pMask; fetches++;
          int indexLsb = 0xff & memory->read(P);
          P++; P &= pMask; fetches++;          
          unsigned short address = (regSets[setSel].r.regX << 8) | indexLsb;
          int value = memory->read(address);
          value += disp;
          memory->write(address, 0xff & value);
          flagCarry[setSel] = (value >> 16) & 0x1;
        } else {
          return 1;
        }
      case 1: // INCP HL
        if (is2200) return 1; // halt if 2200.
        switch (implicit) {
          case 0:
            incrementRegisterPair(REG_H, REG_L, 1);
            break;
          case 0022:
            incrementRegisterPair(REG_X, REG_A, 1);        
            break; 
          case 0062:
            incrementRegisterPair(REG_B, REG_C, 1);        
            break;          
          case 0111:
            incrementRegisterPair(REG_X, REG_A, 2);        
            break;          
          case 0113:
            incrementRegisterPair(REG_B, REG_C, 2);        
            break;         
          case 0115:
            incrementRegisterPair(REG_D, REG_E, 2);         
            break;
          case 0117:
            incrementRegisterPair(REG_H, REG_L, 2);
            break;
          case 0174:
            incrementRegisterPair(REG_D, REG_E, 1);         
            break;
          case 0176:
            return 1;       
        }

        break;
      case 2:
        if (is2200) return 1; 
        //printLog("INFO", "DECI instruction implict=%03\n", implicit);
        if (implicit==0) {
          int disp = 0xff & memory->read(P);
          //printLog("INFO", "DECI instruction disp=%03o indexLsb=%03o address=%05o value=%05o\n ");
          P++; P &= pMask; fetches++;
          int indexLsb = 0xff & memory->read(P);
          P++; P &= pMask; fetches++;
          unsigned short address = (regSets[setSel].r.regX << 8) | indexLsb;
          int value = memory->read(address);
          //printLog("INFO", "Before DECI instruction disp=%03o indexLsb=%03o address=%05o value=%05o carry=%1d\n ", disp, indexLsb, address, value, flagCarry[setSel]);
          value -= disp;
          memory->write(address, 0xff & value);
          flagCarry[setSel] = (value >> 16) & 0x1;
          //printLog("INFO", "After DECI instruction value=%05o carry=%1d \n", value,flagCarry[setSel] );
        } else if (implicit == 0111) {
          int disp = 0xff & memory->read(P);
          P++; P &= pMask; fetches++;
          disp |= (0xff & memory->read(P)) << 8;
          P++; P &= pMask; fetches++;
          int indexLsb = 0xff & memory->read(P);
          P++; P &= pMask; fetches++;          
          unsigned short address = (regSets[setSel].r.regX << 8) | indexLsb;
          int value = memory->read(address);
          value -= disp;
          memory->write(address, 0xff & value);
          flagCarry[setSel] = (value >> 16) & 0x1;
        } else {
          return 1;
        }
        break;
      case 3:
       if (is2200) return 1; // halt if 2200.
        switch (implicit) {
          case 0:
            incrementRegisterPair(REG_H, REG_L, -1);
            break;
          case 0022:
            incrementRegisterPair(REG_X, REG_A, -1);        
            break; 
          case 0062:
            incrementRegisterPair(REG_B, REG_C, -1);        
            break;          
          case 0111:
            incrementRegisterPair(REG_X, REG_A, -2);        
            break;          
          case 0113:
            incrementRegisterPair(REG_B, REG_C, -2);        
            break;         
          case 0115:
            incrementRegisterPair(REG_D, REG_E, -2);         
            break;
          case 0117:
            incrementRegisterPair(REG_H, REG_L, -2);
            break;
          case 0174:
            incrementRegisterPair(REG_D, REG_E, -1);         
            break;
          case 0176:
            return 1;       
        }

        break;
      case 4: //NOP JUMP
        P++; P &= pMask; fetches++;
        P++; P &= pMask; fetches++;
        break;
      case 5:
        if (is2200) return 1;
        switch (implicit) {
          case 0:
            registerStore();
            break;
          case 0022:
            return 1; 
          case 0062:
            return 1;          
          case 0111:
            registerLoad();
            break;          
          case 0113:
            return 1;               
          case 0115:
            return 1;       
          case 0117:
            return 1;
          case 0174:
            return 1; 
          case 0176:
            return 1;       
        }      
        break;
      case 6: 
        if (is2200) return 1;
        if (implicit==0) {
          return stackStore();
        } else if (implicit==0111) {
              return stackLoad();
        } else {
          return 1;
        }
        break;
      default:
        return 1;
      }
      break;
    case 6:               /* load immediate */
      sdata1 = memory->read(P); /* source is immediate */
      P++;
      P &= pMask;
      fetches++;

      reg = (inst >> 3) & 0x7;
      if (reg == 7 && is2200) { // reg 7 is not used in DP2200
        return 1;
      } 
      regSets[setSel].regs[reg] = sdata1;
      //printLog("INFO", "regSets[%d].regs[%d] = %03o data=%03o \n", setSel, reg, regSets[setSel].regs[reg], sdata1);
      break;
    case 7:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* RETURN */
        stackptr = (stackptr - 1) & 0xf;
        P = stack.stk[stackptr] & pMask;
        return 0;
      case 1:
        if (is2200) return 1;
        switch (implicit) {
          case 0:
            incrementRegisterPair(REG_H, REG_L, regSets[setSel].r.regA); //INCP HL,2
            break;
          case 0022:
            incrementRegisterPair(REG_X, REG_A, regSets[setSel].r.regA);        
            break; 
          case 0062:
            incrementRegisterPair(REG_B, REG_C, regSets[setSel].r.regA);        
            break;          
          case 0111:
            return 1;          
          case 0113:
            return 1;               
          case 0115:
            return 1;       
          case 0117:
            return 1;
          case 0174:
            incrementRegisterPair(REG_D, REG_E, regSets[setSel].r.regA);         
            break;
          case 0176:
            return 1;       
        }      
        
        break;
      case 2:
          printLog("INFO", "Double Store  implicit=%03o inst =%03o\n", implicit, inst);
          switch (implicit) {
            case 0:
              // DS DE,HL
              address = ((regSets[setSel].r.regH << 8) | regSets[setSel].r.regL ) & pMask;
              memory->write(address, regSets[setSel].r.regE);
              address++;
              memory->write(address, regSets[setSel].r.regD);              
              break;
            case 0022:         
              return 1; 
            case 0062:        
              return 1;         
            case 0111:
              // DS BC,HL
              address = ((regSets[setSel].r.regH << 8) | regSets[setSel].r.regL ) & pMask;
              //printLog("INFO", "address=%05o\n", address);
              memory->write(address, regSets[setSel].r.regC);
              //printLog("INFO", "memory[%05o]=%03o C=%03o\n", address, memory[address]);
              address++;
              memory->write(address, regSets[setSel].r.regB);
              //printLog("INFO", "memory[%05o]=%03o B=%03o\n", address, memory[address]);
              break;    
            case 0113: 
              address = ((regSets[setSel].r.regD << 8) | regSets[setSel].r.regE ) & pMask;
              memory->write(address, regSets[setSel].r.regC);
              address++;
              memory->write(address, regSets[setSel].r.regB);  
              break;         
            case 0115: 
              return 1; 
            case 0117:
              address = ((regSets[setSel].r.regD << 8) | regSets[setSel].r.regE ) & pMask;
              memory->write(address, regSets[setSel].r.regL);
              address++;
              memory->write(address, regSets[setSel].r.regH); 
              break;              
            case 0174:
              address = ((regSets[setSel].r.regB << 8) | regSets[setSel].r.regC ) & pMask;
              memory->write(address, regSets[setSel].r.regE);
              address++;
              memory->write(address, regSets[setSel].r.regD);                       
              break;
            case 0176:
              address = ((regSets[setSel].r.regB << 8) | regSets[setSel].r.regC ) & pMask;
              memory->write(address, regSets[setSel].r.regL);
              address++;
              memory->write(address, regSets[setSel].r.regH); 
              break;                     
          }
        break;
      case 3:
        if (is2200) return 1; // halt if 2200.
        switch (implicit) {
          case 0:
            incrementRegisterPair(REG_H, REG_L, -regSets[setSel].r.regA); //DECP HL,2
            break;
          case 0022:
            incrementRegisterPair(REG_X, REG_A, -regSets[setSel].r.regA);        
            break; 
          case 0062:
            incrementRegisterPair(REG_B, REG_C, -regSets[setSel].r.regA);        
            break;          
          case 0111:
            return 1;          
          case 0113:
            return 1;               
          case 0115:
            return 1;       
          case 0117:
            return 1;
          case 0174:
            incrementRegisterPair(REG_D, REG_E, -regSets[setSel].r.regA);         
            break;
          case 0176:
            return 1;       
        }   
        
        break;
      case 4:
        if (is2200) return 1; 
        return doubleLoad();
      case 5:
        if (is2200) return 1;
        implicit = 1;
        return doubleLoad();
      default:
        /* Unimplemented */
        return 1;
      }

      break;
    }
    return 0;
  }

  inline unsigned short dp2200_cpu::getPagedAddress () {
    unsigned char loc;
    // Paged Load PL A, (loc)
    loc = memory->read(P);
    P++;
    P &= pMask;
    fetches++;
    return ((regSets[setSel].r.regX << 8) | loc ) & pMask;
  }

  int dp2200_cpu::iojmpcall(unsigned char inst) {
    unsigned int op;
    unsigned int cc; /* condition code to check, if conditional operation */
    unsigned int addrL, addrH;
    int r=registerFromImplict(implicit);
    unsigned short address;
    op = inst & 0x7;
    switch (op) {
    case 0: /* jump conditionally */
      cc = chkconditional(inst);
      addrL = (unsigned int)memory->read(P);
      P++;
      P &= pMask;
      fetches++;

      addrH = (unsigned int)memory->read(P);
      P++;
      P &= pMask;
      fetches++;

      if (cc) {
        timeForInstruction =instTimeInNsTaken[inst];
        P = (addrL + (addrH << 8)) & pMask;
      }
      break;
    case 1:
      op = (inst & 0x38) >> 3;
      
      switch (op) {
      case 0:
        /* INPUT */
        regSets[setSel].regs[r]=ioCtrl->input();
        break;
      case 1:
        /* Unimplemented */
        return 1;
      case 2: 
        /* EX ADR */
        return ioCtrl->exAdr(regSets[setSel].regs[r]);;
        break;
      case 3:
        /* EX COM1 */
        return ioCtrl->exCom1(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX BEEP */
        return ioCtrl->exBeep();
        break;
      case 6:
        /* EX RBK */
        return ioCtrl->exRBK();
        break;
      case 7:
        /* EX SF */
        return ioCtrl->exSF();
        break;
      }
      break;
    case 2: /* call conditionally */
      cc = chkconditional(inst);
      addrL = (unsigned int)memory->read(P);
      P++;
      P &= pMask;
      fetches++;

      addrH = (unsigned int)memory->read(P);
      P++;
      P &= pMask;
      fetches++;

      if (cc) {
        timeForInstruction =instTimeInNsTaken[inst];
        stack.stk[stackptr] = P;
        stackptr = (stackptr + 1) & 0xf;
        P = (addrL + (addrH << 8)) & pMask;
      }
      break;
    case 3:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* PARITY INPUT - We don't care about input really.*/
        regSets[setSel].regs[r]=ioCtrl->input();
        return 0;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX STATUS */
        ioCtrl->exStatus(); 
        break;
      case 3:
        /* EX COM2 */
        printLog("INFO", "implicit=%03o r=%d\n", implicit, r);
        return ioCtrl->exCom2(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX CLICK */
        return ioCtrl->exClick();
        break;
      case 6:
        /* EX WBK */
        return ioCtrl->exWBK();
        break;
      case 7:
        /* EX SB */
        return ioCtrl->exSB();
        break;
      }
      break;
    case 4: /* jump */
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* JMP */
        addrL = (unsigned int)memory->read(P);
        P++;
        P &= pMask;
        fetches++;
        addrH = (unsigned int)memory->read(P);
        P = (addrL + (addrH << 8)) & pMask;
        fetches++;
        break;
      case 1:
        if (is2200) return 1;
        // Paged Load PL B, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regB = memory->read(address);
        break;
      case 2:
        if (is2200) return 1;
        // Paged Load PL V, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regC = memory->read(address);
        break;
      case 3:
        if (is2200) return 1;
        // Paged Load PL D, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regD = memory->read(address);
        break;
      case 4:
        if (is2200) return 1;
        // Paged Load PL E, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regE = memory->read(address);
        break;
      case 5:
        if (is2200) return 1;
        // Paged Load PL H, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regH = memory->read(address);
        break;
      case 6:
        if (is2200) return 1;
        // Paged Load PL L, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regL = memory->read(address);
        break;          
      default:
        /* Unimplemented */
        return 1;
      }
      break;
    case 5:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        if (is2200) return 1;
        // Paged Load PL L, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regA = memory->read(address);
        break;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX DATA */
        ioCtrl->exData();
        break;
      case 3:
        /* EX COM3 */
        return ioCtrl->exCom3(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
        break;
      case 5:
        /* EX DECK1 */
        return ioCtrl->exDeck1();
        break;
      case 6:
        /* Unimplemented */
        return 1;
        break;
      case 7:
        /* EX REWND */
        return ioCtrl->exRewind();
        break;
      }

      break;
    case 6: /* call */
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        addrL = (unsigned int)memory->read(P);
        P++;
        P &= pMask;
        fetches++;

        addrH = (unsigned int)memory->read(P);
        P++;
        P &= pMask;
        fetches++;
        stack.stk[stackptr] = P;
        stackptr = (stackptr + 1) & 0xf;
        P = (addrL + (addrH << 8)) & pMask;
        break;
      case 1:
        if (is2200) return 1;
        // Paged Store PS B, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regB);
        break;            
      case 2: 
          if (is2200) return 1;
          address = getPagedAddress();
          switch (implicit) {
            case 0:
              // Paged Store PS C, (loc)
              memory->write(address, regSets[setSel].r.regC);
              break;
            case 0022:         
              return 1; 
            case 0062:        
              return 1;         
            case 0111:
              memory->write(address, regSets[setSel].r.regC);
              //printLog("INFO", "memory[address]=%03o C=%03o\n", memory[address], regSets[setSel].r.regC);
              address++;
              memory->write(address, regSets[setSel].r.regB); 
              //printLog("INFO", "address=%05o memory[address]=%03o B=%03o\n", address, memory[address], regSets[setSel].r.regB);    
              break;    
            case 0113:
              memory->write(address++, regSets[setSel].r.regE);  
              memory->write(address, regSets[setSel].r.regD);     
              break;         
            case 0115:
              memory->write(address++, regSets[setSel].r.regL);  
              memory->write(address, regSets[setSel].r.regH);     
              break; 
            case 0117:
              return 1;
            case 0174:          
              return 1;
            case 0176:
              return 1;       
          }
        break;
      case 3:
        if (is2200) return 1;
        // Paged Store PS D, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regD);
        break;        
      case 4:
        if (is2200) return 1;
        // Paged Store PS E, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regE);
        break;        
      case 5:
        if (is2200) return 1;
        // Paged Store PS H, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regH);
        break;        
      case 6:
        if (is2200) return 1;
        // Paged Store PS L, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regL);
        break;        
      default:
        /* Unimplemented */
        return 1;
      }
      break;
    case 7:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        if (is2200) return 1;
        // Paged Store PS A, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regA);
        break;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX WRITE */
        return ioCtrl->exWrite(regSets[setSel].regs[r]);
        break;
      case 3:
        /* EX COM4 */
        return ioCtrl->exCom4(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX DECK2 */
        return ioCtrl->exDeck2();
        break;
      case 6:
        /* EX BSP */
        return ioCtrl->exBSP();
        break;
      case 7:
        /* EX TSTOP */
        return ioCtrl->exTStop();
        break;
      }

      break;
    }
    return 0;
  }

  int dp2200_cpu::mathboolean(unsigned char inst) {
    unsigned int src, op;
    int carryzero;
    unsigned int sdata1, sdata2, result;
    int r=registerFromImplict(implicit);
    src = inst & 0x7;
    if (src == 0x7) {
      sdata1 = memory->read(((regSets[setSel].r.regH & hMask) << 8) +
                      regSets[setSel].r.regL);
    } else {
      sdata1 = regSets[setSel].regs[src];
    }
    sdata1 = sdata1 & 0xff;
    sdata2 = (char)regSets[setSel].regs[r];
    sdata2 = sdata2 & 0xff;

    op = (inst >> 3) & 0x7;
    switch (op) {
    case 0: /* add without carry, but set carry */
      result = sdata1 + sdata2;
      regSets[setSel].regs[r] = (unsigned char)result;
      carryzero = 0;
      //          printf (" ADD RA: %2.2x  RS: %2.2x RESULT:
      //          %2.2x",sdata2,sdata1,result);
      break;

    case 1: /* add with carry, and set carry */
      result = sdata1 + sdata2 + flagCarry[setSel];
      regSets[setSel].regs[r] = (unsigned char)result;
      carryzero = 0;
      //          printf (" ADC RA: %2.2x  RS: %2.2x CARRYIN: %d RESULT:
      //          %2.2x",sdata2,sdata1,flagCarry[setSel], result);
      break;

    case 2: /* subtract with no borrow, but set borrow */
      result = sdata2 - sdata1;
      regSets[setSel].regs[r] = (unsigned char)result;
      carryzero = 0;

      //      printf (" SUB RA: %2.2x  RS: %2.2x RESULT:
      //      %2.2x",sdata2,sdata1,result);
      break;
    case 3: /* subtract with borrow, and set borrow */
      result = sdata2 - sdata1 - flagCarry[setSel];
      regSets[setSel].regs[r] = (unsigned char)result;
      carryzero = 0;
      //          printf (" SBC RA: %2.2x  RS: %2.2x BORROWIN: %d RESULT:
      //          %2.2x",sdata2, sdata1, flagCarry[setSel], result);
      break;
    case 4: /* logical and */
      result = sdata1 & sdata2;
      regSets[setSel].regs[r] = (unsigned char)result;
      carryzero = 1;
      break;
    case 5: /* exclusive or */
      result = sdata1 ^ sdata2;
      regSets[setSel].regs[r] = (unsigned char)result;
      carryzero = 1;
      break;
    case 6: /* or */
      result = sdata1 | sdata2;
      regSets[setSel].regs[r] = (unsigned char)result;
      carryzero = 1;
      break;
    case 7: /* compare (assume no borrow for now) */
            //      printf (" CMP RA: %2.2x  RS: %2.2x RESULT:
            //      %2.2x",sdata2,sdata1,result);

      result = sdata2 - sdata1;
      carryzero = 0;
      break;
    }
    setflags(result, carryzero);
    return 0;
  }

  void dp2200_cpu::setflags(int result, int carryzero) {

    if (result & 0xff) {
      flagZero[setSel] = 0;
      if (result & 0x80) {
        flagSign[setSel] = 1;
      } else {
        flagSign[setSel] = 0;
      }
    } else {
      flagSign[setSel] = 0;
      flagZero[setSel] = 1;
    }
    if (carryzero) {
      flagCarry[setSel] = 0;
    } else if (result > 255 || result < 0) {
      //      printf(" Carryout\n");
      flagCarry[setSel] = 1;
    } else {
      //      printf(" NO Carryout\n");
      flagCarry[setSel] = 0;
    }
    setparity(result);
    return;
  }

  void dp2200_cpu::setflagsinc(int result) {
    if (result & 0xff) {
      flagZero[setSel] = 0;
      if (result & 0x80) {
        flagSign[setSel] = 1;
      } else {
        flagSign[setSel] = 0;
      }
    } else {
      flagSign[setSel] = 0;
      flagZero[setSel] = 1;
    }
    setparity(result);
    return;
  }

  void dp2200_cpu::setparity(int result) {
    int i, p;

    p = 0;
    flagParity[setSel] = 1;
    for (i = 0; i < 8; i++) {
      flagParity[setSel] = flagParity[setSel] ^ ((result >> i) & 1);
      if (result & (1 << i)) {
        p++;
      }
    }
    if (p & 1) {
      flagParity[setSel] = 0;
    }
  }

  int dp2200_cpu::load(unsigned char inst) {
    unsigned int src, dest;
    unsigned char data;
    
    src = inst & 0x7;
    dest = (inst >> 3) & 0x7;

    if (src == 0x7 && dest == 0x7) {
      return 1;
    }
    if (src == 0x7) {
      unsigned short address = ((regSets[setSel].regs[getHighRegFromImplicit()] & hMask) << 8) + regSets[setSel].regs[getLowRegFromImplicit()];
      data = memory->read(address);
    } else {
      data = regSets[setSel].regs[src];
    }
    if (dest == 0x7) {
      unsigned short address = ((regSets[setSel].regs[getHighRegFromImplicit()] & hMask) << 8) + regSets[setSel].regs[getLowRegFromImplicit()];
      memory->write(address, data);
    } else {
      regSets[setSel].regs[dest] = data;
    }
    return 0;
  }

  int dp2200_cpu::chkconditional(unsigned char inst) {
    unsigned int cc;
    cc = (inst & 0x38) >> 3;

    switch (cc) {
    case 0:
      cc = !flagCarry[setSel];
      break; /* overflow false */
    case 1:
      cc = !flagZero[setSel];
      break; /* zero false */
    case 2:
      cc = !flagSign[setSel];
      break; /* sign false */
    case 3:
      cc = !flagParity[setSel];
      break; /* parity false */
    case 4:
      cc = flagCarry[setSel];
      break; /* overflow true */
    case 5:
      cc = flagZero[setSel];
      break; /* zero true */
    case 6:
      cc = flagSign[setSel];
      break; /* sign true */
    case 7:
      cc = flagParity[setSel];
      break; /* parity true */
    }
    return (cc);
  }  

dp2200_cpu::dp2200_cpu() {
  ioCtrl = new IOController ();
  memory = new Memory(&is5500, &accessViolation, &writeViolation, &userMode);
}
