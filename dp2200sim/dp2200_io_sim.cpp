

#include "dp2200_io_sim.h"
#include "dp2200Window.h"
#include "RegisterWindow.h"
#include <algorithm>
#include <sys/stat.h>

extern class dp2200Window * dpw;
extern class registerWindow * rw;



void printLog(const char *level, const char *fmt, ...);


IOController::IOController () {
  dev[0] = cassetteDevice = new CassetteDevice();
  dev[1] = screenKeyboardDevice = new ScreenKeyboardDevice();
  dev[12] = floppyDevice = new FloppyDevice();
  dev[6] = parallellInterfaceAdaptorDevice = new ParallellInterfaceAdaptorDevice();
  dev[10] = servoPrinterDevice = new ServoPrinterDevice();
  dev[3] = localPrinterDevice = new LocalPrinterDevice();
  dev[8] = disk9350Device = new Disk9350Device();
  dev[11] = disk9370Device = new Disk9370Device();
  supportedDevices.push_back(0);
  supportedDevices.push_back(1);
  supportedDevices.push_back(12);
  supportedDevices.push_back(6);
  supportedDevices.push_back(10);
  supportedDevices.push_back(3);
  supportedDevices.push_back(8);
  supportedDevices.push_back(11);  
}

int IOController::exAdr (unsigned char address) {
  if (std::find(supportedDevices.begin(), supportedDevices.end(), address&0xf)==supportedDevices.end()) {
    return 1;
  }
  ioAddress = address & 0xf;
  exStatus();
  return 0;
}

void IOController::exStatus () {
  dev[ioAddress]->exStatus();
}

void IOController::exData () {
  dev[ioAddress]->exData();
}

int IOController::exWrite(unsigned char data) {
  return dev[ioAddress]->exWrite(data);
}

int IOController::exCom1(unsigned char data) {
  return dev[ioAddress]->exCom1(data);
}
int IOController::exCom2(unsigned char data) {
  return dev[ioAddress]->exCom2(data);
}
int IOController::exCom3(unsigned char data) {
  return dev[ioAddress]->exCom3(data);
}
int IOController::exCom4(unsigned char data) {
  return dev[ioAddress]->exCom4(data);
}
int IOController::exBeep() {
  return dev[ioAddress]->exBeep();
}
int IOController::exClick() {
  return dev[ioAddress]->exClick();
}
int IOController::exDeck1() {
  return dev[ioAddress]->exDeck1();
}
int IOController::exDeck2() {
  return dev[ioAddress]->exDeck2();
}
int IOController::exRBK() {
  return dev[ioAddress]->exRBK();
}
int IOController::exWBK() {
  return dev[ioAddress]->exWBK();
}
int IOController::exBSP() {
  return dev[ioAddress]->exBSP();
}
int IOController::exSF() {
  return dev[ioAddress]->exSF();
}
int IOController::exSB() {
  return dev[ioAddress]->exSB();
}
int IOController::exRewind() {
  return dev[ioAddress]->exRewind();
}
int IOController::exTStop() {
  return dev[ioAddress]->exTStop();
}

unsigned char IOController::input () {
  return dev[ioAddress]->input();
}


void IOController::IODevice::exStatus () {
  status = true;
}

void IOController::IODevice::exData () {
  status = false;
}

unsigned char IOController::CassetteDevice::input () {
  //printLog("INFO", "input: status=%d statusRegister=%02X dataRegister=%02X\n", status, statusRegister, dataRegister);
  if (status) {
    return statusRegister;
  } else {
    statusRegister &= ~(CASSETTE_STATUS_READ_READY);
    return dataRegister;
  }
}

int IOController::CassetteDevice::exWrite(unsigned char data) {
  return 1;
}

int IOController::CassetteDevice::exCom1(unsigned char data) {
  return 1;
}
int IOController::CassetteDevice::exCom2(unsigned char data) {
  return 1;
}
int IOController::CassetteDevice::exCom3(unsigned char data) {
  return 1;
}
int IOController::CassetteDevice::exCom4(unsigned char data) {
  return 1;
}
int IOController::CassetteDevice::exBeep() {
  return 1;
}
int IOController::CassetteDevice::exClick() {
  return 1;
}
int IOController::CassetteDevice::exDeck1() {
  tapeDeckSelected = 0;
  return 0;
}
int IOController::CassetteDevice::exDeck2() {
  tapeDeckSelected = 1;
  return 0;
}


void IOController::CassetteDevice::removeAllCallbacks() {
  printLog("INFO", "Number of outstanding timers to clear = %d \n", outStandingCallbacks.size());
  for (auto it=outStandingCallbacks.begin(); it < outStandingCallbacks.end(); it++) {
    removeTimerCallback(*it);
  }
}

