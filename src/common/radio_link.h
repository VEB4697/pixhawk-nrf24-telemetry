// radio_link.h - thin wrapper over RF24 for the PTX (ground) / PRX (air) roles.
// All hardware access to the nRF24 goes through here; LinkCore stays hardware-free.
// Satisfies the compile-time contracts in lib/LinkCore/src/radio_traits.h (checked below).
#pragma once
#include <Arduino.h>
#include <RF24.h>
#include "LinkCore.h"
#include "radio_config.h"

class RadioLink {
 public:
  RadioLink(uint8_t cePin, uint8_t csnPin) : radio_(cePin, csnPin) {}

  // Initialise SPI + radio for the given role. Returns false if the config is invalid, the chip does not
  // answer, or a read-back of channel/data-rate shows the SPI writes did not stick.
  bool begin(linkcore::Role role, const RadioConfig& cfg);

  // Cheap health probe: chip answers on SPI AND the channel register still holds our value
  // (a brown-out resets it to the default, which is how we detect a silent power dip).
  bool healthy();

  // Re-run begin() with the last role/config (use after healthy() fails).
  bool reinit() { return begin(role_, cfg_); }

  linkcore::Role role() const { return role_; }
  const RadioConfig& config() const { return cfg_; }

  // ---- PTX (ground) ----
  // Send one frame. Blocks until ACK or retries exhausted (~ms). Returns true if ACKed.
  // If the ACK carried a payload it is copied to rx/rxLen (rxLen = 0 otherwise). `retries` = ARC_CNT.
  bool exchange(const uint8_t* tx, uint8_t txLen, uint8_t* rx, uint8_t& rxLen, uint8_t& retries);

  // ---- PRX (air) ----
  // Fetch one received frame if present (rx must hold 32 bytes).
  bool receive(uint8_t* rx, uint8_t& rxLen);
  // Queue an ACK payload (sent with the ACK of the NEXT received frame). False if the FIFO is full.
  bool loadAck(const uint8_t* tx, uint8_t txLen);
  // Drop anything waiting in the TX FIFO (queued ACK payloads on PRX). Used when the link goes stale.
  void flushTx() { radio_.flush_tx(); }

  RF24& raw() { return radio_; }

 private:
  RF24 radio_;
  linkcore::Role role_ = linkcore::Role::Prx;
  RadioConfig cfg_ = kRadioConfig;
};

static_assert(linkcore::traits::IsPtxRadio<RadioLink>::value, "RadioLink must satisfy the PTX radio contract");
static_assert(linkcore::traits::IsPrxRadio<RadioLink>::value, "RadioLink must satisfy the PRX radio contract");
