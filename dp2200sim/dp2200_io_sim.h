
#ifndef _DP2200_IO_SIM_
#define _DP2200_IO_SIM_

#include <vector>
#include "cassetteTape.h"


class IOController {
  class IODevice {
    protected:
    unsigned char statusRegister, dataRegister;
    bool status;
    public:
    unsigned char input ();
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
    public:
    CassetteTape * tapeDrive[2];
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
    CassetteDevice();
  };

  class IODevice * dev[16];
  int ioAddress; 
  std::vector<unsigned char> supportedDevices;
  public:
  class CassetteDevice * cassetteDevice;
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