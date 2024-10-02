#ifndef _RING_BUFFER_
#define _RING_BUFFER_
#define SIZE 32

class RingBuffer {
  volatile int inPtr, outPtr, bufferFull, bufferEmpty;
  unsigned char buffer[SIZE];
public:
  int isBufferFull () {
    return bufferFull;
  };
  int isBufferEmpty () {
    return bufferEmpty;
  }
  void writeBuffer (unsigned char data);
  unsigned char readBuffer ();
  void  initBuffer ();
};

#endif
