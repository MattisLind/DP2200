#include "RingBuffer.h"
#include <stdio.h>

void RingBuffer::writeBuffer (unsigned char data) {
  //printf ("write data %X |", data);
  buffer[inPtr] = data;
  bufferEmpty = 0;
  inPtr++;
  if (inPtr == SIZE) {
    inPtr = 0;
  }
  if (inPtr == outPtr) {
    bufferFull = 1;
  } else {
    bufferFull = 0;
  }
}

unsigned char RingBuffer::readBuffer () {
  int out = buffer[outPtr];
  bufferFull = 0;
  outPtr++;
  if (outPtr == SIZE) {
    outPtr = 0;
  }
  if (inPtr == outPtr) {
    bufferEmpty = 1;
  } else {
    bufferEmpty = 0;
  }
  return out;
}

void RingBuffer::initBuffer () {
  bufferEmpty = 1;
  bufferFull = 0;
  inPtr = 0;
  outPtr = 0;
}
