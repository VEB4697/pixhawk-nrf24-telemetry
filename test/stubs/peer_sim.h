// peer_sim.h - simulated "other end" used to test the REAL main.cpp files on the host.
// The real firmware runs its own setup()/loop() against fake serial ports + fake RF24; this header provides the peer
// (ground station for the air tests, air unit for the ground tests) built from the same LinkCore/RadioLink code.
#pragma once
#include <stdlib.h>
#include <string>
#include "Arduino.h"
#include "RF24.h"
#include "air_unit.h"
#include "ground_unit.h"

namespace sim {

using Ring = linkcore::RingBuffer<2048>;

// Pushes a deterministic byte pattern into a fake port's input at a given rate (a Pixhawk or PC "talking").
struct Feeder {
  linkcore::PatternGen gen;
  Ring r;
  explicit Feeder(uint32_t rate = 5760) : gen(rate) {}
  void feed(FakePort& p, uint32_t nowMs) {
    gen.pump(r, nowMs);
    uint8_t b[64];
    size_t n;
    while ((n = r.read(b, sizeof(b))) > 0) p.in.insert(p.in.end(), b, b + n);
  }
  uint32_t total() const { return gen.total(); }
};

// Checks everything the firmware wrote to a (pure data) fake port.
struct Sink {
  linkcore::PatternChecker chk;
  void drain(FakePort& p) {
    if (!p.out.empty()) chk.feed(p.out.data(), p.out.size());
    p.out.clear();
  }
  const linkcore::CheckResult& r() const { return chk.result(); }
  void resync() { chk.resync(); }
};

// ---- simulated GROUND station (peer for the real AIR firmware) ----
struct GroundPeer {
  RadioLink radio{PIN_NRF_CE, PIN_NRF_CSN};
  Ring toAir, fromAir;
  linkcore::PtxBridge<RadioLink, Ring, Ring> bridge{radio, toAir, fromAir, ground_unit::bridgeConfig()};
  linkcore::PatternGen gen{5760};
  linkcore::PatternChecker chk;
  bool begin() { return radio.begin(linkcore::Role::Ptx, kRadioConfig); }
  void step(uint32_t nowMs, uint32_t nowUs, bool feed) {
    if (feed) gen.pump(toAir, nowMs);
    bridge.step(nowMs, nowUs);
    chk.consume(fromAir);
  }
};

// ---- simulated AIR unit (peer for the real GROUND firmware) ----
struct AirPeer {
  RadioLink radio{PIN_NRF_CE, PIN_NRF_CSN};
  Ring toGround, fromGround;
  linkcore::PrxBridge<RadioLink, Ring, Ring> bridge{radio, toGround, fromGround, air_unit::bridgeConfig()};
  linkcore::PatternGen gen{5760};
  linkcore::PatternChecker chk;
  bool begin() { return radio.begin(linkcore::Role::Prx, kRadioConfig); }
  void step(uint32_t nowMs, bool feed) {
    if (feed) gen.pump(toGround, nowMs);
    bridge.step(nowMs);
    chk.consume(fromGround);
  }
};

// Parse the number following the LAST occurrence of `tag` in `text` (e.g. "[AIR] link=" -> state). -1 if absent.
inline long lastField(const std::string& text, const char* tag) {
  const size_t p = text.rfind(tag);
  if (p == std::string::npos) return -1;
  return atol(text.c_str() + p + strlen(tag));
}
inline bool contains(const std::string& text, const char* s) { return text.find(s) != std::string::npos; }

}  // namespace sim
