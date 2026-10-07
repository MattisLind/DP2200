

#include "dp2200_io_sim.h"
#include <algorithm>
#include <bitset>
#include <cerrno>
#include <sys/stat.h>






void printLog(const char *level, const char *fmt, ...);

void printBuffer(char * buffer) {
  for (int i=0;i<16;i++) {
    printLog("INFO", "Diskbuffer: %03o %03o %03o %03o %03o %03o %03o %03o %03o %03o %03o %03o %03o %03o %03o %03o\n", 0xff & *(buffer+i*16+0), 0xff & *(buffer+i*16+1), 0xff & *(buffer+i*16+2), 0xff & *(buffer+i*16+3), 0xff & *(buffer+i*16+4), 0xff & *(buffer+i*16+5), 0xff & *(buffer+i*16+6), 0xff & *(buffer+i*16+7), 0xff& *(buffer+i*16+8), 0xff & *(buffer+i*16+9), 0xff & *(buffer+i*16+10), 0xff & *(buffer+i*16+11), 0xff & *(buffer+i*16+12), 0xff & *(buffer+i*16+13), 0xff & *(buffer+i*16+14), 0xff & *(buffer+i*16+15));
  }  
}

IOController::IOController () {
  dev[0xf0] = cassetteDevice = new CassetteDevice();
  dev[0xe1] = screenKeyboardDevice = new ScreenKeyboardDevice();
  dev[0x3c] = floppyDevice = new FloppyDevice();
  dev[0x96] = parallellInterfaceAdaptorDevice = new ParallellInterfaceAdaptorDevice();
  dev[0x5a] = servoPrinterDevice = new ServoPrinterDevice();
  dev[0xc3] = localPrinterDevice = new LocalPrinterDevice();
  dev[0x78] = disk9350Device = new Disk9350Device();
  dev[0x4b] = disk9370Device = new Disk9370Device();
  dev[0x71] = disk9390Device = new Disk9390Device();
}
bool IOController::isAddressOccupied(int address) {
  if (address<0 || address>255) return false;
  return dev[address] || rims[address];
}

void IOController::selectDevices() {
  selectedDevices.clear();
  if (ioAddress<0 || ioAddress>255) return;
  // Existing peripherals retain their exact-address lookup. RIM straps
  // require their four asserted bits and ignore the remaining bus bits.
  if (dev[ioAddress]) selectedDevices.push_back(dev[ioAddress]);
  for (unsigned mask=0; mask<rims.size(); ++mask)
    if (rims[mask] && (ioAddress & mask)==mask)
      selectedDevices.push_back(rims[mask].get());
}

bool IOController::attachRim(int address, int node) {
  if (address<0 || address>255 || node<1 || node>255 ||
      std::bitset<8>(address).count()!=4 || isAddressOccupied(address)) return false;
  for (const auto & device : rims) if (device && device->id()==node) return false;
  rims[address]=std::make_unique<RimDevice>(node);
  // A newly powered module stays unselected until the next EX ADR strobe.
  return true;
}

void IOController::RimDevice::reset() {
  // Buffer and page contents are unspecified at POR. Keep them stable on
  // reset; zero initialization supplies a deterministic fresh-device state.
  statusRegister=TA | RI | POR | DA;
  disableTransmitPending=disableReceivePending=false;
  exStatus();
}

unsigned char IOController::RimDevice::input() {
  if (status) return statusRegister;
  unsigned char value=buffer[processorPage*256+address];
  ++address;
  return value;
}

int IOController::RimDevice::exWrite(unsigned char value) {
  // WRITE accesses buffer memory in either STATUS or DATA mode.
  buffer[processorPage*256+address]=value;
  ++address;
  return 0;
}

int IOController::RimDevice::exCom1(unsigned char value) {
  // Undefined command encodings have no effect. Page fields are valid only
  // for select/enable commands; bits 5..7 are reserved by the 9483 interface.
  if (value & 0xe0) return 0;
  unsigned command=value & 7, page=(value >> 3) & 3;
  if (page && command!=3 && command!=4 && command!=5) return 0;
  switch (command) {
    case 0: statusRegister &= ~IPE; break;
    case 1: disableTransmitPending=!(statusRegister & TA); break;
    case 2: disableReceivePending=!(statusRegister & RI); break;
    case 3: processorPage=page; break;
    case 4:
      if (!(statusRegister & TA)) break;
      transmitPage=page;
      disableTransmitPending=false;
      statusRegister &= ~(TA | TMA | TPE);
      break;
    case 5:
      if (!(statusRegister & RI)) break;
      receivePage=page;
      disableReceivePending=false;
      statusRegister &= ~RI;
      break;
    case 6: statusRegister &= ~POR; break;
    case 7: statusRegister &= ~RECON; break;
  }
  // Without a link, no invitation-to-transmit can complete enables/disables.
  // In particular, never manufacture TMA or receive data in Stage 1.
  return 0;
}

int IOController::exAdr (unsigned char address) {
  ioAddress = address;
  selectDevices();
  exStatus();
  return 0;
}

int IOController::exStatus () {
  // No controller responds at an absent address. Ignore its commands and
  // return zero on INPUT so the ROM can continue probing boot devices.
  for (auto * device : selectedDevices) device->exStatus();
  return 0;
}

int IOController::exData () {
  for (auto * device : selectedDevices) device->exData();
  return 0;
}

int IOController::exWrite(unsigned char data) {
  return outputToSelected(&IODevice::exWrite, data);
}

