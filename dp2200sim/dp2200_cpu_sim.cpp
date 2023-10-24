//
// Datapoint 2200 Disassembler
// Input : raw binary code
//
// Based on M. Willegal 8008 disassembler / simulator
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include "dp2200_cpu_sim.h"



// mnemonic symbols for opcodes
  /*
  const char *mnems[] = {
      // 00xxxxxx  load (immediate), add/subtract (immediate), increment,
      // decrement,
      "HALT", "HALT", "SLC", "RFC", "AD ", "---", "LA ", "RETURN", "SYNC",
      "---", "SRC", "RFZ", "AC ", "---", "LB ", "---", "BETA", "---", "---",
      "RFS", "SU ", "---", "LC ", "---", "ALPHA", "---", "---", "RFP", "SB ",
      "---", "LD ", "---", "DI ", "---", "---", "RTC", "ND ", "---", "LE ",
      "---", "EI ", "---", "---", "RTZ", "XR ", "---", "LH ", "---", "POP",
      "---", "---", "RTS", "OR ", "---", "LL ", "---", "PUSH", "---", "---",
      "RTP", "CP ", "---", "---", "---",
      // 01xxxxxx jump, call, input, output
      "JFC", "INPUT", "CFC", "---", "JMP", "---", "CALL", "---", "JFZ", "---",
      "CFZ", "---", "---", "---", "---", "---", "JFS", "EX_ADR", "CFS",
      "EX_STAT", "---", "EX_DATA", "---", "EX_WRITE", "JFP", "EX_COM1", "CFP",
      "EX_COM2", "---", "EX_COM3", "---", "EX_COM4", "JTC", "---", "CTC", "---",
      "---", "---", "---", "---", "JTZ", "EX_BEEP", "CTZ", "EX_CLICK", "---",
      "EX_DECK1", "---", "EX_DECK2", "JTS", "EX_RBK", "CTS", "EX_WBK", "---",
      "---", "---", "EX_BSP", "JTP", "EX_SF", "CTP", "EX_SB", "---", "EX_RWND",
      "---", "EX_TSTOP",
      // 10xxxxxx add/subtract (not immediate), and/or/eor/cmp (not immediate)
      "ADA", "ADB", "ADC", "ADD", "ADE", "ADH", "ADL", "ADM", "ACA", "ACB",
      "ACC", "ACD", "ACE", "ACH", "ACL", "ACM", "SUA", "SUB", "SUC", "SUD",
      "SUE", "SUH", "SUL", "SUM", "SBA", "SBB", "SBC", "SBD", "SBE", "SBH",
      "SBL", "SBM", "NDA", "NDB", "NDC", "NDD", "NDE", "NDH", "NDL", "NDM",
      "XRA", "XRB", "XRC", "XRD", "XRE", "XRH", "XRL", "XRM", "ORA", "ORB",
      "ORC", "ORD", "ORE", "ORH", "ORL", "ORM", "CPA", "CPB", "CPC", "CPD",
      "CPE", "CPH", "CPL", "CPM",
      // 11xxxxxx load (not immediate), halt
      "NOP", "LAB", "LAC", "LAD", "LAE", "LAH", "LAL", "LAM", "LBA", "---",
      "LBC", "LBD", "LBE", "LBH", "LBL", "LBM", "LCA", "LCB", "---", "LCD",
      "LCE", "LCH", "LCL", "LCM", "LDA", "LDB", "LDC", "---", "LDE", "LDH",
      "LDL", "LDM", "LEA", "LEB", "LEC", "LED", "---", "LEH", "LEL", "LEM",
      "LHA", "LHB", "LHC", "LHD", "LHE", "---", "LHL", "LHM", "LLA", "LLB",
      "LLC", "LLD", "LLE", "LLH", "---", "LLM", "LMA", "LMB", "LMC", "LMD",
      "LME", "LMH", "LML", "HALT"};

  // Instruktion length definitions
  unsigned int instr_l[] = {
      1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1,
      1, 1, 1, 2, 1, 2, 1, 1, 1, 0, 1, 2, 1, 2, 1, 1, 1, 0, 1, 2, 1, 2, 1, 1, 1,
      0, 1, 2, 1, 2, 1, 0, 0, 0, 1, 2, 1, 2, 1,
      //
      3, 1, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 0, 0, 3, 1, 3, 1, 0, 1, 0, 1, 3,
      1, 3, 1, 0, 1, 0, 1, 3, 0, 3, 0, 0, 0, 0, 0, 3, 1, 3, 1, 0, 1, 0, 1, 3, 1,
      3, 1, 0, 0, 0, 1, 3, 1, 3, 1, 0, 1, 0, 1,
      //
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      //
      1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1,
      1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  */
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

    int halted;

    /* Handle interrupt*/

    if (interruptEnabled && interruptPending) {
      unsigned int addrL, addrH;
      interruptPending = 0;

      /* now bump stack to use new pc and save it */
      stack.stk[stackptr] = P;
      stackptr = (stackptr + 1) & 0xf;
      P = 0;
    }

    inst = memory[P];
    P++;
    P &= 0x3fff;
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
    return halted;
  }