void IOController::CassetteDevice::removeFromOutstandCallbacks(class callbackRecord * c) {
  auto it = std::find(outStandingCallbacks.begin(), outStandingCallbacks.end(), c);
  if (it != outStandingCallbacks.end()) {
    outStandingCallbacks.erase(it);
  }
}
void IOController::CassetteDevice::readFromTape() {
  struct timespec then;
  if (tapeDrive[tapeDeckSelected]->isTapeOverGap()) { 
    timeoutInNanosecs(&then, 70000000); // 70 ms timeout
    outStandingCallbacks.push_back( addToTimerQueue([cd=this](class callbackRecord * c)->int {
      struct timespec then;
      printLog("INFO", "70ms timeout ENTRY\n");
      cd->removeFromOutstandCallbacks(c);
      cd->statusRegister &= ~(CASSETTE_STATUS_INTER_RECORD_GAP);
      timeoutInNanosecs(&then, 1000000); 
      cd->outStandingCallbacks.push_back( addToTimerQueue([cd=cd](class callbackRecord * c)->int {
        unsigned char data; 
        printLog("INFO", "1ms timeout to read the actual data after a gap ENTRY\n");
        cd->removeFromOutstandCallbacks(c);
        cd->tapeDrive[cd->tapeDeckSelected]->readByte(cd->forward,  &data);
        cd->statusRegister |= (CASSETTE_STATUS_READ_READY);
        cd->dataRegister = data;      
        cd->readFromTape();
        printLog("INFO", "1ms timeout to set tape gap and stop if necessary EXIT\n");
        return 0;
    }, then));
      printLog("INFO", "70ms timeout EXIT\n");
      return 0;
    }, then));
  } else {
    timeoutInNanosecs(&then, 2800000); // 2.8 ms timeout
    outStandingCallbacks.push_back( addToTimerQueue([cd=this](class callbackRecord * c)->int {
      unsigned char data; 
      printLog("INFO", "2.8ms timeout ENTRY\n");
      cd->removeFromOutstandCallbacks(c); 
      cd->tapeDrive[cd->tapeDeckSelected]->readByte(cd->forward,  &data);
      cd->statusRegister |= (CASSETTE_STATUS_READ_READY);
      cd->dataRegister = data;
      if (cd->tapeDrive[cd->tapeDeckSelected]->isTapeOverGap()) {
        printLog("INFO", "2.8 ms timeout - tape is over gap. ");
        // Now we are over a gap
        struct timespec then;
        timeoutInNanosecs(&then, 1000000);  // we have read the last byte of the record, waited 2.8 ms and then we let it wait another 1 ms until we stop the tape and report gap.
        cd->outStandingCallbacks.push_back( addToTimerQueue([cd=cd](class callbackRecord * c)->int {
          printLog("INFO", "1ms timeout to set tape gap and stop if necessary ENTRY\n");
          cd->removeFromOutstandCallbacks(c);
          if (cd->stopAtGap) {
            cd->statusRegister |= (CASSETTE_STATUS_DECK_READY);
            cd->removeAllCallbacks();
          }  
          cd->statusRegister |= (CASSETTE_STATUS_INTER_RECORD_GAP);
          printLog("INFO", "1ms timeout to set tape gap and stop if necessary EXIT\n");
          return 0;
        }, then));
        if (!cd->stopAtGap) {
          cd->readFromTape(); 
        }
      } else {
        cd->readFromTape();
      }
      printLog("INFO", "2.8ms timeout EXIT\n");
      return 0;
    }, then));  
  }
}

int IOController::CassetteDevice::exRBK() {
  forward = true;
  stopAtGap = true; 
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY); // Clear ready bit
  readFromTape();
  return 0;
}
int IOController::CassetteDevice::exWBK() {
  return 1;
}
int IOController::CassetteDevice::exBSP() {
  forward = false;
  stopAtGap = true; 
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY); // Clear ready bit
  readFromTape();

  return 0;
}
int IOController::CassetteDevice::exSF() {
  forward = true;
  stopAtGap = false; 
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY); // Clear ready bit
  readFromTape();
  return 0;
}
int IOController::CassetteDevice::exSB() {
  forward = false;
  stopAtGap = false; 
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY); // Clear ready bit
  readFromTape();
  return 0;
}
int IOController::CassetteDevice::exRewind() {
  return 1;
}
int IOController::CassetteDevice::exTStop() {
  statusRegister |= (CASSETTE_STATUS_DECK_READY);
  return 0;
}

void IOController::CassetteDevice::updateDataRegisterAndSetStatusRegister( unsigned char data) {
  statusRegister |= (CASSETTE_STATUS_READ_READY);
  dataRegister = data;
  printLog("INFO", "Updated statusRegister to %02X and dataRegister to %02X.\n", statusRegister, dataRegister); 
}

IOController::CassetteDevice::CassetteDevice () {
  tapeRunning = false;
  tapeDeckSelected = 1;   
  tapeDrive[0] = new CassetteTape();
  tapeDrive[1] = new CassetteTape();
}


bool IOController::CassetteDevice::openFile (int drive, std::string fileName) {
  statusRegister |= (CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_CASSETTE_IN_PLACE);
  return tapeDrive[drive]->openFile(fileName);
}
void IOController::CassetteDevice::closeFile (int drive) {
  tapeDrive[drive]->closeFile();
}
std::string IOController::CassetteDevice::getFileName (int drive) {
  return tapeDrive[drive]->getFileName(); 
}
bool IOController::CassetteDevice::loadBoot (unsigned char * address) {
  return tapeDrive[0]->loadBoot(address);
}

void IOController::CassetteDevice::updateTapGapFlag(bool gap) {
  printLog("INFO", "Setting tap gap to %d\n", gap);
  if (gap) {
    statusRegister |= (CASSETTE_STATUS_INTER_RECORD_GAP);
  } else {
    statusRegister &= ~(CASSETTE_STATUS_INTER_RECORD_GAP);
  }
}

void IOController::CassetteDevice::updateReadyFlag(bool gap) {
  printLog("INFO", "Setting tap gap to %d\n", gap);
  if (gap) {
    statusRegister |= (CASSETTE_STATUS_DECK_READY);
  } else {
    statusRegister &= ~(CASSETTE_STATUS_DECK_READY);
  }
}



