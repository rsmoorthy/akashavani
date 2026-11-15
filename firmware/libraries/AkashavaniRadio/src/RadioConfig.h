#pragma once

#include <cstddef>
#include <cstdint>

namespace akashavani {

enum class RadioChip : uint8_t {
  Rfm95 = 0,
  Rfm69 = 1,
};

enum class RadioModulation : uint8_t {
  Fsk = 0,
  Ook = 1,
};

// Values are starting points for the Davis band. The exact values must be
// taken from the validated prototype; see docs/architecture/firmware.md.
struct RadioConfig {
  RadioChip chip = RadioChip::Rfm95;
  float frequencyMhz = 915.0f;
  RadioModulation modulation = RadioModulation::Fsk;
  float bitrateKbps = 4.8f;
  float deviationKhz = 5.0f;
  float bandwidthKhz = 125.0f;
  int8_t powerDbm = 17;
  bool payloadCrc = true;
  uint16_t preambleSymbols = 8;

  // RFM69 Davis mode uses a fixed eight-byte payload (six data bytes plus the
  // two-byte Davis CRC) and the 51-entry US frequency table. `channel` is an
  // index into that table, not a frequency.
  bool highPower = true;
  uint8_t channel = 0;
  uint8_t fixedPayloadLength = 8;
  // Receiver sketches enable the interrupt-driven RX task. The transmitter
  // simulator disables it because it never consumes received frames.
  bool backgroundRx = true;
  // Channel scan policy owned by the RX task. These values are in the sketch's
  // config.h so they can be tuned without touching the driver.
  uint32_t hopIntervalMs = 3200;
  uint32_t searchHopIntervalMs = 3500;
  uint8_t searchAfterMisses = 7;
  // Optional packet validator used to decide whether a received packet counts
  // as a valid frame for channel locking. Null means "treat every packet as
  // valid".
  bool (*frameValidator)(const uint8_t* data, size_t length) = nullptr;
  // Invalid frames at or above this RSSI are logged individually; weaker noise
  // is only aggregated in RadioStats so it cannot flood the serial console.
  int16_t badFrameLogRssi = -85;
};

struct RadioPins {
  uint8_t cs = 10;
  uint8_t reset = 16;
  uint8_t dio0 = 15;
  uint8_t sck = 12;
  uint8_t miso = 13;
  uint8_t mosi = 11;
};

}  // namespace akashavani