int IOController::outputToSelected(int (IODevice::*command)(unsigned char), unsigned char data) {
  int result=0;
  for (auto * device : selectedDevices) {
    int error=(device->*command)(data);
    if (!result) result=error;
  }
  return result;
}

int IOController::exCom1(unsigned char data) {
  return outputToSelected(&IODevice::exCom1, data);
}
int IOController::exCom2(unsigned char data) {
  return outputToSelected(&IODevice::exCom2, data);
}
int IOController::exCom3(unsigned char data) {
  return outputToSelected(&IODevice::exCom3, data);
}
int IOController::exCom4(unsigned char data) {
  return outputToSelected(&IODevice::exCom4, data);
}
int IOController::exBeep() {
  if (selectedDevices.empty()) return 0;
  return screenKeyboardDevice->exBeep();
}
int IOController::exClick() {
  if (selectedDevices.empty()) return 0;
  return screenKeyboardDevice->exClick();
}
int IOController::exDeck1() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exDeck1();
}
int IOController::exDeck2() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exDeck2();
}
int IOController::exRBK() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exRBK();
}
int IOController::exWBK() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exWBK();
}
int IOController::exBSP() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exBSP();
}
int IOController::exSF() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exSF();
}
int IOController::exSB() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exSB();
}
int IOController::exRewind() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exRewind();
}
int IOController::exTStop() {
  if (selectedDevices.empty()) return 0;
  return cassetteDevice->exTStop();
}

int IOController::input (bool checkParity) {
  // The restart ROM probes optional controllers. An absent device has no
  // status bits set; it must not look like a CPU access-protection fault.
  // PIN still detects the missing response as an input parity failure.
  if (selectedDevices.empty()) return checkParity ? -1 : 0;
  int first=-1, agreed=255;
  bool conflict=false;
  for (auto * device : selectedDevices) {
    int value=device->input(); // Each responder consumes its own byte once.
    if (first<0) first=value;
    else if (value!=first) conflict=true;
    agreed &= value;
  }
  // Deterministic contention policy: retain agreed 1 bits, resolve conflicting
  // bits to zero. PIN reports a conflict. This is not an electrical bus model.
  return checkParity && conflict ? -1 : agreed;
}


void IOController::IODevice::exStatus () {
  status = 1;
}

void IOController::IODevice::exData () {
  status = 0;
}

void IOController::CassetteDevice::printStatus (const char * str) {
  char buffer[256];
  buffer[0]=0;
  int n=0;
  if (statusRegister & CASSETTE_STATUS_DECK_READY) {
    n+=snprintf(buffer+n,255, "DECK_READY ");
  }
  if (statusRegister & CASSETTE_STATUS_CASSETTE_IN_PLACE) {
    n+=snprintf(buffer+n, 255, "CASSETTE_IN_PLACE ");
  }
  if (statusRegister & CASSETTE_STATUS_END_OF_TAPE) {
    n+=snprintf(buffer+n, 255, "END_OF_TAPE ");
  }
  if (statusRegister & CASSETTE_STATUS_INTER_RECORD_GAP) {
    n+=snprintf(buffer+n, 255, "INTER_RECORD_GAP ");
  }
  if (statusRegister & CASSETTE_STATUS_READ_READY) {
    n+=snprintf(buffer+n, 255, "READ_READY ");
  }
  if (statusRegister & CASSETTE_STATUS_WRITE_READY) {
    n+=snprintf(buffer+n, 255, "WRITE_READY ");
  } 
  printLog("INFO", "%s. Status register: %s\n", str, buffer);     
}

unsigned char IOController::CassetteDevice::input () {
  char buffer [256];
  //printLog("INFO", "input: status=%d statusRegister=%02X dataRegister=%02X\n", status, statusRegister, dataRegister);
  if (status) {
    printStatus("Getting status");
    if (tapeDrive[tapeDeckSelected]->isOpen()) {
      statusRegister |= CASSETTE_STATUS_CASSETTE_IN_PLACE;
    } else {
      statusRegister &= ~CASSETTE_STATUS_CASSETTE_IN_PLACE;
    }
    if (tapeDrive[tapeDeckSelected]->isWriteProtected())
      statusRegister |= CASSETTE_STATUS_WRITE_PROTECTED;
    else statusRegister &= ~CASSETTE_STATUS_WRITE_PROTECTED;
    return statusRegister;
  } else {
    snprintf(buffer, 255, "Getting data = %03o", dataRegister);
    printLog("TAPE", "READ deck=%d data=%03o\n",tapeDeckSelected,dataRegister);
    printStatus(buffer);
    statusRegister &= ~(CASSETTE_STATUS_READ_READY);
    return dataRegister;
  }
}

int IOController::CassetteDevice::exWrite(unsigned char data) {
  if (!writing || !(statusRegister & CASSETTE_STATUS_WRITE_READY)) return 1;
  removeAllCallbacks();
  writeBuffer.push_back(data);
  statusRegister &= ~CASSETTE_STATUS_WRITE_READY;
  timespec then;
  timeoutInNanosecs(&then,2800000);
  outStandingCallbacks.push_back(addToTimerQueue([this](callbackRecord *record) {
    removeFromOutstandCallbacks(record);
    if (writing) {
      statusRegister |= CASSETTE_STATUS_WRITE_READY;
      timespec end;
      timeoutInNanosecs(&end,2800000);
      outStandingCallbacks.push_back(addToTimerQueue([this](callbackRecord *gap) {
        removeFromOutstandCallbacks(gap);
        // With no next byte supplied, WBK completes the record and stops.
        if (writing && (statusRegister & CASSETTE_STATUS_WRITE_READY)) exTStop();
        return 0;
      },end));
    }
    return 0;
  },then));
  return 0;
}