unsigned char IOController::ScreenKeyboardDevice::input () {
  if (status) {
    if (rw->getDisplayButton()) {
      statusRegister |= 0010;
    } else {
      statusRegister &= ~0010;
    }
    if (rw->getKeyboardButton()) {
      statusRegister |= 0004;
    } else {
      statusRegister &= ~0004;
    }
    return statusRegister;
  } else {
    statusRegister &= ~(SCRNKBD_STATUS_KBD_READY);
    return dataRegister;
  }
}
int IOController::ScreenKeyboardDevice::exWrite(unsigned char data) {
  int ret = dpw->writeCharacter(data);
  if (incrementXOnWrite) {
    dpw->incrementXPos();
  }
  return ret;
} 
int IOController::ScreenKeyboardDevice::exCom1(unsigned char data){
  //
  if (data & SCRNKBD_COM1_ROLL_DOWN) {
    dpw->scrollDown();
  } 
  if (data & SCRNKBD_COM1_ERASE_EOF) {
    dpw->eraseFromCursorToEndOfFrame();  
  }
  if (data & SCRNKBD_COM1_ERASE_EOL) {
    dpw->eraseFromCursorToEndOfLine(); 
  }
  if (data & SCRNKBD_COM1_ROLL) {
    //dpw->rollScreenOneLine();
    dpw->scrollUp();
  }
  if (data & SCRNKBD_COM1_CURSOR_ONOFF) {
    dpw->showCursor(true);
  } else {
    dpw->showCursor(false);
  }
  if (data & SCRNKBD_COM1_KDB_LIGHT) {
    rw->setKeyboardLight(true);
  } else {
    rw->setKeyboardLight(false);
  }
  if (data & SCRNKBD_COM1_DISP_LIGHT) {
    rw->setDisplayLight(true);
  } else {
    rw->setDisplayLight(false);
  }
  if (data & SCRNKBD_COM1_AUTO_INCREMENT) { 
    incrementXOnWrite=true;
  } else {
    incrementXOnWrite=false;
  }
  return 0;
}
int IOController::ScreenKeyboardDevice::exCom2(unsigned char data){
  return dpw->setCursorX(data);
}
int IOController::ScreenKeyboardDevice::exCom3(unsigned char data){
  return dpw->setCursorY(data);
}
int IOController::ScreenKeyboardDevice::exCom4(unsigned char data){
  return 0;  // load font - do nothing...
}
int IOController::ScreenKeyboardDevice::exBeep(){
  beep();
  return 0;
}
int IOController::ScreenKeyboardDevice::exClick(){
  return 0;
}
int IOController::ScreenKeyboardDevice::exDeck1(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exDeck2(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exRBK(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exWBK(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exBSP(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exSF(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exSB(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exRewind(){
  return 1;
}
int IOController::ScreenKeyboardDevice::exTStop(){
  return 1;
}

void IOController::ScreenKeyboardDevice::updateKbd(int key) {
  dataRegister = key;
  statusRegister |= (SCRNKBD_STATUS_KBD_READY);
}

IOController::ScreenKeyboardDevice::ScreenKeyboardDevice() {
  statusRegister = (SCRNKBD_STATUS_CRT_READY);
  incrementXOnWrite = false;
}


unsigned char IOController::ParallellInterfaceAdaptorDevice::input () {
  if (status) {
    return statusRegister;
  } else {
    return dataRegister;
  }
}
int IOController::ParallellInterfaceAdaptorDevice::exWrite(unsigned char data) {
  return 1;
} 
int IOController::ParallellInterfaceAdaptorDevice::exCom1(unsigned char data){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exCom2(unsigned char data){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exCom3(unsigned char data){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exCom4(unsigned char data){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exBeep(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exClick(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exDeck1(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exDeck2(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exRBK(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exWBK(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exBSP(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exSF(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exSB(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exRewind(){
  return 1;
}
int IOController::ParallellInterfaceAdaptorDevice::exTStop(){
  return 1;
}

IOController::ParallellInterfaceAdaptorDevice::ParallellInterfaceAdaptorDevice() {
  statusRegister = 0;
}



unsigned char IOController::ServoPrinterDevice::input () {
  if (status) {
    return statusRegister;
  } else {
    return dataRegister;
  }
}
int IOController::ServoPrinterDevice::exWrite(unsigned char data) {
  return 1;
} 
int IOController::ServoPrinterDevice::exCom1(unsigned char data){
  return 1;
}
int IOController::ServoPrinterDevice::exCom2(unsigned char data){
  return 1;
}
int IOController::ServoPrinterDevice::exCom3(unsigned char data){
  return 1;
}
int IOController::ServoPrinterDevice::exCom4(unsigned char data){
  return 1;
}
int IOController::ServoPrinterDevice::exBeep(){
  return 1;
}
int IOController::ServoPrinterDevice::exClick(){
  return 1;
}
int IOController::ServoPrinterDevice::exDeck1(){
  return 1;
}
int IOController::ServoPrinterDevice::exDeck2(){
  return 1;
}
int IOController::ServoPrinterDevice::exRBK(){
  return 1;
}
int IOController::ServoPrinterDevice::exWBK(){
  return 1;
}
int IOController::ServoPrinterDevice::exBSP(){
  return 1;
}
int IOController::ServoPrinterDevice::exSF(){
  return 1;
}
int IOController::ServoPrinterDevice::exSB(){
  return 1;
}
int IOController::ServoPrinterDevice::exRewind(){
  return 1;
}
int IOController::ServoPrinterDevice::exTStop(){
  return 1;
}

IOController::ServoPrinterDevice::ServoPrinterDevice() {
  statusRegister = 0;
}

unsigned char IOController::LocalPrinterDevice::input () {
  if (status) {
    if (file!=NULL) {
      statusRegister |= 2;
    }    
    return statusRegister;
  } else {
    return dataRegister;
  }
}
int IOController::LocalPrinterDevice::exWrite(unsigned char data) {
  if (file != NULL) fwrite(&data, 1, 1, file);
  return 0;
} 
int IOController::LocalPrinterDevice::exCom1(unsigned char data){
  return 1;
}
int IOController::LocalPrinterDevice::exCom2(unsigned char data){
  return 1;
}
int IOController::LocalPrinterDevice::exCom3(unsigned char data){
  return 1;
}
int IOController::LocalPrinterDevice::exCom4(unsigned char data){
  return 1;
}
int IOController::LocalPrinterDevice::exBeep(){
  return 1;
}
int IOController::LocalPrinterDevice::exClick(){
  return 1;
}
int IOController::LocalPrinterDevice::exDeck1(){
  return 1;
}
int IOController::LocalPrinterDevice::exDeck2(){
  return 1;
}
int IOController::LocalPrinterDevice::exRBK(){
  return 1;
}
int IOController::LocalPrinterDevice::exWBK(){
  return 1;
}
int IOController::LocalPrinterDevice::exBSP(){
  return 1;
}
int IOController::LocalPrinterDevice::exSF(){
  return 1;
}
int IOController::LocalPrinterDevice::exSB(){
  return 1;
}
int IOController::LocalPrinterDevice::exRewind(){
  return 1;
}
int IOController::LocalPrinterDevice::exTStop(){
  return 1;
}

int IOController::LocalPrinterDevice::openFile(int drive, std::string fileName){
  if (file != NULL) {
    fclose(file);
  }
  file = fopen(fileName.c_str(), "w");
  if (file==NULL) { 
    return -1;
  }
  return 0;
}
void IOController::LocalPrinterDevice::closeFile(int drive) {
  printLog("INFO", "Flushing and closing printer file.\n");
  fflush(file);
  fclose(file);
  file = NULL;
}

IOController::LocalPrinterDevice::LocalPrinterDevice() {
  statusRegister = 5;

}

unsigned char IOController::FloppyDevice::input () {
  if (status) {
    if (floppyDrives[selectedDrive]->online()) {
      statusRegister |= FLOPPY_STATUS_DRIVE_ONLINE;
    } else {
      statusRegister &= ~FLOPPY_STATUS_DRIVE_ONLINE;
    }
    if (floppyDrives[selectedDrive]->isWriteProtected()) {
      statusRegister |= FLOPPY_STATUS_WRITE_PROTECT; 
    } else {
      statusRegister &= ~FLOPPY_STATUS_WRITE_PROTECT;
    }
    printLog("INFO", "Returning floppy status = %02X\n", statusRegister);
    return statusRegister;
  } else {
    printLog("INFO", "Reading data from bufferPage %d address %d = %02X\n", selectedBufferPage, bufferAddress, 0xff&buffer[selectedBufferPage][bufferAddress]);
    return buffer[selectedBufferPage][bufferAddress];
  }
}
int IOController::FloppyDevice::exWrite(unsigned char data) {
  //printLog("INFO", "Writing data %02X to address %d in bufferPage %d\n", data&0xff, bufferAddress, selectedBufferPage);
  buffer[selectedBufferPage][bufferAddress]=data;
  return 0;
} 
int IOController::FloppyDevice::exCom1(unsigned char data){
  struct timespec then;
  switch (data) {
    case 0:
    case 1:
    case 2:
    case 3:
      selectedDrive = 0x3 & data;
      printLog("INFO", "Selecting drive %d\n", 0x3&data);
      statusRegister &= ~FLOPPY_STATUS_DRIVE_READY;
      timeoutInNanosecs(&then, 10000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "10us timeout floppy select drive is ready\n");
          t->statusRegister |= FLOPPY_STATUS_DRIVE_READY;
          return 0;
        },
        then);

      break;
    case 4: // Clear Buffer Parity Error
      return 0;
    case  5: // Read Selected Sector into Selected Buffer Page
      printLog("INFO", "Reading from drive\n");
      statusRegister |= FLOPPY_STATUS_DATA_XFER_IN_PROGRESS;
      statusRegister &= ~(FLOPPY_STATUS_SECTOR_NOT_FOUND | FLOPPY_STATUS_DELETED_DATA_MARK | FLOPPY_STATUS_CRC_ERROR | FLOPPY_STATUS_DRIVE_READY);
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          int ret;
          printLog("INFO", "10ms timeout floppy read is ready\n");
          t->statusRegister &= ~FLOPPY_STATUS_DATA_XFER_IN_PROGRESS;
          ret = t->floppyDrives[t->selectedDrive]->readSector(t->buffer[t->selectedBufferPage]);
          switch (ret) {
          case FLOPPY_SECTOR_NOT_FOUND:
            t->statusRegister |= FLOPPY_STATUS_SECTOR_NOT_FOUND;
            break;
          case FLOPPY_DELETED_DATA:
            t->statusRegister |= FLOPPY_STATUS_DELETED_DATA_MARK;
            break;
          case FLOPPY_CRC_ERROR:
            t->statusRegister |= FLOPPY_STATUS_CRC_ERROR;
            break;
          case FLOPPY_OK:
            t->statusRegister |= FLOPPY_STATUS_DRIVE_READY;
            break;
          }
          return 0;
        }, then);
      break;
    case 6: // Write Selected Buffer Page onto Selected Sector
    case 7: // Same as 6 plus read check of CRC
     printLog("INFO", "Writing to drive\n");
      statusRegister |= FLOPPY_STATUS_DATA_XFER_IN_PROGRESS;
      statusRegister &= ~(FLOPPY_STATUS_SECTOR_NOT_FOUND | FLOPPY_STATUS_DELETED_DATA_MARK | FLOPPY_STATUS_CRC_ERROR | FLOPPY_STATUS_DRIVE_READY); 
      timeoutInNanosecs(&then, 1000000); 
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          int ret;
          printLog("INFO", "10ms timeout floppy read is ready\n");
          t->statusRegister &= ~FLOPPY_STATUS_DATA_XFER_IN_PROGRESS;
          ret = t->floppyDrives[t->selectedDrive]->writeSector(t->buffer[t->selectedBufferPage]);
          switch (ret) {
          case FLOPPY_SECTOR_NOT_FOUND:
            t->statusRegister |= FLOPPY_STATUS_SECTOR_NOT_FOUND;
            break;
          case FLOPPY_DELETED_DATA:
            t->statusRegister |= FLOPPY_STATUS_DELETED_DATA_MARK;
            break;
          case FLOPPY_CRC_ERROR:
            t->statusRegister |= FLOPPY_STATUS_CRC_ERROR;
            break;
          case FLOPPY_OK:
            t->statusRegister |= FLOPPY_STATUS_DRIVE_READY;
            break;
          }
          return 0;
        }, then);                
      break;
    case 8: // Restore Selected Drive (seek to track 0)
      printLog("INFO", "Doing a restore to track 0.\n");
      statusRegister &= ~FLOPPY_STATUS_DRIVE_READY;
      timeoutInNanosecs(&then, 100000000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "100ms timeout floppy restore is ready\n");
          t->floppyDrives[t->selectedDrive]->setTrack(0);
          t->statusRegister |= FLOPPY_STATUS_DRIVE_READY;
          return 0;
        }, then);     
      break;
    case 9:
      printLog("INFO", "Select buffer page = %d\n", 0x3 & (data>>6));
      selectedBufferPage = 0x3 & (data>>6);
      break;
    case 10:
    case 11:
      return 1;

  }

  return 0;
}
int IOController::FloppyDevice::exCom2(unsigned char data){
  struct timespec then;
  statusRegister &= ~FLOPPY_STATUS_DRIVE_READY;
  printLog("INFO","Seek to track %d\n", data);
  if (data>76) {
    data = 76;
  }
  timeoutInNanosecs(&then, 10000000);
  addToTimerQueue([t = this, tr=data](class callbackRecord *c) -> int {
        printLog("INFO", "10ms timeout floppy seek is ready\n");
        t->floppyDrives[t->selectedDrive]->setTrack(tr);
        t->statusRegister |= FLOPPY_STATUS_DRIVE_READY;
        return 0;
      }, then);   
  return 0;
}

int IOController::FloppyDevice::exCom3(unsigned char data){
  struct timespec then;
  printLog("INFO","COmmand word %02x, Select sector %d\n", data & 0xff, data & 0xf );
  floppyDrives[selectedDrive]->setSector(data & 0xf);
  statusRegister &= ~FLOPPY_STATUS_DRIVE_READY;
  timeoutInNanosecs(&then, 10000);
  addToTimerQueue([t = this](class callbackRecord *c) -> int {
        printLog("INFO", "10us timeout floppy select sector is ready\n");
        t->statusRegister |= FLOPPY_STATUS_DRIVE_READY;
        return 0;
      }, then);   
  return 0;
}
int IOController::FloppyDevice::exCom4(unsigned char data){
  //printLog("INFO", "Setting bufferAddress=%d\n", data);
  bufferAddress = data;
  return 0;
}
int IOController::FloppyDevice::exBeep(){
  return 0;
}
int IOController::FloppyDevice::exClick(){
  return 0; // For some reason the DOS.C does a EX_CLICK operation towards the floppy disk interface which isn't documented. I wonder why. Now we don not halt any longer.
}
int IOController::FloppyDevice::exDeck1(){
  return 1;
}
int IOController::FloppyDevice::exDeck2(){
  return 1;
}
int IOController::FloppyDevice::exRBK(){
  return 1;
}
int IOController::FloppyDevice::exWBK(){
  return 1;
}
int IOController::FloppyDevice::exBSP(){
  return 1;
}
int IOController::FloppyDevice::exSF(){
  return 1;
}
int IOController::FloppyDevice::exSB(){
  return 1;
}
int IOController::FloppyDevice::exRewind(){
  return 1;
}
int IOController::FloppyDevice::exTStop(){
  return 1;
}

int IOController::FloppyDevice::openFile(int drive, std::string fileName, bool writeProtect,  bool writeBack){
  return floppyDrives[drive]->openFile(fileName, writeProtect, writeBack);
}

void IOController::FloppyDevice::closeFile(int drive){
  floppyDrives[drive]->closeFile();
}

IOController::FloppyDevice::FloppyDevice() {
  statusRegister = 0;
  for (int i=0; i<4; i++) {
    floppyDrives[i] = new FloppyDrive();
  }
}




unsigned char IOController::Disk9350Device::input () {
  if (status) {
    if (drives[selectedDrive]->isOnline()) {
      statusRegister |= DISK9350_STATUS_DRIVE_ONLINE;
    } else {
      statusRegister &= ~DISK9350_STATUS_DRIVE_ONLINE;
    }
    if (drives[selectedDrive]->isWriteProtected()) {
      statusRegister |= DISK9350_STATUS_WRITE_PROTECT_ENABLE; 
    } else {
      statusRegister &= ~DISK9350_STATUS_WRITE_PROTECT_ENABLE;
    }    
    return statusRegister;
  } else {
    printLog("INFO", "Reading data from 9350 bufferPage %d address %d = %02X\n", selectedBufferPage, bufferAddress, 0xff&buffer[selectedBufferPage][bufferAddress]);
    return buffer[selectedBufferPage][bufferAddress];
  }
}
int IOController::Disk9350Device::exWrite(unsigned char data) {
  //printLog("INFO", "9350 Writing data %02X to address %d in bufferPage %d\n", data&0xff, bufferAddress, selectedBufferPage);
  buffer[selectedBufferPage][bufferAddress]=data;
  return 0;
} 
int IOController::Disk9350Device::exCom1(unsigned char data) {
  struct timespec then;
  long address; 
  switch (0xf & data) {
    case 0:
    case 1:
    case 2:
    case 3:
      // Select drive 0..3
      selectedDrive = 0x3 & data;
      printLog("INFO", "Selecting drive %d\n", 0x3&data);
      statusRegister &= ~DISK9350_STATUS_DRIVE_READY;
      timeoutInNanosecs(&then, 10000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "10us timeout 9350 drive select drive is ready\n");
          t->statusRegister |= DISK9350_STATUS_DRIVE_READY;
          return 0;
        },
        then);

      return 0;
    case 4:
      // Clear selected buffer page to all zeros. Set page byte address to zero.
      for (int i=0; i<256; i++) {
        buffer[selectedBufferPage][i]=0;
      }
      bufferAddress = 0;
      return 0;
    case 5:
      // Read selected sector onto selected buffer page.
      printLog("INFO", "Reading from 9350 drive\n");
      statusRegister &= ~(DISK9350_STATUS_CONTROLLER_READY | DISK9350_STATUS_CRC_ERROR | DISK9350_STATUS_INVALID_SECTOR_ADDRESS);
      address = cylinder * head * sector;
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this, address=address](class callbackRecord *c) -> int {
          printLog("INFO", "10ms timeout 9350 disk read is ready\n");
          t->statusRegister |= DISK9350_STATUS_CONTROLLER_READY;
          t->drives[t->selectedDrive]->readSector(t->buffer[t->selectedBufferPage], address);
          return 0;
        }, then);      
      return 0;
      // Write selected buffer page onto selected sector.
    case 6:
      // Same as 6 followd by a read check of CRC. Implemented exactly as 6. No Read done. 
    case 7:
      printLog("INFO", "Writing to 9350 drive\n");
      statusRegister &= ~(DISK9350_STATUS_CONTROLLER_READY | DISK9350_STATUS_CRC_ERROR | DISK9350_STATUS_INVALID_SECTOR_ADDRESS);
      address = cylinder * head * sector;
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this, address=address](class callbackRecord *c) -> int {
          int ret;
          printLog("INFO", "10ms timeout 9350 disk write is ready\n"); 
          ret = t->drives[t->selectedDrive]->writeSector(t->buffer[t->selectedBufferPage], address);
          if (ret!=0) {
            t->statusRegister |= DISK9350_STATUS_WRITE_PROTECT_ENABLE; 
          }
          t->statusRegister |= DISK9350_STATUS_CONTROLLER_READY;
          return 0;
        }, then);      
      return 0;
      // Restore selected drive.
    case 8:
      printLog("INFO", "Restoring drive %d\n", selectedDrive);
      statusRegister &= ~DISK9350_STATUS_DRIVE_READY;
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "1ms timeout 9350 restore drive is ready\n");
          t->cylinder = 0;
          t->statusRegister |= DISK9350_STATUS_DRIVE_READY;
          return 0;
        },
        then);

      return 0;
      // Select buffer page specified by bits 6,7.
    case 9:
      bufferAddress = (data >> 4) & 0xf;
      return 0;
    default:
      return 1;
  }
  return 1;
}
int IOController::Disk9350Device::exCom2(unsigned char data){
  // Select Cylinder number (0..312 octal)
  cylinder = data;
  return 0;
}
int IOController::Disk9350Device::exCom3(unsigned char data){
  // Select Sector number bits 0..4. Select track bit 5.
  sector = 0x1f & data;
  head = (data >> 5) & 1;
  return 0;
}
int IOController::Disk9350Device::exCom4(unsigned char data){
  // Select Buffer Page Byte Adderss (0-255 Decimal 0.377 Octal)
  bufferAddress = data;
  return 0;
}
int IOController::Disk9350Device::exBeep(){
  return 1;
}
int IOController::Disk9350Device::exClick(){
  return 1;
}
int IOController::Disk9350Device::exDeck1(){
  return 1;
}
int IOController::Disk9350Device::exDeck2(){
  return 1;
}
int IOController::Disk9350Device::exRBK(){
  return 1;
}
int IOController::Disk9350Device::exWBK(){
  return 1;
}
int IOController::Disk9350Device::exBSP(){
  return 1;
}
int IOController::Disk9350Device::exSF(){
  return 1;
}
int IOController::Disk9350Device::exSB(){
  return 1;
}
int IOController::Disk9350Device::exRewind(){
  return 1;
}
int IOController::Disk9350Device::exTStop(){
  return 1;
}

int IOController::Disk9350Device::openFile (int drive, std::string fileName, bool wp) {
  return drives[drive]->openFile(fileName, wp);
}

void IOController::Disk9350Device::closeFile (int drive) {
  drives[drive]->closeFile();
}

IOController::Disk9350Device::Disk9350Device() {
  statusRegister = 0;
  drives[0] = new Disk9350Drive();
  drives[1] = new Disk9350Drive();
  drives[2] = new Disk9350Drive();
  drives[3] = new Disk9350Drive();
}

int IOController::Disk9350Device::Disk9350Drive::openFile (std::string fileName, bool wp) {
  // try to open file. If it fails to open create an empty file and attach it insted.
  struct stat buffer;
  if (file != NULL) {
    closeFile();
  }
  writeProtected = wp;
  if (stat (fileName.c_str(), &buffer) == 0) {
    printLog("INFO", "Open old file %s.\n", fileName.c_str());
    file = fopen (fileName.c_str(), "w");
  } else {
    char b [256];
    printLog("INFO", "Open mew file %s.\n", fileName.c_str());
    memset(b, 0, 256);
    file = fopen (fileName.c_str(), "w");
    for (int i=0; i < 0312*027*2; i++) {
      fwrite(b, 256, 1, file); 
    }
    rewind(file);
  }
  return 0;
}

void  IOController::Disk9350Device::Disk9350Drive::closeFile() {
  fclose(file);
  file = NULL;
}

int IOController::Disk9350Device::Disk9350Drive::readSector(char * buffer, long address) {
  fseek(file, address, SEEK_SET);
  fread(buffer, 1, 256, file);
  return 0;
}

int IOController::Disk9350Device::Disk9350Drive::writeSector(char * buffer, long address) {
  if (writeProtected) return 1;
  fseek(file, address, SEEK_SET);
  fwrite(buffer, 1, 256, file);  
  return 0;
}

bool IOController::Disk9350Device::Disk9350Drive::isOnline() {
  return file!=NULL;
}

bool IOController::Disk9350Device::Disk9350Drive::isWriteProtected() {
  return writeProtected;
}

unsigned char IOController::Disk9370Device::input () {
  if (status) {
    if (drives[selectedDrive]->isOnline()) {
      statusRegister |= DISK9370_STATUS_DRIVE_ONLINE;
    } else {
      statusRegister &= ~DISK9370_STATUS_DRIVE_ONLINE;
    }
    if (drives[selectedDrive]->isWriteProtected()) {
      statusRegister |= DISK9370_STATUS_WRITE_PROTECT_ENABLE; 
    } else {
      statusRegister &= ~DISK9370_STATUS_WRITE_PROTECT_ENABLE;
    }     
    return statusRegister;
  } else {
    return dataRegister;
  }
}
int IOController::Disk9370Device::exWrite(unsigned char data) {
  //printLog("INFO", "9370 Writing data %02X to address %d in bufferPage %d\n", data&0xff, bufferAddress, selectedBufferPage);
  buffer[selectedBufferPage][bufferAddress]=data;
  return 0;
} 
int IOController::Disk9370Device::exCom1(unsigned char data){
  struct timespec then;
  long address; 
  switch (data & 0xf) {
    case 0: // Master clear
      tmp=0;
      statusRegister=0;
      return 0;
    case 1: // Disk read
      printLog("INFO", "Reading from 9370 drive\n");
      statusRegister &= ~(DISK9370_STATUS_SECTOR_NOT_FOUND | DISK9370_STATUS_SECTOR_NOT_FOUND);
      statusRegister |= (DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
      address = cylinder * head * sector;
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this, address=address](class callbackRecord *c) -> int {
          printLog("INFO", "10ms timeout 9370 disk read is ready\n");
          t->statusRegister &= ~(DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
          t->drives[t->selectedDrive]->readSector(t->buffer[t->selectedBufferPage], address);
          return 0;
        }, then);      
      return 0;
    case 2: // Disk write
    case 3: // Disk write verify. Same as 2 since we are not checking CRC in the simulator.
      printLog("INFO", "Writing to 9370 drive\n");
      statusRegister &= ~(DISK9370_STATUS_SECTOR_NOT_FOUND | DISK9370_STATUS_SECTOR_NOT_FOUND);
      statusRegister |= (DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
      address = cylinder * head * sector;
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this, address=address](class callbackRecord *c) -> int {
          int ret;
          printLog("INFO", "10ms timeout 9370 disk write is ready\n"); 
          ret = t->drives[t->selectedDrive]->writeSector(t->buffer[t->selectedBufferPage], address);
          if (ret!=0) {
            t->statusRegister |= DISK9370_STATUS_WRITE_PROTECT_ENABLE; 
          }
        t->statusRegister &= ~(DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
          return 0;
        }, then);      
      return 0;
    case 4: // Restore selected drive
      cylinder = 0;
      printLog("INFO", "Restoring drive %d\n", selectedDrive);
      statusRegister |= DISK9370_STATUS_DRIVE_BUSY;
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "1ms timeout 9350 restore drive is ready\n");
          t->statusRegister &= ~DISK9370_STATUS_DRIVE_BUSY;
          return 0;
        },
        then);

      return 0;
    case 5: // Select Physical Drive as per contents of the EX COM2 register 0-7
      selectedDrive = tmp & 0x7;
      printLog("INFO", "Selecting drive %d\n", 0x7&data);
      statusRegister |= DISK9370_STATUS_DRIVE_BUSY;
      timeoutInNanosecs(&then, 10000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "10us timeout 9350 drive select drive is ready\n");
          t->statusRegister &= ~DISK9370_STATUS_DRIVE_BUSY;
          return 0;
        },
        then);
      return 0;
    case 6: // Select cylinder as per contents of EX COM2 Register 0-312 octal (9374 - Sets upper 8 bits of cylinder address)
      cylinder = tmp;
      return 0;
    case 7: // Verify Drive type 001 -> Datapoint 9370, 020 -> Datapoint 9374 ???? What is this??
      return 1;  // Don't know how to really implement this.
    case 8: // Format track 
      printLog("INFO", "Formatting a track on a 9370 drive\n");
      statusRegister &= ~(DISK9370_STATUS_SECTOR_NOT_FOUND | DISK9370_STATUS_SECTOR_NOT_FOUND);
      statusRegister |= (DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
      address = cylinder * head * sector;
      timeoutInNanosecs(&then, 3000000);
      addToTimerQueue([t = this, address=address](class callbackRecord *c) -> int {
          int ret;
          printLog("INFO", "3ms timeout 9370 disk write is ready\n"); 
          ret = t->drives[t->selectedDrive]->writeSector(t->buffer[t->selectedBufferPage], address);
          if (ret!=0) {
            t->statusRegister |= DISK9370_STATUS_WRITE_PROTECT_ENABLE; 
          }
        t->statusRegister &= ~(DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
          return 0;
        }, then);      
      return 0;
    case 9: // Select head as per contents of EX COM2 Register 0-19 decimal 0.-23 octal (9364 - 0-17 octal)
      head = tmp;
      return 0;
    case 10: // Select Sector as per contents of EX COM2 Register (0-24 decimal, 0-27 octal) 9374 - Sets upper 5 bits of sector address
      sector = tmp;
      return 0;
    case 11: // Clear Buffer Parity Error
      return 0;
    case 12: // Diagnostic Reset: Clear File Unsafe (9374 - not used)
      return 0;
    case 13: // Set Track Offset per contents pf EX COM2 register (9374 only)
      return 0;
    default:
      return 1; 
  }
}
int IOController::Disk9370Device::exCom2(unsigned char data){
  tmp = data;
  return 0;
}
int IOController::Disk9370Device::exCom3(unsigned char data){
  selectedBufferPage = data & 0xf;
  return 0;
}
int IOController::Disk9370Device::exCom4(unsigned char data){
  // Select Buffer Page Byte Adderss (0-255 Decimal 0.377 Octal)
  bufferAddress = data;
  return 0;
}
int IOController::Disk9370Device::exBeep(){
  return 1;
}
int IOController::Disk9370Device::exClick(){
  return 1;
}
int IOController::Disk9370Device::exDeck1(){
  return 1;
}
int IOController::Disk9370Device::exDeck2(){
  return 1;
}
int IOController::Disk9370Device::exRBK(){
  return 1;
}
int IOController::Disk9370Device::exWBK(){
  return 1;
}
int IOController::Disk9370Device::exBSP(){
  return 1;
}
int IOController::Disk9370Device::exSF(){
  return 1;
}
int IOController::Disk9370Device::exSB(){
  return 1;
}
int IOController::Disk9370Device::exRewind(){
  return 1;
}
int IOController::Disk9370Device::exTStop(){
  return 1;
}

