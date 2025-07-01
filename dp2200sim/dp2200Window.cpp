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
  SDL_Event evt;
  screenDirty = false;
  scrollok(innerWin, TRUE);
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    SDL_Log("SDL_Init fel: %s", SDL_GetError());
    exit(1);
  }
  printLog("INFO", "SDL_init\n");
  // Skapa fönster
  sdlwin = SDL_CreateWindow("Datapoint 5500 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
  if (!sdlwin)
  {
    SDL_Log("SDL_CreateWindow fel: %s", SDL_GetError());
    SDL_Quit();
    exit(1);
  }
  printLog("INFO", "SDL_CreateWindow\n");
  // Skapa renderer
  ren = SDL_CreateRenderer(sdlwin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren)
  {
    SDL_Log("SDL_CreateRenderer fel: %s", SDL_GetError());
    SDL_DestroyWindow(sdlwin);
    SDL_Quit();
    exit(1);
  }
  while (SDL_PollEvent(&evt));
  printLog("INFO", "SDL_CreateRenderer\n");
}

dp2200Window::~dp2200Window() {
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(sdlwin);
  SDL_Quit();
}

void dp2200Window::resize() {
  if (activeWindow) {
    hightlightWindow();
  } else {
    normalWindow();
  }
  wrefresh(win);
  redrawwin(innerWin);
  wrefresh(innerWin);  
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
  case 0x7f:
    printLog("INFO", "Got BS\n");
    cpu->ioCtrl->screenKeyboardDevice->updateKbd(0x08);
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
    screen[i][cursorY]=' ';
  }
  for (int i=cursorY+1; i <12; i++) {
    wmove(innerWin, i, 1);
    for (int j=0; j<80; j++) {
      waddch(innerWin, ' ');
      screen[j][i]=' ';
    }
  }
  wmove(innerWin, cursorY, cursorX);
  wrefresh(innerWin);
  screenDirty = true;
  return 0;
}
int dp2200Window::eraseFromCursorToEndOfLine() {
  printLog("INFO", "Erasing from X=%d, Y=%d to end of line\n", cursorX, cursorY);
  for (int i=cursorX; i<80;i++) {
    waddch(innerWin, ' ');
    screen[i][cursorY]=' ';
  }
  wmove(innerWin, cursorY, cursorX);
  wrefresh(innerWin);
  screenDirty = true;
  return 0;
}
int dp2200Window::rollScreenOneLine() {
  printLog("INFO", "Roll one line\n");
  wmove(innerWin, 12, 0);
  waddch(innerWin, '\n');
  wmove(innerWin, cursorY, cursorX);
  wrefresh(innerWin);
  screenDirty = true;
  return 0;
}
int dp2200Window::showCursor(bool value) {
  cursorEnabled=value;
  printLog("INFO", "Setting cursor status = %d \n", cursorEnabled);
  return 0;
}
int dp2200Window::setCursorX(int value) {
  if (value >= 80 || value < 0) {
    printLog("INFO", "setCursorX: Value is outside limits : %d\n", value);
    return 0;
  }
  cursorX = value;
  printLog("INFO", "Setting Cursor X X=%d Y=%d\n", cursorX, cursorY);
  screenDirty = true;
  return 0;
}

int dp2200Window::setCursorY(int value) {
  if (value >= 12 || value < 0) {
    printLog("INFO", "setCursorY: Value is outside limits : %d\n", value);
    return 0;
  }
  cursorY = value;
  printLog("INFO", "Setting Cursor Y X=%d Y=%d\n", cursorX, cursorY);
  screenDirty = true;
  return 0;
}

int dp2200Window::writeCharacter(int value) {
  printLog("INFO", "Writing char=%c to screen\n", value);
  wmove(innerWin, cursorY, cursorX);
  screen[cursorX][cursorY]=value;
  waddch(innerWin, value);
  wrefresh(innerWin);
  screenDirty = true;
  return 0;
}

int dp2200Window::scrollDown() {
  int i,j;
  wscrl(innerWin, -1);
  for (i=0; i < 80; i++) screen[i][0] = ' ';
  for (j=1; j < 12; j++) {
    for (i=0; i < 80; i++) {
      screen[i][j] = screen[i][j-1];
    }
  }  
  screenDirty = true;
  return 0;
}

int dp2200Window::scrollUp() {
  int i,j;
  wscrl(innerWin, 1);
  for (i=0; i < 80; i++) screen[i][11] = ' ';
  for (j=0; j < 11; j++) {
    for (i=0; i < 80; i++) {
      screen[i][j] = screen[i][j+1];
    }
  }  
  screenDirty = true;
  return 0;
}

void dp2200Window::incrementXPos() {
  cursorX++; 
}


void dp2200Window::setCharGenChar(int data) {
  lastCharGenChar=data & 0177;
  charGenIndex=0;
}

void dp2200Window::updateCharGen(int data) {
  font5x7[lastCharGenChar][charGenIndex] = 0177 & data;
  printLog("INFO", "CHARGEN:  %04o %01o: %04o \n", lastCharGenChar, charGenIndex, 0177 & data);
  charGenIndex++;
  if (charGenIndex==5) {
    charGenIndex = 0;
    lastCharGenChar = 0177 & (lastCharGenChar+1);
  }
}

void dp2200Window::drawChar(int c, int cx, int cy) {
  // pixel-offset för övre vänstra hörnet (global padding + 1px per-cell padding)
  int x0 = PADDING + cx * CELL_W + 1;
  int y0 = PADDING + cy * CELL_H + 1;

  for (int row = 0; row < 5; ++row)
  {
    uint8_t bits = font5x7[c][row];
    for (int col = 0; col < 7; ++col) {
      if (bits & (1 << (6 - col))) {
        // Draw a pixel
        SDL_RenderDrawPoint(ren, x0 + row, y0 + col);
      }
    }
  }
}

void dp2200Window::updateScreen() {
  //printLog("INFO", "updateScreen ENTRY\n");
  SDL_Event evt;
  if (!screenDirty) return;
  // pump the event queue so the window actually appears

  SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(ren);

  // Set render color (green)
  SDL_SetRenderDrawColor(ren, 0, 255, 0, SDL_ALPHA_OPAQUE);
  // Draw the screen
  for (int row = 0; row < CHARS_H; ++row) {
    for (int col = 0; col < CHARS_W; ++col) {
      drawChar(screen[col][row], col, row);
    }
  }

  // Paint
  SDL_RenderPresent(ren);
  while (SDL_PollEvent(&evt));
  screenDirty = false;
  //printLog("INFO", "updateScreen EXIT\n");
}