// Focused read-only RFM69 SPI/CS scanner. Not production firmware.
// Tries plausible ESP32 VSPI/HSPI/alternate pin permutations and CS pins,
// reading the SX1231 version register (0x42, expected 0x24). It never
// transmits and never writes to the radio.

#include <Arduino.h>
#include <SPI.h>

struct SpiSet {
  uint8_t sck;
  uint8_t miso;
  uint8_t mosi;
  const char *name;
};

// Read-only scan. Include swapped MISO/MOSI because a miswired or
// differently-labelled header is the most likely reason a known-good board
// does not answer.
const SpiSet kSets[] = {
    {18, 19, 23, "VSPI 18/19/23"},
    {18, 23, 19, "VSPI swap 18/23/19"},
    {14, 12, 13, "HSPI 14/12/13"},
    {14, 13, 12, "HSPI swap 14/13/12"},
    {12, 13, 14, "HSPI alt 12/13/14"},
    {12, 14, 13, "HSPI alt 12/14/13"},
    {13, 12, 14, "HSPI alt 13/12/14"},
    {13, 14, 12, "HSPI alt 13/14/12"},
    {25, 26, 27, "alt 25/26/27"},
    {25, 27, 26, "alt swap 25/27/26"},
    {26, 25, 27, "alt 26/25/27"},
    {26, 27, 25, "alt 26/27/25"},
    {27, 25, 26, "alt 27/25/26"},
    {27, 26, 25, "alt 27/26/25"},
    {32, 33, 25, "alt 32/33/25"},
    {32, 25, 33, "alt 32/25/33"},
    {33, 32, 25, "alt 33/32/25"},
    {33, 25, 32, "alt 33/25/32"},
    {25, 32, 33, "alt 25/32/33"},
    {25, 33, 32, "alt 25/33/32"},
    {4, 5, 18, "alt 4/5/18"},
    {4, 18, 5, "alt 4/18/5"},
    {5, 4, 18, "alt 5/4/18"},
    {5, 18, 4, "alt 5/18/4"},
    {18, 4, 5, "alt 18/4/5"},
    {18, 5, 4, "alt 18/5/4"},
    {15, 14, 13, "alt 15/14/13"},
    {15, 13, 14, "alt 15/13/14"},
    {13, 15, 14, "alt 13/15/14"},
    {14, 15, 13, "alt 14/15/13"},
    {13, 14, 15, "alt 13/14/15"},
    {14, 13, 15, "alt 14/13/15"},
};
constexpr size_t kSetCount = sizeof(kSets) / sizeof(kSets[0]);

const uint8_t kCs[] = {0,  2,  3,  4,  5,  12, 13, 14, 15,
                       16, 17, 21, 22, 25, 26, 27, 32, 33};
constexpr size_t kCsCount = sizeof(kCs) / sizeof(kCs[0]);

bool isSetPin(uint8_t pin, const SpiSet &s) {
  return pin == s.sck || pin == s.miso || pin == s.mosi;
}

uint8_t readReg(uint8_t cs, uint8_t reg) {
  digitalWrite(cs, LOW);
  delayMicroseconds(2);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(reg & 0x7F);
  const uint8_t v = SPI.transfer(0x00);
  SPI.endTransaction();
  delayMicroseconds(2);
  digitalWrite(cs, HIGH);
  return v;
}

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println();
  Serial.println("RFM69 read-only SPI/CS scan");
  Serial.printf("Chip: %s rev %u\n", ESP.getChipModel(), ESP.getChipRevision());

  for (size_t si = 0; si < kSetCount; ++si) {
    const SpiSet &s = kSets[si];
    SPI.end();
    SPI.begin(s.sck, s.miso, s.mosi);
    for (size_t i = 0; i < kCsCount; ++i) {
      if (!isSetPin(kCs[i], s)) {
        pinMode(kCs[i], OUTPUT);
        digitalWrite(kCs[i], HIGH);
      }
    }
    for (size_t i = 0; i < kCsCount; ++i) {
      const uint8_t cs = kCs[i];
      if (isSetPin(cs, s)) {
        continue;
      }
      const uint8_t v = readReg(cs, 0x42);
      if (v == 0x24) {
        Serial.printf("*** RFM69 FOUND: %s CS=%u version=0x24 ***\n", s.name, cs);
      } else if (v != 0x00 && v != 0xFF) {
        Serial.printf("candidate: %s CS=%u version=0x%02X\n", s.name, cs, v);
      }
    }
    SPI.end();
  }
  Serial.println("scan complete");
}

void loop() {
  delay(5000);
  Serial.println("idle");
}
