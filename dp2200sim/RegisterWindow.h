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

extern class dp2200_cpu cpu;

class hookExecutor {
  public:
  virtual void exec(FIELD *field) = 0;  
};


class registerWindow : public virtual Window {

  class Form {



    virtual void updateForm() = 0;
    protected:

    class memoryDataHookExecutor : hookExecutor {
      int data;
      class registerWindow::Form * rwf;
      public:
      memoryDataHookExecutor(class registerWindow::Form * r, int d);
      void exec (FIELD *field);
    };

    class memoryAddressHookExecutor : hookExecutor {
      int address;
      class registerWindow::Form * rwf;
      public:
      memoryAddressHookExecutor(class registerWindow::Form * r, int a);
      void exec(FIELD *field);
    };

    class charPointerHookExecutor : hookExecutor {
      unsigned char *  address;
      class registerWindow::Form * rwf;
      public:
      charPointerHookExecutor(class registerWindow::Form * r, unsigned char * a);
      void exec(FIELD *field);
    };

    class shortPointerHookExecutor : hookExecutor {
      unsigned short *  address;
      class registerWindow::Form * rwf;
      public:
      shortPointerHookExecutor(class registerWindow::Form * r, unsigned short * a);
      void exec(FIELD *field);
    };    
    FIELD * createAField(std::vector<FIELD *> * fields, int length, int y, int x, const char * str);
    FIELD * createAField(std::vector<FIELD *> * fields, int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h);
  };

  class HexForm : public virtual Form {
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
    FORM *frm;
    public:
    void updateForm();
    FORM * getForm();
    HexForm();
    ~HexForm();
  };

  class OctalForm : public virtual Form {
    void updateForm();
    OctalForm();
  };

  int cursorX, cursorY;
  WINDOW *win;
  //class dp2200_cpu *cpu;
  bool octal;
  bool activeWindow;
  registerWindow::HexForm * formHex;

  registerWindow::HexForm * createHexForm ();
  FIELD * createAField(std::vector<FIELD *> * , int length, int y, int x, const char * str);

  FIELD * createAField(std::vector<FIELD *> *, int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h) ;



  typedef struct m {
    int data;
    int address;
    int ascii;
  } M;




  //void updateForm();
  //void updateFormOctal();
public:

  WINDOW *getWin();
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