int dp2200_cpu::immediateplus(unsigned char inst) {
    unsigned int op;
    unsigned int reg;
    int oldflag;
    int carryzero;
    unsigned int sdata1, sdata2, result;
    unsigned int cc; /* condition code to check, if conditional operation */

    /* first check for halt */
    if ((inst & 0xfe) == 0x0) {
      printf("halt:%2x\n", inst);
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
        stackptr = (stackptr - 1) & 0xf;
        regSets[setSel].r.regH = (stack.stk[stackptr] >> 8) & 0xff;
        regSets[setSel].r.regL = stack.stk[stackptr] & 0xff;
        break;
      case 7:
        /* PUSH */
        stack.stk[stackptr] =
            ((unsigned int)(regSets[setSel].r.regH & 0x3f) << 8) +
            regSets[setSel].r.regL;
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
      default: /*  unimplemented - handle as HALT for now. */
        return 1;
        break;
      }
      break;
    case 3: /* conditional return */
      cc = chkconditional(inst);
      if (cc) {
        P = stack.stk[stackptr] & 0x3fff;
        stackptr = (stackptr - 1) & 0xf;
      }
      break;
    case 4: /* math with immediate operands */
      sdata2 = (unsigned char)regSets[setSel]
                   .r.regA;              /* reg A is source and target */
      sdata1 = (unsigned char)memory[P]; /* source is immediate */
      P++;
      P &= 0x3fff;
      fetches++;

      sdata1 = sdata1 & 0xff;
      sdata2 = sdata2 & 0xff;

      op = (inst >> 3) & 0x7;
      switch (op) {
      case 0: /* add without carry, but set carry */
        result = sdata1 + sdata2;
        //              printf ("mADD RA: %2.2x  RS: %2.2x RESULT:
        //              %2.2x",sdata2,sdata1, result);
        regSets[setSel].r.regA = (unsigned char)result;
        carryzero = 0;
        break;
      case 1: /* add with carry, and set carry */
        result = sdata1 + sdata2 + flagCarry[setSel];
        //              printf ("mADC RA: %2.2x  RS: %2.2x CARRYIN: %d RESULT:
        //              %2.2x",sdata2,sdata1,flagCarry, result);
        regSets[setSel].r.regA = (unsigned char)result;
        carryzero = 0;
        break;
      case 2: /* subtract with no borrow, but set borrow */
        result = sdata2 - sdata1;
        //              printf ("mSUB RA: %2.2x  RS: %2.2x RESULT:
        //              %2.2x",sdata2,sdata1, result);
        regSets[setSel].r.regA = (unsigned char)result;
        carryzero = 0;
        break;
      case 3: /* subtract with borrow, and set borrow */
        result = sdata2 - sdata1 - flagCarry[setSel];
        //              printf ("mSBC RA: %2.2x  RS: %2.2x CARRYIN: %d RESULT:
        //              %2.2x",sdata2,sdata1,flagCarry, result);
        regSets[setSel].r.regA = (unsigned char)result;
        carryzero = 0;
        break;
      case 4: /* logical and */
        result = sdata1 & sdata2;
        regSets[setSel].r.regA = (unsigned char)result;
        carryzero = 1;
        break;
      case 5: /* exclusive or */
        result = sdata1 ^ sdata2;
        regSets[setSel].r.regA = (unsigned char)result;
        carryzero = 1;
        break;
      case 6: /* or */
        result = sdata1 | sdata2;
        regSets[setSel].r.regA = (unsigned char)result;
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
      default:
        return 1;
      }
      break;
    case 6:               /* load immediate */
      sdata1 = memory[P]; /* source is immediate */
      P++;
      P &= 0x3fff;
      fetches++;

      reg = (inst >> 3) & 0x7;
      if (reg == 7) { // reg 7 is not used in DP2200
        // nop
      } else {
        regSets[setSel].regs[reg] = sdata1;
      }
      break;
    case 7:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* RETURN */
        P = stack.stk[stackptr] & 0x3fff;
        stackptr = (stackptr - 1) & 0xf;
        return 0;
      default:
        /* Unimplemented */
        return 1;
      }

      break;
    }
    return 0;
  }

  int dp2200_cpu::iojmpcall(unsigned char inst) {
    unsigned int op;
    unsigned int cc; /* condition code to check, if conditional operation */
    unsigned int addrL, addrH;

    op = inst & 0x7;
    switch (op) {
    case 0: /* jump conditionally */
      cc = chkconditional(inst);
      addrL = (unsigned int)memory[P];
      P++;
      P &= 0x3fff;
      fetches++;

      addrH = (unsigned int)memory[P];
      P++;
      P &= 0x3fff;
      fetches++;

      if (cc) {
        P = (addrL + (addrH << 8)) & 0x3fff;
      }
      break;
    case 1:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* INPUT */
        break;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX ADR */
        break;
      case 3:
        /* EX COM1 */
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX BEEP */
        break;
      case 6:
        /* EX RBK */
        break;
      case 7:
        /* EX SF */
        break;
      }
      break;
    case 2: /* call conditionally */
      cc = chkconditional(inst);
      addrL = (unsigned int)memory[P];
      P++;
      P &= 0x3fff;
      fetches++;

      addrH = (unsigned int)memory[P];
      P++;
      P &= 0x3fff;
      fetches++;

      if (cc) {
        stack.stk[stackptr] = P;
        stackptr = (stackptr + 1) & 0xf;
        P = (addrL + (addrH << 8)) & 0x3fff;
      }
      break;
    case 3:
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* Unimplemented */
        return 1;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX STATUS */
        break;
      case 3:
        /* EX COM2 */
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX CLICK */
        break;
      case 6:
        /* EX WBK */
        break;
      case 7:
        /* EX SB */
        break;
      }
      break;
    case 4: /* jump */
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        /* JMP */
        addrL = (unsigned int)memory[P];
        P++;
        P &= 0x3fff;
        fetches++;
        addrH = (unsigned int)memory[P];
        P = (addrL + (addrH << 8)) & 0x3fff;
        fetches++;
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
        /* Unimplemented */
        return 1;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX DATA */
        break;
      case 3:
        /* EX COM3 */
        break;
      case 4:
        /* Unimplemented */
        break;
      case 5:
        /* EX DECK1 */
        break;
      case 6:
        /* Unimplemented */
        break;
      case 7:
        /* EX REWND */
        break;
      }

      break;
    case 6: /* call */
      op = (inst & 0x38) >> 3;
      switch (op) {
      case 0:
        addrL = (unsigned int)memory[P];
        P++;
        P &= 0x3fff;
        fetches++;

        addrH = (unsigned int)memory[P];
        P++;
        P &= 0x3fff;
        fetches++;
        stack.stk[stackptr] = P;
        stackptr = (stackptr + 1) & 0xf;
        P = (addrL + (addrH << 8)) & 0x3fff;
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
        /* Unimplemented */
        return 1;
      case 1:
        /* Unimplemented */
        return 1;
      case 2:
        /* EX WRITE */
        break;
      case 3:
        /* EX COM4 */
        break;
      case 4:
        /* Unimplemented */
        return 1;
      case 5:
        /* EX DECK2 */
        break;
      case 6:
        /* EX BSP */
        break;
      case 7:
        /* EX TSTOP */
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

    src = inst & 0x7;
    if (src == 0x7) {
      sdata1 = memory[((regSets[setSel].r.regH & 0x3f) << 8) +
                      regSets[setSel].r.regL];
    } else {
      sdata1 = regSets[setSel].regs[src];
    }
    sdata1 = sdata1 & 0xff;
    sdata2 = (char)regSets[setSel]
                 .r.regA; /* reg A is other source and usually target */
    sdata2 = sdata2 & 0xff;

    op = (inst >> 3) & 0x7;
    switch (op) {
    case 0: /* add without carry, but set carry */
      result = sdata1 + sdata2;
      regSets[setSel].r.regA = (unsigned char)result;
      carryzero = 0;
      //          printf (" ADD RA: %2.2x  RS: %2.2x RESULT:
      //          %2.2x",sdata2,sdata1,result);
      break;

    case 1: /* add with carry, and set carry */
      result = sdata1 + sdata2 + flagCarry[setSel];
      regSets[setSel].r.regA = (unsigned char)result;
      carryzero = 0;
      //          printf (" ADC RA: %2.2x  RS: %2.2x CARRYIN: %d RESULT:
      //          %2.2x",sdata2,sdata1,flagCarry[setSel], result);
      break;

    case 2: /* subtract with no borrow, but set borrow */
      result = sdata2 - sdata1;
      regSets[setSel].r.regA = (unsigned char)result;
      carryzero = 0;

      //      printf (" SUB RA: %2.2x  RS: %2.2x RESULT:
      //      %2.2x",sdata2,sdata1,result);
      break;
    case 3: /* subtract with borrow, and set borrow */
      result = sdata2 - sdata1 - flagCarry[setSel];
      regSets[setSel].r.regA = (unsigned char)result;
      carryzero = 0;
      //          printf (" SBC RA: %2.2x  RS: %2.2x BORROWIN: %d RESULT:
      //          %2.2x",sdata2, sdata1, flagCarry[setSel], result);
      break;
    case 4: /* logical and */
      result = sdata1 & sdata2;
      regSets[setSel].r.regA = (unsigned char)result;
      carryzero = 1;
      break;
    case 5: /* exclusive or */
      result = sdata1 ^ sdata2;
      regSets[setSel].r.regA = (unsigned char)result;
      carryzero = 1;
      break;
    case 6: /* or */
      result = sdata1 | sdata2;
      regSets[setSel].r.regA = (unsigned char)result;
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
      printf("halt:%2x\n", inst);
      return 1;
    }
    if (src == 0x7) {
      data = memory[((unsigned int)(regSets[setSel].r.regH & 0x3f) << 8) +
                    regSets[setSel].r.regL];
    } else {
      data = regSets[setSel].regs[src];
    }
    if (dest == 0x7) {
      memory[((unsigned int)(regSets[setSel].r.regH & 0x3f) << 8) +
             regSets[setSel].r.regL] = data;
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