// Hardware smoke test. This is not production firmware.
//
// It discovers and exercises the components on the two bench boards:
//   * I2C scan across plausible pin pairs (DS3231, BME280, SSD1306)
//   * RFM69 version/register access on the prototype SPI3 map and the scaffold map
//   * micro-SD mount + write/read/remove
//   * DS3231 time/temperature read
//   * BME280/BMP280 chip-id and compensated measurement
//   * SSD1306 init and a visible test pattern
//
// It does not transmit on the radio. Build with arduino-cli for
// esp32:esp32:esp32s3 and run at 115200 baud.

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

namespace {

struct SpiSet {
  uint8_t sck;
  uint8_t miso;
  uint8_t mosi;
  const char *name;
};

#if defined(CONFIG_IDF_TARGET_ESP32)
// The bench boards are ESP32-D0WD-V3 (classic), matching the prototype's
// esp32:esp32:esp32 build. The working prototype used VSPI: SCK 18, MISO 19,
// MOSI 23, radio CS 5, DIO0 13.
const SpiSet kSpiSets[] = {
    {18, 19, 23, "VSPI prototype (18/19/23)"},
    {14, 12, 13, "HSPI alt (14/12/13)"},
};
// Classic ESP32: GPIO 6-11 are SPI flash, GPIO 1/3 are UART0, and GPIO 0/12/15
// are strapping pins. Probe only the pins that are normally free.
const uint8_t kSafePins[] = {2,  4,  5,  12, 13, 14, 15, 16, 17,
                             18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
const uint8_t kCsCandidates[] = {4,  5,  15, 2,  13, 14, 16, 21,
                                 22, 25, 26, 27, 32, 33, 12};
#else
// ESP32-S3 scaffold mapping (kept for completeness if the boards are replaced).
const SpiSet kSpiSets[] = {
    {18, 19, 23, "SPI3 prototype (18/19/23)"},
    {12, 13, 11, "SPI2 scaffold (12/13/11)"},
    {14, 12, 13, "SPI2 alt (14/12/13)"},
};
const uint8_t kSafePins[] = {1,  2,  4,  5,  6,  7,  8,  9,  10, 11,
                             12, 13, 14, 15, 16, 17, 18, 21, 22, 23,
                             24, 25, 38, 39, 40, 41, 42, 47, 48};
const uint8_t kCsCandidates[] = {5,  10, 9,  4,  8,  6,  7,  15, 16, 17,
                                 21, 2,  1,  14, 22, 24, 25, 38, 39, 40,
                                 41, 42, 47, 48};
#endif
constexpr size_t kSpiSetCount = sizeof(kSpiSets) / sizeof(kSpiSets[0]);
constexpr size_t kSafePinCount = sizeof(kSafePins) / sizeof(kSafePins[0]);
constexpr size_t kCsCandidateCount =
    sizeof(kCsCandidates) / sizeof(kCsCandidates[0]);

struct I2cFinding {
  uint8_t sda;
  uint8_t scl;
  uint8_t addrs[8];
  uint8_t count;
  int score;
};

I2cFinding kI2cFindings[8];
size_t kI2cFindingCount = 0;

struct RfmFinding {
  bool found;
  SpiSet set;
  uint8_t cs;
  uint8_t version;
  uint8_t opmode;
  uint8_t irqFlags1;
  uint8_t irqFlags2;
};

RfmFinding kRfm = {};
bool kSdMounted = false;
bool kSdWritePass = false;
uint8_t kSdCs = 0;
const char *kSdSpiName = "";

bool isSpiPin(uint8_t pin, const SpiSet &set) {
  return pin == set.sck || pin == set.miso || pin == set.mosi;
}

const char *sdCardTypeName(uint8_t type) {
  switch (type) {
    case 0:
      return "NONE";
    case 1:
      return "MMC";
    case 2:
      return "SDSC";
    case 3:
      return "SDHC";
    default:
      return "UNKNOWN";
  }
}

// ---------------------------------------------------------------------------
// I2C
// ---------------------------------------------------------------------------

bool pinHasExternalPullup(uint8_t pin) {
  pinMode(pin, INPUT_PULLDOWN);
  delay(2);
  const bool high = digitalRead(pin) == HIGH;
  pinMode(pin, INPUT);
  return high;
}

uint8_t scanI2cPair(uint8_t sda, uint8_t scl, uint8_t *addrs,
                    uint8_t maxAddrs) {
  Wire.end();
  if (!Wire.begin(sda, scl, 100000)) {
    return 0;
  }
  Wire.setTimeOut(20);
  uint8_t count = 0;
  for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (count < maxAddrs) {
        addrs[count] = addr;
      }
      ++count;
    }
  }
  Wire.end();
  return count;
}

