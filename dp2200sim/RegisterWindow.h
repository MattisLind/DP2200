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

 
    

    protected:
    
    class hexMemoryDataHookExecutor : hookExecutor {
      int data;
      class registerWindow::Form * rwf;
      public:
      hexMemoryDataHookExecutor(class registerWindow::Form * r, int d);
      void exec (FIELD *field);
    };

    class hexMemoryAddressHookExecutor : hookExecutor {
      int address;
      class registerWindow::Form * rwf;
      public:
      hexMemoryAddressHookExecutor(class registerWindow::Form * r, int a);
      void exec(FIELD *field);
    };

    class octalMemoryDataHookExecutor : hookExecutor {
      int data;
      class registerWindow::Form * rwf;
      public:
      octalMemoryDataHookExecutor(class registerWindow::Form * r, int d);
      void exec (FIELD *field);
    };

    class octalMemoryAddressHookExecutor : hookExecutor {
      int address;
      class registerWindow::Form * rwf;
      public:
      octalMemoryAddressHookExecutor(class registerWindow::Form * r, int a);
      void exec(FIELD *field);
    };    

    class hexCharPointerHookExecutor : hookExecutor {
      unsigned char *  address;
      class registerWindow::Form * rwf;
      public:
      hexCharPointerHookExecutor(class registerWindow::Form * r, unsigned char * a);
      void exec(FIELD *field);
    };

    class hexShortPointerHookExecutor : hookExecutor {
      unsigned short *  address;
      class registerWindow::Form * rwf;
      public:
      hexShortPointerHookExecutor(class registerWindow::Form * r, unsigned short * a);
      void exec(FIELD *field);
    };    

    class octalCharPointerHookExecutor : hookExecutor {
      unsigned char *  address;
      class registerWindow::Form * rwf;
      public:
      octalCharPointerHookExecutor(class registerWindow::Form * r, unsigned char * a);
      void exec(FIELD *field);
    };

    class octalShortPointerHookExecutor : hookExecutor {
      unsigned short *  address;
      class registerWindow::Form * rwf;
      public:
      octalShortPointerHookExecutor(class registerWindow::Form * r, unsigned short * a);
      void exec(FIELD *field);
    };  

    class accessibleHookExecutor : hookExecutor {
      bool *  address;
      class registerWindow::Form * rwf;
      public:
      accessibleHookExecutor(class registerWindow::Form * r, bool * a);
      void exec(FIELD *field);
    }; 

    class writeableHookExecutor : hookExecutor {
      bool *  address;
      class registerWindow::Form * rwf;
      public:
      writeableHookExecutor(class registerWindow::Form * r, bool * a);
      void exec(FIELD *field);
    };


    FIELD * createAField(std::vector<FIELD *> * fields, int length, int y, int x, const char * str);
    FIELD * createAField(std::vector<FIELD *> * fields, int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h);
    public:
    virtual void set2200Mode(bool) = 0;
    virtual void updateForm() = 0;
    virtual FORM *  getForm() = 0;
  };

  class HexForm : public virtual Form {
    struct sectorTableFieldStruct {
      FIELD * ident;
      FIELD * writeable;
      FIELD * accessible;
      FIELD * physicalSector;
    };
    FIELD * sectorTableHeader;
    typedef struct sectorTableFieldStruct S;
    S sectorTableFields[16];    
    std::vector<FIELD *> addressFields;
    std::vector<FIELD *> dataFields;
    std::vector<FIELD *> asciiFields;
    std::vector<FIELD *> registerViewFields;
    FIELD * regs[2][8];
    FIELD * regsIdents[2][8];
    FIELD * stack[16];
    FIELD * flagParity[2];
    FIELD * flagSign[2];
    FIELD * flagCarry[2];
    FIELD * flagZero[2];
    FIELD * pc;
    //FIELD * interruptEnabled;
    //FIELD * interruptPending; 
    FIELD * base;
    FIELD * baseIdents;    
    FIELD * mnemonic;
    FIELD * instructionTrace[16];
    FIELD * breakpoints[8];
    FIELD * displayLightField;
    FIELD * displayButtonField;
    FIELD * keyboardLightField;
    FIELD * keyboardButtonField;
    FIELD * mode[2];
    FORM *frm;
    int numRegs;
    public:
    void set2200Mode(bool);
    void updateForm();
    FORM * getForm();
    HexForm();
    ~HexForm();
  };

  class OctalForm : public virtual Form {
    struct sectorTableFieldStruct {
      FIELD * ident;
      FIELD * writeable;
      FIELD * accessible;
      FIELD * physicalSector;
    };
    FIELD * sectorTableHeader;
    typedef struct sectorTableFieldStruct S;
    S sectorTableFields[16];
    std::vector<FIELD *> addressFields;
    std::vector<FIELD *> dataFields;
    std::vector<FIELD *> asciiFields;
    std::vector<FIELD *> registerViewFields;
    FIELD * regs[2][8];
    FIELD * regsIdents[2][8];    
    FIELD * stack[16];
    FIELD * flagParity[2];
    FIELD * flagSign[2];
    FIELD * flagCarry[2];
    FIELD * flagZero[2];
    FIELD * pc;
    FIELD * base;
    FIELD * baseIdents;
    //FIELD * interruptEnabled;
    //FIELD * interruptPending; 
    FIELD * mnemonic;
    FIELD * instructionTrace[16];
    FIELD * breakpoints[8];
    FIELD * displayLightField;
    FIELD * displayButtonField;
    FIELD * keyboardLightField;
    FIELD * keyboardButtonField;
    FIELD * mode[2];
    FORM *frm;
    public:
    void set2200Mode(bool);
    void updateForm();
    FORM * getForm();
    OctalForm();
    ~OctalForm();
  };

  int cursorX, cursorY;
  WINDOW *win;
  WINDOW *dwinhex;
  WINDOW *dwinoctal;

  bool activeWindow;
  registerWindow::HexForm * formHex;
  registerWindow::Form * currentForm;
  registerWindow::OctalForm * formOctal;
  registerWindow::HexForm * createHexForm ();
  registerWindow::OctalForm * createOctalForm ();
  FIELD * createAField(std::vector<FIELD *> * , int length, int y, int x, const char * str);

  FIELD * createAField(std::vector<FIELD *> *, int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h) ;



  typedef struct m {
    int data;
    int address;
    int ascii;
  } M;

public:
  bool octal;
  WINDOW *getWin();
  registerWindow(class dp2200_cpu *c);
  ~registerWindow();

  void updateWindow();
  void set2200Mode (bool);
  void hightlightWindow();
  void normalWindow();
  void resetCursor();
  void handleKey(int key);
  void resize();
  int setDisplayLight(bool);
  int setKeyboardLight(bool);
  int setKeyboardButton(bool);
  int setDisplayButton(bool);
  bool getDisplayButton();
  bool getKeyboardButton();
  void setOctal(bool);
};

#endif