#ifndef _REGISTER_WINDOW_
#define _REGISTER_WINDOW_
#include <form.h>
#include <ncurses.h>
#include "Window.h"
#include <vector>
#include "dp2200_cpu_sim.h"
#include <cassert>
#include <cstring>


void form_hook_proxy(formnode *);
void printLog(const char *level, const char *fmt, ...);

class hookExecutor {
  public:
  virtual void exec(FIELD *field) = 0;  
};


class registerWindow : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win;
  class dp2200_cpu *cpu;
  bool octal;
  bool activeWindow;
  FORM *form;
  bool keyboardLightStatus=false;
  bool displayLightStatus=false;
  bool keyboardButtonStatus=false;
  bool displayButtonStatus=false;
  FORM * createHexForm();

  int base = 0;
  typedef struct m {
    int data;
    int address;
    int ascii;
  } M;
  std::vector<FIELD *> addressFields;
  std::vector<FIELD *> dataFields;
  std::vector<FIELD *> asciiFields;
  std::vector<FIELD *> registerViewFields;
  FIELD * regs[2][7];
  FIELD * stack[16];
  FIELD * flagParity[2];
  FIELD * flagSign[2];
  FIELD * flagCarry[2];
  FIELD * flagZero[2];
  FIELD * pc;
  //FIELD * interruptEnabled;
  //FIELD * interruptPending; 
  FIELD * mnemonic;
  FIELD * instructionTrace[8];
  FIELD * breakpoints[8];
  FIELD * displayLightField;
  FIELD * displayButtonField;
  FIELD * keyboardLightField;
  FIELD * keyboardButtonField;

  class memoryDataHookExecutor : hookExecutor {
    int data;
    class registerWindow * rw;
    public:
    memoryDataHookExecutor(class registerWindow * r, int d);
    void exec (FIELD *field);
  };

  class memoryAddressHookExecutor : hookExecutor {
    int address;
    class registerWindow * rw;
    public:
    memoryAddressHookExecutor(class registerWindow * r, int a);
    void exec(FIELD *field);
  };

  class charPointerHookExecutor : hookExecutor {
    unsigned char *  address;
    class registerWindow * rw;
    public:
    charPointerHookExecutor(class registerWindow * r, unsigned char * a);
    void exec(FIELD *field);
  };

   class shortPointerHookExecutor : hookExecutor {
    unsigned short *  address;
    class registerWindow * rw;
    public:
    shortPointerHookExecutor(class registerWindow * r, unsigned short * a);
    void exec(FIELD *field);
  };

  void updateForm(int startAddress);
public:

  FIELD * createAField(int length, int y, int x, const char * str);

  FIELD * createAField(int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h) ;

  registerWindow(class dp2200_cpu *c);
  ~registerWindow();

  void updateWindow();

  void hightlightWindow();
  void normalWindow();
  void resetCursor();
  void handleKey(int key);
  int setDisplayLight(bool);
  int setKeyboardLight(bool);
  int setKeyboardButton(bool);
  int setDisplayButton(bool);
  bool getDisplayButton();
  bool getKeyboardButton();
};

#endif