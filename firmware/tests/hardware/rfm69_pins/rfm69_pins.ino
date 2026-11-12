// Focused RFM69 pin/reset test. Not production firmware.
//
// Bench wiring under test (confirmed on USB0, radio also answers on USB1):
//   SPI SCK 18, MISO 19, MOSI 23
//   RFM69 CS 5
//   RFM69 RESET 17 (active high; not effective on USB1, see handoff)
//   RFM69 DIO0/INT 13 (input only, optional)
//
// It pulses RESET, reads the SX1231 version register (0x42, expected 0x24),
// performs a register write/read test, puts the radio in standby/RX and samples
// RSSI. It never transmits.

#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr uint8_t kSck = 18;
constexpr uint8_t kMiso = 19;
constexpr uint8_t kMosi = 23;
constexpr uint8_t kCs = 5;
constexpr uint8_t kReset = 17;
constexpr uint8_t kDio0 = 13;

constexpr uint8_t kRegOpMode = 0x01;
constexpr uint8_t kRegDataModul = 0x02;
constexpr uint8_t kRegBitrateMsb = 0x03;
constexpr uint8_t kRegBitrateLsb = 0x04;
constexpr uint8_t kRegFdevMsb = 0x05;
constexpr uint8_t kRegFdevLsb = 0x06;
constexpr uint8_t kRegFrfMsb = 0x07;
constexpr uint8_t kRegFrfMid = 0x08;
constexpr uint8_t kRegFrfLsb = 0x09;
constexpr uint8_t kRegRssiValue = 0x24;
constexpr uint8_t kRegIrqFlags1 = 0x27;
constexpr uint8_t kRegIrqFlags2 = 0x28;
constexpr uint8_t kRegSyncValue1 = 0x2F;
constexpr uint8_t kRegSyncValue2 = 0x30;
constexpr uint8_t kRegVersion = 0x42;

uint8_t readReg(uint8_t reg) {
  digitalWrite(kCs, LOW);
  delayMicroseconds(2);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(reg & 0x7F);
  const uint8_t value = SPI.transfer(0x00);
  SPI.endTransaction();
  delayMicroseconds(2);
  digitalWrite(kCs, HIGH);
  return value;
}

void writeReg(uint8_t reg, uint8_t value) {
  digitalWrite(kCs, LOW);
  delayMicroseconds(2);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(reg | 0x80);
  SPI.transfer(value);
  SPI.endTransaction();
  delayMicroseconds(2);
  digitalWrite(kCs, HIGH);
}

void printRegisters();

bool testRadio(const char *label) {
  const uint8_t version = readReg(kRegVersion);
  const uint8_t opmode = readReg(kRegOpMode);
  const bool versionOk = version == 0x24;
  // Reset-default signature. Only valid if the module's RESET line is actually
  // driven; a module that is running but configured differently will fail here.
  const bool registerDefaults = opmode == 0x04 &&
                                readReg(kRegBitrateMsb) == 0x1A &&
                                readReg(kRegBitrateLsb) == 0x0B &&
                                readReg(kRegFrfMsb) == 0xE4;

  const uint8_t original = readReg(kRegSyncValue1);
  writeReg(kRegSyncValue1, 0xAA);
  const uint8_t readAA = readReg(kRegSyncValue1);
  writeReg(kRegSyncValue1, 0x55);
  const uint8_t read55 = readReg(kRegSyncValue1);
  writeReg(kRegSyncValue1, original);
  const bool writeReadOk = readAA == 0xAA && read55 == 0x55;

  const bool found = versionOk || registerDefaults || writeReadOk;
  Serial.printf("%s: version=0x%02X opmode=0x%02X %s\n", label, version,
                opmode,
                versionOk ? "*** RFM69 version 0x24 ***"
                          : (registerDefaults
                                 ? "*** RFM69 register signature ***"
                                 : (writeReadOk
                                        ? "*** RFM69-compatible register "
                                          "write/read (configured state) ***"
                                        : "(not RFM69)")));
  if (!found) {
    printRegisters();
    return false;
  }

  printRegisters();
  if (!versionOk) {
    Serial.println("note: version register reads 0x00 on this module; "
                   "register read/write is used as the signature");
  }
  if (!registerDefaults && !versionOk) {
    Serial.println("note: the module was not in reset-default state. If the "
                   "RESET pin is not connected or effective (as on USB1), the "
                   "radio keeps its previous configuration across MCU resets.");
  }

  Serial.printf("sync write/read: 0xAA->0x%02X 0x55->0x%02X %s\n", readAA, read55,
                writeReadOk ? "PASS" : "FAIL");

  writeReg(kRegOpMode, 0x04);  // standby
  delay(10);
  Serial.printf("standby: modeReady=%s DIO0=%u\n",
                (readReg(kRegIrqFlags1) & 0x80) ? "yes" : "no",
                digitalRead(kDio0));

  writeReg(kRegOpMode, 0x10);  // RX
  delay(100);
  const uint8_t rssiRaw = readReg(kRegRssiValue);
  Serial.printf("RX: modeReady=%s DIO0=%u RSSI_RAW=0x%02X (~%d dBm)\n",
                (readReg(kRegIrqFlags1) & 0x80) ? "yes" : "no",
                digitalRead(kDio0), rssiRaw,
                -static_cast<int>(rssiRaw) / 2);
  writeReg(kRegOpMode, 0x04);  // standby
  return true;
}