int IOController::CassetteDevice::exCom1(unsigned char data) {
  printLog("INFO", "EX_COM_1 is a noop for the cassette device - why is it executed?.\n");
  return 0;
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
  printStatus("exDeck1");
  return 0;
}
int IOController::CassetteDevice::exDeck2() {
  tapeDeckSelected = 1;
  printStatus("exDeck2");
  return 0;
}


void IOController::CassetteDevice::removeAllCallbacks() {
  printLog("INFO", "RemoveAllCallbacks: Number of outstanding timers to clear = %d \n", outStandingCallbacks.size());
  for (auto *record : outStandingCallbacks) removeTimerCallback(record);
  outStandingCallbacks.clear();
}

void IOController::CassetteDevice::removeFromOutstandCallbacks(class callbackRecord * c) {
  printLog("INFO", "removeFromOutstandCallbacks %p Number of outstanding timers to clear = %d \n",c, outStandingCallbacks.size());
  auto it = std::find(outStandingCallbacks.begin(), outStandingCallbacks.end(), c);
  if (it != outStandingCallbacks.end()) {
    printLog("INFO", "Removing one outstanding callback.\n");
    outStandingCallbacks.erase(it);
  }
}
void IOController::CassetteDevice::readFromTape() {
  struct timespec then;
  if (tapeDrive[tapeDeckSelected]->isTapeOverGap()) {
    timeoutInNanosecs(&then, 70000000);
    outStandingCallbacks.push_back(addToTimerQueue([this](callbackRecord *record) {
      removeFromOutstandCallbacks(record);
      statusRegister &= ~CASSETTE_STATUS_INTER_RECORD_GAP;
      scheduleReadByte();
      return 0;
    }, then));
  } else {
    scheduleReadByte();
  }
}

void IOController::CassetteDevice::scheduleReadByte() {
  struct timespec then;
  timeoutInNanosecs(&then, 2800000);
  outStandingCallbacks.push_back(addToTimerQueue([this](callbackRecord *record) {
    removeFromOutstandCallbacks(record);
    unsigned char data = 0;
    int result = tapeDrive[tapeDeckSelected]->readByte(forward, &data);
    if (result == 2 || result == 3) {
      // EOF is still a present cassette, but provides no extra byte. Never
      // expose an uninitialized byte or keep scheduling reads past the end.
      statusRegister &= ~CASSETTE_STATUS_READ_READY;
      statusRegister |= CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_INTER_RECORD_GAP;
      if (result == 2) statusRegister &= ~CASSETTE_STATUS_CASSETTE_IN_PLACE;
      else statusRegister |= CASSETTE_STATUS_END_OF_TAPE;
      removeAllCallbacks();
      return 0;
    }
    dataRegister = data;
    statusRegister |= CASSETTE_STATUS_READ_READY;
    if (!tapeDrive[tapeDeckSelected]->isTapeOverGap()) {
      readFromTape();
      return 0;
    }
    // Use the same record completion path for the first byte of a one-byte
    // record as for the final byte of every longer record.
    struct timespec gap;
    timeoutInNanosecs(&gap, 2800000);
    outStandingCallbacks.push_back(addToTimerQueue([this, result](callbackRecord *record) {
      removeFromOutstandCallbacks(record);
      statusRegister |= CASSETTE_STATUS_INTER_RECORD_GAP;
      if (stopAtGap || result == 1) {
        struct timespec ready;
        timeoutInNanosecs(&ready, stopAtGap ? 1000000 : 70000000);
        outStandingCallbacks.push_back(addToTimerQueue([this, result](callbackRecord *record) {
          removeFromOutstandCallbacks(record);
          statusRegister |= CASSETTE_STATUS_DECK_READY;
          if (result == 1) statusRegister |= CASSETTE_STATUS_END_OF_TAPE;
          removeAllCallbacks();
          return 0;
        }, ready));
      } else {
        readFromTape();
      }
      return 0;
    }, gap));
    return 0;
  }, then));
}

