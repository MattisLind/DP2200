

#include "dp2200_io_sim.h"
#include "dp2200Window.h"
#include "RegisterWindow.h"

extern class dp2200Window * dpw;
extern class registerWindow * rw;



void printLog(const char *level, const char *fmt, ...);


IOController::IOController () {
  dev[0] = cassetteDevice = new CassetteDevice();
  dev[1] = screenKeyboardDevice = new ScreenKeyboardDevice();
  supportedDevices.push_back(0);
  supportedDevices.push_back(1);
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
    return statusRegister;
  } else {
    statusRegister &= ~(SCRNKBD_STATUS_KBD_READY);
    return dataRegister;
  }
}
int IOController::ScreenKeyboardDevice::exWrite(unsigned char data) {
  return dpw->writeCharacter(data);
} 
int IOController::ScreenKeyboardDevice::exCom1(unsigned char data){
  // 
  if (data & SCRNKBD_COM1_ERASE_EOF) {
    return dpw->eraseFromCursorToEndOfFrame();  
  }
  if (data & SCRNKBD_COM1_ERASE_EOL) {
    return dpw->eraseFromCursorToEndOfLine(); 
  }
  if (data & SCRNKBD_COM1_ROLL) {
    return dpw->rollScreenOneLine();
  }
  if (data & SCRNKBD_COM1_CURSOR_ONOFF) {
    return dpw->showCursor(true);
  } else {
    return dpw->showCursor(false);
  }
  if (data & SCRNKBD_COM1_KDB_LIGHT) {
    return rw->setKeyboardLight(true);
  } else {
    return rw->setKeyboardLight(false);
  }
  if (data & SCRNKBD_COM1_DISP_LIGHT) {
    return rw->setDisplayLight(true);
  } else {
    return rw->setDisplayLight(false);
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
  return 1;
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
}
