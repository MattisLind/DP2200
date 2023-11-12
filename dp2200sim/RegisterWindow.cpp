#include "RegisterWindow.h"

registerWindow::memoryDataHookExecutor::memoryDataHookExecutor(class registerWindow * r, int d) {
  data=d;
  rw = r;
}

registerWindow::memoryAddressHookExecutor::memoryAddressHookExecutor(class registerWindow * r, int a) { 
  address = a; 
  rw = r;
}

registerWindow::charPointerHookExecutor::charPointerHookExecutor(class registerWindow * r, unsigned char * a) { 
  address = a; 
  rw = r;
}

registerWindow::shortPointerHookExecutor::shortPointerHookExecutor(class registerWindow * r, unsigned short * a) { 
  address = a; 
  rw = r;
}

void registerWindow::updateForm(int startAddress) {
  int i, k = 0, j;
  char b[5];
  unsigned char t;
  char asciiB[17], fieldB[19];
  for (i = 0; i < 16 && startAddress < 0x4000; startAddress += 16, i++) {
    snprintf(b, 5, "%04X", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (j = 0; j < 16; j++) {
      t = cpu->memory[startAddress + j];
      if ((startAddress + j) == cpu->P ) {
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
      t = cpu->memory[startAddress + j];
      if ((startAddress + j) == cpu->P ) {
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
      auto r = cpu->regSets[regset].regs[i];
      snprintf(b, 3, "%02X", r);
      set_field_buffer(f, 0, b);
    }
    snprintf(b, 3, "%01X", cpu->flagCarry[regset]);
    set_field_buffer(flagCarry[regset], 0, b);
    snprintf(b, 3, "%01X", cpu->flagZero[regset]);
    set_field_buffer(flagZero[regset], 0, b);
    snprintf(b, 3, "%01X", cpu->flagParity[regset]);
    set_field_buffer(flagParity[regset], 0, b);
    snprintf(b, 3, "%01X", cpu->flagSign[regset]);
    set_field_buffer(flagSign[regset], 0, b);                  
  }
  snprintf(b, 5, "%04X", cpu->P);
  set_field_buffer(pc, 0, b);

  for (auto i = 0; i<16; i++) {
    snprintf(b, 5, "%04X", cpu->stack.stk[i]);
    set_field_buffer(stack[i], 0, b);
    if (i == cpu->stackptr ) {
      set_field_back(stack[i], A_UNDERLINE);
    } else {
      set_field_back(stack[i], A_NORMAL);
    }   
  }

  // update mnemonic

  set_field_buffer(mnemonic, 0, cpu->disassembleLine(asciiB, 16, false, cpu->P)); 
  i=0;

  // update trace
  for (auto it=cpu->instructionTrace.begin(); it<cpu->instructionTrace.end(); it++, i++) {
    snprintf(fieldB, 18, "%04X %s", it->address, cpu->disassembleLine(asciiB, 16, false, it->data));
    set_field_buffer(instructionTrace[i], 0, fieldB); 
  }
  i=0;
  for (auto it=cpu->breakpoints.begin(); it<cpu->breakpoints.end(); it++, i++) {
    snprintf(fieldB, 5, "%04X", *it);
    set_field_buffer(breakpoints[i], 0, fieldB); 
  }
  for (; i<8; i++) {
    set_field_buffer(breakpoints[i], 0, "");  
  }

  if (keyboardLightStatus) {
    set_field_back(keyboardLightField, A_STANDOUT);
  } else {
    set_field_back(keyboardLightField, A_NORMAL);
  }

  if (displayLightStatus) {
    set_field_back(displayLightField, A_STANDOUT);
  } else {
    set_field_back(displayLightField, A_NORMAL);
  }

  if (keyboardButtonStatus) {
    set_field_back(keyboardButtonField, A_STANDOUT);
  } else {
    set_field_back(keyboardButtonField, A_NORMAL);
  }  

  if (displayButtonStatus) {
    set_field_back(displayButtonField, A_STANDOUT);
  } else {
    set_field_back(displayButtonField, A_NORMAL);
  }    

}


FIELD * registerWindow::createAField(int length, int y, int x, const char * str) {
  FIELD * t;
  t = new_field(1, length, y, x, 0, 0); // HEADER - REGISTERS TEXT
  registerViewFields.push_back(t);
  set_field_buffer(t, 0, str);
  set_field_opts(t, O_VISIBLE | O_PUBLIC  | O_STATIC);
  return t;
}

FIELD * registerWindow::createAField(int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h) {
  FIELD * t = createAField(length, y, x, str);
  set_field_opts(t, O_VISIBLE | O_PUBLIC  | O_STATIC | f);
  set_field_userptr(t, h);
  if (regexp != NULL) set_field_type(t, TYPE_REGEXP, regexp);
  return t;
}

FORM * registerWindow::createHexForm () {
  const char * rName[]={"A:","B:","C:","D:","E:","H:","L:"};
  FIELD **f;
  int i;
  int offset=21;
  FORM *frm;
  createAField(10,2,6, "REGISTERS");
  createAField(65,3,3,"ALPHA          BETA          STACK    TRACE          BREAKPOINTS");

  for (auto regset=0;regset < 2; regset++) {
    for (auto reg=0; reg<7; reg++) {
      createAField(2,4+reg, 3+15*regset,rName[reg]);
      regs[regset][reg]=createAField(2,4+reg, 5+15*regset, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new charPointerHookExecutor(this, &cpu->regSets[regset].regs[reg]));
    }
    // flags
    createAField(2, 12,3+15*regset, "P:" );
    flagParity[regset]=createAField(1,12,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagParity[regset]));
    createAField(2, 12,8+15*regset, "S:" );
    flagSign[regset]=createAField(1,12,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagSign[regset]));
    createAField(2, 13,3+15*regset, "C:" );
    flagCarry[regset]=createAField(1,13,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagCarry[regset]));
    createAField(2, 13,8+15*regset, "Z:" );
    flagZero[regset]=createAField(1,13,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagZero[regset]));
  }

  createAField(2, 15,3, "P:" );
  pc=createAField(4,15,5, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new shortPointerHookExecutor(this, &cpu->P));
  mnemonic = createAField(10, 15,10, "" );

  for (auto i=0; i<16; i++) {
    stack[i] = createAField(4,4+i,32, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new shortPointerHookExecutor(this, &cpu->stack.stk[i]));
  }
  
  for (auto line = 0; line < 16; line++) {
    addressFields.push_back(createAField(4,line+offset, 3, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new memoryAddressHookExecutor(this, line)));
    for (auto column = 0; column < 16; column++) {
      dataFields.push_back(createAField(2,line+offset, 8 + 3 * column, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new memoryDataHookExecutor(this, line * 16 + column)));
    }
    asciiFields.push_back(createAField(18, line+offset,57,"|................|" ));
  }
  
  const char * displayLightText = "DISPLAY LIGHT";
  displayLightField = createAField(strlen(displayLightText), 39 , 4, displayLightText);
  const char * displayButtonText = "DISPLAY BUTTON";
  displayButtonField = createAField(strlen(displayButtonText), 40 , 4, displayButtonText);
  const char * keyboardLightText = "KEYBOARD LIGHT";
  keyboardLightField = createAField(strlen(keyboardLightText), 39 , 25, keyboardLightText);
  const char * keyboardButtonText = "KEYBOARD BUTTON";
  keyboardButtonField = createAField(strlen(keyboardButtonText), 40 , 25, keyboardButtonText);    
  
  for (auto i=0; i<8; i++) {
    instructionTrace[i] = createAField(15,4+i,41, "" );
  }

  for (auto i=0; i<8; i++) {
    breakpoints[i] = createAField(5,4+i,56, "" );
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
  form_driver(frm, REQ_OVL_MODE);
  return frm;
}

