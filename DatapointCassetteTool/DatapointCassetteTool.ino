#include <SPI.h>

/*
 * 
 * 
 * // The Datapoint 2200 records data bits using a FSK method, namely one
// half cycle at frequency X means a one bit and one full cycle at
// frequency 2X means a zero bit.  This is stated this way because
// the samples processed here are off a tape recorder running at 1 7/8 ips
// while the datapoint runs its tape at 7.5 ips, a factor of four times
// faster.  To be specific, at this reduced speed, a one bit is a half cycle
// of 481.25 Hz and a zero bit is a full cycle of 962.5 Hz.  The original
// 7.5 ips frequencies are 1925 Hz and 3850 Hz.  The preamble while the
// tape is coming up to speed is a train of 1 bits.

Thus the pwmrate has to be 962.5 *2 = 1925 Hz



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

Unit test case 

Do not use interrupts. Instead we just loop over and output over SPI. Do the check on the SIP interface.


 */


HardwareTimer pwmtimer(1);
const int pwmOutPin = PA8; // pin10
// Connect PA8 to PB13 as the clock input for SPI2
SPIClass SPI_2(2);

// the setup function runs once when you press reset or power the board
void setup() {
  int prescaler = 73;
  int divisor = 512;
  // initialize digital pin LED_BUILTIN as an output.
  SPI_2.beginSlave(); //Initialize the SPI_2 port.
  SPI_2.setBitOrder(MSBFIRST); // Set the SPI_2 bit order
  SPI_2.setDataMode(SPI_MODE0); //Set the  SPI_2 data mode 0
  pinMode(pwmOutPin, PWM);
  pwmtimer.pause();
  pwmtimer.setPrescaleFactor(prescaler);
  pwmtimer.setOverflow(divisor); 
  pwmtimer.refresh();
  pwmtimer.resume();
  pwmWrite(pwmOutPin, divisor>>1);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Serial.print("DATAPOINT CASSETTE TOOL> ");

}

int flag=0;

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

char buffer[] = {0xff, 0x00};




void writeCassette() {
  char mask;
  int bitsinoutbuffer = 0;
  int shifts;
  int shiftOut = 0; 
  int i;
  //spi_tx_reg(SPI2, 0xff); // dummy write 
  //spi_rx_reg(SPI2); // dummy read
  // fill ringbuffer with  "1" pattern (even number of bytes worth. Approximately 270 bytes
  for (int i=0; i<270; i++) { 
    while (!spi_is_tx_empty(SPI2));
    spi_tx_reg(SPI2, 0xcc);
  }  
  // The CC pattern has 0 as last bit.
  lastBit = 0;
  for (i=0; i < sizeof buffer; i++) {
    char c = buffer[i];
    // Add sync pattern
    shiftIn(0);
    shiftIn(1);
    shiftIn(0);
    mask = 0x80;
    for (int j=7; j >=0; j--) {
      shiftIn(c & mask);
      mask = mask >> 1;  
    }
    shifts = 10 - bitsinoutbuffer; // 32 - 22 since we now have 22 bits to be shifted out and concatenated with the remaining bits.
    shiftOut = shiftOut | bitAcc << shifts;
    bitsinoutbuffer +=22;  // Add 22 new bits.
    for (;bitsinoutbuffer >= 8; bitsinoutbuffer -= 8) {
      while (!spi_is_tx_empty(SPI2));
      spi_tx_reg(SPI2, (shiftOut >> 24) & 0xff); 
      shiftOut = shiftOut << 8; 
    }
  }
  for(i=8-bitsinoutbuffer;i>0; i-=2) {
    shiftIn(1);  
  }
  while (!spi_is_tx_empty(SPI2));
  spi_tx_reg(SPI2, bitAcc & 0xff);
  for (int i=0; i<270; i++) { 
    while (!spi_is_tx_empty(SPI2));
    if (lastBit == 1) {
      spi_tx_reg(SPI2, 0x33); // if we had a 1 as last bit we need to start with 0
    } else {
      spi_tx_reg(SPI2, 0xcc); // if we had a 0 as last bit we need to start with 1
    }
  } 
  spi_tx_reg(SPI2, 0x00);
}

// the loop function runs over and over again forever
void loop() {
  int ch;  
  //Serial.println("HEJ");
  if (Serial.available()) {
    ch = Serial.read();
    Serial.write(ch);
    Serial.write("\r\n");
    switch (ch) {
      case 'S':  // Start 
      case 's':
        Serial.println("Starting tape");
        break;
      case 'T':  // Stop
      case 't':
        Serial.println("Stoping tape");
        break;
      case 'W':  // Write
      case 'w':
        Serial.println("Writing buffer");
        writeCassette();
        break;
    }

    Serial.print("DATAPOINT CASSETTE TOOL> ");
  }
}
