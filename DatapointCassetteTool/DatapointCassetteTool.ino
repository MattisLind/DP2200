#include <SPI.h>
#include "RingBuffer.h"

class RingBuffer txBuffer;

/*
 * 
 *t



Lastbit = 0
Bitsinoutbuffer = 0

fill ringbuffer with  0 pattern (even number of bytes worth. Approximately 270 bytes


While not out of buffer
  // add the sync pattern
  call function too shift in bit above with argument 0
  call function too shift in bit above with argument 1
  call function too shift in bit above with argument 0
  Get one byte from buffer
  loop for all 8 bits
    Start with MSB bit. Mask for this bit 
    call function too shift in bit above with argument the bit from above
  Now we have 22 bits in the buffer
  Calculate number of shifts: 32-22-bitsinoutbuffer = 10 shifts now
  shift ten times and or it to the outbitbuffer.
  we now have 22 bits shifted to the left.
  loop until bitsinoutbuffer is less than 8. I.e less than a full byte
    extract the top byte and put into the output ring buffer.
    shift 8 steps
  After first round we should have the value 6 in bitsinoutbuffer. I.e. next turn in the outher loop we will shift 4 times left.
When buffer is out of chars
  shift in 0, 1 0 sync char
  shift in ff byte
  fill up the out buffer to full 32 bits
  transfer those to the ringbuffer

Then fill ringbuffer with  0 pattern (even number of bytes worth. Approximately 270 bytes

 */

SPIClass SPI_2(2);

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  SPI_2.beginSlave(); //Initialize the SPI_2 port.
  SPI_2.setBitOrder(MSBFIRST); // Set the SPI_2 bit order
  SPI_2.setDataMode(SPI_MODE0); //Set the  SPI_2 data mode 0
  Serial.begin(9600);
  Serial.print("DATAPOINT CASSETTE TOOL> ");

}

int flag=0;

extern "C" void __irq_spi2 (void) {
  unsigned char ch;     
  if (spi_is_tx_empty(SPI2)){  
    if (txBuffer.isBufferEmpty()) {
      spi_tx_reg(SPI2, 0xff);  
      if (flag) {
        spi_irq_disable(SPI2, SPI_RXNE_INTERRUPT);  
        flag = 0;
      }
    } else {
      flag = 1;
      ch = txBuffer.readBuffer();         
      spi_tx_reg(SPI2, ch);  
    }    
  }
  if (spi_is_rx_nonempty(SPI2)) {
    ch = spi_rx_reg(SPI2);
  }
}

/*
 * 
 *  Function shift in a bit in bitaccumulator variable:
 Test bit for 0 or 1.
  If 1
    invert lastbit
    shift in lastbit in bitaccumulaor variable from the right
    Do this again.
  If 0
    invert lastbit
    shift in lastbit in bitaccumulaor variable from the right
    invert lastbit
    shift in lastbit in bitaccumulaor variable from the righ
 */

char lastBit = 0;
long bitAcc = 0;

void shiftIn (char bit) {
  if (bit) {
    lastBit ^= 1; 
    bitAcc = bitAcc << 1;
    bitAcc = bitAcc | lastBit;
    bitAcc = bitAcc << 1;
    bitAcc = bitAcc | lastBit;       
  } else {
    lastBit ^= 1; 
    bitAcc = bitAcc << 1;
    bitAcc = bitAcc | lastBit;
    lastBit ^= 1; 
    bitAcc = bitAcc << 1;
    bitAcc = bitAcc | lastBit;     
  }
}

char buffer[] = {};

void writeCassette() {
  char mask;
  int bitsinoutbuffer = 0;
  int shifts;
  int shiftOut; 
  spi_tx_reg(SPI2, 0xff); // dummy write 
  spi_rx_reg(SPI2); // dummy read
  spi_irq_enable(SPI2, SPI_RXNE_INTERRUPT);
  // fill ringbuffer with  "1" pattern (even number of bytes worth. Approximately 270 bytes
  for (int i=0; i<270; i++) { 
    while (txBuffer.isBufferFull()) Serial.write('.');
    txBuffer.writeBuffer(0xcc);
  }  
  for (i=0; i < sizeof buffer; i++) {
    char c = buffer[i];
    // Add sync pattern
    shiftIn(0);
    shiftIn(1);
    shiftIn(0);
    mask = 0x80;
    for (i=7; i >=0; i--) {
      shiftIn(c & mask);
      mask = mask >> 1;  
    }
    shifts = 10 - bitsinoutbuffer; // 32 - 22 since we now have 22 bits to be shifted out and concatenated with the remaining bits.
    shiftOut = shiftOut | bitacc << shifts;
    bitsinoutbuffer +=22;  // Add 22 new bits.
    for (;bitsinoutbuffer >= 8; bitsinoutbuffer -= 8) {
      txBuffer.writeBuffer((shiftout >> 24) & 0xff);
    }
  }
}

// the loop function runs over and over again forever
void loop() {
  int ch;
  if (Serial.available()) {
    ch = Serial.read();
    switch (c) {
      case 'S':  // Start 
      case 's':
        break;
      case 'T':  // Stop
      case 't':
        break;
      case 'W':  // Write
      case 'w':
        writeCassette();
        break;
    }
  }
}
