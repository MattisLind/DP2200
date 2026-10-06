#include "dp2200Window.h"
#include <form.h>
#include <ncurses.h>
#include <cstring>


dp2200Window::dp2200Window(class dp2200_cpu * c) {
  cpu = c;
  cursorX = 0;
  cursorY = 0;
  win = newwin(14, 82, 0, 0);
  innerWin = newwin(12, 80, 1, 1);
  normalWindow();
  wrefresh(win);
  activeWindow = false;
  screenDirty = false;
  // Scrolling is controlled by EX COM1, never by curses writing the last cell.
  scrollok(innerWin, FALSE);
  memset(screen, ' ', sizeof screen);
#if DP2200_WITH_SDL
  SDL_Event evt;
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    SDL_Log("SDL_Init fel: %s", SDL_GetError());
    exit(1);
  }
  printLog("INFO", "SDL_init\n");
  // Skapa fönster
  sdlwin = SDL_CreateWindow("Datapoint 5500 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W*2, WINDOW_H*2, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!sdlwin)
  {
    SDL_Log("SDL_CreateWindow fel: %s", SDL_GetError());
    SDL_Quit();
    exit(1);
  }
  printLog("INFO", "SDL_CreateWindow\n");
  // Skapa renderer
  ren = SDL_CreateRenderer(sdlwin, -1, SDL_RENDERER_ACCELERATED);
  if (!ren) ren=SDL_CreateRenderer(sdlwin,-1,SDL_RENDERER_SOFTWARE);
  if (!ren)
  {
    SDL_Log("SDL_CreateRenderer fel: %s", SDL_GetError());
    SDL_DestroyWindow(sdlwin);
    SDL_Quit();
    exit(1);
  }
  SDL_RenderSetLogicalSize(ren,WINDOW_W,WINDOW_H);
  SDL_RenderSetIntegerScale(ren,SDL_TRUE);
  SDL_StartTextInput();
  screenDirty=true;
  while (SDL_PollEvent(&evt));
  printLog("INFO", "SDL_CreateRenderer\n");
#endif
}

dp2200Window::~dp2200Window() {
#if DP2200_WITH_SDL
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(sdlwin);
  SDL_Quit();
#endif
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
  if (cursorInBounds()) wmove(innerWin, cursorY, cursorX);
  if (cursorEnabled && cursorInBounds()) curs_set(2);
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
  case KEY_BACKSPACE:
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
    curs_set(cursorEnabled && cursorInBounds() ? 2 : 0);
    if (cursorInBounds()) wmove(innerWin, cursorY, cursorX);
    wrefresh(innerWin);
  }
}
int dp2200Window::eraseFromCursorToEndOfFrame() {
  printLog("INFO", "Erasing from X=%d, Y=%d to end of frame\n", cursorX, cursorY);
  if (!cursorInBounds()) return 0;
  for (int x=cursorX; x<80; ++x) screen[x][cursorY]=' ';
  for (int y=cursorY+1; y<12; ++y)
    for (int x=0; x<80; ++x) screen[x][y]=' ';
  redrawTextScreen();
  screenDirty = true;
  return 0;
}
int dp2200Window::eraseFromCursorToEndOfLine() {
  printLog("INFO", "Erasing from X=%d, Y=%d to end of line\n", cursorX, cursorY);
  if (!cursorInBounds()) return 0;
  for (int x=cursorX; x<80; ++x) screen[x][cursorY]=' ';
  redrawTextScreen();
  screenDirty = true;
  return 0;
}
int dp2200Window::rollScreenOneLine() {
  printLog("INFO", "Roll one line\n");
  return scrollUp();
}
int dp2200Window::showCursor(bool value) {
  cursorEnabled=value;
  screenDirty=true;
  printLog("INFO", "Setting cursor status = %d \n", cursorEnabled);
  return 0;
}
int dp2200Window::setCursorX(int value) {
  if (value < 0 || value > 255) return 0;
  cursorX = value;
  printLog("INFO", "Setting Cursor X X=%d Y=%d\n", cursorX, cursorY);
  screenDirty = true;
  return 0;
}

int dp2200Window::setCursorY(int value) {
  if (value < 0 || value > 255) return 0;
  cursorY = value;
  printLog("INFO", "Setting Cursor Y X=%d Y=%d\n", cursorX, cursorY);
  screenDirty = true;
  return 0;
}

int dp2200Window::writeCharacter(int value) {
  printLog("INFO", "Writing char=%c to screen\n", value);
  if (!cursorInBounds()) return 0;
  int code=value & 0177;
  screen[cursorX][cursorY]=code;
  // Guest control codes are glyphs, not terminal newlines, tabs or caret pairs.
  chtype cell=(code>=32 && code<127) ? code : ' ';
  mvwaddchnstr(innerWin, cursorY, cursorX, &cell, 1);
  wrefresh(innerWin);
  screenDirty = true;
  return 0;
}