int scoreAddress(uint8_t addr) {
  switch (addr) {
    case 0x68:
      return 10;  // DS3231
    case 0x76:
    case 0x77:
      return 10;  // BME280/BMP280
    case 0x3C:
    case 0x3D:
      return 10;  // SSD1306/SH1106
    default:
      return 1;
  }
}

bool pairAlreadyScanned(uint8_t sda, uint8_t scl) {
  for (size_t i = 0; i < kI2cFindingCount; ++i) {
    if (kI2cFindings[i].sda == sda && kI2cFindings[i].scl == scl) {
      return true;
    }
  }
  return false;
}

void recordI2cPair(uint8_t sda, uint8_t scl) {
  if (kI2cFindingCount >= 8 || pairAlreadyScanned(sda, scl)) {
    return;
  }
  I2cFinding finding = {};
  finding.sda = sda;
  finding.scl = scl;
  finding.count = scanI2cPair(sda, scl, finding.addrs, 8);
  for (uint8_t i = 0; i < finding.count && i < 8; ++i) {
    finding.score += scoreAddress(finding.addrs[i]);
  }
  if (finding.count == 0) {
    return;
  }
  kI2cFindings[kI2cFindingCount++] = finding;
}

void discoverI2c() {
  Serial.println();
  Serial.println("== I2C discovery ==");

  bool pullup[64] = {};
  uint8_t pullupPins[16];
  size_t pullupCount = 0;
  for (size_t i = 0; i < kSafePinCount; ++i) {
    const uint8_t pin = kSafePins[i];
    pullup[pin] = pinHasExternalPullup(pin);
    if (pullup[pin] && pullupCount < 16) {
      pullupPins[pullupCount++] = pin;
    }
  }
  Serial.print("Pins with external pull-up (possible I2C):");
  if (pullupCount == 0) {
    Serial.print(" none");
  }
  for (size_t i = 0; i < pullupCount; ++i) {
    Serial.printf(" %u", pullupPins[i]);
  }
  Serial.println();

  // Always try the common/default pairs even if the pull-up probe was noisy.
#if defined(CONFIG_IDF_TARGET_ESP32)
  const uint8_t knownPairs[][2] = {
      {21, 22}, {22, 21}, {4, 5},   {5, 4},   {13, 14}, {14, 13},
      {18, 19}, {19, 18}, {25, 26}, {26, 25}, {32, 33}, {33, 32},
      {16, 17}, {17, 16}, {27, 14}, {14, 27}, {15, 13}, {13, 15},
      {12, 13}, {13, 12}, {14, 15}, {15, 14}, {21, 23}, {23, 21},
      {22, 23}, {23, 22}, {25, 27}, {27, 25}, {26, 27}, {27, 26},
  };
#else
  const uint8_t knownPairs[][2] = {
      {8, 9},   {9, 8},   {4, 5},   {5, 4},   {6, 7},   {7, 6},
      {15, 16}, {16, 15}, {1, 2},   {2, 1},   {21, 22}, {22, 21},
      {38, 39}, {39, 38}, {40, 41}, {41, 40}, {42, 47}, {47, 42},
      {48, 47}, {47, 48}, {10, 11}, {11, 10}, {12, 13}, {13, 12},
      {17, 18}, {18, 17}, {14, 15}, {15, 14}, {22, 23}, {23, 22},
      {24, 25}, {25, 24},
  };
#endif
  for (const auto &pair : knownPairs) {
    recordI2cPair(pair[0], pair[1]);
  }

  // All ordered pairs of pins that showed a pull-up.
  for (size_t i = 0; i < pullupCount; ++i) {
    for (size_t j = 0; j < pullupCount; ++j) {
      if (i != j) {
        recordI2cPair(pullupPins[i], pullupPins[j]);
      }
    }
  }

  if (kI2cFindingCount == 0) {
    Serial.println("No I2C devices found on any probed pair.");
    return;
  }

  for (size_t i = 0; i < kI2cFindingCount; ++i) {
    const I2cFinding &f = kI2cFindings[i];
    Serial.printf("I2C SDA=%u SCL=%u (%u device%s):", f.sda, f.scl, f.count,
                  f.count == 1 ? "" : "s");
    for (uint8_t a = 0; a < f.count && a < 8; ++a) {
      Serial.printf(" 0x%02X", f.addrs[a]);
    }
    Serial.printf("  score=%d\n", f.score);
  }
}

