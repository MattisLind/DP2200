#include "RegisterWindow.h"

registerWindow::Form::memoryDataHookExecutor::memoryDataHookExecutor(class registerWindow::Form * r, int d) {
  data=d;
  rwf = r;
}

registerWindow::Form::memoryAddressHookExecutor::memoryAddressHookExecutor(class registerWindow::Form * r, int a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::charPointerHookExecutor::charPointerHookExecutor(class registerWindow::Form * r, unsigned char * a) { 
  address = a; 
  rwf = r;
}

registerWindow::Form::shortPointerHookExecutor::shortPointerHookExecutor(class registerWindow::Form * r, unsigned short * a) { 
  address = a; 
  rwf = r;
}

void registerWindow::OctalForm::updateForm() {
}

void registerWindow::HexForm::updateForm() {
  int i, k = 0, j;
  char b[5];
  unsigned char t;
  unsigned short startAddress = cpu.startAddress;
  char asciiB[17], fieldB[19];
  for (i = 0; i < 16 && startAddress < 0x4000; startAddress += 16, i++) {
    snprintf(b, 5, "%04X", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (j = 0; j < 16; j++) {
      t = cpu.memory[startAddress + j];
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
  for (; i < 16; startAddress += 16, i++) {
    snprintf(b, 5, "%04X", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (int j = 0; j < 16; j++) {
      t = cpu.memory[startAddress + j];
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
    for (auto i=0; i<7; i++) {
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

  // update mnemonic

  set_field_buffer(mnemonic, 0, cpu.disassembleLine(asciiB, 16, false, cpu.P)); 
  i=0;

  // update trace
  for (auto it=cpu.instructionTrace.begin(); it<cpu.instructionTrace.end(); it++, i++) {
    snprintf(fieldB, 18, "%04X %s", it->address, cpu.disassembleLine(asciiB, 16, false, it->data));
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

registerWindow::OctalForm::OctalForm () {}


registerWindow::HexForm::HexForm () {
  const char * rName[]={"A:","B:","C:","D:","E:","H:","L:"};
  FIELD **f;
  int i;
  int offset=21;

  createAField(&registerViewFields, 10,2,6, "REGISTERS");
  createAField(&registerViewFields,65,3,3,"ALPHA          BETA          STACK    TRACE          BREAKPOINTS");

  for (auto regset=0;regset < 2; regset++) {
    for (auto reg=0; reg<7; reg++) {
      createAField(&registerViewFields,2,4+reg, 3+15*regset,rName[reg]);
      regs[regset][reg]=createAField(&registerViewFields,2,4+reg, 5+15*regset, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new charPointerHookExecutor(this, &cpu.regSets[regset].regs[reg]));
    }
    // flags
    createAField(&registerViewFields,2, 12,3+15*regset, "P:" );
    flagParity[regset]=createAField(&registerViewFields,1,12,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu.flagParity[regset]));
    createAField(&registerViewFields,2, 12,8+15*regset, "S:" );
    flagSign[regset]=createAField(&registerViewFields,1,12,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu.flagSign[regset]));
    createAField(&registerViewFields,2, 13,3+15*regset, "C:" );
    flagCarry[regset]=createAField(&registerViewFields,1,13,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu.flagCarry[regset]));
    createAField(&registerViewFields,2, 13,8+15*regset, "Z:" );
    flagZero[regset]=createAField(&registerViewFields,1,13,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu.flagZero[regset]));
  }

  createAField(&registerViewFields,2, 15,3, "P:" );
  pc=createAField(&registerViewFields,4,15,5, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new shortPointerHookExecutor(this, &cpu.P));
  mnemonic = createAField(&registerViewFields,10, 15,10, "" );

  for (auto i=0; i<16; i++) {
    stack[i] = createAField(&registerViewFields,4,4+i,32, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new shortPointerHookExecutor(this, &cpu.stack.stk[i]));
  }
  
  for (auto line = 0; line < 16; line++) {
    addressFields.push_back(createAField(&registerViewFields,4,line+offset, 3, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new memoryAddressHookExecutor(this, line)));
    for (auto column = 0; column < 16; column++) {
      dataFields.push_back(createAField(&registerViewFields,2,line+offset, 8 + 3 * column, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new memoryDataHookExecutor(this, line * 16 + column)));
    }
    asciiFields.push_back(createAField(&registerViewFields,18, line+offset,57,"|................|" ));
  }
  
  const char * displayLightText = "DISPLAY LIGHT";
  displayLightField = createAField(&registerViewFields,strlen(displayLightText), 39 , 4, displayLightText);
  const char * displayButtonText = "DISPLAY BUTTON";
  displayButtonField = createAField(&registerViewFields,strlen(displayButtonText), 40 , 4, displayButtonText);
  const char * keyboardLightText = "KEYBOARD LIGHT";
  keyboardLightField = createAField(&registerViewFields,strlen(keyboardLightText), 39 , 25, keyboardLightText);
  const char * keyboardButtonText = "KEYBOARD BUTTON";
  keyboardButtonField = createAField(&registerViewFields,strlen(keyboardButtonText), 40 , 25, keyboardButtonText);    
  
  for (auto i=0; i<8; i++) {
    instructionTrace[i] = createAField(&registerViewFields,15,4+i,41, "" );
  }

  for (auto i=0; i<8; i++) {
    breakpoints[i] = createAField(&registerViewFields,5,4+i,56, "" );
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
  //return frm;
}

registerWindow::HexForm::~HexForm () {
  for (auto it = registerViewFields.begin(); it < registerViewFields.end(); it++) {
    free_field(*it);
  }
}

FORM * registerWindow::HexForm::getForm() {
  return frm;
}

registerWindow::HexForm * registerWindow::createHexForm () {
  return new registerWindow::HexForm();
}

registerWindow::registerWindow(class dp2200_cpu *c) {

  cursorX = 4;  
  cursorY = 1;
  win = newwin(LINES, COLS - 82, 0, 82);
  normalWindow();
  wrefresh(win);
  octal = false;
  activeWindow = false;
  
  formHex = createHexForm ();
  //formOctal = createHexForm ();


  set_form_win(formHex->getForm(), win);
  set_form_sub(formHex->getForm(), derwin(win, 44, 76, 1, 1));
  //set_form_win(formOctal, win);
  //set_form_sub(formOctal, derwin(win, 44, 76, 1, 1));
  post_form(formHex->getForm());
  form_driver(formHex->getForm(), REQ_OVL_MODE);

  formHex->updateForm(); 
  //updateFormOctal(); 
  wmove(win, cursorY, cursorX);
  pos_form_cursor(formHex->getForm());
  normalWindow();
}
registerWindow::~registerWindow() {
  unpost_form(formHex->getForm());
  free_form(formHex->getForm());
}

void registerWindow::updateWindow() {
  if (!activeWindow)  {
    formHex->updateForm();
    //updateFormOctal();
    wrefresh(win);
  }
}

void registerWindow::hightlightWindow() {
  curs_set(2);

  wattrset(win, A_STANDOUT);
  box(win, 0, 0);
  mvwprintw(win, 0, 1, "REGISTER WINDOW");
  wattrset(win, 0);
  pos_form_cursor(formHex->getForm());
  wmove(win, cursorY, cursorX);
  printLog("INFO", "cursorY=%d cursorX=%d\n", cursorY, cursorX);
  refresh();  
  wrefresh(win);
  activeWindow = true;
  pos_form_cursor(formHex->getForm());
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
    formHex->updateForm();
    //updateFormOctal();
    break;
  case 'o':
  case 'O':
    octal = !octal;
    if (octal) {
      unpost_form(formHex->getForm());
      //post_form(formOctal);
    } else {
      //unpost_form(formOctal);
      post_form(formHex->getForm());
    }
    formHex->updateForm();
    //updateFormOctal();
    refresh(); 
    wrefresh(win);
    break;
  case KEY_DOWN:
    form_driver(formHex->getForm(), REQ_DOWN_FIELD);
    form_driver(formHex->getForm(), REQ_BEG_FIELD);
    break;

  case KEY_UP:
    form_driver(formHex->getForm(), REQ_UP_FIELD);
    form_driver(formHex->getForm(), REQ_BEG_FIELD);
    break;

  case KEY_LEFT:
    form_driver(formHex->getForm(), REQ_LEFT_FIELD);
    break;

  case KEY_RIGHT:
    form_driver(formHex->getForm(), REQ_RIGHT_FIELD);
    break;

  // Delete the char before cursor
  case KEY_BACKSPACE:
  case 127:
    form_driver(formHex->getForm(), REQ_DEL_PREV);
    break;

  // Delete the char under the cursor
  case KEY_DC:
    form_driver(formHex->getForm(), REQ_DEL_CHAR);
    break;

  case '\n':
    form_driver(formHex->getForm(), REQ_NEXT_FIELD);
    form_driver(formHex->getForm(), REQ_PREV_FIELD);
    break;

  default:
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'F') ||
        (key >= 'a' && key <= 'f')) {
      form_driver(formHex->getForm(), toupper(key));
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

