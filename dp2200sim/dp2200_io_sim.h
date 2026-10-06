
#ifndef _DP2200_IO_SIM_
#define _DP2200_IO_SIM_

#include <vector>
#include "cassetteTape.h"
#include "FloppyDrive.h"
#include "Console.h"
#include <ctime>

class callbackRecord * addToTimerQueue(std::function<int(class callbackRecord *)>, struct timespec);
void timeoutInNanosecs (struct timespec *, long);
void removeTimerCallback(class callbackRecord * c);



#define CASSETTE_STATUS_DECK_READY (1 << 0)
#define CASSETTE_STATUS_END_OF_TAPE (1 << 1)
#define CASSETTE_STATUS_READ_READY (1 << 2)
#define CASSETTE_STATUS_WRITE_READY (1 << 3)
#define CASSETTE_STATUS_INTER_RECORD_GAP (1 << 4)
#define CASSETTE_STATUS_CASSETTE_IN_PLACE (1 << 6)
#define CASSETTE_STATUS_WRITE_PROTECTED (1 << 7)

#define SCRNKBD_STATUS_CRT_READY (1 << 0)
#define SCRNKBD_STATUS_KBD_READY (1 << 1)
#define SCRNKBD_STATUS_KEYBOAD_BUTTON_PRESSED (1 << 2)
#define SCRNKBD_STATUS_DISPLAY_BUTTON_PRESSED (1 << 3)
#define SCRNKBD_STATUS_RAM_DISPLAY (1 << 4)

#define SCRNKBD_COM1_ROLL_DOWN (1 << 0)
#define SCRNKBD_COM1_ERASE_EOL (1 << 1)
#define SCRNKBD_COM1_ERASE_EOF (1 << 2)
#define SCRNKBD_COM1_ROLL (1 << 3)
#define SCRNKBD_COM1_CURSOR_ONOFF (1 << 4)
#define SCRNKBD_COM1_KDB_LIGHT (1 << 5)
#define SCRNKBD_COM1_DISP_LIGHT (1 << 6)
#define SCRNKBD_COM1_AUTO_INCREMENT (1 << 7)


#define FLOPPY_STATUS_DRIVE_ONLINE (1 << 0)
#define FLOPPY_STATUS_DATA_XFER_IN_PROGRESS (1 << 1)
#define FLOPPY_STATUS_DRIVE_READY (1 << 2)
#define FLOPPY_STATUS_WRITE_PROTECT (1 << 3)
#define FLOPPY_STATUS_CRC_ERROR (1 << 4)
#define FLOPPY_STATUS_BUFFER_PARITY_ERROR (1 << 5)
#define FLOPPY_STATUS_DELETED_DATA_MARK (1 << 6)
#define FLOPPY_STATUS_SECTOR_NOT_FOUND (1 << 7)

#define DISK9350_STATUS_DRIVE_ONLINE (1 << 0)
#define DISK9350_STATUS_CONTROLLER_READY (1 << 1)
#define DISK9350_STATUS_DRIVE_READY (1 << 2)
#define DISK9350_STATUS_WRITE_PROTECT_ENABLE (1 << 3)
#define DISK9350_STATUS_CRC_ERROR (1 << 4)
#define DISK9350_STATUS_COMMAND_ERROR (1 << 5)
#define DISK9350_STATUS_INVALID_SECTOR_ADDRESS (1 << 6)
#define DISK9350_STATUS_OVERFLOW (1 << 7)

#define DISK9370_STATUS_DRIVE_ONLINE (1 << 0)
#define DISK9370_STATUS_DATA_XFER_IN_PROGRESS (1 << 1)
#define DISK9370_STATUS_DRIVE_BUSY (1 << 2)
#define DISK9370_STATUS_SEEK_INCOMPLETE_ERROR (1 << 3)
#define DISK9370_STATUS_CRC_ERROR (1 << 4)
#define DISK9370_STATUS_WRITE_PROTECT_ENABLE (1 << 5)
#define DISK9370_STATUS_SECTOR_NOT_FOUND (1 << 6)
#define DISK9370_STATUS_BUFFER_PARITY_ERROR (1 << 7)



class IOController {
  class IODevice {
    protected:
    unsigned char statusRegister = 0, dataRegister = 0;
    int status = 1;
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
    bool writing = false;
    std::vector<unsigned char> writeBuffer;
    bool tapeRunning; 
    int tapeDeckSelected;
    void updateTapGapFlag(bool);
    void updateReadyFlag(bool);
    bool forward;
    bool stopAtGap;
    void readFromTape ();
    void scheduleReadByte();
    std::vector<class callbackRecord *>outStandingCallbacks;
    void removeFromOutstandCallbacks (class callbackRecord *);
    void removeAllCallbacks();
    void printStatus(const char *);
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
    bool openFile (int, std::string fileName, bool writeProtect);
    void closeFile (int);
    std::string getFileName (int);
    bool loadBoot (std::function<void(int address, unsigned char)> writeMem);
    CassetteDevice();
    bool transportRunning() const { return writing || !outStandingCallbacks.empty(); }
    int selectedDeck() const { return tapeDeckSelected; }
    unsigned char cassetteStatus() const { return statusRegister; }
  };

