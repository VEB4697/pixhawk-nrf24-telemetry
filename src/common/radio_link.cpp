#include "radio_link.h"
#include "pins.h"

using linkcore::Role;

bool RadioLink::begin(Role role, const RadioConfig& cfg) {
  if (!validRadioConfig(cfg)) return false;  // runtime guard for future CLI/NVS-provided configs
  role_ = role;
  cfg_ = cfg;

  SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN);
  if (!radio_.begin(&SPI)) return false;     // false = no chip / wiring / power problem
  if (!radio_.isChipConnected()) return false;

  radio_.setPALevel(cfg.pa);
  if (!radio_.setDataRate(cfg.rate)) return false;   // RF24 reads the register back and reports failure
  radio_.setChannel(cfg.channel);
  radio_.setCRCLength(RF24_CRC_16);
  radio_.setAddressWidth(5);
  radio_.setAutoAck(true);
  radio_.enableDynamicPayloads();            // variable 1..32 byte packets
  radio_.enableAckPayload();                 // downlink rides on ACKs
  radio_.setRetries(cfg.retryDelay, cfg.retryCount);

  if (role == Role::Ptx) {
    radio_.openWritingPipe(cfg.address);
    radio_.stopListening();
  } else {
    radio_.openReadingPipe(1, cfg.address);
    radio_.startListening();
  }
  radio_.flush_tx();
  radio_.flush_rx();

  // Read-back verification: catches SPI wiring faults that still let begin() succeed.
  return radio_.getChannel() == cfg.channel && radio_.getDataRate() == cfg.rate;
}

bool RadioLink::healthy() {
  return radio_.isChipConnected() && radio_.getChannel() == cfg_.channel;
}

bool RadioLink::exchange(const uint8_t* tx, uint8_t txLen, uint8_t* rx, uint8_t& rxLen, uint8_t& retries) {
  rxLen = 0;
  const bool ok = radio_.write(tx, txLen);   // RF24 flushes the TX FIFO itself on MAX_RT
  retries = radio_.getARC();                 // retransmit count of the last packet
  if (ok && radio_.available()) {
    const uint8_t n = radio_.getDynamicPayloadSize();
    if (n >= 1 && n <= linkcore::kMaxFrame) {
      radio_.read(rx, n);
      rxLen = n;
    } else {
      radio_.flush_rx();                     // corrupt width: RF24 docs say flush
    }
  }
  return ok;
}

bool RadioLink::receive(uint8_t* rx, uint8_t& rxLen) {
  rxLen = 0;
  if (!radio_.available()) return false;
  const uint8_t n = radio_.getDynamicPayloadSize();
  if (n < 1 || n > linkcore::kMaxFrame) {
    radio_.flush_rx();
    return false;
  }
  radio_.read(rx, n);
  rxLen = n;
  return true;
}

bool RadioLink::loadAck(const uint8_t* tx, uint8_t txLen) {
  return radio_.writeAckPayload(1, tx, txLen);
}