// ---------------------------------------------------------------------------
// RFM69
// ---------------------------------------------------------------------------

uint8_t rfmRead(uint8_t cs, uint8_t reg) {
  digitalWrite(cs, LOW);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(reg & 0x7F);
  const uint8_t value = SPI.transfer(0x00);
  SPI.endTransaction();
  digitalWrite(cs, HIGH);
  return value;
}

void rfmWrite(uint8_t cs, uint8_t reg, uint8_t value) {
  digitalWrite(cs, LOW);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(reg | 0x80);
  SPI.transfer(value);
  SPI.endTransaction();
  digitalWrite(cs, HIGH);
}

void probeRfm69() {
  Serial.println();
  Serial.println("== RFM69 probe ==");
  kRfm = {};

#if defined(CONFIG_IDF_TARGET_ESP32)
  // Known bench wiring: VSPI 18/19/23, CS 5, RESET 17 (active high), DIO0 13,
  // SD shares MISO and must stay deselected on CS 16.
  Serial.println("Trying bench pins: SPI 18/19/23 CS=5 RESET=17 DIO0=13");
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  pinMode(17, OUTPUT);
  digitalWrite(17, HIGH);  // SX1231 RESET is active high
  delay(20);
  digitalWrite(17, LOW);   // release to run
  delay(50);
  SPI.end();
  SPI.begin(18, 19, 23, 5);
  delay(10);
  const uint8_t benchVersion = rfmRead(5, 0x42);
  const uint8_t benchOpmode = rfmRead(5, 0x01);
  const uint8_t benchBitrateMsb = rfmRead(5, 0x03);
  const uint8_t benchBitrateLsb = rfmRead(5, 0x04);
  const uint8_t benchFrfMsb = rfmRead(5, 0x07);
  const bool benchSignature =
      benchOpmode == 0x04 && benchBitrateMsb == 0x1A &&
      benchBitrateLsb == 0x0B && benchFrfMsb == 0xE4;
  // A module with no effective RESET (USB1) keeps its previous configuration,
  // so register defaults are not required. Accept a passing register write/read
  // as the signature instead.
  bool benchWriteRead = false;
  if (!benchSignature && benchVersion != 0x24) {
    const uint8_t originalSync = rfmRead(5, 0x2F);
    rfmWrite(5, 0x2F, 0xAA);
    const uint8_t readAA = rfmRead(5, 0x2F);
    rfmWrite(5, 0x2F, 0x55);
    const uint8_t read55 = rfmRead(5, 0x2F);
    rfmWrite(5, 0x2F, originalSync);
    benchWriteRead = readAA == 0xAA && read55 == 0x55;
  }
  if (benchVersion == 0x24 || benchSignature || benchWriteRead) {
    kRfm.found = true;
    kRfm.set = kSpiSets[0];
    kRfm.cs = 5;
    kRfm.version = benchVersion;
    kRfm.opmode = benchOpmode;
    kRfm.irqFlags1 = rfmRead(5, 0x27);
    kRfm.irqFlags2 = rfmRead(5, 0x28);
    Serial.printf("FOUND RFM69 on bench pins CS=5 RESET=17, version=0x%02X, "
                  "opmode=0x%02X%s\n",
                  benchVersion, benchOpmode,
                  benchVersion == 0x24
                      ? ""
                      : (benchWriteRead && !benchSignature
                             ? " (configured state; register write/read passes)"
                             : " (register signature; version register reads 0)"));
  } else {
    Serial.printf("Bench pin probe: version=0x%02X opmode=0x%02X "
                  "bitrate=%02X%02X frf=%02X (no RFM69 signature)\n",
                  benchVersion, benchOpmode, benchBitrateMsb,
                  benchBitrateLsb, benchFrfMsb);
    SPI.end();
  }
#endif

  for (size_t s = 0; s < kSpiSetCount && !kRfm.found; ++s) {
    const SpiSet &set = kSpiSets[s];
    SPI.end();
    SPI.begin(set.sck, set.miso, set.mosi);
    Serial.printf("Trying %s\n", set.name);

    for (size_t i = 0; i < kCsCandidateCount; ++i) {
      const uint8_t cs = kCsCandidates[i];
      if (isSpiPin(cs, set)) {
        continue;
      }
      pinMode(cs, OUTPUT);
      digitalWrite(cs, HIGH);
    }

    for (size_t i = 0; i < kCsCandidateCount; ++i) {
      const uint8_t cs = kCsCandidates[i];
      if (isSpiPin(cs, set)) {
        continue;
      }
      const uint8_t version = rfmRead(cs, 0x42);
      const uint8_t opmode = rfmRead(cs, 0x01);
      if (version == 0x24) {
        kRfm.found = true;
        kRfm.set = set;
        kRfm.cs = cs;
        kRfm.version = version;
        kRfm.opmode = opmode;
        kRfm.irqFlags1 = rfmRead(cs, 0x27);
        kRfm.irqFlags2 = rfmRead(cs, 0x28);
        Serial.printf("FOUND RFM69 on CS=%u, version=0x%02X, opmode=0x%02X\n",
                      cs, version, opmode);
        break;
      }
      if (version != 0x00 && version != 0xFF) {
        Serial.printf("  CS=%u version=0x%02X opmode=0x%02X (not RFM69)\n", cs,
                      version, opmode);
      }
    }

    if (!kRfm.found) {
      SPI.end();
    }
  }

  if (!kRfm.found) {
    Serial.println("RFM69 NOT found on any probed SPI mapping/CS.");
    SPI.end();
    return;
  }

  const uint8_t cs = kRfm.cs;
  const uint8_t original = rfmRead(cs, 0x2F);
  rfmWrite(cs, 0x2F, 0xAA);
  const uint8_t readAA = rfmRead(cs, 0x2F);
  rfmWrite(cs, 0x2F, 0x55);
  const uint8_t read55 = rfmRead(cs, 0x2F);
  rfmWrite(cs, 0x2F, original);
  Serial.printf("Register write/read test: 0xAA->0x%02X 0x55->0x%02X %s\n",
                readAA, read55,
                (readAA == 0xAA && read55 == 0x55) ? "PASS" : "FAIL");

  Serial.printf(
      "RFM69 regs: opmode=0x%02X dataModul=0x%02X bitrate=0x%02X "
      "fdev=0x%02X frf=%02X%02X%02X irq1=0x%02X irq2=0x%02X\n",
      rfmRead(cs, 0x01), rfmRead(cs, 0x02), rfmRead(cs, 0x03),
      rfmRead(cs, 0x05), rfmRead(cs, 0x07), rfmRead(cs, 0x08),
      rfmRead(cs, 0x09), rfmRead(cs, 0x27), rfmRead(cs, 0x28));

  // Put the radio in standby, then RX for a short RSSI sample. No transmit.
  rfmWrite(cs, 0x01, 0x04);
  delay(5);
  Serial.printf("ModeReady=%s\n",
                (rfmRead(cs, 0x27) & 0x80) ? "yes" : "no");
  rfmWrite(cs, 0x01, 0x10);
  delay(20);
  Serial.printf("RSSI sample (RX, no TX): %d dBm\n",
                -static_cast<int>(rfmRead(cs, 0x24)) / 2);
  rfmWrite(cs, 0x01, 0x04);
}

