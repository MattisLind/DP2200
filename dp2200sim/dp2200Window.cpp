#include "dp2200Window.h"
#include <form.h>
#include <ncurses.h>

dp2200Window::dp2200Window(class dp2200_cpu *) {
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
  default:
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
  for (int i=cursorX; i<81;i++) {
    waddch(innerWin, ' ');
  }
  for (int i=cursorY+1; i <12; i++) {
    wmove(innerWin, i, 1);
    for (int j=1; j<80; j++) {
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
