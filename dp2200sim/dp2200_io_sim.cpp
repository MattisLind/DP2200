

#include "dp2200_io_sim.h"



ioController::ioController () {
  class cassetteDrive * c = new cassetteDrive();
  //dev[0] = new cassetteDrive();
  dev[0] = c;
}

void ioController::exAdr (unsigned char address) {
  ioAddress = address;
  exStatus();
}

void ioController::exStatus () {
  dev[ioAddress]->exStatus();
}

void ioController::exData () {
  dev[ioAddress]->exData();
}

void ioController::ioDevice::exStatus () {
  status = true;
}

void ioController::ioDevice::exData () {
  status = false;
}

unsigned char ioController::ioDevice::input () {
  if (status) {
    return statusRegister;
  } else {
    return dataRegister;
  }
}

void ioController::cassetteDrive::exWrite(unsigned char data) {

}

void ioController::cassetteDrive::exCom1(unsigned char data) {

}
void ioController::cassetteDrive::exCom2(unsigned char data) {

}
void ioController::cassetteDrive::exCom3(unsigned char data) {

}
void ioController::cassetteDrive::exCom4(unsigned char data) {

}
void ioController::cassetteDrive::exBeep() {

}
void ioController::cassetteDrive::exClick() {

}
void ioController::cassetteDrive::exDeck1() {

}
void ioController::cassetteDrive::exDeck2() {

}
void ioController::cassetteDrive::exRBK() {

}
void ioController::cassetteDrive::exWBK() {

}
void ioController::cassetteDrive::exBSP() {

}
void ioController::cassetteDrive::exSF() {

}
void ioController::cassetteDrive::exSB() {

}
void ioController::cassetteDrive::exRewind() {

}
void ioController::cassetteDrive::exTStop() {

}

ioController::cassetteDrive::cassetteDrive () {

}

