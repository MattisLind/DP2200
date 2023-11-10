#include "dp2200Window.h"
#include <form.h>
#include <ncurses.h>

dp2200Window::dp2200Window(class dp2200_cpu * c) {
  cpu = c;
  cursorX = 0;
  cursorY = 0;
  win = newwin(14, 82, 0, 0);
  innerWin = newwin(12, 80, 1, 1);
  normalWindow();
  wrefresh(win);
  activeWindow = false;
  scrollok(innerWin, TRUE);
}
void dp2200Window::hightlightWindow() {
  wattrset(win, A_STANDOUT);
  box(win, 0, 0);
  mvwprintw(win, 0, 1, "DATAPOINT 2200 SCREEN");
  wattrset(win, 0);
  wmove(innerWin, cursorY, cursorX);
  if (cursorEnabled) curs_set(2);
  else curs_set(0);
  wrefresh(win);
  redrawwin(innerWin);
  wrefresh(innerWin);
  activeWindow = true;
}
void dp2200Window::normalWindow() {
  curs_set(0);
  box(win, 0, 0);
  mvwprintw(win, 0, 1, "DATAPOINT 2200 SCREEN");
  wrefresh(win);
  redrawwin(innerWin);
  wrefresh(innerWin);
  activeWindow = false;
}
void dp2200Window::handleKey(int key) {
  switch (key) {
  case KEY_F(5):
    rw->setKeyboardButton(!rw->getKeyboardButton());
    break;
  case KEY_F(6):
    rw->setDisplayButton(!rw->getDisplayButton());
    break;
  case 0x0a:
    cpu->ioCtrl->screenKeyboardDevice->updateKbd(0x0d);
    break;
  case 0x1b:
    cpu->ioCtrl->screenKeyboardDevice->updateKbd(0x30);
    break;
  case 0060:
  case 0061:
  case 0062:
  case 0063:
  case 0064:
  case 0065:
  case 0066:
  case 0067:
  case 0070:
  case 0071:
  case 0040:
  case 0041:
  case 0042:
  case 0043:
  case 0044:
  case 0045:
  case 0046:
  case 0047:
  case 0050:
  case 0051:
  case 0052:
  case 0053:
  case 0054:
  case 0055:
  case 0056:
  case 0057:
  case 0072:
  case 0073:
  case 0074:
  case 0075:
  case 0076:
  case 0077:
  case 0133:
  case 0176:
  case 0135:
  case 0136:
  case 0137:
  case 0100:
  case 0173:
  case 0134:
  case 0140:
  case 0174:
  case 0175:
    cpu->ioCtrl->screenKeyboardDevice->updateKbd(key);
    break;
  case 0101:
  case 0102:
  case 0103:
  case 0104:
  case 0105:
  case 0106:
  case 0107:
  case 0110:
  case 0111:
  case 0112:
  case 0113:
  case 0114:
  case 0115:
  case 0116:
  case 0117:
  case 0120:
  case 0121:
  case 0122:
  case 0123:
  case 0124:
  case 0125:
  case 0126:
  case 0127:
  case 0130:
  case 0131:
  case 0132:
    key |= 0x20;
    cpu->ioCtrl->screenKeyboardDevice->updateKbd(key);
    break;
  case 0141:
  case 0142:
  case 0143:
  case 0144:
  case 0145:
  case 0146:
  case 0147:
  case 0150:
  case 0151:
  case 0152:
  case 0153:
  case 0154:
  case 0155:
  case 0156:
  case 0157:
  case 0160:
  case 0161:
  case 0162:
  case 0163:
  case 0164:
  case 0165:
  case 0166:
  case 0167:
  case 0170:
  case 0171:
  case 0172:
    key &= ~0x20;
    cpu->ioCtrl->screenKeyboardDevice->updateKbd(key);
    break;  
  default:
    printLog("INFO", "Unhandled key=%c %d %02X %03o\n", key, key, key, key);
    break;
  }


}
void dp2200Window::resetCursor() {
  if (activeWindow) {
    if (cursorEnabled) curs_set(2);
    wmove(innerWin, cursorY, cursorX);
    wrefresh(innerWin);
  }
}
int dp2200Window::eraseFromCursorToEndOfFrame() {
  printLog("INFO", "Erasing from X=%d, Y=%d to end of frame\n", cursorX, cursorY);
  for (int i=cursorX; i<80;i++) {
    waddch(innerWin, ' ');
  }
  for (int i=cursorY+1; i <12; i++) {
    wmove(innerWin, i, 1);
    for (int j=0; j<80; j++) {
      waddch(innerWin, ' ');
    }
  }
  wmove(innerWin, cursorY, cursorX);
  wrefresh(innerWin);
  return 0;
}
int dp2200Window::eraseFromCursorToEndOfLine() {
  printLog("INFO", "Erasing from X=%d, Y=%d to end of line\n", cursorX, cursorY);
  for (int i=cursorX; i<80;i++) {
    waddch(innerWin, ' ');
  }
  wmove(innerWin, cursorY, cursorX);
  wrefresh(innerWin);
  return 0;
}
int dp2200Window::rollScreenOneLine() {
  printLog("INFO", "Roll one line\n");
  wmove(innerWin, 12, 0);
  waddch(innerWin, '\n');
  wmove(innerWin, cursorY, cursorX);
  wrefresh(innerWin);
  return 0;
}
int dp2200Window::showCursor(bool value) {
  cursorEnabled=value;
  printLog("INFO", "Setting cursor status = %d \n", cursorEnabled);
  return 0;
}
int dp2200Window::setCursorX(int value) {
  cursorX = value;
  printLog("INFO", "Setting Cursor X X=%d Y=%d\n", cursorX, cursorY);
  return 0;
}

int dp2200Window::setCursorY(int value) {
  cursorY = value;
  printLog("INFO", "Setting Cursor Y X=%d Y=%d\n", cursorX, cursorY);
  return 0;
}

int dp2200Window::writeCharacter(int value) {
  printLog("INFO", "Writing char=%c to screen\n", value);
  wmove(innerWin, cursorY, cursorX);
  waddch(innerWin, value);
  wrefresh(innerWin);
  return 0;
}
