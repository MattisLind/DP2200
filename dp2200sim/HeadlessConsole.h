#ifndef DP2200_HEADLESS_CONSOLE_H
#define DP2200_HEADLESS_CONSOLE_H
#include "Console.h"
#include "CharacterGenerator.h"
#include <array>
#include <string>

class HeadlessConsole : public Console {
  std::array<std::string, 12> rows;
  int x = 0, y = 0;
  bool cursorInBounds() const { return x>=0 && x<80 && y>=0 && y<12; }
public:
  CharacterGenerator characterGenerator;
  bool keyboardButton = false, displayButton = false;
  bool keyboardLight = false, displayLight = false, cursor = false;
  unsigned long beeps = 0;
  HeadlessConsole() { rows.fill(std::string(80, ' ')); }
  std::string snapshot() const {
    std::string result;
    for (const auto & row : rows) result += row + '\n';
    return result;
  }
  int eraseFromCursorToEndOfLine() override {
    if (!cursorInBounds()) return 0;
    rows[y].replace(x, 80-x, 80-x, ' '); return 0;
  }
  int eraseFromCursorToEndOfFrame() override {
    if (!cursorInBounds()) return 0;
    eraseFromCursorToEndOfLine();
    for (int row=y+1; row<12; ++row) rows[row].assign(80, ' ');
    return 0;
  }
  int scrollUp() override {
    for (int row=0; row<11; ++row) rows[row] = rows[row+1];
    rows[11].assign(80, ' '); return 0;
  }
  int scrollDown() override {
    for (int row=11; row>0; --row) rows[row] = rows[row-1];
    rows[0].assign(80, ' '); return 0;
  }
  int showCursor(bool value) override { cursor=value; return 0; }
  int setCursorX(int value) override { if (value>=0 && value<=255) x=value; return 0; }
  int setCursorY(int value) override { if (value>=0 && value<=255) y=value; return 0; }
  int writeCharacter(int value) override { if (cursorInBounds()) rows[y][x]=value & 0x7f; return 0; }
  void incrementXPos() override { if (x<80) ++x; }
  void setCharGenChar(int value) override { characterGenerator.select(value); }
  void updateCharGen(int value) override { characterGenerator.write(value); }
  void soundBeep() override { ++beeps; }
  int setKeyboardLight(bool value) override { keyboardLight=value; return 0; }
  int setDisplayLight(bool value) override { displayLight=value; return 0; }
  bool getKeyboardButton() override { return keyboardButton; }
  bool getDisplayButton() override { return displayButton; }
};
#endif
