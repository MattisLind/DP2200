
#ifndef _DP2200_IO_SIM_
#define _DP2200_IO_SIM_

#include <vector>
#include "cassetteTape.h"
#include "dp2200Window.h"


#define CASSETTE_STATUS_DECK_READY 1 << 0
#define CASSETTE_STATUS_END_OF_TAPE 1 << 1
#define CASSETTE_STATUS_READ_READY 1 << 2
#define CASSETTE_STATUS_WRITE_READY 1 << 3
#define CASSETTE_STATUS_INTER_RECORD_GAP 1 << 4
#define CASSETTE_STATUS_CASSETTE_IN_PLACE 1 << 6

#define SCRNKBD_STATUS_CRT_READY 1 << 0
#define SCRNKBD_STATUS_KBD_READY 1 << 1
#define SCRNKBD_STATUS_KEYBOAD_BUTTON_PRESSED 1 << 2
#define SCRNKBD_STATUS_DISPLAY_BUTTON_PRESSED 1 << 3


#define SCRNKBD_COM1_ERASE_EOL 1 << 1
#define SCRNKBD_COM1_ERASE_EOF 1 << 2
#define SCRNKBD_COM1_ROLL 1 << 3
#define SCRNKBD_COM1_CURSOR_ONOFF 1 << 4
#define SCRNKBD_COM1_KDB_LIGHT 1 << 5
#define SCRNKBD_COM1_DISP_LIGHT 1 << 6

extern class dp2200Window * dpw;

class IOController {
  class IODevice {
    protected:
    unsigned char statusRegister, dataRegister;
    bool status;
    public:
    virtual unsigned char input () = 0;
    void exStatus ();
    void exData ();
    virtual int exWrite(unsigned char data) = 0; 
    virtual int exCom1(unsigned char data) = 0;
    virtual int exCom2(unsigned char data) = 0;
    virtual int exCom3(unsigned char data) = 0;
    virtual int exCom4(unsigned char data) = 0;
    virtual int exBeep() = 0;
    virtual int exClick() = 0;
    virtual int exDeck1() = 0;
    virtual int exDeck2() = 0;
    virtual int exRBK() = 0;
    virtual int exWBK() = 0;
    virtual int exBSP() = 0;
    virtual int exSF() = 0;
    virtual int exSB() = 0;
    virtual int exRewind() = 0;
    virtual int exTStop() = 0;
  };

  class CassetteDevice : public virtual IODevice  {
    bool tapeRunning; 
    int tapeDeckSelected;
    void updateTapGapFlag(bool);
    void updateReadyFlag(bool);
    public:
    CassetteTape * tapeDrive[2];
    unsigned char input ();
    int exWrite(unsigned char data); 
    int exCom1(unsigned char data);
    int exCom2(unsigned char data);
    int exCom3(unsigned char data);
    int exCom4(unsigned char data);
    int exBeep();
    int exClick();
    int exDeck1();
    int exDeck2();
    int exRBK();
    int exWBK();
    int exBSP();
    int exSF();
    int exSB();
    int exRewind();
    int exTStop();
    bool openFile (int, std::string fileName);
    void closeFile (int);
    std::string getFileName (int);
    void loadBoot (unsigned char * address);
    CassetteDevice();
    void updateDataRegisterAndSetStatusRegister( unsigned char);
  };

    class ScreenKeyboardDevice : public virtual IODevice  {
    public:
    unsigned char input ();
    int exWrite(unsigned char data); 
    int exCom1(unsigned char data);
    int exCom2(unsigned char data);
    int exCom3(unsigned char data);
    int exCom4(unsigned char data);
    int exBeep();
    int exClick();
    int exDeck1();
    int exDeck2();
    int exRBK();
    int exWBK();
    int exBSP();
    int exSF();
    int exSB();
    int exRewind();
    int exTStop();
    ScreenKeyboardDevice();
  };

  class IODevice * dev[16];
  int ioAddress; 
  std::vector<unsigned char> supportedDevices;
  public:
  class CassetteDevice * cassetteDevice;
  class ScreenKeyboardDevice * screenKeyboardDevice;
  IOController ();
  unsigned char input ();
  int exAdr (unsigned char address);
  void exStatus ();
  void exData ();
  int exWrite(unsigned char data); 
  int exCom1(unsigned char data);
  int exCom2(unsigned char data);
  int exCom3(unsigned char data);
  int exCom4(unsigned char data);
  int exBeep();
  int exClick();
  int exDeck1();
  int exDeck2();
  int exRBK();
  int exWBK();
  int exBSP();
  int exSF();
  int exSB();
  int exRewind();
  int exTStop();
}; 

#endif