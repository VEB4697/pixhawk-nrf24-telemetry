// Host-side Arduino stub for the firmware tests: controllable clock, GPIO log, fake serial ports.
// NOT shipped to the target - only used by the native test environment (-Itest/stubs).
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <string>
#include <vector>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define SERIAL_8N1 0x800001c

// ---- simulated clock (microseconds). Tests advance it; delay() also advances it ----
inline uint32_t g_simNowUs = 0;
inline uint32_t micros() { return g_simNowUs; }
inline uint32_t millis() { return g_simNowUs / 1000u; }
inline void delay(uint32_t ms) { g_simNowUs += ms * 1000u; }
inline void delayMicroseconds(uint32_t us) { g_simNowUs += us; }

// ---- GPIO log ----
inline int g_pinLevel[64] = {};
inline uint32_t g_digitalWrites = 0;
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t p, uint8_t v) {
  g_pinLevel[p] = v;
  g_digitalWrites++;
}

// ---- fake serial port with the subset of the Arduino Stream/Print API the firmware uses ----
struct FakePort {
  std::deque<uint8_t> in;      // bytes "arriving" from the peer (PC / Pixhawk)
  std::vector<uint8_t> out;    // everything the firmware wrote (data AND printed text, like real hardware)
  int txRoom = 1 << 20;        // availableForWrite()
  int acceptMax = 1 << 20;     // write() accepts at most this many bytes per call
  // configuration calls
  unsigned long baud = 0;
  int rxPin = -1, txPin = -1;
  size_t rxBuffer = 0;
  uint32_t txTimeoutMs = 100;
  int beginCalls = 0;

  void begin(unsigned long b) { baud = b; beginCalls++; }
  void begin(unsigned long b, uint32_t, int rx, int tx) { baud = b; rxPin = rx; txPin = tx; beginCalls++; }
  size_t setRxBufferSize(size_t n) { rxBuffer = n; return n; }
  void setTxTimeoutMs(uint32_t t) { txTimeoutMs = t; }
  int available() { return static_cast<int>(in.size()); }
  int availableForWrite() { return txRoom; }
  size_t readBytes(uint8_t* b, size_t n) {
    size_t k = 0;
    while (k < n && !in.empty()) { b[k++] = in.front(); in.pop_front(); }
    return k;
  }
  size_t write(const uint8_t* b, size_t n) {
    const size_t k = n < static_cast<size_t>(acceptMax) ? n : static_cast<size_t>(acceptMax);
    out.insert(out.end(), b, b + k);
    return k;
  }
  size_t write(uint8_t b) { return write(&b, 1); }
  size_t println(const char* s) { const size_t n = strlen(s); write(reinterpret_cast<const uint8_t*>(s), n); write('\n'); return n + 1; }
  int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n < 511 ? n : 511));
    return n;
  }
  std::string text() const { return std::string(out.begin(), out.end()); }
  void clearOut() { out.clear(); }
};
inline FakePort Serial, Serial1;

struct SPIClass {
  int beginCalls = 0;
  int sck = -1, miso = -1, mosi = -1, ss = -1;
  void begin(int a, int b, int c, int d) { beginCalls++; sck = a; miso = b; mosi = c; ss = d; }
};
inline SPIClass SPI;
