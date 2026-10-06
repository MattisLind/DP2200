#ifndef _DP2200WINDOW_
#define _DP2200WINDOW_

#include "Window.h"
#include "Console.h"
#include "CharacterGenerator.h"
#include <ncurses.h>
#include <functional>
#include <deque>
#include "RegisterWindow.h"
#if DP2200_WITH_SDL
#include <SDL.h>
#endif

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


class dp2200Window : public virtual Window, public Console {
  int cursorX, cursorY;
  bool cursorEnabled=false;
  WINDOW *win, *innerWin;
  bool activeWindow;
  class dp2200_cpu * cpu;
  CharacterGenerator characterGenerator;
  char screen[80][12];
  bool screenDirty;
  bool cursorInBounds() const;
  void redrawTextScreen();
#if DP2200_WITH_SDL
  SDL_Window* sdlwin;
  SDL_Renderer* ren;
  bool sdlCursorPhase=false;
  std::deque<int> sdlKeys;
#endif

public:
  dp2200Window(class dp2200_cpu *);
  ~dp2200Window();
  void hightlightWindow() override;
  void normalWindow() override;
  void handleKey(int key) override;
  void resetCursor() override;
  int eraseFromCursorToEndOfFrame () override;
  int eraseFromCursorToEndOfLine() override;
  int rollScreenOneLine();
  int showCursor(bool) override;
  int setCursorX(int) override;
  int setCursorY(int) override;
  int writeCharacter(int) override;
  void setHandleKeyCallback(std::function<void(unsigned char)>);
  void resize() override;
  int setKeyboardLight(bool) override;
  int setDisplayLight(bool) override;
  bool getKeyboardButton() override;
  bool getDisplayButton() override;
  void soundBeep() override;
  int scrollUp() override;
  int scrollDown() override;
  void incrementXPos() override;
  void setCharGenChar(int) override;
  void updateCharGen(int) override;
  void updateScreen();
#if DP2200_WITH_SDL
  void drawChar(int, int, int);
#endif
};

#endif