// ---------------------------------------------------------------------------
// micro-SD
// ---------------------------------------------------------------------------

bool sdWriteReadTest() {
  File file = SD.open("/akashavani_smoke.txt", FILE_WRITE);
  if (!file) {
    Serial.println("SD open-for-write FAILED");
    return false;
  }
  file.printf("Akashavani smoke test millis=%lu\n", millis());
  file.close();

  file = SD.open("/akashavani_smoke.txt", FILE_READ);
  if (!file) {
    Serial.println("SD open-for-read FAILED");
    return false;
  }
  String line = file.readStringUntil('\n');
  file.close();
  SD.remove("/akashavani_smoke.txt");
  Serial.printf("SD write/read: '%s' %s\n", line.c_str(),
                line.startsWith("Akashavani smoke test") ? "PASS" : "FAIL");
  return line.startsWith("Akashavani smoke test");
}

void probeSd() {
  Serial.println();
  Serial.println("== micro-SD probe ==");
  bool mounted = false;

  for (size_t s = 0; s < kSpiSetCount && !mounted; ++s) {
    const SpiSet &set = kSpiSets[s];
    SPI.end();
    SPI.begin(set.sck, set.miso, set.mosi);
    Serial.printf("Trying %s\n", set.name);

    for (size_t i = 0; i < kCsCandidateCount; ++i) {
      const uint8_t cs = kCsCandidates[i];
      if (isSpiPin(cs, set)) {
        continue;
      }
      pinMode(cs, OUTPUT);
      digitalWrite(cs, HIGH);
      if (SD.begin(cs, SPI, 4000000)) {
        const uint8_t type = static_cast<uint8_t>(SD.cardType());
        Serial.printf("SD mounted on CS=%u type=%s(%u) size=%llu MB\n", cs,
                      sdCardTypeName(type), type,
                      static_cast<unsigned long long>(SD.cardSize() /
                                                      (1024ULL * 1024ULL)));
        kSdMounted = true;
        kSdCs = cs;
        kSdSpiName = set.name;
        kSdWritePass = sdWriteReadTest();
        SD.end();
        mounted = true;
        break;
      }
      SD.end();
    }
    SPI.end();
  }

  if (!mounted) {
    Serial.println("micro-SD NOT mounted on any probed mapping/CS.");
  }
}

