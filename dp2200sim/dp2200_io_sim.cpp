

#include "dp2200_io_sim.h"

void printLog(const char *level, const char *fmt, ...);


IOController::IOController () {
  dev[0] = cassetteDevice = new CassetteDevice();
  supportedDevices.push_back(0);
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

unsigned char IOController::IODevice::input () {
  printLog("INFO", "status=%d statusRegister=%02X dataRegister=%02X\n", status, statusRegister, dataRegister);
  if (status) {
    return statusRegister;
  } else {
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
int IOController::CassetteDevice::exRBK() {
  return 1;
}
int IOController::CassetteDevice::exWBK() {
  return 1;
}
int IOController::CassetteDevice::exBSP() {
  return 1;
}
int IOController::CassetteDevice::exSF() {
  return 1;
}
int IOController::CassetteDevice::exSB() {
  return 1;
}
int IOController::CassetteDevice::exRewind() {
  return 1;
}
int IOController::CassetteDevice::exTStop() {
  statusRegister |= CASSETTE_STATUS_DECK_READY;
  return 0;
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
void IOController::CassetteDevice::loadBoot (unsigned char * address) {
  tapeDrive[0]->loadBoot(address);
}