    class ScreenKeyboardDevice : public virtual IODevice  {
    Console * console = nullptr;
    bool incrementXOnWrite;
    bool loadingFont;  
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
    void updateKbd(int);
    void setConsole(Console * value) { console = value; }
    bool keyboardReady() const { return statusRegister & SCRNKBD_STATUS_KBD_READY; }
  };


  class ParallellInterfaceAdaptorDevice : public virtual IODevice  {
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
    ParallellInterfaceAdaptorDevice();
  };

  class ServoPrinterDevice : public virtual IODevice  {
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
    ServoPrinterDevice();
  };  

  class LocalPrinterDevice : public virtual IODevice  {
    FILE * file = nullptr;
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
    int openFile (int, std::string fileName);
    void closeFile (int);    
    LocalPrinterDevice();
  }; 


  class FloppyDevice : public virtual IODevice  {
    int selectedDrive = 0;
    int selectedBufferPage = 0;
    char buffer[4][256] = {};
    int bufferAddress = 0;
    class FloppyDrive * floppyDrives[4];
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
    int openFile (int, std::string fileName, bool, bool);
    void closeFile (int);    
    FloppyDevice();
  };

  class Disk9350Device : public virtual IODevice  {

    class Disk9350Drive {
      std::string fileName;
      FILE * file;
      bool writeProtected;
      public:
      int openFile (std::string fileName, bool writeProtected);
      void closeFile();
      int readSector(char * buffer, long address);
      int writeSector(char * buffer, long address); 
      bool isWriteProtected();
      bool isOnline();
    };

    int selectedDrive;
    int selectedBufferPage;
    char buffer[16][256];
    int bufferAddress;
    int head;
    int sector;
    int cylinder;
    class Disk9350Drive * drives[4];

    public:
    unsigned char input ();
    int openFile(int drive, std::string fileName, bool wp);
    void closeFile (int drive);
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
    Disk9350Device();
  };


  class Disk9370Device : public virtual IODevice  {
    class Disk9370Drive {
      std::string fileName;
      FILE * file = nullptr;
      bool writeProtected = true;
      //unsigned char diskBuffer [203*20*24*256];
      public:
      int openFile (std::string fileName, bool writeProtected, long imageBytes);
      void closeFile();
      int readSector(char * buffer, long address);
      int writeSector(char * buffer, long address);
      bool isWriteProtected();
      bool isOnline();       
      void setWriteProtected(bool value) { writeProtected=value; }
    };
    int tmp = 0;
    int selectedDrive = 0;
    int selectedBufferPage = 0;
    char buffer[16][256] = {};
    int bufferAddress = 0;
    int head = 0;
    int sector = 0;
    int cylinder = 0;
    int model = 9370;
    unsigned long resetGeneration = 0;
    int cylinderCount() const { return model==9374 ? 204 : 203; }
    int headCount() const { return model==9374 ? 8 : 20; }
    int driveCount() const { return model==9374 ? 16 : 8; }
    bool validAddress() const;
    int mediaDrive() const { return model==9374 ? selectedDrive*2+((head>>3)&1) : selectedDrive; }
    class Disk9370Drive * drives[16];
    public:
    struct Statistics {
      unsigned long reads=0, writes=0, formats=0, seeks=0, errors=0;
      int maxCylinder=0, maxHead=0, maxSector=0;
    } statistics;
    bool setModel(int value);
    int getModel() const { return model; }
    void setWriteProtected(int drive, bool value);
    int openFile(int drive, std::string fileName, bool wp);
    void closeFile (int drive);    
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
    Disk9370Device();
  };  

  class Disk9390Device : public virtual IODevice  {
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
    Disk9390Device();
  };


  class IODevice * dev[256];
  int ioAddress = -1;
  std::vector<unsigned char> supportedDevices;
  bool isDeviceSupported(unsigned char address);
  public:
  class CassetteDevice * cassetteDevice;
  class ScreenKeyboardDevice * screenKeyboardDevice;
  class ParallellInterfaceAdaptorDevice * parallellInterfaceAdaptorDevice;
  class FloppyDevice * floppyDevice;
  class ServoPrinterDevice * servoPrinterDevice;
  class LocalPrinterDevice * localPrinterDevice;
  class Disk9350Device * disk9350Device;
  class Disk9370Device * disk9370Device;
  class Disk9390Device * disk9390Device;
  IOController ();
  int input (bool checkParity = false);
  int exAdr (unsigned char address);
  int exStatus ();
  int exData ();
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