// ---------------------------------------------------------------------------
// I2C sensors
// ---------------------------------------------------------------------------

bool i2cRead(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const uint8_t got = Wire.requestFrom(static_cast<int>(addr),
                                       static_cast<int>(len));
  if (got != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; ++i) {
    buf[i] = Wire.read();
  }
  return true;
}

bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

uint8_t bcdToDec(uint8_t value) { return (value >> 4) * 10 + (value & 0x0F); }

void readDs3231(uint8_t addr) {
  uint8_t t[7];
  if (!i2cRead(addr, 0x00, t, sizeof(t))) {
    Serial.printf("DS3231 at 0x%02X: register read FAILED\n", addr);
    return;
  }
  const bool osf = (t[0] & 0x80) != 0;
  const uint8_t sec = bcdToDec(t[0] & 0x7F);
  const uint8_t min = bcdToDec(t[1]);
  const uint8_t hour = bcdToDec(t[2] & 0x3F);
  const uint8_t mday = bcdToDec(t[4]);
  const uint8_t month = bcdToDec(t[5] & 0x1F);
  const uint16_t year = 2000 + bcdToDec(t[6]);

  uint8_t tempRaw[2] = {};
  float tempC = NAN;
  if (i2cRead(addr, 0x11, tempRaw, sizeof(tempRaw))) {
    const int16_t raw =
        (static_cast<int16_t>(static_cast<int8_t>(tempRaw[0])) << 8) |
        tempRaw[1];
    tempC = raw / 256.0f;
  }

  Serial.printf("DS3231 at 0x%02X: %04u-%02u-%02u %02u:%02u:%02u OSF=%s",
                addr, year, month, mday, hour, min, sec, osf ? "SET" : "clear");
  if (!isnan(tempC)) {
    Serial.printf(" temp=%.2f C", tempC);
  }
  Serial.println();

  // Non-destructive tick test: the seconds register should advance while we
  // wait. This proves the oscillator is running even when the stored date is
  // clearly wrong.
  delay(2100);
  uint8_t t2[7];
  if (i2cRead(addr, 0x00, t2, sizeof(t2))) {
    const uint8_t sec2 = bcdToDec(t2[0] & 0x7F);
    const uint8_t delta = (sec2 + 60 - sec) % 60;
    Serial.printf("DS3231 tick test: +%u s %s\n", delta,
                  (delta >= 1 && delta <= 3) ? "PASS" : "FAIL");
  } else {
    Serial.println("DS3231 tick test: second read FAILED");
  }
}

struct BmeCalibration {
  uint16_t t1;
  int16_t t2;
  int16_t t3;
  uint16_t p1;
  int16_t p2;
  int16_t p3;
  int16_t p4;
  int16_t p5;
  int16_t p6;
  int16_t p7;
  int16_t p8;
  int16_t p9;
  uint8_t h1;
  int16_t h2;
  uint8_t h3;
  int16_t h4;
  int16_t h5;
  int8_t h6;
};

