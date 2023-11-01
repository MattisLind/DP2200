
#ifndef _DP2200_IO_SIM_
#define _DP2200_IO_SIM_




class ioController {
  class ioDevice {
    protected:
    unsigned char statusRegister, dataRegister;
    bool status;
    public:
    unsigned char input ();
    void exStatus ();
    void exData ();
    virtual void exWrite(unsigned char data) = 0; 
    virtual void exCom1(unsigned char data) = 0;
    virtual void exCom2(unsigned char data) = 0;
    virtual void exCom3(unsigned char data) = 0;
    virtual void exCom4(unsigned char data) = 0;
    virtual void exBeep() = 0;
    virtual void exClick() = 0;
    virtual void exDeck1() = 0;
    virtual void exDeck2() = 0;
    virtual void exRBK() = 0;
    virtual void exWBK() = 0;
    virtual void exBSP() = 0;
    virtual void exSF() = 0;
    virtual void exSB() = 0;
    virtual void exRewind() = 0;
    virtual void exTStop() = 0;
  };

  class cassetteDrive : public virtual ioDevice  {
    public:
    void exWrite(unsigned char data); 
    void exCom1(unsigned char data);
    void exCom2(unsigned char data);
    void exCom3(unsigned char data);
    void exCom4(unsigned char data);
    void exBeep();
    void exClick();
    void exDeck1();
    void exDeck2();
    void exRBK();
    void exWBK();
    void exBSP();
    void exSF();
    void exSB();
    void exRewind();
    void exTStop();
    cassetteDrive();
  };

  class ioDevice * dev[16];
  int ioAddress; 

  public:
  ioController ();
  unsigned char input ();
  void exAdr (unsigned char address);
  void exStatus ();
  void exData ();
  void exWrite(unsigned char data); 
  void exCom1(unsigned char data);
  void exCom2(unsigned char data);
  void exCom3(unsigned char data);
  void exCom4(unsigned char data);
  void exBeep();
  void exClick();
  void exDeck1();
  void exDeck2();
  void exRBK();
  void exWBK();
  void exBSP();
  void exSF();
  void exSB();
  void exRewind();
  void exTStop();
}; 

#endif