int IOController::CassetteDevice::exRBK() {
  if (!(statusRegister & CASSETTE_STATUS_DECK_READY)) return 0;
  if (!tapeDrive[tapeDeckSelected]->isOpen()) return 0;
  forward = true;
  stopAtGap = true; 
  printStatus("exRBK Forward read one block");
  removeAllCallbacks();
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY | CASSETTE_STATUS_INTER_RECORD_GAP | CASSETTE_STATUS_END_OF_TAPE ); // Clear ready bit
  readFromTape();
  return 0;
}
int IOController::CassetteDevice::exWBK() {
  if (!(statusRegister & CASSETTE_STATUS_DECK_READY)) return 0;
  if (!tapeDrive[tapeDeckSelected]->isOpen()) return 0;
  if (tapeDrive[tapeDeckSelected]->isWriteProtected()) return 1;
  removeAllCallbacks();
  writing=true; writeBuffer.clear();
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY
                      | CASSETTE_STATUS_END_OF_TAPE | CASSETTE_STATUS_INTER_RECORD_GAP);
  statusRegister |= CASSETTE_STATUS_WRITE_READY;
  // A WBK with no subsequent data must still release the deck. TAPTIM uses
  // this to measure the write cycle; no empty record is added to the image.
  struct timespec idle;
  timeoutInNanosecs(&idle, 2800000);
  outStandingCallbacks.push_back(addToTimerQueue([this](callbackRecord *record) {
    removeFromOutstandCallbacks(record);
    if (writing && writeBuffer.empty()) exTStop();
    return 0;
  }, idle));
  return 0;
}
int IOController::CassetteDevice::exBSP() {
  if (!(statusRegister & CASSETTE_STATUS_DECK_READY)) return 0;
  if (!tapeDrive[tapeDeckSelected]->isOpen()) return 0;
  forward = false;
  stopAtGap = true; 
  printStatus("exBSP Backwards read one block");
  removeAllCallbacks();
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY | CASSETTE_STATUS_INTER_RECORD_GAP | CASSETTE_STATUS_END_OF_TAPE ); // Clear ready bit
  readFromTape();

  return 0;
}
int IOController::CassetteDevice::exSF() {
  if (!(statusRegister & CASSETTE_STATUS_DECK_READY)) return 0;
  if (!tapeDrive[tapeDeckSelected]->isOpen()) return 0;
  forward = true;
  stopAtGap = false; 
  printStatus("exSF Read Forward");
  removeAllCallbacks();
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY | CASSETTE_STATUS_INTER_RECORD_GAP | CASSETTE_STATUS_END_OF_TAPE ); // Clear ready bit
  readFromTape();
  return 0;
}
int IOController::CassetteDevice::exSB() {
  if (!(statusRegister & CASSETTE_STATUS_DECK_READY)) return 0;
  if (!tapeDrive[tapeDeckSelected]->isOpen()) return 0;
  // A blank cassette is already at the beginning; do not read before byte 0.
  if (tapeDrive[tapeDeckSelected]->atBeginning()) {
    removeAllCallbacks();
    statusRegister |= CASSETTE_STATUS_END_OF_TAPE | CASSETTE_STATUS_DECK_READY
                      | CASSETTE_STATUS_INTER_RECORD_GAP;
    return 0;
  }
  forward = false;
  stopAtGap = false; 
  printStatus("exSB Read Backwards");
  removeAllCallbacks();
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_READ_READY | CASSETTE_STATUS_INTER_RECORD_GAP | CASSETTE_STATUS_END_OF_TAPE ); // Clear ready bit
  readFromTape();
  return 0;
}
int IOController::CassetteDevice::exRewind() {
  struct timespec then;
  if (!(statusRegister & CASSETTE_STATUS_DECK_READY)) return 0;
  if (!tapeDrive[tapeDeckSelected]->isOpen()) return 0;
  printStatus("exrewind Rewind");
  statusRegister &= ~(CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_INTER_RECORD_GAP | CASSETTE_STATUS_END_OF_TAPE | CASSETTE_STATUS_READ_READY | CASSETTE_STATUS_WRITE_READY);
  tapeDrive[tapeDeckSelected]->rewind();
  timeoutInNanosecs(&then, 1000000);  
  outStandingCallbacks.push_back( addToTimerQueue([cd=this](class callbackRecord * c)->int {
    printLog("INFO", "1 ms timeout rewind ENTRY\n");
    cd->removeFromOutstandCallbacks(c);
    cd->statusRegister |= (CASSETTE_STATUS_END_OF_TAPE | CASSETTE_STATUS_DECK_READY);
    cd->removeAllCallbacks();
    printLog("INFO", "1 ms timeout rewind EXIT\n");
    return 0;
  }, then));  
  return 0;
}
int IOController::CassetteDevice::exTStop() {
  printStatus("exTStop");
  removeAllCallbacks();
  int result=0;
  if (writing) {
    if (!writeBuffer.empty())
      result=tapeDrive[tapeDeckSelected]->writeBlock(writeBuffer) ? 0 : 1;
    writing=false; writeBuffer.clear();
    statusRegister &= ~CASSETTE_STATUS_WRITE_READY;
    statusRegister |= CASSETTE_STATUS_INTER_RECORD_GAP;
  }
  if (tapeDrive[tapeDeckSelected]->isOpen()) {
    statusRegister |= (CASSETTE_STATUS_DECK_READY);
  } else {
    statusRegister &= ~(CASSETTE_STATUS_DECK_READY);
  }
  return result;
}


IOController::CassetteDevice::CassetteDevice () {
  tapeRunning = false;
  tapeDeckSelected = 0; 
  tapeDrive[0] = new CassetteTape();
  tapeDrive[1] = new CassetteTape();
}


