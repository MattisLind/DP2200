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

unsigned char inline dp2200_cpu::Memory::read(unsigned short virtualAddress, bool performChecks, bool fetch, int from) {
  int physicalAddress, logicalPage, physicalPage;
  unsigned char data;
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
  data = physicalMemoryRead(physicalAddress);
  if (!fetch && performChecks) {
    // printLog("TRACE", "%06o %03o        DATA READ FROM %06o     \n", virtualAddress, data, from); 
  }
  return data;
}


void inline dp2200_cpu::Memory::write(unsigned short virtualAddress, unsigned char data, int from) {
  int physicalAddress, logicalPage, physicalPage;
  //printLog("TRACE", "%06o %03o        DATA WRITE FROM %06o     \n", virtualAddress, data, from); 
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
    }
    if (!sectorTable[logicalPage].accessEnable && *userMode) {
      *accessViolation = true;
    } else {
      *accessViolation = false;
    }
    if (sectorTable[logicalPage].writeEnable) {
      *writeViolation = false;
    } else {
      *writeViolation = true;
    } 
    if ((*accessViolation) || (*writeViolation)) {
      return;
    }     
  } else {
    physicalAddress = virtualAddress;
  } 
  if (memoryWatch[physicalAddress]) {
    printLog("INFO", "Writing to address %06o - halting\n", physicalAddress);
    running=false;
    return;
  }
  if (*is5500 & ((physicalAddress & 0xf000) == 0xf000)) return; // This is ROM. We cannot change the ROM...  
  physicalMemoryWrite(physicalAddress, data);
}

dp2200_cpu::Memory::Memory(bool * is, bool * av, bool * wv, bool * um) {
  // clear the entire memoryWatch array
  for (int address=0;address <=65535; address++) {
    memoryWatch[address]=false;
  }
  baseRegister = 0;
  for (int i=0; i<15; i++) { 
    sectorTable[i].physicalPage=0; 
    sectorTable[i].accessEnable=true;
    sectorTable[i].writeEnable=true;
  }
  sectorTable[017].writeEnable=false;
  sectorTable[017].accessEnable=true;  
  sectorTable[017].physicalPage=017;
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



char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, int address, std::function<unsigned char(int)> readMem, int implicitNum) {
    unsigned char instruction = readMem(address);
    int set;
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
    switch (instructionSet[set*256+instruction].type) {
    case 0:
      snprintf(outputBuf, size, "%s", instructionSet[set*256+instruction].mnemonic);
    break;
    case 1:
      if (octal) {
        snprintf(outputBuf, size, "%s %03o", instructionSet[set*256+instruction].mnemonic, readMem(address+1));
      } else {
        snprintf(outputBuf, size, "%s %02X", instructionSet[set*256+instruction].mnemonic, readMem(address+1));
      }
    break;
    case 2:
      if (octal) {
        snprintf(outputBuf, size, "%s %06o", instructionSet[set*256+instruction].mnemonic, (readMem(address+2) << 8) | readMem(address+1));
      } else {
        snprintf(outputBuf, size, "%s %04X", instructionSet[set*256+instruction].mnemonic, (readMem(address+2) << 8) | readMem(address+1));
      }
    break;
    case 4:
      if (octal) {
        snprintf(outputBuf, size, "%s %03o,%03o ", instructionSet[set*256+instruction].mnemonic, readMem(address+1), readMem(address+2));
      } else {
        snprintf(outputBuf, size, "%s %02X,%02X ", instructionSet[set*256+instruction].mnemonic, readMem(address+1), readMem(address+2));
      }  
    break;  
    case 6:
      if (octal) {
        snprintf(outputBuf, size, "%s %06o,%03o", instructionSet[set*256+instruction].mnemonic, (readMem(address+2) << 8) | readMem(address+1),readMem(address+3));
      } else {
        snprintf(outputBuf, size, "%s %04X,%02X", instructionSet[set*256+instruction].mnemonic, (readMem(address+2) << 8) | readMem(address+1),readMem(address+3));
      }    
    break;
  }
  return outputBuf;
}

char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, int address, std::function<unsigned char(int)> readMem) {
  int imp;
  unsigned char instruction = readMem(address);
  switch (instruction) {
    case 022:
    case 062:
    case 0111:
    case 0113:
    case 0115:
    case 0117:
    case 0174:
    case 0176:
      imp = instruction;
      address++;
      break;
    default:
      imp=0;
      break;
  }
  return disassembleLine(outputBuf, size, octal, address, readMem, imp);
}

char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, int address, int imp) {
  return disassembleLine(outputBuf, size, octal,  address, [memory=memory](int address)->unsigned char { return memory->read(address, false);}, imp);
}

char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, int address) {
  return disassembleLine(outputBuf, size, octal,  address, [memory=memory](int address)->unsigned char { return memory->read(address, false);});
}


char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, unsigned char * buffer, int imp) {
  return disassembleLine(outputBuf, size, octal,  0 , [buffer=buffer](int address)->unsigned char { return *(buffer+address);}, imp);
}