int dp2200Window::scrollDown() {
  for (int y=11; y>0; --y)
    for (int x=0; x<80; ++x) screen[x][y]=screen[x][y-1];
  for (int x=0; x<80; ++x) screen[x][0]=' ';
  redrawTextScreen();
  screenDirty = true;
  return 0;
}

int dp2200Window::scrollUp() {
  for (int y=0; y<11; ++y)
    for (int x=0; x<80; ++x) screen[x][y]=screen[x][y+1];
  for (int x=0; x<80; ++x) screen[x][11]=' ';
  redrawTextScreen();
  screenDirty = true;
  return 0;
}

void dp2200Window::incrementXPos() {
  // Writing column 79 moves off-screen; later writes wait for repositioning.
  if (cursorX<80) ++cursorX;
}

bool dp2200Window::cursorInBounds() const {
  return cursorX>=0 && cursorX<80 && cursorY>=0 && cursorY<12;
}

void dp2200Window::redrawTextScreen() {
  for (int y=0; y<12; ++y) {
    chtype cells[80];
    for (int x=0; x<80; ++x) {
      unsigned char code=screen[x][y] & 0177;
      cells[x]=(code>=32 && code<127) ? code : ' ';
    }
    mvwaddchnstr(innerWin, y, 0, cells, 80);
  }
  if (cursorInBounds()) wmove(innerWin, cursorY, cursorX);
  wrefresh(innerWin);
}


void dp2200Window::setCharGenChar(int data) {
  characterGenerator.select(data);
}

void dp2200Window::updateCharGen(int data) {
  characterGenerator.write(data);
  // Redefining a glyph changes every cell using it, even without screen writes.
  screenDirty=true;
}

#if DP2200_WITH_SDL
void dp2200Window::drawChar(int c, int cx, int cy) {
  // pixel-offset för övre vänstra hörnet (global padding + 1px per-cell padding)
  int x0 = PADDING + cx * CELL_W + 1;
  int y0 = PADDING + cy * CELL_H + 1;

  for (unsigned x=0; x<5; ++x)
    for (unsigned y=0; y<7; ++y)
      if (characterGenerator.pixel(c,x,y)) SDL_RenderDrawPoint(ren,x0+x,y0+y);
}
#endif

void dp2200Window::updateScreen() {
#if DP2200_WITH_SDL
  //printLog("INFO", "updateScreen ENTRY\n");
  SDL_Event evt;
  while (SDL_PollEvent(&evt)) {
    if (evt.type==SDL_TEXTINPUT)
      for (const unsigned char *text=reinterpret_cast<const unsigned char *>(evt.text.text); *text; ++text)
        if (*text<128) sdlKeys.push_back(*text);
    if (evt.type==SDL_KEYDOWN) {
      switch (evt.key.keysym.sym) {
      case SDLK_RETURN: case SDLK_KP_ENTER: sdlKeys.push_back('\n'); break;
      case SDLK_BACKSPACE: sdlKeys.push_back(127); break;
      case SDLK_ESCAPE: sdlKeys.push_back(27); break;
      case SDLK_F5: handleKey(KEY_F(5)); break;
      case SDLK_F6: handleKey(KEY_F(6)); break;
      default: break;
      }
    }
    if (evt.type==SDL_WINDOWEVENT &&
        (evt.window.event==SDL_WINDOWEVENT_EXPOSED ||
         evt.window.event==SDL_WINDOWEVENT_SHOWN)) screenDirty=true;
    if (evt.type==SDL_QUIT) SDL_HideWindow(sdlwin);
  }
  if (cpu && !sdlKeys.empty() && !cpu->ioCtrl->screenKeyboardDevice->keyboardReady()) {
    handleKey(sdlKeys.front());
    sdlKeys.pop_front();
  }
  bool phase=(SDL_GetTicks()/500)%2;
  if (cursorEnabled && phase!=sdlCursorPhase) screenDirty=true;
  sdlCursorPhase=phase;
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
  if (cursorEnabled && cursorInBounds() && sdlCursorPhase) {
    int x=PADDING+cursorX*CELL_W+1, y=PADDING+cursorY*CELL_H+8;
    SDL_RenderDrawLine(ren,x,y,x+4,y);
  }

  // Paint
  SDL_RenderPresent(ren);
  screenDirty = false;
  //printLog("INFO", "updateScreen EXIT\n");
#endif
}
int dp2200Window::setKeyboardLight(bool value) { return rw->setKeyboardLight(value); }
int dp2200Window::setDisplayLight(bool value) { return rw->setDisplayLight(value); }
bool dp2200Window::getKeyboardButton() { return rw->getKeyboardButton(); }
bool dp2200Window::getDisplayButton() { return rw->getDisplayButton(); }
void dp2200Window::soundBeep() { beep(); }