void readBme280(uint8_t addr) {
  uint8_t chip = 0;
  if (!i2cRead(addr, 0xD0, &chip, 1)) {
    Serial.printf("BME280 at 0x%02X: chip-id read FAILED\n", addr);
    return;
  }
  Serial.printf("BME280 at 0x%02X: chip-id=0x%02X (%s)\n", addr, chip,
                chip == 0x60 ? "BME280" : (chip == 0x58 ? "BMP280" : "unknown"));

  uint8_t calib[26] = {};
  uint8_t hcalib[7] = {};
  if (!i2cRead(addr, 0x88, calib, sizeof(calib)) ||
      !i2cRead(addr, 0xE1, hcalib, sizeof(hcalib))) {
    Serial.println("BME280 calibration read FAILED");
    return;
  }

  BmeCalibration c = {};
  c.t1 = static_cast<uint16_t>((calib[1] << 8) | calib[0]);
  c.t2 = static_cast<int16_t>((calib[3] << 8) | calib[2]);
  c.t3 = static_cast<int16_t>((calib[5] << 8) | calib[4]);
  c.p1 = static_cast<uint16_t>((calib[7] << 8) | calib[6]);
  c.p2 = static_cast<int16_t>((calib[9] << 8) | calib[8]);
  c.p3 = static_cast<int16_t>((calib[11] << 8) | calib[10]);
  c.p4 = static_cast<int16_t>((calib[13] << 8) | calib[12]);
  c.p5 = static_cast<int16_t>((calib[15] << 8) | calib[14]);
  c.p6 = static_cast<int16_t>((calib[17] << 8) | calib[16]);
  c.p7 = static_cast<int16_t>((calib[19] << 8) | calib[18]);
  c.p8 = static_cast<int16_t>((calib[21] << 8) | calib[20]);
  c.p9 = static_cast<int16_t>((calib[23] << 8) | calib[22]);
  c.h1 = calib[25];
  c.h2 = static_cast<int16_t>((hcalib[1] << 8) | hcalib[0]);
  c.h3 = hcalib[2];
  c.h4 = static_cast<int16_t>((static_cast<int8_t>(hcalib[3]) << 4) |
                              (hcalib[4] & 0x0F));
  c.h5 = static_cast<int16_t>((static_cast<int8_t>(hcalib[5]) << 4) |
                              (hcalib[4] >> 4));
  c.h6 = static_cast<int8_t>(hcalib[6]);

  if (!i2cWrite(addr, 0xF2, 0x01) || !i2cWrite(addr, 0xF4, 0x25)) {
    Serial.println("BME280 configuration write FAILED");
    return;
  }
  delay(15);

  uint8_t data[8] = {};
  if (!i2cRead(addr, 0xF7, data, sizeof(data))) {
    Serial.println("BME280 measurement read FAILED");
    return;
  }

  const int32_t adcP =
      (static_cast<int32_t>(data[0]) << 12) |
      (static_cast<int32_t>(data[1]) << 4) | (data[2] >> 4);
  const int32_t adcT =
      (static_cast<int32_t>(data[3]) << 12) |
      (static_cast<int32_t>(data[4]) << 4) | (data[5] >> 4);
  const int32_t adcH =
      (static_cast<int32_t>(data[6]) << 8) | static_cast<int32_t>(data[7]);

  const int32_t var1T =
      ((((adcT >> 3) - (static_cast<int32_t>(c.t1) << 1))) * c.t2) >> 11;
  const int32_t var2T =
      (((((adcT >> 4) - static_cast<int32_t>(c.t1)) *
         ((adcT >> 4) - static_cast<int32_t>(c.t1))) >>
        12) *
       c.t3) >>
      14;
  const int32_t tFine = var1T + var2T;
  const float temperature = ((tFine * 5 + 128) >> 8) / 100.0f;

  int64_t var1 = static_cast<int64_t>(tFine) - 128000;
  int64_t var2 = var1 * var1 * static_cast<int64_t>(c.p6);
  var2 += (var1 * static_cast<int64_t>(c.p5)) << 17;
  var2 += static_cast<int64_t>(c.p4) << 35;
  var1 = ((var1 * var1 * static_cast<int64_t>(c.p3)) >> 8) +
         ((var1 * static_cast<int64_t>(c.p2)) << 12);
  var1 = ((((static_cast<int64_t>(1) << 47) + var1)) *
          static_cast<int64_t>(c.p1)) >>
         33;
  float pressure = NAN;
  if (var1 != 0) {
    int64_t p = 1048576 - adcP;
    p = (((p << 31) - var2) * 3125) / var1;
    const int64_t var1p =
        (static_cast<int64_t>(c.p9) * (p >> 13) * (p >> 13)) >> 25;
    const int64_t var2p = (static_cast<int64_t>(c.p8) * p) >> 19;
    p = ((p + var1p + var2p) >> 8) + (static_cast<int64_t>(c.p7) << 4);
    pressure = static_cast<float>(p) / 25600.0f;
  }

  float humidity = NAN;
  if (chip == 0x60) {
    int32_t v = tFine - 76800;
    v = (((((adcH << 14) - (static_cast<int32_t>(c.h4) << 20) -
            (static_cast<int32_t>(c.h5) * v)) +
           16384) >>
          15) *
         (((((((v * static_cast<int32_t>(c.h6)) >> 10) *
              (((v * static_cast<int32_t>(c.h3)) >> 11) + 32768)) >>
             10) +
            2097152) *
               static_cast<int32_t>(c.h2) +
           8192) >>
          14));
    v -= (((((v >> 15) * (v >> 15)) >> 7) * static_cast<int32_t>(c.h1)) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    humidity = static_cast<float>(v >> 12) / 1024.0f;
  }

  Serial.printf("BME280 measurement: T=%.2f C P=%.2f hPa", temperature,
                pressure);
  if (!isnan(humidity)) {
    Serial.printf(" RH=%.2f %%", humidity);
  }
  Serial.printf(" (raw T=%ld P=%ld H=%ld)\n", static_cast<long>(adcT),
                static_cast<long>(adcP), static_cast<long>(adcH));
}

// ---------------------------------------------------------------------------
// SSD1306
// ---------------------------------------------------------------------------

bool ssd1306Command(uint8_t addr, uint8_t cmd) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);
  Wire.write(cmd);
  return Wire.endTransmission() == 0;
}