registerWindow::registerWindow(class dp2200_cpu *c) {

  cursorX = 4;  
  cursorY = 1;
  win = newwin(LINES, COLS - 82, 0, 82);
  normalWindow();
  wrefresh(win);
  cpu = c;
  octal = false;
  activeWindow = false;
  
  form = createHexForm ();

  set_form_win(form, win);
  set_form_sub(form, derwin(win, 44, 76, 1, 1));
  post_form(form);

  updateForm(base);  
  wmove(win, cursorY, cursorX);
  pos_form_cursor(form);
  normalWindow();
}
registerWindow::~registerWindow() {
  unpost_form(form);
  free_form(form);
  for (auto it = registerViewFields.begin(); it < registerViewFields.end(); it++) {
    free_field(*it);
  }
}

void registerWindow::updateWindow() {
  if (!activeWindow)  {
    updateForm(base);
    wrefresh(win);
  }
}

void registerWindow::hightlightWindow() {
  curs_set(2);

  wattrset(win, A_STANDOUT);
  box(win, 0, 0);
  mvwprintw(win, 0, 1, "REGISTER WINDOW");
  wattrset(win, 0);
  pos_form_cursor(form);
  wmove(win, cursorY, cursorX);
  printLog("INFO", "cursorY=%d cursorX=%d\n", cursorY, cursorX);
  refresh();  
  wrefresh(win);
  activeWindow = true;
  pos_form_cursor(form);
  updateForm(base);
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
void registerWindow::handleKey(int key) {
  /*
  '?' toggle a help screen for the register window

  */
  

  switch (key) {
  case 27: // ESC
    updateForm(base);
    break;
  case KEY_DOWN:
    form_driver(form, REQ_DOWN_FIELD);
    form_driver(form, REQ_BEG_FIELD);
    break;

  case KEY_UP:
    form_driver(form, REQ_UP_FIELD);
    form_driver(form, REQ_BEG_FIELD);
    break;

  case KEY_LEFT:
    form_driver(form, REQ_LEFT_FIELD);
    break;

  case KEY_RIGHT:
    form_driver(form, REQ_RIGHT_FIELD);
    break;

  // Delete the char before cursor
  case KEY_BACKSPACE:
  case 127:
    form_driver(form, REQ_DEL_PREV);
    break;

  // Delete the char under the cursor
  case KEY_DC:
    form_driver(form, REQ_DEL_CHAR);
    break;

  case '\n':
    form_driver(form, REQ_NEXT_FIELD);
    form_driver(form, REQ_PREV_FIELD);
    break;

  default:
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'F') ||
        (key >= 'a' && key <= 'f')) {
      form_driver(form, toupper(key));
    }
    break;
  }

  wrefresh(win);
}

int registerWindow::setDisplayLight(bool value) {
  displayLightStatus=value;
  return 0; 
}
int registerWindow::setKeyboardLight(bool value) {
  keyboardLightStatus=value;
  return 0;
}

int registerWindow::setKeyboardButton(bool value) {
  keyboardButtonStatus = value;
  return 0;
}

int registerWindow::setDisplayButton(bool value) {
  displayButtonStatus = value;
  return 0;
}

bool registerWindow::getDisplayButton() {
  return displayButtonStatus;
}

bool registerWindow::getKeyboardButton() {
  return keyboardButtonStatus;
}

