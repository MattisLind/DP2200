#include "RegisterWindow.h"

registerWindow::Form::hexMemoryDataHookExecutor::hexMemoryDataHookExecutor(class registerWindow::Form * r, int d) {
  data=d;
  rwf = r;
}

registerWindow::Form::hexMemoryAddressHookExecutor::hexMemoryAddressHookExecutor(class registerWindow::Form * r, int a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::octalMemoryDataHookExecutor::octalMemoryDataHookExecutor(class registerWindow::Form * r, int d) {
  data=d;
  rwf = r;
}

registerWindow::Form::octalMemoryAddressHookExecutor::octalMemoryAddressHookExecutor(class registerWindow::Form * r, int a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::hexCharPointerHookExecutor::hexCharPointerHookExecutor(class registerWindow::Form * r, unsigned char * a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::hexShortPointerHookExecutor::hexShortPointerHookExecutor(class registerWindow::Form * r, unsigned short * a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::octalCharPointerHookExecutor::octalCharPointerHookExecutor(class registerWindow::Form * r, unsigned char * a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::octalShortPointerHookExecutor::octalShortPointerHookExecutor(class registerWindow::Form * r, unsigned short * a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::accessibleHookExecutor::accessibleHookExecutor(class registerWindow::Form * r, bool * a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::writeableHookExecutor::writeableHookExecutor(class registerWindow::Form * r, bool * a) { 
  address = a; 
  rwf = r;
}

void registerWindow::OctalForm::set2200Mode(bool b) {
  if (b) {
    set_field_opts(regs[0][7],  O_PUBLIC  | O_STATIC); 
    set_field_opts(regs[1][7],  O_PUBLIC  | O_STATIC);
    set_field_opts(regsIdents[0][7],  O_PUBLIC  | O_STATIC); 
    set_field_opts(regsIdents[1][7],  O_PUBLIC  | O_STATIC); 
    set_field_opts(base, O_PUBLIC  | O_STATIC); 
    set_field_opts(baseIdents, O_PUBLIC  | O_STATIC); 
    // SECTOR TABLE
    set_field_opts(sectorTableHeader, O_PUBLIC  | O_STATIC);
    for (int i=0; i<16; i++) {
      set_field_opts(sectorTableFields[i].ident,O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].accessible, O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].writeable, O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].physicalSector, O_PUBLIC  | O_STATIC);
    }    
  } else {
    set_field_opts(regs[0][7], O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE); 
    set_field_opts(regs[1][7], O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE); 
    set_field_opts(regsIdents[0][7],  O_VISIBLE | O_PUBLIC  | O_STATIC); 
    set_field_opts(regsIdents[1][7],  O_VISIBLE | O_PUBLIC  | O_STATIC); 
    set_field_opts(base, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
    set_field_opts(baseIdents, O_VISIBLE | O_PUBLIC  | O_STATIC);    
    // SECTOR TABLE
    set_field_opts(sectorTableHeader, O_VISIBLE | O_PUBLIC  | O_STATIC);
    for (int i=0; i<16; i++) {
      set_field_opts(sectorTableFields[i].ident, O_VISIBLE | O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].accessible, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
      set_field_opts(sectorTableFields[i].writeable, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
      set_field_opts(sectorTableFields[i].physicalSector, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
    }
  }
}

void registerWindow::HexForm::set2200Mode(bool b) {
  if (b) {
    set_field_opts(regs[0][7],  O_PUBLIC  | O_STATIC); 
    set_field_opts(regs[1][7],  O_PUBLIC  | O_STATIC);
    set_field_opts(regsIdents[0][7],  O_PUBLIC  | O_STATIC); 
    set_field_opts(regsIdents[1][7],  O_PUBLIC  | O_STATIC); 
    set_field_opts(base, O_PUBLIC  | O_STATIC); 
    set_field_opts(baseIdents, O_PUBLIC  | O_STATIC); 
    // SECTOR TABLE
    set_field_opts(sectorTableHeader, O_PUBLIC  | O_STATIC);
    for (int i=0; i<16; i++) {
      set_field_opts(sectorTableFields[i].ident,O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].accessible, O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].writeable, O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].physicalSector, O_PUBLIC  | O_STATIC);
    }    
  } else {
    set_field_opts(regs[0][7], O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE); 
    set_field_opts(regs[1][7], O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE); 
    set_field_opts(regsIdents[0][7],  O_VISIBLE | O_PUBLIC  | O_STATIC); 
    set_field_opts(regsIdents[1][7],  O_VISIBLE | O_PUBLIC  | O_STATIC); 
    set_field_opts(base, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
    set_field_opts(baseIdents, O_VISIBLE | O_PUBLIC  | O_STATIC);    
    // SECTOR TABLE
    set_field_opts(sectorTableHeader, O_VISIBLE | O_PUBLIC  | O_STATIC);
    for (int i=0; i<16; i++) {
      set_field_opts(sectorTableFields[i].ident, O_VISIBLE | O_PUBLIC  | O_STATIC);
      set_field_opts(sectorTableFields[i].accessible, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
      set_field_opts(sectorTableFields[i].writeable, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
      set_field_opts(sectorTableFields[i].physicalSector, O_VISIBLE | O_PUBLIC  | O_STATIC | O_EDIT | O_ACTIVE);
    }
  }
}

void registerWindow::OctalForm::updateForm() {
  
  int i, k = 0, j;
  char b[7];
  unsigned char t;
  unsigned short startAddress = cpu.startAddress;
  char asciiB[24], fieldB[28];
  for (i = 0; i < 16 && startAddress <= 0xFFFF; startAddress += 16, i++) {
    snprintf(b, 7, "%06o", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (j = 0; j < 16; j++) {
      t = cpu.memory->physicalMemoryRead(startAddress + j);
      if ((startAddress + j) == cpu.P ) {
        set_field_back(dataFields[k], A_UNDERLINE);
      } else {
        set_field_back(dataFields[k], A_NORMAL);
      }
      snprintf(b, 4, "%03o", t);
      set_field_buffer(dataFields[k], 0, b);
      k++;
      asciiB[j] = (t >= 0x20 && t < 127) ? t : '.';
    }
    asciiB[j] = 0;
    snprintf(fieldB, 26, "|%s|", asciiB);
    set_field_buffer(asciiFields[i], 0, fieldB);
  }
  for (; i < 16; startAddress += 16, i++) {
    snprintf(b, 7, "%05o", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (int j = 0; j < 16; j++) {
      t = cpu.memory->physicalMemoryRead(startAddress + j);
      if ((startAddress + j) == cpu.P ) {
        set_field_back(dataFields[k], A_UNDERLINE);
      } else {
        set_field_back(dataFields[k], A_NORMAL);
      }        
      snprintf(b, 4, "%03o", t);
      set_field_buffer(dataFields[k], 0, b);
      k++;
      asciiB[j] = (t >= 0x20 && t < 127) ? t : '.';
    }
    asciiB[j] = 0;
    snprintf(fieldB, 19, "|%s|", asciiB);
    set_field_buffer(asciiFields[i], 0, fieldB);
  }
  // update registers.
  for (auto regset =0; regset<2; regset++) {
    int i;
    for (i=0; i<7; i++) {
      auto f = regs[regset][i];
      auto r = cpu.regSets[regset].regs[i];
      snprintf(b, 5, "%03o", r);
      set_field_buffer(f, 0, b);
    }

    snprintf(b, 3, "%01X", cpu.flagCarry[regset]);
    set_field_buffer(flagCarry[regset], 0, b);
    snprintf(b, 3, "%01X", cpu.flagZero[regset]);
    set_field_buffer(flagZero[regset], 0, b);
    snprintf(b, 3, "%01X", cpu.flagParity[regset]);
    set_field_buffer(flagParity[regset], 0, b);
    snprintf(b, 3, "%01X", cpu.flagSign[regset]);
    set_field_buffer(flagSign[regset], 0, b);                  
  }

  // Sector table

  for (int i=0; i<16; i++) {
    snprintf(b, 4, "%02o", cpu.memory->sectorTable[i].physicalPage);
    set_field_buffer(sectorTableFields[i].physicalSector, 0, b);
    snprintf(b, 2, "%c", cpu.memory->sectorTable[i].accessEnable?'U':'S');
    set_field_buffer(sectorTableFields[i].accessible, 0, b);
    snprintf(b, 2, "%c", cpu.memory->sectorTable[i].writeEnable?'W':'R');
    set_field_buffer(sectorTableFields[i].writeable, 0, b);    
  }
  
  // Base register
  snprintf(b, 4, "%03o", cpu.memory->baseRegister);
  set_field_buffer(base, 0, b);

  snprintf(b, 7, "%06o", cpu.P);
  set_field_buffer(pc, 0, b);

  for (auto i = 0; i<16; i++) {
    snprintf(b, 7, "%06o", cpu.stack.stk[i]);
    set_field_buffer(stack[i], 0, b);
    if (i == cpu.stackptr ) {
      set_field_back(stack[i], A_UNDERLINE);
    } else {
      set_field_back(stack[i], A_NORMAL);
    }   
  }

  if (cpu.setSel==0) {
    set_field_back(mode[0], A_UNDERLINE);
    set_field_back(mode[1], A_NORMAL);  
  } else {
    set_field_back(mode[1], A_UNDERLINE);
    set_field_back(mode[0], A_NORMAL);   
  }

  // update mnemonic

  set_field_buffer(mnemonic, 0, cpu.disassembleLine(asciiB, 23, true, cpu.P)); 
  i=0;

  // update trace
  for (auto it=cpu.instructionTrace.begin(); it<cpu.instructionTrace.end(); it++, i++) {
    snprintf(fieldB, 27, "%06o %03o %s", it->address, it->data[0], cpu.disassembleLine(asciiB, 27, true, it->data));
    set_field_buffer(instructionTrace[i], 0, fieldB); 
  }
  i=0;
  for (auto it=cpu.breakpoints.begin(); it<cpu.breakpoints.end(); it++, i++) {
    snprintf(fieldB, 7, "%05o", *it);
    set_field_buffer(breakpoints[i], 0, fieldB); 
  }
  for (; i<8; i++) {
    set_field_buffer(breakpoints[i], 0, "");  
  }

  if (cpu.keyboardLightStatus) {
    set_field_back(keyboardLightField, A_STANDOUT);
  } else {
    set_field_back(keyboardLightField, A_NORMAL);
  }

  if (cpu.displayLightStatus) {
    set_field_back(displayLightField, A_STANDOUT);
  } else {
    set_field_back(displayLightField, A_NORMAL);
  }

  if (cpu.keyboardButtonStatus) {
    set_field_back(keyboardButtonField, A_STANDOUT);
  } else {
    set_field_back(keyboardButtonField, A_NORMAL);
  }  

  if (cpu.displayButtonStatus) {
    set_field_back(displayButtonField, A_STANDOUT);
  } else {
    set_field_back(displayButtonField, A_NORMAL);
  }    
   
}

void registerWindow::HexForm::updateForm() {
  int i, k = 0, j;
  char b[5];
  unsigned char t;
  unsigned short startAddress = cpu.startAddress;
  char asciiB[24], fieldB[27];
  for (i = 0; i < 16 && startAddress <= 0xFFFF; startAddress += 16, i++) {
    snprintf(b, 5, "%04X", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (j = 0; j < 16; j++) {
      t = cpu.memory->physicalMemoryRead(startAddress + j);
      if ((startAddress + j) == cpu.P ) {
        set_field_back(dataFields[k], A_UNDERLINE);
      } else {
        set_field_back(dataFields[k], A_NORMAL);
      }
      snprintf(b, 3, "%02X", t);
      set_field_buffer(dataFields[k], 0, b);
      k++;
      asciiB[j] = (t >= 0x20 && t < 127) ? t : '.';
    }
    asciiB[j] = 0;
    snprintf(fieldB, 26, "|%s|", asciiB);
    set_field_buffer(asciiFields[i], 0, fieldB);
  }
  for (; i < 16; startAddress += 16, i++) {
    snprintf(b, 5, "%04X", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (int j = 0; j < 16; j++) {
      t = cpu.memory->physicalMemoryRead(startAddress + j);
      if ((startAddress + j) == cpu.P ) {
        set_field_back(dataFields[k], A_UNDERLINE);
      } else {
        set_field_back(dataFields[k], A_NORMAL);
      }        
      snprintf(b, 3, "%02X", t);
      set_field_buffer(dataFields[k], 0, b);
      k++;
      asciiB[j] = (t >= 0x20 && t < 127) ? t : '.';
    }
    asciiB[j] = 0;
    snprintf(fieldB, 19, "|%s|", asciiB);
    set_field_buffer(asciiFields[i], 0, fieldB);
  }
  // update registers.
  for (auto regset =0; regset<2; regset++) {
    int i;
    for (i=0; i<7; i++) {
      auto f = regs[regset][i];
      auto r = cpu.regSets[regset].regs[i];
      snprintf(b, 3, "%02X", r);
      set_field_buffer(f, 0, b);
    }
    snprintf(b, 3, "%01X", cpu.flagCarry[regset]);
    set_field_buffer(flagCarry[regset], 0, b);
    snprintf(b, 3, "%01X", cpu.flagZero[regset]);
    set_field_buffer(flagZero[regset], 0, b);
    snprintf(b, 3, "%01X", cpu.flagParity[regset]);
    set_field_buffer(flagParity[regset], 0, b);
    snprintf(b, 3, "%01X", cpu.flagSign[regset]);
    set_field_buffer(flagSign[regset], 0, b);                  
  }

  // Sector table

  for (int i=0; i<16; i++) {
    snprintf(b, 4, "%02X", cpu.memory->sectorTable[i].physicalPage);
    set_field_buffer(sectorTableFields[i].physicalSector, 0, b);
    snprintf(b, 2, "%c", cpu.memory->sectorTable[i].accessEnable?'U':'S');
    set_field_buffer(sectorTableFields[i].accessible, 0, b);
    snprintf(b, 2, "%c", cpu.memory->sectorTable[i].writeEnable?'W':'R');
    set_field_buffer(sectorTableFields[i].writeable, 0, b);    
  }
  
  // Base register
  snprintf(b, 4, "%02X", cpu.memory->baseRegister);
  set_field_buffer(base, 0, b);


  snprintf(b, 5, "%04X", cpu.P);
  set_field_buffer(pc, 0, b);

  for (auto i = 0; i<16; i++) {
    snprintf(b, 5, "%04X", cpu.stack.stk[i]);
    set_field_buffer(stack[i], 0, b);
    if (i == cpu.stackptr ) {
      set_field_back(stack[i], A_UNDERLINE);
    } else {
      set_field_back(stack[i], A_NORMAL);
    }   
  }

  if (cpu.setSel==0) {
    set_field_back(mode[0], A_UNDERLINE);
    set_field_back(mode[1], A_NORMAL);  
  } else {
    set_field_back(mode[1], A_UNDERLINE);
    set_field_back(mode[0], A_NORMAL);   
  }

  // update mnemonic

  set_field_buffer(mnemonic, 0, cpu.disassembleLine(asciiB, 23, false, cpu.P)); 
  i=0;

  // update trace
  for (auto it=cpu.instructionTrace.begin(); it<cpu.instructionTrace.end(); it++, i++) {
    snprintf(fieldB, 23, "%04X %02X %s", it->address, it->data[0], cpu.disassembleLine(asciiB, 23, false, it->data));
    set_field_buffer(instructionTrace[i], 0, fieldB); 
  }
  i=0;
  for (auto it=cpu.breakpoints.begin(); it<cpu.breakpoints.end(); it++, i++) {
    snprintf(fieldB, 5, "%04X", *it);
    set_field_buffer(breakpoints[i], 0, fieldB); 
  }
  for (; i<8; i++) {
    set_field_buffer(breakpoints[i], 0, "");  
  }

  if (cpu.keyboardLightStatus) {
    set_field_back(keyboardLightField, A_STANDOUT);
  } else {
    set_field_back(keyboardLightField, A_NORMAL);
  }

  if (cpu.displayLightStatus) {
    set_field_back(displayLightField, A_STANDOUT);
  } else {
    set_field_back(displayLightField, A_NORMAL);
  }

  if (cpu.keyboardButtonStatus) {
    set_field_back(keyboardButtonField, A_STANDOUT);
  } else {
    set_field_back(keyboardButtonField, A_NORMAL);
  }  

  if (cpu.displayButtonStatus) {
    set_field_back(displayButtonField, A_STANDOUT);
  } else {
    set_field_back(displayButtonField, A_NORMAL);
  }     
}




FIELD * registerWindow::Form::createAField(std::vector<FIELD *> * fields, int length, int y, int x, const char * str) {
  FIELD * t;
  t = new_field(1, length, y, x, 0, 0); // HEADER - REGISTERS TEXT
  fields->push_back(t);
  set_field_buffer(t, 0, str);
  set_field_opts(t, O_VISIBLE | O_PUBLIC  | O_STATIC);
  return t;
}

FIELD * registerWindow::Form::createAField(std::vector<FIELD *> * fields, int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h) {
  FIELD * t = createAField(fields, length, y, x, str);
  set_field_opts(t, O_VISIBLE | O_PUBLIC  | O_STATIC | f);
  set_field_userptr(t, h);
  if (regexp != NULL) set_field_type(t, TYPE_REGEXP, regexp);
  return t;
}

registerWindow::OctalForm::OctalForm () {
  const char * rName[]={"A:","B:","C:","D:","E:","H:","L:","X:"};
  FIELD **f;
  int i;
  int offset=26;
  createAField(&registerViewFields, 10,2,6, "REGISTERS");
  createAField(&registerViewFields,44,3,31,"STACK    TRACE                    BREAKPOINTS");
  const char * alphaText = "ALPHA";
  mode[0] = createAField(&registerViewFields,strlen(alphaText), 3 , 3, alphaText);
  const char * betaText = "BETA";
  mode[1] = createAField(&registerViewFields,strlen(betaText), 3 , 18, betaText);  

  for (auto regset=0;regset < 2; regset++) {
    for (auto reg=0; reg<8; reg++) {
      regsIdents[regset][reg] = createAField(&registerViewFields,2,4+reg, 3+15*regset,rName[reg]);
      regs[regset][reg]=createAField(&registerViewFields,3,4+reg, 5+15*regset, "000", O_EDIT | O_ACTIVE, "[0-3][0-7][0-7]", JUSTIFY_LEFT, (char *) new octalCharPointerHookExecutor(this, &cpu.regSets[regset].regs[reg]));
    }
    // flags
    createAField(&registerViewFields,2, 13,3+15*regset, "P:" );
    flagParity[regset]=createAField(&registerViewFields,1,13,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new octalCharPointerHookExecutor(this, &cpu.flagParity[regset]));
    createAField(&registerViewFields,2, 13,8+15*regset, "S:" );
    flagSign[regset]=createAField(&registerViewFields,1,13,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new octalCharPointerHookExecutor(this, &cpu.flagSign[regset]));
    createAField(&registerViewFields,2, 14,3+15*regset, "C:" );
    flagCarry[regset]=createAField(&registerViewFields,1,14,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new octalCharPointerHookExecutor(this, &cpu.flagCarry[regset]));
    createAField(&registerViewFields,2, 14,8+15*regset, "Z:" );
    flagZero[regset]=createAField(&registerViewFields,1,14,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new octalCharPointerHookExecutor(this, &cpu.flagZero[regset]));
  }

  createAField(&registerViewFields,2, 16,3, "P:" );
  pc=createAField(&registerViewFields,6,16,5, "000000", O_EDIT | O_ACTIVE, "[0-1][0-7][0-7][0-7][0-7][0-7]", JUSTIFY_LEFT, (char *) new octalShortPointerHookExecutor(this, &cpu.P));
  mnemonic = createAField(&registerViewFields,10, 16,12, "" );
  // BASE REGISTER
  baseIdents = createAField(&registerViewFields,5, 17,3, "BASE:" );
  base=createAField(&registerViewFields,3,17,8, "000", O_EDIT | O_ACTIVE, "[0-3][0-7][0-7]", JUSTIFY_LEFT, (char *) new octalCharPointerHookExecutor(this, &(cpu.memory->baseRegister)));
  // SECTOR TABLE
  sectorTableHeader = createAField(&registerViewFields,13, 20,3, "SECTOR TABLE:" );
  for (int i=0; i<4; i++) {
    for (int j=0; j<4; j++) {
      char buffer[32];
      snprintf(buffer, 32, "%02o:", i*4+j);
      sectorTableFields[i*4+j].ident = createAField(&registerViewFields,3, 21+i,3+j*8, buffer);
      sectorTableFields[i*4+j].accessible = createAField(&registerViewFields,1,21+i,6+j*8, "S", O_EDIT | O_ACTIVE, "[US]", JUSTIFY_LEFT, (char *) new accessibleHookExecutor(this, &(cpu.memory->sectorTable[i*4+j].accessEnable)));
      sectorTableFields[i*4+j].writeable = createAField(&registerViewFields,1,21+i,7+j*8, "R", O_EDIT | O_ACTIVE, "[RW]", JUSTIFY_LEFT, (char *) new writeableHookExecutor(this, &(cpu.memory->sectorTable[i*4+j].writeEnable)));
      sectorTableFields[i*4+j].physicalSector = createAField(&registerViewFields,2,21+i,8+j*8, "00", O_EDIT | O_ACTIVE, "[0-7][0-7]", JUSTIFY_LEFT, (char *) new octalCharPointerHookExecutor(this, &(cpu.memory->sectorTable[i*4+j].physicalPage)));
    }
  }
  // STACK
  for (auto i=0; i<16; i++) {
    stack[i] = createAField(&registerViewFields,6,4+i,31, "000000", O_EDIT | O_ACTIVE, "[0-1][0-7][0-7][0-7][0-7][0-7]", JUSTIFY_LEFT, (char *) new octalShortPointerHookExecutor(this, &cpu.stack.stk[i]));
  }
  
  for (auto line = 0; line < 16; line++) {
    addressFields.push_back(createAField(&registerViewFields,6,line+offset, 3, "000000", O_EDIT | O_ACTIVE, "[0-7][0-7][0-7][0-7][0-7][0-7]", JUSTIFY_LEFT, (char *) new octalMemoryAddressHookExecutor(this, line)));
    for (auto column = 0; column < 16; column++) {
      dataFields.push_back(createAField(&registerViewFields,3,line+offset, 10 + 4 * column, "000", O_EDIT | O_ACTIVE, "[0-3][0-7][0-7]", JUSTIFY_LEFT, (char *) new octalMemoryDataHookExecutor(this, line * 16 + column)));
    }
    asciiFields.push_back(createAField(&registerViewFields,18, line+offset,75,"|................|" ));
  }
  
  const char * displayLightText = "DISPLAY LIGHT";
  displayLightField = createAField(&registerViewFields,strlen(displayLightText), 43 , 4, displayLightText);
  const char * displayButtonText = "DISPLAY BUTTON";
  displayButtonField = createAField(&registerViewFields,strlen(displayButtonText), 44 , 4, displayButtonText);
  const char * keyboardLightText = "KEYBOARD LIGHT";
  keyboardLightField = createAField(&registerViewFields,strlen(keyboardLightText), 43 , 25, keyboardLightText);
  const char * keyboardButtonText = "KEYBOARD BUTTON";
  keyboardButtonField = createAField(&registerViewFields,strlen(keyboardButtonText), 44 , 25, keyboardButtonText);    
  
  for (auto i=0; i<16; i++) {
    instructionTrace[i] = createAField(&registerViewFields,24,4+i,40, "" );
  }

  for (auto i=0; i<8; i++) {
    breakpoints[i] = createAField(&registerViewFields,5,4+i,65, "" );
  }


  

  f = (FIELD **)malloc(
      (sizeof(FIELD *)) *
      (registerViewFields.size()+ 1));
  i = 0;

  for (auto it = registerViewFields.begin(); it < registerViewFields.end(); it++) {
    f[i] = *it;
    i++;
  }
  f[i] = NULL;

  frm = new_form(f);
  set_field_term(frm, form_hook_proxy);  
  set2200Mode(true);
}


registerWindow::HexForm::HexForm () {
  const char * rName[]={"A:","B:","C:","D:","E:","H:","L:","X:"};
  FIELD **f;
  int i;
  int offset=26;
  numRegs = 7;
  createAField(&registerViewFields, 10,2,6, "REGISTERS");
  createAField(&registerViewFields,40,3,31,"STACK    TRACE                BREAKPOINTS");
  const char * alphaText = "ALPHA";
  mode[0] = createAField(&registerViewFields,strlen(alphaText), 3 , 3, alphaText);
  const char * betaText = "BETA";
  mode[1] = createAField(&registerViewFields,strlen(betaText), 3 , 18, betaText);  

  for (auto regset=0;regset < 2; regset++) {
    for (auto reg=0; reg<8; reg++) {
      regsIdents[regset][reg] = createAField(&registerViewFields,2,4+reg, 3+15*regset,rName[reg]);
      regs[regset][reg]=createAField(&registerViewFields,2,4+reg, 5+15*regset, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new hexCharPointerHookExecutor(this, &cpu.regSets[regset].regs[reg]));
    }
    // flags
    createAField(&registerViewFields,2, 13,3+15*regset, "P:" );
    flagParity[regset]=createAField(&registerViewFields,1,13,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new hexCharPointerHookExecutor(this, &cpu.flagParity[regset]));
    createAField(&registerViewFields,2, 13,8+15*regset, "S:" );
    flagSign[regset]=createAField(&registerViewFields,1,13,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new hexCharPointerHookExecutor(this, &cpu.flagSign[regset]));
    createAField(&registerViewFields,2, 14,3+15*regset, "C:" );
    flagCarry[regset]=createAField(&registerViewFields,1,14,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new hexCharPointerHookExecutor(this, &cpu.flagCarry[regset]));
    createAField(&registerViewFields,2, 14,8+15*regset, "Z:" );
    flagZero[regset]=createAField(&registerViewFields,1,14,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new hexCharPointerHookExecutor(this, &cpu.flagZero[regset]));
  }

  createAField(&registerViewFields,2, 16,3, "P:" );
  pc=createAField(&registerViewFields,4,16,5, "0000", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new hexShortPointerHookExecutor(this, &cpu.P));
  mnemonic = createAField(&registerViewFields,10, 16,10, "" );

  // BASE REGISTER
  baseIdents = createAField(&registerViewFields,5, 17,3, "BASE:" );
  base=createAField(&registerViewFields,2,17,8, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new hexCharPointerHookExecutor(this, &(cpu.memory->baseRegister)));
  // SECTOR TABLE
  sectorTableHeader = createAField(&registerViewFields,13, 20,3, "SECTOR TABLE:" );
  for (int i=0; i<4; i++) {
    for (int j=0; j<4; j++) {
      char buffer[32];
      snprintf(buffer, 32, "%01X:", i*4+j);
      sectorTableFields[i*4+j].ident = createAField(&registerViewFields,3, 21+i,3+j*7, buffer);
      sectorTableFields[i*4+j].accessible = createAField(&registerViewFields,1,21+i,5+j*7, "S", O_EDIT | O_ACTIVE, "[US]", JUSTIFY_LEFT, (char *) new accessibleHookExecutor(this, &(cpu.memory->sectorTable[i*4+j].accessEnable)));
      sectorTableFields[i*4+j].writeable = createAField(&registerViewFields,1,21+i,6+j*7, "R", O_EDIT | O_ACTIVE, "[RW]", JUSTIFY_LEFT, (char *) new writeableHookExecutor(this, &(cpu.memory->sectorTable[i*4+j].writeEnable)));
      sectorTableFields[i*4+j].physicalSector = createAField(&registerViewFields,2,21+i,7+j*7, "00", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new hexCharPointerHookExecutor(this, &(cpu.memory->sectorTable[i*4+j].physicalPage)));
    }
  }

  for (auto i=0; i<16; i++) {
    stack[i] = createAField(&registerViewFields,4,4+i,31, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new hexShortPointerHookExecutor(this, &cpu.stack.stk[i]));
  }
  
  for (auto line = 0; line < 16; line++) {
    addressFields.push_back(createAField(&registerViewFields,4,line+offset, 3, "0000", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new hexMemoryAddressHookExecutor(this, line)));
    for (auto column = 0; column < 16; column++) {
      dataFields.push_back(createAField(&registerViewFields,2,line+offset, 8 + 3 * column, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new hexMemoryDataHookExecutor(this, line * 16 + column)));
    }
    asciiFields.push_back(createAField(&registerViewFields,18, line+offset,57,"|................|" ));
  }
  
  const char * displayLightText = "DISPLAY LIGHT";
  displayLightField = createAField(&registerViewFields,strlen(displayLightText), 43 , 4, displayLightText);
  const char * displayButtonText = "DISPLAY BUTTON";
  displayButtonField = createAField(&registerViewFields,strlen(displayButtonText), 44 , 4, displayButtonText);
  const char * keyboardLightText = "KEYBOARD LIGHT";
  keyboardLightField = createAField(&registerViewFields,strlen(keyboardLightText), 43 , 25, keyboardLightText);
  const char * keyboardButtonText = "KEYBOARD BUTTON";
  keyboardButtonField = createAField(&registerViewFields,strlen(keyboardButtonText), 44 , 25, keyboardButtonText);    
  
  for (auto i=0; i<16; i++) {
    instructionTrace[i] = createAField(&registerViewFields,15,4+i,40, "" );
  }

  for (auto i=0; i<8; i++) {
    breakpoints[i] = createAField(&registerViewFields,5,4+i,61, "" );
  }

  f = (FIELD **)malloc(
      (sizeof(FIELD *)) *
      (registerViewFields.size()+ 1));
  i = 0;

  for (auto it = registerViewFields.begin(); it < registerViewFields.end(); it++) {
    f[i] = *it;
    i++;
  }
  f[i] = NULL;

  frm = new_form(f);
  set_field_term(frm, form_hook_proxy);
  set2200Mode(true);
}

registerWindow::HexForm::~HexForm () {
  for (auto it = registerViewFields.begin(); it < registerViewFields.end(); it++) {
    free_field(*it);
  }
}

FORM * registerWindow::HexForm::getForm() {
  return frm;
}

FORM * registerWindow::OctalForm::getForm() {
  return frm;
}

registerWindow::HexForm * registerWindow::createHexForm () {
  return new registerWindow::HexForm();
}

registerWindow::OctalForm * registerWindow::createOctalForm () {
  return new registerWindow::OctalForm();
}

registerWindow::registerWindow(class dp2200_cpu *c) {

  cursorX = 4;  
  cursorY = 1;
  win = newwin(LINES, COLS - 82, 0, 82);
  normalWindow();
  wrefresh(win);
  octal = true;
  activeWindow = false;
  
  formHex = createHexForm ();
  formOctal = createOctalForm ();
  currentForm = formOctal;
  dwinoctal=derwin(win, 48, 95, 1, 1);
  dwinhex=derwin(win, 48, 95, 1, 1);
  set_form_win(formHex->getForm(), win);
  set_form_sub(formHex->getForm(), dwinhex);
  set_form_win(formOctal->getForm(), win);
  set_form_sub(formOctal->getForm(), dwinoctal);
  post_form(currentForm->getForm());
  form_driver(currentForm->getForm(), REQ_OVL_MODE);

  formHex->updateForm(); 
  formOctal->updateForm();
  wmove(win, cursorY, cursorX);
  pos_form_cursor(currentForm->getForm());
  normalWindow();
}
registerWindow::~registerWindow() {
  unpost_form(formHex->getForm());
  unpost_form(formOctal->getForm());
  free_form(formHex->getForm());
  free_form(formOctal->getForm());


}

void registerWindow::updateWindow() {
  if (!activeWindow)  {
    currentForm->updateForm();
    wrefresh(win);
  }
}

void registerWindow::hightlightWindow() {
  curs_set(2);

  wattrset(win, A_STANDOUT);
  box(win, 0, 0);
  mvwprintw(win, 0, 1, "REGISTER WINDOW");
  wattrset(win, 0);
  pos_form_cursor(currentForm->getForm());

  wmove(win, cursorY, cursorX);
  printLog("INFO", "cursorY=%d cursorX=%d\n", cursorY, cursorX);
  refresh();  
  wrefresh(win);
  activeWindow = true;
  pos_form_cursor(currentForm->getForm());
  formHex->updateForm();
  //updateFormOctal();
}
void registerWindow::normalWindow() {
  printLog("INFO", "registerWindow::normalWindow()");
  getyx(win,cursorY,cursorX);
  curs_set(0);
  box(win, 0, 0);
  mvwprintw(win, 0, 1, "REGISTER WINDOW");
  wrefresh(win);
  activeWindow = false;
}
void registerWindow::resetCursor() {
  if (activeWindow) {
  //  curs_set(2);
  //  wmove(win, cursorY, cursorX);
  //  wrefresh(win);
  }
}

WINDOW * registerWindow::getWin() {
  return win;
}


void registerWindow::handleKey(int key) {
  /*
  '?' toggle a help screen for the register window

  */
  

  switch (key) {
  case 27: // ESC
    currentForm->updateForm();
    //updateFormOctal();
    break;
  case 'o':
  case 'O':
    octal = !octal;
    setOctal(octal);
    break;
  case KEY_DOWN:
    form_driver(currentForm->getForm(), REQ_DOWN_FIELD);
    form_driver(currentForm->getForm(), REQ_BEG_FIELD);
    break;

  case KEY_UP:
    form_driver(currentForm->getForm(), REQ_UP_FIELD);
    form_driver(currentForm->getForm(), REQ_BEG_FIELD);
    break;

  case KEY_LEFT:
    form_driver(currentForm->getForm(), REQ_LEFT_FIELD);
    break;

  case KEY_RIGHT:
    form_driver(currentForm->getForm(), REQ_RIGHT_FIELD);
    break;

  // Delete the char before cursor
  case KEY_BACKSPACE:
  case 127:
    form_driver(currentForm->getForm(), REQ_DEL_PREV);
    break;

  // Delete the char under the cursor
  case KEY_DC:
    form_driver(currentForm->getForm(), REQ_DEL_CHAR);
    break;

  case '\n':
    form_driver(currentForm->getForm(), REQ_NEXT_FIELD);
    form_driver(currentForm->getForm(), REQ_PREV_FIELD);
    break;

  default:
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'F') ||
        (key >= 'a' && key <= 'f') || (key == 'r') || (key == 'R') || 
        (key == 'w') || (key == 'W') || (key == 'U') || (key == 'u') ||
        (key == 'S') || (key == 's')) {
      form_driver(currentForm->getForm(), toupper(key));
    }
    break;
  }

  wrefresh(win);
}

int registerWindow::setDisplayLight(bool value) {
  cpu.displayLightStatus=value;
  return 0; 
}
int registerWindow::setKeyboardLight(bool value) {
  cpu.keyboardLightStatus=value;
  return 0;
}

int registerWindow::setKeyboardButton(bool value) {
  cpu.keyboardButtonStatus = value;
  return 0;
}

int registerWindow::setDisplayButton(bool value) {
  cpu.displayButtonStatus = value;
  return 0;
}

bool registerWindow::getDisplayButton() {
  return cpu.displayButtonStatus;
}

bool registerWindow::getKeyboardButton() {
  return cpu.keyboardButtonStatus;
}

void registerWindow::setOctal(bool octal) {
  if (octal) {
    unpost_form(currentForm->getForm());
    currentForm = formOctal;
    post_form(currentForm->getForm());
    form_driver(currentForm->getForm(), REQ_OVL_MODE);
  } else {
    unpost_form(currentForm->getForm());
    currentForm = formHex;
    post_form(currentForm->getForm());
    form_driver(currentForm->getForm(), REQ_OVL_MODE);
  }
  currentForm->updateForm();
  refresh(); 
  wrefresh(win); 
}

void registerWindow::resize() {
  wresize(win, LINES, COLS - 82); 
  if (activeWindow) {
    hightlightWindow();
  } else {
    normalWindow();
  }
  currentForm->updateForm();
  wrefresh(win); 
  wrefresh(dwinoctal);
  wrefresh(dwinhex);
  refresh();
}

void registerWindow::set2200Mode (bool b) {
  formHex->set2200Mode(b);
  formOctal->set2200Mode(b);
}