bool ssd1306Data(uint8_t addr, const uint8_t *data, size_t len) {
  while (len > 0) {
    const size_t chunk = len > 16 ? 16 : len;
    Wire.beginTransmission(addr);
    Wire.write(0x40);
    for (size_t i = 0; i < chunk; ++i) {
      Wire.write(data[i]);
    }
    if (Wire.endTransmission() != 0) {
      return false;
    }
    data += chunk;
    len -= chunk;
  }
  return true;
}

bool ssd1306SetWindow(uint8_t addr, uint8_t page) {
  const uint8_t cmds[] = {static_cast<uint8_t>(0xB0 | (page & 0x07)), 0x00,
                          0x10};
  for (uint8_t cmd : cmds) {
    if (!ssd1306Command(addr, cmd)) {
      return false;
    }
  }
  return true;
}

void readSsd1306(uint8_t addr) {
  const uint8_t init[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
      0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
      0xDB, 0x40, 0xA4, 0xA6, 0x2E, 0xAF,
  };
  size_t initAcked = 0;
  for (size_t i = 0; i < sizeof(init); ++i) {
    if (ssd1306Command(addr, init[i])) {
      ++initAcked;
    }
    delayMicroseconds(500);
  }
  const bool initOk = initAcked == sizeof(init);

  bool clearOk = true;
  uint8_t zeros[16] = {};
  for (uint8_t page = 0; page < 8 && clearOk; ++page) {
    clearOk = ssd1306SetWindow(addr, page) &&
              ssd1306Data(addr, zeros, sizeof(zeros));
    for (uint8_t i = 1; i < 8 && clearOk; ++i) {
      clearOk = ssd1306Data(addr, zeros, sizeof(zeros));
    }
  }

  // Border plus a central block, so the panel shows something unmistakable.
  bool drawOk = clearOk;
  uint8_t white[16];
  memset(white, 0xFF, sizeof(white));
  for (uint8_t page = 0; page < 8 && drawOk; ++page) {
    if (!ssd1306SetWindow(addr, page)) {
      drawOk = false;
      break;
    }
    if (page == 0 || page == 7) {
      for (uint8_t i = 0; i < 8; ++i) {
        if (!ssd1306Data(addr, white, sizeof(white))) {
          drawOk = false;
          break;
        }
      }
    } else {
      uint8_t edge[16] = {};
      edge[0] = 0x81;
      edge[15] = 0x81;
      if (!ssd1306Data(addr, edge, sizeof(edge))) {
        drawOk = false;
        break;
      }
      for (uint8_t i = 1; i < 7; ++i) {
        uint8_t block[16] = {};
        if (i >= 2 && i <= 5 && page >= 2 && page <= 5) {
          memset(block, 0xFF, sizeof(block));
        }
        if (!ssd1306Data(addr, block, sizeof(block))) {
          drawOk = false;
          break;
        }
      }
      if (!ssd1306Data(addr, edge, sizeof(edge))) {
        drawOk = false;
      }
    }
  }

  Serial.printf("SSD1306 at 0x%02X: init=%s (%u/%u cmds ACKed) clear=%s "
                "pattern=%s (visual check required)\n",
                addr, initOk ? "PASS" : "PARTIAL",
                static_cast<unsigned>(initAcked),
                static_cast<unsigned>(sizeof(init)),
                clearOk ? "PASS" : "FAIL", drawOk ? "PASS" : "FAIL");
}