char *  dp2200_cpu::disassembleLine(char * outputBuf, int size, bool octal, unsigned char * buffer) {
  return disassembleLine(outputBuf, size, octal,  0 , [buffer=buffer](int address)->unsigned char { return *(buffer+address);});
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

bool dp2200_cpu::cpuIs2200 () {
  return is2200;
}

bool dp2200_cpu::cpuIs5500 () {
  return is5500;
}

bool dp2200_cpu::isAutorestartEnabled() {
  return autorestartEnabled;
}

void dp2200_cpu::setAutorestart(bool value) {
  autorestartEnabled = value;
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
    if (is2200) {
      P=0;
    } else if (is5500) { 
      P = 0170036;
    }
    stackptr = 0;
    setSel = 0;
    interruptPending = 0;
    interruptEnabled = 0;
    interruptEnabledToBeEnabled = 0;
    privilegeViolation = false;
    userMode = false;
}
// 5500
// 170000 (167400) Memory parity Failure Vector
// 170003 (167406) Input Parity Failure Vector
// 170006 (167414) Output Parity Failure Vector
// 170011 (167422) Write Protect Violation Vector
// 170014 (167430) Access Protect Violation Vector
// 170017 (167436) Priviledged Instruction Violation Vector
// 170022 (167444) One Milliseconf Clock Vector
// 170025 (167452) User System Call Vector
// 170030 (167460) Breakpoint Vector 
// 170033 Jumps to HALT!
// 170036 Startup routine most likely
int dp2200_cpu::execute() {
  unsigned char inst;
  char buffer[32]; 
  int timeForInstruction;
  int halted, j;
  struct inst i;
  unsigned char instructionData;
  /* Handle interrupt*/

  if ((interruptEnabled && interruptPending) || accessViolation || writeViolation || privilegeViolation) {
    
    /* now bump stack to use new pc and save it */
    stack.stk[stackptr] = P;
    stackptr = (stackptr + 1) & 0xf;
    if (accessViolation) {
      accessViolation=false;
      P=0170014;
    } else if (writeViolation) {
      writeViolation = false;
      P=0170011;
    } else if (privilegeViolation) {
      privilegeViolation = false;
      P=0170017;
    } else if (interruptPending && is5500) {
      interruptPending = 0;
      P=0170022;
    } else if (interruptPending && is2200) {
      interruptPending = 0;
      P=0;
    } else {
      printLog("INFO", "Unknown interrupt. interruptPending=%d interruptEnabled=%d accessViolation=%d writeViolation=%d privilegeViolation=%d", interruptPending,interruptEnabled, accessViolation,writeViolation,privilegeViolation);
      return 1;
    }
  }
  if (interruptEnabledToBeEnabled) {
    interruptEnabledToBeEnabled = 0;
    interruptEnabled = 1;
  }
  previousP = P;
  inst = memory->read(P, true, true, previousP);

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
        inst = memory->read(P, true, true, previousP);
        printLog("INFO", "Got implicit=%03o opcode=%03o\n", implicit, inst);
      } else {
        return 1;  // halt on 2200.
      }
      break;
    default:
      implicit = 0;
  }
  j=0;
  if (implicit !=0) {
    i.data[j++]=implicit;  
  }
  i.data[j++] = memory->read(P, true, true, previousP);
  i.data[j++] = memory->read(P+1, true, true, previousP);
  i.data[j++] = memory->read(P+2, true, true, previousP);
  i.data[j++] = memory->read(P+3, true, true, previousP);
  i.address = P;
  if (instructionTrace.size()>=16) {
    instructionTrace.erase(instructionTrace.begin());
  }

  instructionTrace.push_back(i);
  timeForInstruction = instTimeInNsNotTkn[inst];
  disassembleLine(buffer, 32, octal, P, implicit);
  instructionData = memory->read(P, true, true, previousP);
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
      if (implicit!=0) {
        i.address--;
        printLog("TRACE", "%06o %03o %03o %-21s    -> %s | A=%03o B=%03o C=%03o D=%03o E=%03o H=%03o L=%03o X=%03o C=%1d Z=%1d P=%1d S=%1d | A=%03o B=%03o C=%03o D=%03o E=%03o H=%03o L=%03o X=%03o C=%1d Z=%1d P=%1d S=%1d \n", i.address, implicit, instructionData, buffer, setSel==0?"ALPHA":"BETA", regSets[0].r.regA,regSets[0].r.regB,regSets[0].r.regC,regSets[0].r.regD,regSets[0].r.regE,regSets[0].r.regH,regSets[0].r.regL,regSets[0].r.regX, flagCarry[0], flagZero[0], flagParity[0], flagSign[0], regSets[1].r.regA,regSets[1].r.regB,regSets[1].r.regC,regSets[1].r.regD,regSets[1].r.regE,regSets[1].r.regH,regSets[1].r.regL, regSets[1].r.regX, flagCarry[1], flagZero[1], flagParity[1], flagSign[1]);
      } else {
        printLog("TRACE", "%06o %03o     %-21s    -> %s | A=%03o B=%03o C=%03o D=%03o E=%03o H=%03o L=%03o X=%03o C=%1d Z=%1d P=%1d S=%1d | A=%03o B=%03o C=%03o D=%03o E=%03o H=%03o L=%03o X=%03o C=%1d Z=%1d P=%1d S=%1d \n", i.address, instructionData, buffer, setSel==0?"ALPHA":"BETA", regSets[0].r.regA,regSets[0].r.regB,regSets[0].r.regC,regSets[0].r.regD,regSets[0].r.regE,regSets[0].r.regH,regSets[0].r.regL,regSets[0].r.regX, flagCarry[0], flagZero[0], flagParity[0], flagSign[0], regSets[1].r.regA,regSets[1].r.regB,regSets[1].r.regC,regSets[1].r.regD,regSets[1].r.regE,regSets[1].r.regH,regSets[1].r.regL, regSets[1].r.regX, flagCarry[1], flagZero[1], flagParity[1], flagSign[1]);
      }
    } else {
      if (implicit!=0) {
        i.address--;
        printLog("TRACE", "%04X %02X %02X %-21s    -> %s | A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X X=%02X C=%1d Z=%1d P=%1d S=%1d | A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X X=%02X C=%1d Z=%1d P=%1d S=%1d \n", i.address, implicit, instructionData, buffer, setSel==0?"ALPHA":"BETA", regSets[0].r.regA,regSets[0].r.regB,regSets[0].r.regC,regSets[0].r.regD,regSets[0].r.regE,regSets[0].r.regH,regSets[0].r.regL,regSets[0].r.regX, flagCarry[0], flagZero[0], flagParity[0], flagSign[0], regSets[1].r.regA,regSets[1].r.regB,regSets[1].r.regC,regSets[1].r.regD,regSets[1].r.regE,regSets[1].r.regH,regSets[1].r.regL, regSets[1].r.regX, flagCarry[1], flagZero[1], flagParity[1], flagSign[1]);
      } else {
        printLog("TRACE", "%04X %02X    %-21s    -> %s | A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X X=%02X C=%1d Z=%1d P=%1d S=%1d | A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X X=%02X C=%1d Z=%1d P=%1d S=%1d \n", i.address, instructionData, buffer, setSel==0?"ALPHA":"BETA", regSets[0].r.regA,regSets[0].r.regB,regSets[0].r.regC,regSets[0].r.regD,regSets[0].r.regE,regSets[0].r.regH,regSets[0].r.regL,regSets[0].r.regX, flagCarry[0], flagZero[0], flagParity[0], flagSign[0], regSets[1].r.regA,regSets[1].r.regB,regSets[1].r.regC,regSets[1].r.regD,regSets[1].r.regE,regSets[1].r.regH,regSets[1].r.regL, regSets[1].r.regX, flagCarry[1], flagZero[1], flagParity[1], flagSign[1]);
      }
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

struct dp2200_cpu::doubleLoadStoreRegisterTable dp2200_cpu::getSourceAndIndex(int implicit) {
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

int dp2200_cpu::doubleLoad (int implicit) {
  struct doubleLoadStoreRegisterTable r = getSourceAndIndex(implicit);
  if (r.dstH == -1) return 1;
  unsigned short address = (regSets[setSel].regs[r.indexH] << 8) | (0xff & regSets[setSel].regs[r.indexL]);
  regSets[setSel].regs[r.dstL] = memory->read(address, true, false, previousP);
  address++; address &= pMask;
  regSets[setSel].regs[r.dstH] = memory->read(address, true, false, previousP);
  return 0;
}

int dp2200_cpu::doubleStore (int implicit) {
  struct doubleLoadStoreRegisterTable r = getSourceAndIndex(implicit);
  if (r.dstH == -1) return 1;
  unsigned short address = (regSets[setSel].regs[r.indexH] << 8) | (0xff & regSets[setSel].regs[r.indexL]);
  printLog("INFO", "doubleStore address=%06o r.dstH = %d r.dstL = %dsrcH = %03o srcL=%03o\n", address,r.dstH, r.dstL, regSets[setSel].regs[r.dstH], regSets[setSel].regs[r.dstL] );
  memory->write(address, regSets[setSel].regs[r.dstL], previousP);
  address++; address &= pMask;
  memory->write(address, regSets[setSel].regs[r.dstH], previousP);
  return 0;
}

int dp2200_cpu::registerStore() {
  int address = 0xffff & stack.stk[(stackptr-1) & 0xf];
  for (int i=7; i >= 0; i-- ) {
    memory->write(address, regSets[setSel].regs[i], previousP);
    address--; address &= pMask; 
  }  
  stack.stk[(stackptr-1) & 0xf] = 0xffff & address;
  return 0;
}

int dp2200_cpu::registerLoad() {
  int address = regSets[setSel].r.regH << 8 | (0xff & regSets[setSel].r.regL);
  for (int i=7; i >= 0; i--) {
    regSets[setSel].regs[i] = memory->read(address, true, false, previousP);
    address--; address &= pMask; 
  }
  return 0;
}

int dp2200_cpu::stackLoad() {
  unsigned int address = ((unsigned int)(regSets[setSel].r.regH) << 8) + (0xff & regSets[setSel].r.regL);
  int count = regSets[setSel].r.regC==0?16:regSets[setSel].r.regC;
  count = count>16?16:count;
  printLog("INFO", "stackLoad count=%d address=%06o stackptr=%d\n", count, address, stackptr);
  for (int i=0; i<count;i++) {
    stack.stk[stackptr] = (memory->read(address, true, false, previousP) & 0xff) << 8;
    //printLog("INFO", "Reading %03o from memort address %05o.\n",memory.read(address) & 0xff, address);
    address--;
    stack.stk[stackptr] |= memory->read(address, true, false, previousP) & 0xff;
    //printLog("INFO", "Reading %03o from memort address %05o.\n",memory.read(address) & 0xff, address);
    address-- ;
    printLog("INFO", "Storing %06o on stackLocation %d\n", stack.stk[stackptr], stackptr);
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
    memory->write(address, stack.stk[stackptr] & 0xff, previousP);
    //printLog("INFO", "Storing %03o into address %05o from stackptr=%d\n", memory.read(address), address, stackptr);
    address++;
    memory->write(address, (stack.stk[stackptr] >> 8) & 0xff, previousP);
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
    memory->write(destinationAddress, memory->read(sourceAddress, true, false, previousP) + regSets[setSel].r.regA, previousP);
    //printLog("INFO", "B=%03o stored into memory=%03o condition=%03o \n", regSets[setSel].r.regB, memory[destinationAddress], (memory[destinationAddress] + regSets[setSel].r.regB) & 0xff);
    if (regSets[setSel].r.regB !=0 && (((memory->read(destinationAddress, true, false, previousP) + regSets[setSel].r.regB) & 0xff) == 0)) {
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
  int value = regSets[setSel].regs[highReg] << 8 |  (0xff & regSets[setSel].regs[lowReg]);
  value += decrement;
  flagCarry[setSel] = 0x1 & (value >> 16);
  regSets[setSel].regs[highReg] = 0xff & (value >> 8);
  regSets[setSel].regs[lowReg] = 0xff & value;
}


void dp2200_cpu::incrementIndexShort (int direction, int highReg, int lowReg) {
  int disp = 0xff & memory->read(P, true, true, previousP);
  //printLog("INFO", "DECI instruction disp=%03o indexLsb=%03o address=%05o value=%05o\n ");
  P++; P &= pMask; fetches++;
  int indexLsb = 0xff & memory->read(P, true, true, previousP);
  P++; P &= pMask; fetches++;
  unsigned short address = (regSets[setSel].r.regX << 8) | indexLsb;
  int value = memory->read(address, true, false, previousP);
  value |= (memory->read((address+1) & pMask, true, false, previousP) << 8);
  //printLog("INFO", "Before DECI instruction disp=%03o indexLsb=%03o address=%05o value=%05o carry=%1d\n ", disp, indexLsb, address, value, flagCarry[setSel]);
  value += direction * disp;
  memory->write(address, 0xff & value, previousP);
  memory->write((address+1) & pMask, 0xff & (value >> 8), previousP);  
  flagCarry[setSel] = (value >> 16) & 0x1;
  //printLog("INFO", "After DECI instruction value=%05o carry=%1d \n", value,flagCarry[setSel] );
  if ((highReg != -1) && (lowReg != -1)) {
    regSets[setSel].regs[lowReg] = value & 0xff;
    regSets[setSel].regs[highReg] = (value >> 8)  & 0xff;  
  }
}

void dp2200_cpu::incrementIndexLong (int direction, int highReg, int lowReg) {
  int disp = 0xff & memory->read(P, true, true, previousP);
  P++; P &= pMask; fetches++;
  disp |= (0xff & memory->read(P, true, true, previousP)) << 8;
  P++; P &= pMask; fetches++;
  int indexLsb = 0xff & memory->read(P, true, true, previousP);
  P++; P &= pMask; fetches++;          
  unsigned short address = (regSets[setSel].r.regX << 8) | indexLsb;
  int value = memory->read(address, true, false, previousP);
  value |= (memory->read((address+1) & pMask, true, false, previousP) << 8);
  value += direction * disp;
  memory->write(address, 0xff & value, previousP);
  memory->write((address+1) & pMask, 0xff & (value >> 8), previousP);
  flagCarry[setSel] = (value >> 16) & 0x1;
  if ((highReg != -1) && (lowReg != -1)) {
    regSets[setSel].regs[lowReg] = value & 0xff;
    regSets[setSel].regs[highReg] = (value >> 8)  & 0xff;  
  }  
}

int dp2200_cpu::immediateplus(unsigned char inst) {
    int r;
    unsigned int addrL, addrH;
    unsigned int op;
    unsigned char savedCarry;
    unsigned int reg;
    int carryzero;
    unsigned int sdata1, sdata2, result;
    unsigned int cc; /* condition code to check, if conditional operation */
    unsigned short address;
    int rl, rh;
    int count, data, i;
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
        if (is2200) return 1;
        if (is5500) return 0; // This is the System Information instrutcion on the 6600 but is a NOP on a 5500.
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
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }        
        interruptEnabled = 0;
        break;
      case 5:
        /* EI */
        switch (implicit) {
          case 0:
            if (is5500 && userMode) {
              privilegeViolation = true;
              return 0;
            }
            interruptEnabledToBeEnabled = 1;
            break;
          case 062:
            /* EUR */
            if (is5500 && userMode) {
              privilegeViolation = true;
              return 0;
            }  
            userMode=true;          
            interruptEnabledToBeEnabled = 1;
            stackptr = (stackptr - 1) & 0xf;
            P = stack.stk[stackptr] & pMask; 
            if (traceEnabled) printLog("TRACE", "%06o            RETURN FROM %06o     \n", P, previousP);         
            break;
          case 0111:
            if (is5500 && userMode) {
              privilegeViolation = true;
              return 0;
            }          
            interruptEnabledToBeEnabled = 1;  /* EJMP */
            addrL = (unsigned int)memory->read(P, true, true, previousP);
            P++;
            P &= pMask;
            fetches++;
            addrH = (unsigned int)memory->read(P, true, true, previousP);
            P = (addrL + (addrH << 8)) & pMask;
            if (traceEnabled) printLog("TRACE", "%06o            JUMP FROM %06o     \n", P, previousP);
            fetches++;          
            break;
          default:
            return 1;
        }
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
      case 4:
        // DFAC
        if (implicit == 0111) {
          int count = regSets[setSel].r.regC;
          int srcAddress, dstAddress;
          unsigned char srcData, dstData;
          do {
            srcAddress = ((regSets[setSel].r.regH << 8) | regSets[setSel].r.regL ) & pMask;
            dstAddress = ((regSets[setSel].r.regD << 8) | regSets[setSel].r.regE ) & pMask;
            srcData = 0xf & memory->read(srcAddress, true, false, previousP);
            dstData = 0xf & memory->read(dstAddress, true, false, previousP);
            printLog("INFO", "count=%d srcAddress=%06o dstAddress=%06o srcData=%03o dstData=%03o carry=%d\n", count,  srcAddress, dstAddress, srcData, dstData, flagCarry[setSel]);
            dstData += (srcData + flagCarry[setSel]);
            printLog("INFO", "Result = %03o\n", dstData);
            if (dstData > 9) {
              flagCarry[setSel]=1;
              dstData -= 10;
            } else {
              flagCarry[setSel] = 0;
            }
            dstData |= regSets[setSel].r.regB;
            memory->write(dstAddress, dstData, previousP);
            printLog("INFO", "Write %03o to address=%06o\n", dstAddress, dstData);
            incrementRegisterPair(REG_D, REG_E, -1);
            incrementRegisterPair(REG_H, REG_L, -1);
            count--;
          } while (count > 0);
        } else return 1;
        break;
      case 5: // PUSH IMMEDIATE
        if (is2200) return 1;
        sdata1 = 0xff & memory->read(P, true, true, previousP);
        P++; P &= pMask; fetches++;
        sdata1 |= 0xff00 & (memory->read(P, true, true, previousP)<<8);
        P++; P &= pMask; fetches++;
        stack.stk[stackptr] = sdata1;
        stackptr = (stackptr + 1) & 0xf;
        break;
      case 6:
        if (implicit == 0111) {
          if (is5500 && userMode) {
            privilegeViolation = true;
            return 0;
          }
          int dstAddress = ((regSets[setSel].r.regH << 8) | regSets[setSel].r.regL ) & pMask;
          int count = ((regSets[setSel].r.regC & 0xf) == 0)?16:(regSets[setSel].r.regC & 0xf);
          int iterations = ((regSets[setSel].r.regC & 0xf0) == 0)?256:(regSets[setSel].r.regC & 0xf0);
          do {
            memory->write(dstAddress, ioCtrl->input(), previousP);
            dstAddress++;
            count--;
          } while (count > 0);
          iterations -= 16;
          if (iterations == 0) {
            flagZero[setSel] = 1;
          } else {
            flagZero[setSel] = 0;
          }
          regSets[setSel].r.regC = 0xff & (count | iterations);
          regSets[setSel].r.regH = 0xff & (dstAddress >> 8);
          regSets[setSel].r.regL = 0xff & dstAddress;
        } else return 1;
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
        r=registerFromImplict(implicit);
        //printLog("INFO", "SLC implict = %03o r=%d \n", implicit, r);
        result = (int)regSets[setSel].regs[r] << 1;
        regSets[setSel].regs[r] =
            (unsigned char)(((result >> 8) | result) & 0xff);
        flagCarry[setSel] = regSets[setSel].regs[r] & 1;
        break;
      case 1: /* rotate right - SRC */
        r=registerFromImplict(implicit);
        //printLog("INFO", "SRC implict = %03o r=%d \n", implicit, r);
        flagCarry[setSel] = regSets[setSel].regs[r] & 1;
        result = (int)(regSets[setSel].regs[r] >> 1);
        regSets[setSel].regs[r] =
            (unsigned char)(((regSets[setSel].regs[r] << 7) | result) & 0xff);
        break;
      case 3:
          r=registerFromImplict(implicit);
          savedCarry = flagCarry[setSel];
          flagCarry[setSel] = 0x1 & regSets[setSel].regs[r];
          regSets[setSel].regs[r] = (regSets[setSel].regs[r] >> 1) | ((savedCarry << 7) & 0x80);
        break;
      case 4:
        r=registerFromImplict(implicit);
        regSets[setSel].regs[r] = 0;
        if (flagCarry[setSel]) {
          regSets[setSel].regs[r] = 1 << 7;  
        }
        if (flagSign[setSel]) {
          regSets[setSel].regs[r] |= 1 << 6;
        }
        if (!flagZero[setSel] && !flagSign[setSel]) {
          regSets[setSel].regs[r] |= 1 << 1;
        }
        if (!flagZero[setSel] && !flagParity[setSel]) {
          regSets[setSel].regs[r] |= 1 << 0;
        } 
        break;       
      case 7: // BRL
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }       
         r=registerFromImplict(implicit);
         memory->baseRegister = regSets[setSel].regs[r];
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
        if (traceEnabled) printLog("TRACE", "%06o            RETURN FROM %06o     \n", P, previousP);
      }
      break;
    case 4: /* math with immediate operands */
      //printLog("INFO", "implicit=%d\n", implicit);
      r=registerFromImplict(implicit);
      sdata2 = (unsigned char)regSets[setSel].regs[r]; /* reg r is source and target */
      sdata1 = (unsigned char)memory->read(P, true, true, previousP); /* source is immediate */
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
        switch (implicit) {
          case 0:
            incrementIndexShort(1, -1, -1); // Increment do not store in register
            break;
          case 0022:         
            return 1;
          case 0062:        
            incrementIndexShort(1, REG_B, REG_C);  
            break;       
          case 0111:
            incrementIndexLong(1, -1, -1);  
            break;
          case 0113: 
            incrementIndexLong(1, REG_B, REG_C);  
            break;        
          case 0115:
            incrementIndexLong(1, REG_D, REG_E);  
            break;  
          case 0117:
            incrementIndexLong(1, REG_H, REG_L);  
            break;                
          case 0174:
            incrementIndexShort(1, REG_D, REG_E);  
            break;      
          case 0176:
            incrementIndexShort(1, REG_H, REG_L);  
            break;     
          default:
            return 1;                   
        } 
        break; 
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
        switch (implicit) {
          case 0:
            incrementIndexShort(-1, -1, -1); // Decrement do not store in register
            break;
          case 0022:         
            return 1;
          case 0062:        
            incrementIndexShort(-1, REG_B, REG_C);  
            break;       
          case 0111:
            incrementIndexLong(-1, -1, -1);  
            break;
          case 0113: 
            incrementIndexLong(-1, REG_B, REG_C);  
            break;        
          case 0115:
            incrementIndexLong(-1, REG_D, REG_E);  
            break;  
          case 0117:
            incrementIndexLong(-1, REG_H, REG_L);  
            break;                
          case 0174:
            incrementIndexShort(-1, REG_D, REG_E);  
            break;      
          case 0176:
            incrementIndexShort(-1, REG_H, REG_L);  
            break;     
          default:
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
      sdata1 = memory->read(P, true, true, previousP); /* source is immediate */
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
        if (traceEnabled) printLog("TRACE", "%06o            RETURN FROM %06o     \n", P, previousP);
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
          //printLog("INFO", "Double Store  implicit=%03o inst =%03o\n", implicit, inst);
          switch (implicit) {
            case 0:
              // DS DE,HL
              address = ((regSets[setSel].r.regH << 8) | regSets[setSel].r.regL ) & pMask;
              memory->write(address, regSets[setSel].r.regE, previousP);
              address++;
              memory->write(address, regSets[setSel].r.regD, previousP);              
              break;
            case 0022:         
              return 1; 
            case 0062:        
              return 1;         
            case 0111:
              // DS BC,HL
              address = ((regSets[setSel].r.regH << 8) | regSets[setSel].r.regL ) & pMask;
              //printLog("INFO", "address=%05o\n", address);
              memory->write(address, regSets[setSel].r.regC, previousP);
              //printLog("INFO", "memory[%05o]=%03o C=%03o\n", address, memory[address]);
              address++;
              memory->write(address, regSets[setSel].r.regB, previousP);
              //printLog("INFO", "memory[%05o]=%03o B=%03o\n", address, memory[address]);
              break;    
            case 0113: 
              address = ((regSets[setSel].r.regD << 8) | regSets[setSel].r.regE ) & pMask;
              memory->write(address, regSets[setSel].r.regC, previousP);
              address++;
              memory->write(address, regSets[setSel].r.regB, previousP);  
              break;         
            case 0115: 
              return 1; 
            case 0117:
              address = ((regSets[setSel].r.regD << 8) | regSets[setSel].r.regE ) & pMask;
              memory->write(address, regSets[setSel].r.regL, previousP);
              address++;
              memory->write(address, regSets[setSel].r.regH, previousP); 
              break;              
            case 0174:
              address = ((regSets[setSel].r.regB << 8) | regSets[setSel].r.regC ) & pMask;
              memory->write(address, regSets[setSel].r.regE, previousP);
              address++;
              memory->write(address, regSets[setSel].r.regD, previousP);                       
              break;
            case 0176:
              address = ((regSets[setSel].r.regB << 8) | regSets[setSel].r.regC ) & pMask;
              memory->write(address, regSets[setSel].r.regL, previousP);
              address++;
              memory->write(address, regSets[setSel].r.regH, previousP); 
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
        return doubleLoad(implicit);
      case 5:
        if (is2200) return 1;
        return doubleLoad(1);
      case 7:
        // Sector table load
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        count = regSets[setSel].r.regC;
        address = ((regSets[setSel].r.regH << 8) & 0xff00) | (regSets[setSel].r.regL & 0xff);
        //printLog("INFO", "STL: count=%d address=%06o\n", count, address);
        for (i=0; i<count; i++) {
          data = memory->read(address, true, false, previousP);
          //printLog("INFO", "STL: data=%03o i=%d\n", data, i);
          memory->sectorTable[i].physicalPage = (data >> 4) & 0xf;
          if (data & 0x4) {
            memory->sectorTable[i].accessEnable=true;
          } else {
            memory->sectorTable[i].accessEnable=false;
          }
          if (data & 0x8) {
            memory->sectorTable[i].writeEnable=true;
          } else {
            memory->sectorTable[i].writeEnable=false;
          }
          address++;
        } 
        break;
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
    loc = memory->read(P, true, true, previousP);
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
      addrL = (unsigned int)memory->read(P, true, true, previousP);
      P++;
      P &= pMask;
      fetches++;

      addrH = (unsigned int)memory->read(P, true, true, previousP);
      P++;
      P &= pMask;
      fetches++;

      if (cc) {
        timeForInstruction =instTimeInNsTaken[inst];
        P = (addrL + (addrH << 8)) & pMask;
        if (traceEnabled) printLog("TRACE", "%06o            JUMP FROM %06o     \n", P, previousP);
      }
      break;
    case 1:
      op = (inst & 0x38) >> 3;
      
      switch (op) {
      case 0:
        /* INPUT */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }
        regSets[setSel].regs[r]=ioCtrl->input();
        break;
      case 1:
        /* Unimplemented */
        return 1;
      case 2: 
        /* EX ADR */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }
        return ioCtrl->exAdr(regSets[setSel].regs[r]);;
        break;
      case 3:
        /* EX COM1 */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }        
        return ioCtrl->exCom1(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX BEEP */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }        
        return ioCtrl->exBeep();
        break;
      case 6:
        /* EX RBK */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }        
        return ioCtrl->exRBK();
        break;
      case 7:
        /* EX SF */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }        
        return ioCtrl->exSF();
        break;
      }
      break;
    case 2: /* call conditionally */
      cc = chkconditional(inst);
      addrL = (unsigned int)memory->read(P, true, true, previousP);
      P++;
      P &= pMask;
      fetches++;

      addrH = (unsigned int)memory->read(P, true, true, previousP);
      P++;
      P &= pMask;
      fetches++;

      if (cc) {
        timeForInstruction =instTimeInNsTaken[inst];
        stack.stk[stackptr] = P;
        stackptr = (stackptr + 1) & 0xf;
        P = (addrL + (addrH << 8)) & pMask;
        if (traceEnabled) printLog("TRACE", "%06o            CALL FROM %06o     \n", P, previousP);
      }
      break;
    case 3:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* PARITY INPUT - We don't care about input really.*/
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }        
        regSets[setSel].regs[r]=ioCtrl->input();
        return 0;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX STATUS */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }        
        ioCtrl->exStatus(); 
        break;
      case 3:
        /* EX COM2 */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exCom2(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX CLICK */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exClick();
        break;
      case 6:
        /* EX WBK */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exWBK();
        break;
      case 7:
        /* EX SB */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exSB();
        break;
      }
      break;
    case 4: /* jump */
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* JMP */
        addrL = (unsigned int)memory->read(P, true, true, previousP);
        P++;
        P &= pMask;
        fetches++;
        addrH = (unsigned int)memory->read(P, true, true, previousP);
        P = (addrL + (addrH << 8)) & pMask;
        if (traceEnabled) printLog("TRACE", "%06o            JUMP FROM %06o     \n", P, previousP);
        fetches++;
        break;
      case 1:
        if (is2200) return 1;
        // Paged Load PL B, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regB = memory->read(address, true, false, previousP);
        break;
      case 2:
        if (is2200) return 1;
        // Paged Load PL V, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regC = memory->read(address, true, false, previousP);
        break;
      case 3:
        if (is2200) return 1;
        // Paged Load PL D, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regD = memory->read(address, true, false, previousP);
        break;
      case 4:
        if (is2200) return 1;
        // Paged Load PL E, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regE = memory->read(address, true, false, previousP);
        break;
      case 5:
        if (is2200) return 1;
        // Paged Load PL H, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regH = memory->read(address, true, false, previousP);
        break;
      case 6:
        if (is2200) return 1;
        // Paged Load PL L, (loc)
        address = getPagedAddress();
        regSets[setSel].r.regL = memory->read(address, true, false, previousP);
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
        regSets[setSel].r.regA = memory->read(address, true, false, previousP);
        break;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX DATA */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        ioCtrl->exData();
        break;
      case 3:
        /* EX COM3 */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exCom3(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
        break;
      case 5:
        /* EX DECK1 */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exDeck1();
        break;
      case 6:
        /* Unimplemented */
        return 1;
        break;
      case 7:
        /* EX REWND */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exRewind();
        break;
      }

      break;
    case 6: /* call */
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        addrL = (unsigned int)memory->read(P, true, true, previousP);
        P++;
        P &= pMask;
        fetches++;

        addrH = (unsigned int)memory->read(P, true, true, previousP);
        P++;
        P &= pMask;
        fetches++;
        stack.stk[stackptr] = P;
        stackptr = (stackptr + 1) & 0xf;
        P = (addrL + (addrH << 8)) & pMask;
        if (traceEnabled) printLog("TRACE", "%06o            CALL FROM %06o     \n", P, previousP);
        break;
      case 1:
        if (is2200) return 1;
        // Paged Store PS B, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regB, previousP);
        break;            
      case 2: 
          if (is2200) return 1;
          address = getPagedAddress();
          switch (implicit) {
            case 0:
              // Paged Store PS C, (loc)
              memory->write(address, regSets[setSel].r.regC, previousP);
              break;
            case 0022:         
              return 1; 
            case 0062:        
              return 1;         
            case 0111:
              memory->write(address, regSets[setSel].r.regC, previousP);
              //printLog("INFO", "memory[address]=%03o C=%03o\n", memory[address], regSets[setSel].r.regC);
              address++;
              memory->write(address, regSets[setSel].r.regB, previousP); 
              //printLog("INFO", "address=%05o memory[address]=%03o B=%03o\n", address, memory[address], regSets[setSel].r.regB);    
              break;    
            case 0113:
              memory->write(address++, regSets[setSel].r.regE, previousP);  
              memory->write(address, regSets[setSel].r.regD, previousP);     
              break;         
            case 0115:
              memory->write(address++, regSets[setSel].r.regL, previousP);  
              memory->write(address, regSets[setSel].r.regH, previousP);     
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
        memory->write(address, regSets[setSel].r.regD, previousP);
        break;        
      case 4:
        if (is2200) return 1;
        // Paged Store PS E, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regE, previousP);
        break;        
      case 5:
        if (is2200) return 1;
        // Paged Store PS H, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regH, previousP);
        break;        
      case 6:
        if (is2200) return 1;
        // Paged Store PS L, (loc)
        address = getPagedAddress();
        memory->write(address, regSets[setSel].r.regL, previousP);
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
        memory->write(address, regSets[setSel].r.regA, previousP);
        break;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX WRITE */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exWrite(regSets[setSel].regs[r]);
        break;
      case 3:
        /* EX COM4 */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exCom4(regSets[setSel].regs[r]);
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX DECK2 */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exDeck2();
        break;
      case 6:
        /* EX BSP */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
        return ioCtrl->exBSP();
        break;
      case 7:
        /* EX TSTOP */
        if (is5500 && userMode) {
          privilegeViolation = true;
          return 0;
        }         
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
                      regSets[setSel].r.regL, true, false, previousP);
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
      data = memory->read(address, true, false, previousP);
    } else {
      data = regSets[setSel].regs[src];
    }
    if (dest == 0x7) {
      unsigned short address = ((regSets[setSel].regs[getHighRegFromImplicit()] & hMask) << 8) + regSets[setSel].regs[getLowRegFromImplicit()];
      memory->write(address, data, previousP);
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
  is5500=false;
  is2200=true;
  ioCtrl = new IOController ();
  memory = new Memory(&is5500, &accessViolation, &writeViolation, &userMode);
}