bool IOController::CassetteDevice::openFile (int drive, std::string fileName, bool wp) {
  if (drive<0 || drive>1) return false;
  tapeDrive[drive]->setWriteProtected(wp);
  const bool opened=tapeDrive[drive]->openFile(fileName);
  if (drive==tapeDeckSelected && opened && !transportRunning())
    statusRegister=CASSETTE_STATUS_DECK_READY | CASSETTE_STATUS_INTER_RECORD_GAP
                   | CASSETTE_STATUS_CASSETTE_IN_PLACE;
  return opened;
}
void IOController::CassetteDevice::closeFile (int drive) {
  tapeDrive[drive]->closeFile();
}
std::string IOController::CassetteDevice::getFileName (int drive) {
  return tapeDrive[drive]->getFileName(); 
}
bool IOController::CassetteDevice::loadBoot (std::function<void(int address, unsigned char)> writeMem) {
  return tapeDrive[0]->loadBoot(writeMem);
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
    if (console->getDisplayButton()) {
      statusRegister |= 0010;
    } else {
      statusRegister &= ~0010;
    }
    if (console->getKeyboardButton()) {
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
  if (!loadingFont) {
    int ret = console->writeCharacter(data);
    if (incrementXOnWrite) {
      console->incrementXPos();
    } 
    return ret;    
  } else {
      console->updateCharGen(data);
      return 0;
  }
} 
int IOController::ScreenKeyboardDevice::exCom1(unsigned char data){
  //
  loadingFont = false;
  if (data & SCRNKBD_COM1_ROLL_DOWN) { // roll down
    console->scrollDown();
  } 
  if (data & SCRNKBD_COM1_ERASE_EOF) {
    console->eraseFromCursorToEndOfFrame();
  }
  if (data & SCRNKBD_COM1_ERASE_EOL) {
    console->eraseFromCursorToEndOfLine();
  }
  if (data & SCRNKBD_COM1_ROLL) { // roll up 
    console->scrollUp();
  }
  if (data & SCRNKBD_COM1_CURSOR_ONOFF) {
    console->showCursor(true);
  } else {
    console->showCursor(false);
  }
  if (data & SCRNKBD_COM1_KDB_LIGHT) {
    console->setKeyboardLight(true);
  } else {
    console->setKeyboardLight(false);
  }
  if (data & SCRNKBD_COM1_DISP_LIGHT) {
    console->setDisplayLight(true);
  } else {
    console->setDisplayLight(false);
  }
  if (data & SCRNKBD_COM1_AUTO_INCREMENT) { 
    incrementXOnWrite=true;
  } else {
    incrementXOnWrite=false;
  }
  return 0;
}
int IOController::ScreenKeyboardDevice::exCom2(unsigned char data){
  return console->setCursorX(data);
}
int IOController::ScreenKeyboardDevice::exCom3(unsigned char data){
  return console->setCursorY(data);
}
int IOController::ScreenKeyboardDevice::exCom4(unsigned char data){
  loadingFont=true;
  console->setCharGenChar(data);
  return 0; 
}
int IOController::ScreenKeyboardDevice::exBeep(){
  console->soundBeep();
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
  // Quick Reference Guide, CRT/KEYBOARD: bit 4 identifies the RAM display.
  // DOS FUNC11 tests this bit before attempting to load its font.
  statusRegister = SCRNKBD_STATUS_CRT_READY | SCRNKBD_STATUS_RAM_DISPLAY;
  incrementXOnWrite = false;
  loadingFont = false;
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
    char tmp;
    printLog("INFO", "Reading floppy data (%03o) from buffer address (%03o) in selectedBufferPage=%d\n", 0xff&buffer[selectedBufferPage][bufferAddress], 0xff & bufferAddress, selectedBufferPage);
    tmp = buffer[selectedBufferPage][bufferAddress];
    bufferAddress++;
    if (bufferAddress==256) bufferAddress=0;
    return tmp;

  }
}
int IOController::FloppyDevice::exWrite(unsigned char data) {
  printLog("INFO", "Floppy writing data %03o to address %03o in bufferPage %d\n", data&0xff, bufferAddress, selectedBufferPage);
  buffer[selectedBufferPage][bufferAddress]=data;
  bufferAddress++;
  if (bufferAddress==256) bufferAddress=0;  
  return 0;

} 
int IOController::FloppyDevice::exCom1(unsigned char data){
  struct timespec then;
  switch (data & 0xf) {
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
    char tmp;
    printLog("INFO", "Reading data from 9350 bufferPage %d address %d = %02X\n", selectedBufferPage, bufferAddress, 0xff&buffer[selectedBufferPage][bufferAddress]);
    tmp =  buffer[selectedBufferPage][bufferAddress];
    if (bufferAddress == 0377) {
      bufferAddress=0;
      statusRegister |= DISK9350_STATUS_OVERFLOW;
    } else {
      bufferAddress++;
    }
    return tmp;
  }
}
int IOController::Disk9350Device::exWrite(unsigned char data) {
  //printLog("INFO", "9350 Writing data %02X to address %d in bufferPage %d\n", data&0xff, bufferAddress, selectedBufferPage);
  buffer[selectedBufferPage][bufferAddress]=data;
   if (bufferAddress == 0377) {
      bufferAddress=0;
      statusRegister |= DISK9350_STATUS_OVERFLOW;
    } else {
      bufferAddress++;
    }  
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
      statusRegister &= ~(DISK9350_STATUS_DRIVE_READY | DISK9350_STATUS_CONTROLLER_READY);
      timeoutInNanosecs(&then, 10000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "10us timeout 9350 drive select drive is ready\n");
          t->statusRegister |= DISK9350_STATUS_DRIVE_READY | DISK9350_STATUS_CONTROLLER_READY;
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
      printLog("INFO", "Clear buffer page %d from 9350 drive\n", selectedBufferPage);
      statusRegister &= ~(DISK9350_STATUS_CONTROLLER_READY | DISK9350_STATUS_CRC_ERROR | DISK9350_STATUS_INVALID_SECTOR_ADDRESS);
      address = (cylinder * 24 * 2 + head * 24 + sector) * 256;
      timeoutInNanosecs(&then, 500000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "500us timeout 9350 disk read is ready\n");
          t->statusRegister |= (DISK9350_STATUS_CONTROLLER_READY);
          t->statusRegister &= ~(DISK9350_STATUS_OVERFLOW);
          for (int i=0; i<256; i++) {
            t->buffer[t->selectedBufferPage][i]=0;
          }
          t->bufferAddress = 0;
          return 0;
        }, then);      
      return 0;
    case 5:
      // Read selected sector onto selected buffer page.
      printLog("INFO", "Reading from 9350 drive\n");
      statusRegister &= ~(DISK9350_STATUS_CONTROLLER_READY | DISK9350_STATUS_CRC_ERROR | DISK9350_STATUS_INVALID_SECTOR_ADDRESS);
      address = (cylinder * 24 * 2 + head * 24 + sector) * 256;
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
      statusRegister &= ~(DISK9350_STATUS_CONTROLLER_READY | DISK9350_STATUS_CRC_ERROR | DISK9350_STATUS_INVALID_SECTOR_ADDRESS | DISK9350_STATUS_DRIVE_READY);
      address = (cylinder * 24 * 2 + head * 24 + sector) * 256;
      timeoutInNanosecs(&then, 1000000);
      addToTimerQueue([t = this, address=address](class callbackRecord *c) -> int {
          int ret;
          printLog("INFO", "10ms timeout 9350 disk write is ready\n"); 
          ret = t->drives[t->selectedDrive]->writeSector(t->buffer[t->selectedBufferPage], address);
          if (ret!=0) {
            t->statusRegister |= DISK9350_STATUS_WRITE_PROTECT_ENABLE; 
          }
          t->statusRegister |= (DISK9350_STATUS_CONTROLLER_READY | DISK9350_STATUS_DRIVE_READY);
          return 0;
        }, then);      
      return 0;
      // Restore selected drive.
    case 8:
      printLog("INFO", "Restoring drive %d\n", selectedDrive);
      statusRegister &= ~(DISK9350_STATUS_DRIVE_READY | DISK9350_STATUS_CONTROLLER_READY);
      timeoutInNanosecs(&then, 10000000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "10ms timeout 9350 restore drive is ready\n");
          t->cylinder = 0;
          t->statusRegister |= DISK9350_STATUS_DRIVE_READY;
          return 0;
        },
        then);
      timeoutInNanosecs(&then, 50000);
      addToTimerQueue([t = this](class callbackRecord *c) -> int {
          printLog("INFO", "50us controller timeout 9350 restore drive is ready\n");
          t->cylinder = 0;
          t->statusRegister |= DISK9350_STATUS_CONTROLLER_READY;
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
  struct timespec then;
  if (data > 0312) {
    statusRegister |= DISK9350_STATUS_COMMAND_ERROR;
    return 0; 
  }
  statusRegister &= ~(DISK9350_STATUS_DRIVE_READY | DISK9350_STATUS_CRC_ERROR | DISK9350_STATUS_INVALID_SECTOR_ADDRESS | DISK9350_STATUS_CONTROLLER_READY);
  timeoutInNanosecs(&then, 1000000);
  addToTimerQueue([t = this, data=data](class callbackRecord *c) -> int {
      printLog("INFO", "10ms timeout 9350 disk seek, drive is ready new track is %d\n", t->cylinder); 
      t->statusRegister |= DISK9350_STATUS_DRIVE_READY;
      t->cylinder = data;
      return 0;
    }, then);  
  timeoutInNanosecs(&then, 50000);
  addToTimerQueue([t = this, data=data](class callbackRecord *c) -> int {
      printLog("INFO", "50us timeout 9350 disk seek, controller is ready new track is %d\n", t->cylinder); 
      t->statusRegister |= DISK9350_STATUS_CONTROLLER_READY;
      t->cylinder = data;
      return 0;
    }, then);       
  return 0;
}
int IOController::Disk9350Device::exCom3(unsigned char data){
  // Select Sector number bits 0..4. Select track bit 5.
  if ((0x1f & data) > 027) {
    statusRegister |= DISK9350_STATUS_INVALID_SECTOR_ADDRESS;
    return 0;
  }
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
  if (status==1) {
    if (drives[mediaDrive()]->isOnline()) {
      statusRegister |= DISK9370_STATUS_DRIVE_ONLINE;
    } else {
      statusRegister &= ~DISK9370_STATUS_DRIVE_ONLINE;
    }
    if (drives[mediaDrive()]->isWriteProtected()) {
      statusRegister |= DISK9370_STATUS_WRITE_PROTECT_ENABLE; 
    } else {
      statusRegister &= ~DISK9370_STATUS_WRITE_PROTECT_ENABLE;
    }     
    return statusRegister;
  } else if (status == 0) {
    char tmp;
    printLog("INFO", "Reading data (%03o) from buffer address (%d) in selectedBufferPage=%d\n", buffer[selectedBufferPage][bufferAddress], bufferAddress, selectedBufferPage);
    tmp = buffer[selectedBufferPage][bufferAddress];
    bufferAddress++;
    if (bufferAddress==256) bufferAddress=0;
    return tmp;
  } else {
    // 9370=001 is documented. 9374=020 follows the original source's
    // tentative identification; the supplied diagnostics do not query it.
    return model==9374 ? 020 : 001;
  }
}
int IOController::Disk9370Device::exWrite(unsigned char data) {
  printLog("INFO", "9370 Writing data %03o to address %03o in bufferPage %d\n", data&0xff, bufferAddress, selectedBufferPage);
  buffer[selectedBufferPage][bufferAddress]=data;
  bufferAddress++;
  if (bufferAddress==256) bufferAddress=0;  
  return 0;
} 
bool IOController::Disk9370Device::validAddress() const {
  return cylinder>=0 && cylinder<cylinderCount() && head>=0 && (model==9374 ? (head&7) : head)<headCount() && sector>=0 && sector<24;
}

int IOController::Disk9370Device::exCom1(unsigned char data) {
  printLog("DISK", "COM1=%03o arg=%03o drive=%d cylinder=%d head=%d sector=%d page=%d\n",
           data,tmp,selectedDrive,cylinder,head,sector,selectedBufferPage);
  timespec then;
  const auto generation=resetGeneration;
  switch (data & 15) {
    case 0: // Master clear invalidates outstanding controller operations.
      ++resetGeneration;
      tmp=0; statusRegister=0; status=1;
      cylinder=0; head=0; sector=0; selectedBufferPage=0; bufferAddress=0;
      return 0;
    case 1: // Read
    case 2: // Write
    case 3: { // Write with CRC verification (CRC faults are not modeled).
      const int command=data & 15, drive=mediaDrive(), page=selectedBufferPage;
      const long address=(cylinder*24L*headCount()+(model==9374 ? (head&7) : head)*24L+sector)*256;
      const bool valid=validAddress();
      statusRegister &= ~(DISK9370_STATUS_SECTOR_NOT_FOUND | DISK9370_STATUS_CRC_ERROR);
      statusRegister |= DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS;
      statistics.maxCylinder=std::max(statistics.maxCylinder,cylinder);
      statistics.maxHead=std::max(statistics.maxHead,model==9374 ? (head&7) : head);
      statistics.maxSector=std::max(statistics.maxSector,sector);
      timeoutInNanosecs(&then,1000000);
      addToTimerQueue([this,generation,command,drive,page,address,valid](callbackRecord *) {
        if (generation!=resetGeneration) return 0;
        int result=1;
        if (command==1) {
          ++statistics.reads;
          if (valid) result=drives[drive]->readSector(buffer[page],address);
        } else {
          ++statistics.writes;
          if (valid) result=drives[drive]->writeSector(buffer[page],address);
        }
        if (result) {
          ++statistics.errors;
          if (command!=1 && drives[drive]->isWriteProtected())
            statusRegister |= DISK9370_STATUS_WRITE_PROTECT_ENABLE;
          else statusRegister |= DISK9370_STATUS_SECTOR_NOT_FOUND;
        }
        statusRegister &= ~(DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
        printLog("DISK", "DONE command=%d drive=%d page=%d address=%ld result=%d\n",command,drive,page,address,result);
        return 0;
      },then);
      return 0;
    }
    case 4: // Restore
      cylinder=0;
      ++statistics.seeks;
      statusRegister &= ~DISK9370_STATUS_SEEK_INCOMPLETE_ERROR;
      statusRegister |= DISK9370_STATUS_DRIVE_BUSY;
      timeoutInNanosecs(&then,1000000);
      break;
    case 5: // Select physical drive; 9374 head bit 3 selects its pack.
      selectedDrive=tmp & 7;
      statusRegister |= DISK9370_STATUS_DRIVE_BUSY;
      timeoutInNanosecs(&then,10000);
      break;
    case 6: // The 9374 uses 204 logical cylinders with eight logical heads.
      cylinder=tmp;
      ++statistics.seeks;
      statistics.maxCylinder=std::max(statistics.maxCylinder,cylinder);
      statusRegister &= ~DISK9370_STATUS_SEEK_INCOMPLETE_ERROR;
      if (cylinder>=cylinderCount()) statusRegister |= DISK9370_STATUS_SEEK_INCOMPLETE_ERROR;
      statusRegister |= DISK9370_STATUS_DRIVE_BUSY;
      timeoutInNanosecs(&then,10000000);
      break;
    case 7: // Identification is returned on INPUT until EX STATUS/EX DATA.
      status=2;
      return 0;
    case 8: { // 9374 physical tracks contain two 24-sector logical halves.
      const int drive=mediaDrive();
      const int trackHead=model==9374 ? (head & 7) & ~2 : head;
      const long address=(cylinder*24L*headCount()+trackHead*24L)*256;
      const int halves=model==9374 ? 2 : 1;
      const bool valid=validAddress();
      statusRegister &= ~(DISK9370_STATUS_SECTOR_NOT_FOUND | DISK9370_STATUS_CRC_ERROR);
      statusRegister |= DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS;
      timeoutInNanosecs(&then,3000000);
      addToTimerQueue([this,generation,drive,address,valid,halves](callbackRecord *) {
        if (generation!=resetGeneration) return 0;
        ++statistics.formats;
        char fill[256]; memset(fill,0377,sizeof fill);
        int result=valid ? 0 : 1;
        for (int half=0; half<halves && !result; ++half)
          for (int i=0; i<24 && !result; ++i)
            result=drives[drive]->writeSector(fill,address+(half*48L+i)*256);
        if (result) {
          ++statistics.errors;
          if (drives[drive]->isWriteProtected()) statusRegister |= DISK9370_STATUS_WRITE_PROTECT_ENABLE;
          else statusRegister |= DISK9370_STATUS_SECTOR_NOT_FOUND;
        }
        statusRegister &= ~(DISK9370_STATUS_DRIVE_BUSY | DISK9370_STATUS_DATA_XFER_IN_PROGRESS);
        return 0;
      },then);
      return 0;
    }
    case 9:
      head=model==9374 ? tmp & 15 : tmp;
      if (model==9370 && head>=headCount()) statusRegister |= DISK9370_STATUS_SECTOR_NOT_FOUND;
      return 0;
    case 10:
      sector=model==9374 ? tmp & 31 : tmp;
      if (model==9370 && sector>=24) statusRegister |= DISK9370_STATUS_SECTOR_NOT_FOUND;
      return 0;
    case 11:
      statusRegister &= ~DISK9370_STATUS_BUFFER_PARITY_ERROR;
      return 0;
    case 12: // File-unsafe faults are not modeled.
      return 0;
    case 13: // 9374 track offset: stored mechanical displacement is not modeled.
      return 0;
    default:
      return 1;
  }
  addToTimerQueue([this,generation](callbackRecord *) {
    if (generation==resetGeneration) statusRegister &= ~DISK9370_STATUS_DRIVE_BUSY;
    return 0;
  },then);
  return 0;
}
int IOController::Disk9370Device::exCom2(unsigned char data){
  printLog("DISK", "COM2=%03o\n",data);
  printLog("INFO", "Disk 9370 ExCom2 storing %03o data into tmp\n", data);
  tmp = data;
  return 0;
}
int IOController::Disk9370Device::exCom3(unsigned char data){
  printLog("DISK", "COM3=%03o\n",data);
  selectedBufferPage = data & 0xf;
  return 0;
}
int IOController::Disk9370Device::exCom4(unsigned char data){
  printLog("DISK", "COM4=%03o\n",data);
  // Select Buffer Page Byte Adderss (0-255 Decimal 0.377 Octal)
  bufferAddress = data;
  return 0;
}
int IOController::Disk9370Device::exBeep(){
  printLog("INFO", "Disk 9370 Beep\n");
  return 0;
}
int IOController::Disk9370Device::exClick(){
  printLog("INFO", "Disk 9370 Click\n");
  return 0;
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
  printLog("INFO", "Got a EX_SF - undocumented 9370 event!");
  return 0;
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
  if (drive<0 || drive>=driveCount()) return -1;
  return drives[drive]->openFile(fileName, wp, cylinderCount()*headCount()*24L*256);
}

void IOController::Disk9370Device::closeFile (int drive) {
  if (drive<0 || drive>=driveCount()) return;
  drives[drive]->closeFile();
}


bool IOController::Disk9370Device::setModel(int value) {
  if (value!=9370 && value!=9374) return false;
  if (value==model) return true;
  for (auto * drive : drives) if (drive->isOnline()) return false;
  model=value;
  return true;
}

void IOController::Disk9370Device::setWriteProtected(int drive, bool value) {
  if (drive>=0 && drive<driveCount()) drives[drive]->setWriteProtected(value);
}

IOController::Disk9370Device::Disk9370Device() {
  for (auto & drive : drives) drive=new Disk9370Drive();
}

int IOController::Disk9370Device::Disk9370Drive::openFile (std::string fileName, bool wp, long imageBytes) {
  FILE * next=fopen(fileName.c_str(),wp ? "rb" : "r+b");
  if (!next && !wp && errno==ENOENT) {
    next=fopen(fileName.c_str(),"w+b");
    if (next && (fseek(next,imageBytes-1,SEEK_SET)!=0 || fputc(0,next)==EOF || fflush(next)!=0)) {
      fclose(next); next=nullptr;
    }
  }
  if (!next) return -1;
  closeFile();
  file=next;
  this->fileName=fileName;
  writeProtected=wp;
  return 0;
}

void  IOController::Disk9370Device::Disk9370Drive::closeFile() {
  if (file) fclose(file);
  file = NULL;
}

int IOController::Disk9370Device::Disk9370Drive::readSector(char * buffer, long address) {
  if (!file || address<0 || fseek(file,address,SEEK_SET)!=0) return 1;
  if (fread(buffer,1,256,file)!=256) return 1;
  return 0;
}

int IOController::Disk9370Device::Disk9370Drive::writeSector(char * buffer, long address) {
  if (!file || writeProtected || address<0 || fseek(file,address,SEEK_SET)!=0) return 1;
  if (fwrite(buffer,1,256,file)!=256 || fflush(file)!=0) return 1;
  return 0;
}

bool IOController::Disk9370Device::Disk9370Drive::isOnline() {
  return file!=NULL;
}

bool IOController::Disk9370Device::Disk9370Drive::isWriteProtected() {
  return writeProtected;
}


unsigned char IOController::Disk9390Device::input () {
  if (status) {
    return statusRegister;
  } else {
    return dataRegister;
  }
}
int IOController::Disk9390Device::exWrite(unsigned char data) {
  return 1;
} 
int IOController::Disk9390Device::exCom1(unsigned char data){
  return 1;
}
int IOController::Disk9390Device::exCom2(unsigned char data){
  return 1;
}
int IOController::Disk9390Device::exCom3(unsigned char data){
  return 1;
}
int IOController::Disk9390Device::exCom4(unsigned char data){
  return 1;
}
int IOController::Disk9390Device::exBeep(){
  return 1;
}
int IOController::Disk9390Device::exClick(){
  return 1;
}
int IOController::Disk9390Device::exDeck1(){
  return 1;
}
int IOController::Disk9390Device::exDeck2(){
  return 1;
}
int IOController::Disk9390Device::exRBK(){
  return 1;
}
int IOController::Disk9390Device::exWBK(){
  return 1;
}
int IOController::Disk9390Device::exBSP(){
  return 1;
}
int IOController::Disk9390Device::exSF(){
  return 1;
}
int IOController::Disk9390Device::exSB(){
  return 1;
}
int IOController::Disk9390Device::exRewind(){
  return 1;
}
int IOController::Disk9390Device::exTStop(){
  return 1;
}

IOController::Disk9390Device::Disk9390Device() {
  statusRegister = 0;
  dataRegister = 0;
}
