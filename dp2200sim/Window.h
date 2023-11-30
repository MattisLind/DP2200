#ifndef _WINDOW_
#define _WINDOW_

class Window {
public:
  virtual void hightlightWindow() = 0;
  virtual void normalWindow() = 0;
  virtual void handleKey(int) = 0;
  virtual void resetCursor() = 0;
  virtual void resize() = 0;
};

#endif