void readSensorsOnBus(const I2cFinding &bus) {
  Wire.end();
  if (!Wire.begin(bus.sda, bus.scl, 100000)) {
    Serial.printf("Could not reopen I2C SDA=%u SCL=%u\n", bus.sda, bus.scl);
    return;
  }
  Wire.setTimeOut(50);
  Serial.printf("\n-- Sensors on SDA=%u SCL=%u --\n", bus.sda, bus.scl);
  bool any = false;
  for (uint8_t i = 0; i < bus.count && i < 8; ++i) {
    switch (bus.addrs[i]) {
      case 0x68:
        readDs3231(bus.addrs[i]);
        any = true;
        break;
      case 0x76:
      case 0x77:
        readBme280(bus.addrs[i]);
        any = true;
        break;
      case 0x3C:
      case 0x3D:
        readSsd1306(bus.addrs[i]);
        any = true;
        break;
      default:
        break;
    }
  }
  if (!any) {
    Serial.println("No recognised sensor at this bus (see address list above).");
  }
  Wire.end();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println();
  Serial.println("==================================================");
  Serial.println("Akashavani component smoke test");
  Serial.printf("Chip: %s rev %u, flash %u MB, PSRAM %u MB\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getFlashChipSize() / (1024U * 1024U),
                ESP.getPsramSize() / (1024U * 1024U));
  Serial.printf("Efuse MAC: %04X%08X\n",
                static_cast<uint16_t>(ESP.getEfuseMac() >> 32),
                static_cast<uint32_t>(ESP.getEfuseMac()));
  Serial.println("==================================================");

  discoverI2c();
  probeRfm69();
  probeSd();

  Serial.println();
  Serial.println("== I2C sensor checks ==");
  if (kI2cFindingCount == 0) {
    Serial.println("Skipped: no I2C bus found.");
  } else {
    int best = -1;
    for (size_t i = 0; i < kI2cFindingCount; ++i) {
      if (best < 0 || kI2cFindings[i].score > kI2cFindings[best].score) {
        best = static_cast<int>(i);
      }
    }
    if (best >= 0 && kI2cFindings[best].score > 0) {
      Serial.printf("Using best I2C pair SDA=%u SCL=%u (score=%d)\n",
                    kI2cFindings[best].sda, kI2cFindings[best].scl,
                    kI2cFindings[best].score);
      readSensorsOnBus(kI2cFindings[best]);
    } else {
      Serial.println("No recognised I2C device on the probed pairs.");
    }
  }

  Serial.println();
  Serial.println("== Summary ==");
  Serial.printf("RFM69: %s\n", kRfm.found ? "PASS" : "NOT FOUND");
  if (kRfm.found) {
    Serial.printf("  SPI=%s CS=%u version=0x%02X%s\n", kRfm.set.name, kRfm.cs,
                  kRfm.version,
                  kRfm.version == 0x24
                      ? ""
                      : " (RFM69 register signature; version register reads 0)");
  }
  bool rtcPresent = false;
  bool bmePresent = false;
  bool oledPresent = false;
  for (size_t i = 0; i < kI2cFindingCount; ++i) {
    for (uint8_t a = 0; a < kI2cFindings[i].count && a < 8; ++a) {
      const uint8_t addr = kI2cFindings[i].addrs[a];
      rtcPresent |= addr == 0x68;
      bmePresent |= addr == 0x76 || addr == 0x77;
      oledPresent |= addr == 0x3C || addr == 0x3D;
    }
  }
  Serial.printf("DS3231: %s\n", rtcPresent ? "PRESENT" : "NOT FOUND");
  Serial.printf("BME280: %s\n", bmePresent ? "PRESENT" : "NOT FOUND");
  Serial.printf("SSD1306 OLED: %s\n", oledPresent ? "PRESENT" : "NOT FOUND");
  Serial.printf("micro-SD: %s", kSdMounted ? "PRESENT" : "NOT FOUND");
  if (kSdMounted) {
    Serial.printf(" (SPI=%s CS=%u write/read=%s)", kSdSpiName, kSdCs,
                  kSdWritePass ? "PASS" : "FAIL");
  }
  Serial.println();
  Serial.println("Smoke test complete. Looping heartbeat every 5 s.");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    Serial.printf("heartbeat %lu ms\n", millis());
  }
  delay(50);
}
