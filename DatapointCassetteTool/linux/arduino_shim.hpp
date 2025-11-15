/*
 * arduino_shim.hpp — minimal Arduino/Maple API on Linux
 * ------------------------------------------------------
 * - SerialUSB emulated over a single accepted TCP socket.
 * - SPI.transfer(byte) writes ASCII '1'/'0' to a file based on MSB of byte.
 * - pinMode/digitalWrite are no-ops (we just track PB1 if you want logs).
 * - delay(ms) uses usleep.
 *
 * Build note:
 *   Define -DSHIM_LISTEN_PORT=5555 or pass port via env var in linux_main.cpp.
 */

#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <arpa/inet.h>
#include <string>
#include <atomic>
#include <sys/ioctl.h> 

// --------- GPIO stubs ---------
#define PB1   1
#define PB12  12
#define OUTPUT 1
#define HIGH 1          
#define LOW  0          
inline void pinMode(int /*pin*/, int /*mode*/) {}
inline void digitalWrite(int /*pin*/, int /*val*/) {}

// --------- time ---------
inline void delay(unsigned long ms) { usleep(ms*1000); }

// --------- SerialUSB over TCP ---------
class _ShimSerialUSB {
  int _fd = -1;                       // connected client socket
  std::atomic<bool> _connected{false};
public:
  // Set by linux_main after accept()
  void _attach_fd(int fd) { _fd = fd; _connected.store(true); }

  void begin(unsigned long /*baud*/=115200) {}     // not used
  bool isConnected() const { return _connected.load(); }

  int available() {
    if (_fd < 0) return 0;
    struct pollfd pfd{_fd, POLLIN, 0};
    int r = poll(&pfd, 1, 0);
    if (r > 0 && (pfd.revents & POLLIN)) {
      int count; ioctl(_fd, FIONREAD, &count);
      return count;
    }
    return 0;
  }

  int read() {
    if (_fd < 0) return -1;
    uint8_t b;
    ssize_t n = ::read(_fd, &b, 1);
    if (n == 1) return b;
    return -1;
  }

  size_t write(uint8_t b) {
    //printf("C: SerialUSB.write(%02X)\n", b);
    if (_fd < 0) return 0;
    return ::send(_fd, &b, 1, 0)==1 ? 1 : 0;
  }
  size_t write(const char* s) { return ::send(_fd, s, strlen(s), 0); }

  void println(const char* s) {
    write(s); write('\n');
  }
};
static _ShimSerialUSB SerialUSB;      // global like on Maple

// --------- SPI shim: MSB -> '1' or '0' to file ---------
class _ShimSPI {
  FILE* _f = nullptr;                 // opened by linux_main
public:
  void _attach_file(FILE* f) { _f = f; }
  void begin() {}
    uint8_t transfer(uint8_t v) {
        if (_f) {
            // Output the *full 8-bit SPI byte* as ASCII '0'/'1'
            // LSB-first or MSB-first? 
            // → The FM encoder produces LSB-first in bitAccumulator,
            //   and ISR sends the lowest 8 bits first.
            // So we print LSB-first too.
            
            for (int i = 0; i < 8; i++) {
                uint8_t bit = (v >> i) & 1;       // LSB first
                fputc(bit ? '1' : '0', _f);
            }
        }
        return 0;
    }
};
static _ShimSPI SPI;

// --------- compatibility ---------
class _ShimUSBConnectedOnce {
public:
  // no impl; just placeholder if sketch references others
};
