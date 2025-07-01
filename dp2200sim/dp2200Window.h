#ifndef _DP2200WINDOW_
#define _DP2200WINDOW_

#include "Window.h"
#include <ncurses.h>
#include <functional>
#include "RegisterWindow.h"
#include <SDL.h>

// Size of screen
const int CHARS_W = 80;
const int CHARS_H = 12;

// Char size 2 pixel padding around each char
const int CELL_W = 5 + 2;
const int CELL_H = 7 + 2;

// Extra padding
const int PADDING = 10;
// Window size
const int WINDOW_W = CHARS_W * CELL_W + 2 * PADDING; // 560 + 20 = 580
const int WINDOW_H = CHARS_H * CELL_H + 2 * PADDING; // 108 + 20 = 128


extern class registerWindow * rw;


class dp2200Window : public virtual Window {
  int cursorX, cursorY;
  bool cursorEnabled=false;
  WINDOW *win, *innerWin;
  bool activeWindow;
  class dp2200_cpu * cpu;
  int lastCharGenChar;
  unsigned char font5x7[128][5];
  char screen[80][12];
  bool screenDirty;
  int charGenIndex;
  SDL_Window* sdlwin;
  SDL_Renderer* ren;

public:
  dp2200Window(class dp2200_cpu *);
  ~dp2200Window();
  void hightlightWindow();
  void normalWindow();
  void handleKey(int key);
  void resetCursor();
  int eraseFromCursorToEndOfFrame ();
  int eraseFromCursorToEndOfLine();
  int rollScreenOneLine();
  int showCursor(bool);
  int setCursorX(int);
  int setCursorY(int);
  int writeCharacter(int);
  void setHandleKeyCallback(std::function<void(unsigned char)>);
  void resize();
  int scrollUp();
  int scrollDown();
  void incrementXPos();
  void setCharGenChar(int);
  void updateCharGen(int);
  void updateScreen();
  void drawChar(int, int, int);
};

#endif