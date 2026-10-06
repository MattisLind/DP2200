#ifndef DP2200_CONSOLE_H
#define DP2200_CONSOLE_H

// The emulated I/O device talks to this interface, independent of a UI.
class Console {
public:
  virtual ~Console() = default;
  virtual int eraseFromCursorToEndOfFrame() = 0;
  virtual int eraseFromCursorToEndOfLine() = 0;
  virtual int scrollUp() = 0;
  virtual int scrollDown() = 0;
  virtual int showCursor(bool) = 0;
  virtual int setCursorX(int) = 0;
  virtual int setCursorY(int) = 0;
  virtual int writeCharacter(int) = 0;
  virtual int setKeyboardLight(bool) = 0;
  virtual int setDisplayLight(bool) = 0;
  virtual void incrementXPos() = 0;
  virtual void setCharGenChar(int) = 0;
  virtual void updateCharGen(int) = 0;
  virtual void soundBeep() = 0;
  virtual bool getKeyboardButton() = 0;
  virtual bool getDisplayButton() = 0;
};

#endif
