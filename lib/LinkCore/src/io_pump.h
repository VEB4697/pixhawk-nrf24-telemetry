// io_pump.h - move bytes between a Stream-like port and a RingBuffer WITHOUT blocking and WITHOUT loss.
//
// `S` needs: int available(); int availableForWrite(); size_t readBytes(uint8_t*, size_t);
//            size_t write(const uint8_t*, size_t).   (Arduino HardwareSerial / HWCDC satisfy this.)
// Written as templates so the exact same code is unit-tested on the host with a fake stream.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace linkcore {

// Port -> ring. Reads at most `maxBytes` per call so the main loop stays responsive.
// Bytes the ring cannot hold are dropped by the ring and counted in ring.dropped().
template <typename S, typename Ring>
size_t pumpIn(S& port, Ring& ring, size_t maxBytes = 256) {
  uint8_t buf[64];
  size_t total = 0;
  while (total < maxBytes) {
    int av = port.available();
    if (av <= 0) break;
    size_t n = static_cast<size_t>(av);
    if (n > sizeof(buf)) n = sizeof(buf);
    if (n > maxBytes - total) n = maxBytes - total;
    n = port.readBytes(buf, n);
    if (n == 0) break;
    ring.write(buf, n);
    total += n;
  }
  return total;
}

// Ring -> port. Writes only what the port can take right now (availableForWrite) and removes from the
// ring only what the port actually accepted, so nothing is lost and the call never blocks.
template <typename S, typename Ring>
size_t pumpOut(Ring& ring, S& port, size_t maxBytes = 256) {
  uint8_t buf[64];
  size_t total = 0;
  while (total < maxBytes) {
    int room = port.availableForWrite();
    if (room <= 0) break;
    size_t n = ring.available();
    if (n == 0) break;
    if (n > sizeof(buf)) n = sizeof(buf);
    if (n > static_cast<size_t>(room)) n = static_cast<size_t>(room);
    if (n > maxBytes - total) n = maxBytes - total;
    n = ring.peek(buf, n);
    const size_t w = port.write(buf, n);
    ring.discard(w);
    total += w;
    if (w < n) break;  // port is full: try again next loop
  }
  return total;
}

}  // namespace linkcore
