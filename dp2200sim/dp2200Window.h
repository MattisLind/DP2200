#ifndef _DP2200WINDOW_
#define _DP2200WINDOW_

#include "Window.h"
#include <ncurses.h>
#include <functional>
#include "RegisterWindow.h"

extern class registerWindow * rw;


class dp2200Window : public virtual Window {
  int cursorX, cursorY;
  bool cursorEnabled=false;
  WINDOW *win, *innerWin;
  bool activeWindow;
  class dp2200_cpu * cpu;

public:
  dp2200Window(class dp2200_cpu *);
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
};

#endif