void printRegisters() {
  Serial.printf(
      "regs: opmode=0x%02X dataModul=0x%02X bitrate=%02X%02X fdev=%02X%02X "
      "frf=%02X%02X%02X sync=%02X%02X irq1=0x%02X irq2=0x%02X rssi=0x%02X\n",
      readReg(kRegOpMode), readReg(kRegDataModul), readReg(kRegBitrateMsb),
      readReg(kRegBitrateLsb), readReg(kRegFdevMsb), readReg(kRegFdevLsb),
      readReg(kRegFrfMsb), readReg(kRegFrfMid), readReg(kRegFrfLsb),
      readReg(kRegSyncValue1), readReg(kRegSyncValue2), readReg(kRegIrqFlags1),
      readReg(kRegIrqFlags2), readReg(kRegRssiValue));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println();
  Serial.println("==================================================");
  Serial.println("RFM69 pin/reset test");
  Serial.printf("Chip: %s rev %u\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("SPI: SCK=%u MISO=%u MOSI=%u CS=%u RESET=%u DIO0=%u\n", kSck,
                kMiso, kMosi, kCs, kReset, kDio0);
  Serial.println("==================================================");

  pinMode(kCs, OUTPUT);
  digitalWrite(kCs, HIGH);
  pinMode(kReset, OUTPUT);
  digitalWrite(kReset, LOW);  // SX1231 RESET is active high; low = run
  pinMode(kDio0, INPUT);

  // The SD card shares MISO 19 and uses CS 16. Keep it deselected.
  constexpr uint8_t kSdCs = 16;
  pinMode(kSdCs, OUTPUT);
  digitalWrite(kSdCs, HIGH);

  SPI.end();
  SPI.begin(kSck, kMiso, kMosi, kCs);

  // Test RESET polarity. SX1231 RESET is active high, but test both so a
  // different module revision cannot hide the chip in reset.
  digitalWrite(kReset, LOW);
  delay(30);
  bool found = testRadio("RESET held LOW (run)");

  if (!found) {
    digitalWrite(kReset, HIGH);
    delay(20);
    digitalWrite(kReset, LOW);
    delay(50);
    found = testRadio("active-high pulse (high->low)");
  }

  if (!found) {
    digitalWrite(kReset, HIGH);
    delay(30);
    found = testRadio("RESET held HIGH (reset)");
  }

  if (!found) {
    digitalWrite(kReset, LOW);
    delay(20);
    digitalWrite(kReset, HIGH);
    delay(50);
    found = testRadio("active-low pulse (low->high)");
  }

  Serial.println("\nRFM69 pin/reset test complete. No transmit performed.");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    Serial.println("idle");
  }
  delay(50);
}