int IOController::Disk9370Device::openFile (int drive, std::string fileName, bool wp) {
  return drives[drive]->openFile(fileName, wp);
}

void IOController::Disk9370Device::closeFile (int drive) {
  drives[drive]->closeFile();
}


IOController::Disk9370Device::Disk9370Device() {
  statusRegister = 0;
  drives[0] = new Disk9370Drive();
  drives[1] = new Disk9370Drive();
  drives[2] = new Disk9370Drive();
  drives[3] = new Disk9370Drive();
  drives[4] = new Disk9370Drive();
  drives[5] = new Disk9370Drive();
  drives[6] = new Disk9370Drive();
  drives[7] = new Disk9370Drive();  
}


int IOController::Disk9370Device::Disk9370Drive::openFile (std::string fileName, bool wp) {
  // try to open file. If it fails to open create an empty file and attach it insted.
  struct stat buffer;
  if (file != NULL) {
    closeFile();
  }
  writeProtected = wp;
  if (stat (fileName.c_str(), &buffer) == 0) {
    printLog("INFO", "Open old file %s.\n", fileName.c_str());
    file = fopen (fileName.c_str(), "w");
  } else {
    char b [256];
    printLog("INFO", "Open mew file %s.\n", fileName.c_str());
    memset(b, 0, 256);
    file = fopen (fileName.c_str(), "w");
    for (int i=0; i < 0312*027*2; i++) {
      fwrite(b, 256, 1, file); 
    }
    rewind(file);
  }
  return 0;
}

void  IOController::Disk9370Device::Disk9370Drive::closeFile() {
  fclose(file);
  file = NULL;
}

int IOController::Disk9370Device::Disk9370Drive::readSector(char * buffer, long address) {
  fseek(file, address, SEEK_SET);
  fread(buffer, 1, 256, file);
  return 0;
}

int IOController::Disk9370Device::Disk9370Drive::writeSector(char * buffer, long address) {
  if (writeProtected) return 1;
  fseek(file, address, SEEK_SET);
  fwrite(buffer, 1, 256, file);  
  return 0;
}

bool IOController::Disk9370Device::Disk9370Drive::isOnline() {
  return file!=NULL;
}

bool IOController::Disk9370Device::Disk9370Drive::isWriteProtected() {
  return writeProtected;
}