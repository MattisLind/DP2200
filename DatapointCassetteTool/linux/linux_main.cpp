/*
 * linux_main.cpp — run the *same* sketch on Linux:
 * ------------------------------------------------
 * - Opens an output file for SPI line-bits.
 * - Listens on TCP (default :5555), accepts a single host connection.
 * - Calls setup() once, then in a loop:
 *     * poll() the socket with 100 ms timeout
 *     * call loop() if data available
 *     * call onSpiTimerISR() once per iteration (simulated “timer”)
 */




#include "arduino_shim.hpp"          // Arduino layer replacement

void spi_irq_disable (SPIClass, int) {

}

void spi_irq_enable(SPIClass, int) {

}


#ifdef STANDARD
#include "../tap_fm_tx_sketch.hpp"   // <-- your exact sketch code
#endif
#ifdef FIFO
#include "../tap_fm_tx_sketch_fifo.hpp"
#endif
#ifdef LONGACC
#include "../tap_fm_sketch_64bitacc.hpp"
#endif
#ifdef LONGACC_CRITSECTION
#include "../tap_fm_sketch_64bitacc_critsections.hpp"
#endif

#ifndef SHIM_LISTEN_PORT
#define SHIM_LISTEN_PORT 5555
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int listen_and_accept(int port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  int yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
  sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr.s_addr=htonl(INADDR_ANY);
  if (bind(s,(sockaddr*)&addr,sizeof(addr))<0) { perror("bind"); exit(1); }
  if (listen(s,1)<0) { perror("listen"); exit(1); }
  fprintf(stderr,"[sim] Listening on 0.0.0.0:%d\n",port);
  int c = accept(s,nullptr,nullptr);
  if (c<0) { perror("accept"); exit(1); }
  close(s);
  fprintf(stderr,"[sim] Client connected.\n");
  return c;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,"Usage: %s <out_bits.txt> [port]\n", argv[0]);
    return 2;
  }
  const char* outpath = argv[1];
  int port = (argc>=3)? atoi(argv[2]) : SHIM_LISTEN_PORT;

  // Open output file used by SPI shim
  FILE* f = fopen(outpath, "w");
  if (!f) { perror("fopen"); return 1; }
  SPI_2._attach_file(f);

  // Accept TCP client (host) and attach to SerialUSB
  int cli = listen_and_accept(port);
  Serial._attach_fd(cli);
  int counter=0;
  // Run sketch setup()
  setup();
  printf ("After setup\n");
  // Main poll loop: 100 ms cadence acts as both input poll and ISR tick

  while (true) {
    // Poll for up to 100 ms; if readable, loop() will read
    //printf("Before poll\n");
    struct pollfd pfd{cli, POLLIN, 0};
    poll(&pfd, 1, 1);
    //printf ("r=%d\n", r);
    loop();                         // <-- handles incoming commands/hex, buffer mgmt, 'A' flow ctrl
    // Even if no input arrived, we still advance the transmitter once per tick
    if ((counter % 50) == 0) {
      onSpiTimerISR();                  // <-- same ISR function as on MCU
    }

    fflush(f);
  }
}