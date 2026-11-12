#include "Rfm69Radio.h"

#include "DavisChannels.h"
#include "Log.h"

namespace akashvani {
namespace {

// SX1231 register addresses. Only the registers used by the Davis configuration
// are listed; values are from the SX1231 datasheet and the validated prototype.
constexpr uint8_t kRegFifo = 0x00;
constexpr uint8_t kRegOpMode = 0x01;
constexpr uint8_t kRegDataModul = 0x02;
constexpr uint8_t kRegBitrateMsb = 0x03;
constexpr uint8_t kRegBitrateLsb = 0x04;
constexpr uint8_t kRegFdevMsb = 0x05;
constexpr uint8_t kRegFdevLsb = 0x06;
constexpr uint8_t kRegFrfMsb = 0x07;
constexpr uint8_t kRegFrfMid = 0x08;
constexpr uint8_t kRegFrfLsb = 0x09;
constexpr uint8_t kRegAfcCtrl = 0x0B;
constexpr uint8_t kRegVersion = 0x10;
constexpr uint8_t kRegPaLevel = 0x11;
constexpr uint8_t kRegOcp = 0x13;
constexpr uint8_t kRegLna = 0x18;
constexpr uint8_t kRegRxBw = 0x19;
constexpr uint8_t kRegAfcBw = 0x1A;
constexpr uint8_t kRegAfcFei = 0x1E;
constexpr uint8_t kRegRssiValue = 0x24;
constexpr uint8_t kRegDioMapping1 = 0x25;
constexpr uint8_t kRegDioMapping2 = 0x26;
constexpr uint8_t kRegIrqFlags1 = 0x27;
constexpr uint8_t kRegIrqFlags2 = 0x28;
constexpr uint8_t kRegRssiThresh = 0x29;
constexpr uint8_t kRegPreambleLsb = 0x2D;
constexpr uint8_t kRegSyncConfig = 0x2E;
constexpr uint8_t kRegSyncValue1 = 0x2F;
constexpr uint8_t kRegSyncValue2 = 0x30;
constexpr uint8_t kRegPacketConfig1 = 0x37;
constexpr uint8_t kRegPayloadLength = 0x38;
constexpr uint8_t kRegFifoThresh = 0x3C;
constexpr uint8_t kRegPacketConfig2 = 0x3D;
constexpr uint8_t kRegTestPa1 = 0x5A;
constexpr uint8_t kRegTestPa2 = 0x5C;
constexpr uint8_t kRegTestDagc = 0x6F;
constexpr uint8_t kRegTestAfc = 0x71;

// Operating modes (RegOpMode bits 4..2).
constexpr uint8_t kModeStandby = 0x04;
constexpr uint8_t kModeTx = 0x0C;
constexpr uint8_t kModeRx = 0x10;

// IRQ flags.
constexpr uint8_t kIrq1ModeReady = 0x80;
constexpr uint8_t kIrq2PacketSent = 0x08;
constexpr uint8_t kIrq2PayloadReady = 0x04;
constexpr uint8_t kPacket2RxRestart = 0x04;
constexpr uint8_t kAfcFeiFeiStart = 0x20;

// RegPaLevel PA selection bits.
constexpr uint8_t kPa0On = 0x80;
constexpr uint8_t kPa1On = 0x40;
constexpr uint8_t kPa2On = 0x20;

constexpr uint32_t kModeReadyTimeoutMs = 50;
constexpr uint32_t kPacketSentTimeoutMs = 1000;

constexpr uint8_t kConfig[][2] = {
    // Standby, packet sequencer on, listen mode off.
    {kRegOpMode, kModeStandby},
    // Packet mode, FSK, Gaussian shaping BT=0.5 (Davis uses BT=0.5).
    {kRegDataModul, 0x02},
    // 19.2 kbps: 0x0683 = round(32e6 / 19200).
    {kRegBitrateMsb, 0x06},
    {kRegBitrateLsb, 0x83},
    // 4.8 kHz deviation.
    {kRegFdevMsb, 0x00},
    {kRegFdevLsb, 0x4E},
    // LNA: 50 ohm input, automatic gain.
    {kRegLna, 0x00},
    // RX bandwidth: DCCFREQ=010, MANT=20, EXP=4 -> about 25 kHz.
    {kRegRxBw, 0x4C},
    // AFC bandwidth: DCCFREQ=010, MANT=20, EXP=3 -> about 50 kHz.
    {kRegAfcBw, 0x4B},
    // AFC auto on, auto clear on.
    {kRegAfcFei, 0x0C},
    // DIO0 = PayloadReady in RX, PacketSent in TX.
    {kRegDioMapping1, 0x40},
    // No CLKOUT.
    {kRegDioMapping2, 0x07},
    // Clear FIFO overrun (write one to clear).
    {kRegIrqFlags2, 0x10},
    // RSSI threshold: 0xA5 = -82.5 dBm. Not a packet filter in normal RX.
    {kRegRssiThresh, 0xA5},
    // Four preamble bytes 0xAAAAAAAA.
    {kRegPreambleLsb, 0x04},
    // Sync on, fill FIFO automatically, two sync bytes, tolerance 2.
    {kRegSyncConfig, 0x8A},
    // Davis ISS sync word.
    {kRegSyncValue1, 0xCB},
    {kRegSyncValue2, 0x89},
    // Fixed packet format, no DC-free coding, hardware CRC off (Davis CRC is
    // checked in software), no address filtering.
    {kRegPacketConfig1, 0x08},
    // Eight bytes: six data bytes plus the two-byte Davis CRC.
    {kRegPayloadLength, 8},
    // TX starts when the FIFO is not empty.
    {kRegFifoThresh, 0x07},
    // RX restart delay 2 bits, automatic RX restart off, AES off.
    {kRegPacketConfig2, 0x10},
    // Standard AFC routine.
    {kRegAfcCtrl, 0x00},
    // Continuous DAGC, recommended for AfcLowBetaOn=0.
    {kRegTestDagc, 0x30},
    // AFC offset for low modulation index systems.
    {kRegTestAfc, 0x00},
};

}  // namespace

Rfm69Radio::Rfm69Radio(SPIClass& spi, const RadioPins& pins) : spi_(spi), pins_(pins) {}

uint8_t Rfm69Radio::reverseBits(uint8_t value) {
  value = static_cast<uint8_t>(((value & 0xF0u) >> 4) | ((value & 0x0Fu) << 4));
  value = static_cast<uint8_t>(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
  value = static_cast<uint8_t>(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
  return value;
}

void Rfm69Radio::select() {
  spi_.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins_.cs, LOW);
}

void Rfm69Radio::unselect() {
  digitalWrite(pins_.cs, HIGH);
  spi_.endTransaction();
}

uint8_t Rfm69Radio::readReg(uint8_t address) {
  select();
  spi_.transfer(address & 0x7F);
  const uint8_t value = spi_.transfer(0);
  unselect();
  return value;
}

void Rfm69Radio::writeReg(uint8_t address, uint8_t value) {
  select();
  spi_.transfer(address | 0x80);
  spi_.transfer(value);
  unselect();
}

uint8_t Rfm69Radio::readRegister(uint8_t address) { return readReg(address); }

void Rfm69Radio::writeRegister(uint8_t address, uint8_t value) { writeReg(address, value); }

int16_t Rfm69Radio::readRssiDbm() {
  int16_t rssi = -static_cast<int16_t>(readReg(kRegRssiValue));
  rssi >>= 1;
  return rssi;
}

void Rfm69Radio::setModeStandby() {
  writeReg(kRegOpMode, static_cast<uint8_t>((readReg(kRegOpMode) & 0xE3u) | kModeStandby));
}

void Rfm69Radio::setModeRx() {
  writeReg(kRegOpMode, static_cast<uint8_t>((readReg(kRegOpMode) & 0xE3u) | kModeRx));
}

void Rfm69Radio::setModeTx() {
  writeReg(kRegOpMode, static_cast<uint8_t>((readReg(kRegOpMode) & 0xE3u) | kModeTx));
}

void Rfm69Radio::configurePower() {
  const int8_t requested = config_.powerDbm;
  uint8_t level = 0;
  uint8_t paBits = 0;

  if (highPower_) {
    int8_t dbm = requested;
    if (dbm < -2) {
      dbm = -2;
    } else if (dbm > 20) {
      dbm = 20;
    }
    powerDbm_ = dbm;
    if (dbm < 12) {
      level = static_cast<uint8_t>(2 + dbm);
    } else if (dbm < 16) {
      level = static_cast<uint8_t>(4 + dbm);
    } else {
      level = static_cast<uint8_t>(3 + dbm);
    }

    uint8_t output = level;
    if (level < 16) {
      output = static_cast<uint8_t>(level + 16);
      paBits = kPa1On;
    } else {
      output = static_cast<uint8_t>(level < 20 ? level + 10 : level + 8);
      paBits = static_cast<uint8_t>(kPa1On | kPa2On);
    }
    writeReg(kRegPaLevel, static_cast<uint8_t>(paBits | (output & 0x1Fu)));
  } else {
    int8_t dbm = requested;
    if (dbm < -18) {
      dbm = -18;
    } else if (dbm > 13) {
      dbm = 13;
    }
    powerDbm_ = dbm;
    level = static_cast<uint8_t>(18 + dbm);
    writeReg(kRegPaLevel, static_cast<uint8_t>(kPa0On | (level & 0x1Fu)));
  }

  writeReg(kRegOcp, highPower_ ? 0x0F : 0x1A);
  const bool boost = highPower_ && level >= 20;
  writeReg(kRegTestPa1, boost ? 0x5D : 0x55);
  writeReg(kRegTestPa2, boost ? 0x7C : 0x70);
}

void Rfm69Radio::configureModem() {
  for (const auto& entry : kConfig) {
    writeReg(entry[0], entry[1]);
  }
}

void Rfm69Radio::applyChannel(uint8_t channel) {
  const uint32_t frf = davisChannelRegisterValue(channel);
  writeReg(kRegFrfMsb, static_cast<uint8_t>(frf >> 16));
  writeReg(kRegFrfMid, static_cast<uint8_t>(frf >> 8));
  writeReg(kRegFrfLsb, static_cast<uint8_t>(frf));
}

void Rfm69Radio::restartRx() {
  if ((readReg(kRegIrqFlags2) & kIrq2PayloadReady) != 0) {
    writeReg(kRegPacketConfig2,
             static_cast<uint8_t>((readReg(kRegPacketConfig2) & 0xFBu) | kPacket2RxRestart));
  }
  writeReg(kRegDioMapping1, 0x40);
  // Writing one to FIFOOVERRUN resets the FIFO and its status flags. Without
  // this, stale bytes from a noisy packet can be read as the next packet.
  writeReg(kRegIrqFlags2, 0x10);
  // Kick AFC/FEI like the prototype's receiveBegin().
  writeReg(kRegAfcFei, static_cast<uint8_t>(readReg(kRegAfcFei) | kAfcFeiFeiStart));
  setModeRx();
}

Status Rfm69Radio::begin(const RadioConfig& config) {
  config_ = config;
  highPower_ = config.highPower;
  payloadLength_ = config.fixedPayloadLength == 0 ? 8 : config.fixedPayloadLength;
  channel_ = config.channel < kDavisChannelCount ? config.channel : 0;

  pinMode(pins_.cs, OUTPUT);
  digitalWrite(pins_.cs, HIGH);

  // SX1231 RESET is active high. Pulse it high and return it low so a module
  // that kept a bad state across an MCU reset starts from a known state. On
  // USB1 this pin is not connected; the pulse is harmless there.
  pinMode(pins_.reset, OUTPUT);
  digitalWrite(pins_.reset, LOW);
  delay(2);
  digitalWrite(pins_.reset, HIGH);
  delay(5);
  digitalWrite(pins_.reset, LOW);
  delay(10);
  pinMode(pins_.dio0, INPUT);

  spi_.begin(pins_.sck, pins_.miso, pins_.mosi, pins_.cs);

  // Register read/write probe used by the prototype: if this fails the module
  // is not responding or the SPI wiring is wrong.
  uint32_t start = millis();
  do {
    writeReg(kRegSyncValue1, 0xAA);
  } while (readReg(kRegSyncValue1) != 0xAA && millis() - start < 50);
  if (readReg(kRegSyncValue1) != 0xAA) {
    ready_ = false;
    AK_LOG_ERROR("radio", "RFM69 SPI probe failed (sync register did not stick)");
    return Status::NotReady;
  }
  start = millis();
  do {
    writeReg(kRegSyncValue1, 0x55);
  } while (readReg(kRegSyncValue1) != 0x55 && millis() - start < 50);
  if (readReg(kRegSyncValue1) != 0x55) {
    ready_ = false;
    AK_LOG_ERROR("radio", "RFM69 SPI probe failed on second pattern");
    return Status::NotReady;
  }

  configureModem();
  applyChannel(channel_);
  configurePower();

  // Encryption is persistent across MCU resets; always turn it off.
  writeReg(kRegPacketConfig2,
           static_cast<uint8_t>(readReg(kRegPacketConfig2) & 0xFEu));

  setModeStandby();
  start = millis();
  while ((readReg(kRegIrqFlags1) & kIrq1ModeReady) == 0 &&
         millis() - start < kModeReadyTimeoutMs) {
    delay(1);
  }
  if ((readReg(kRegIrqFlags1) & kIrq1ModeReady) == 0) {
    ready_ = false;
    AK_LOG_ERROR("radio", "RFM69 never reported ModeReady");
    return Status::NotReady;
  }

  const uint8_t version = readReg(kRegVersion);
  ready_ = true;
  restartRx();
  AK_LOG_INFO("radio",
              "RFM69 ready: version=0x%02X ch=%u freq=%.3fMHz bitrate=19.2k dev=4.8k "
              "sync=CB89 power=%ddBm%s",
              version, static_cast<unsigned>(channel_), davisChannelFrequencyMhz(channel_),
              static_cast<int>(powerDbm_), highPower_ ? " high-power" : "");
  return Status::Ok;
}

Status Rfm69Radio::setChannel(uint8_t channel) {
  if (!ready_) {
    return Status::NotReady;
  }
  if (channel >= kDavisChannelCount) {
    return Status::InvalidArgument;
  }
  channel_ = channel;
  applyChannel(channel_);
  restartRx();
  return Status::Ok;
}

Status Rfm69Radio::hop() { return setChannel(nextDavisChannel(channel_)); }

Status Rfm69Radio::hopBack() { return setChannel(previousDavisChannel(channel_)); }

Status Rfm69Radio::receive(RadioFrame& frame, uint32_t timeoutMs) {
  if (!ready_) {
    return Status::NotReady;
  }

  const uint32_t start = millis();
  do {
    if ((readReg(kRegIrqFlags2) & kIrq2PayloadReady) != 0) {
      setModeStandby();
      const int16_t rssi = readRssiDbm();

      select();
      spi_.transfer(kRegFifo & 0x7F);
      for (uint8_t i = 0; i < payloadLength_; ++i) {
        frame.data[i] = reverseBits(spi_.transfer(0));
      }
      unselect();

      frame.length = payloadLength_;
      frame.rssiDbm = rssi;
      frame.receivedAtMs = millis();
      restartRx();
      return Status::Ok;
    }
    delay(1);
  } while (millis() - start < timeoutMs);

  return Status::Timeout;
}

Status Rfm69Radio::transmit(const uint8_t* data, size_t length) {
  if (!ready_) {
    return Status::NotReady;
  }
  if (data == nullptr || length != payloadLength_) {
    return Status::InvalidArgument;
  }

  setModeStandby();
  const uint32_t modeStart = millis();
  while ((readReg(kRegIrqFlags1) & kIrq1ModeReady) == 0 &&
         millis() - modeStart < kModeReadyTimeoutMs) {
    delay(1);
  }

  select();
  spi_.transfer(kRegFifo | 0x80);
  for (size_t i = 0; i < length; ++i) {
    spi_.transfer(reverseBits(data[i]));
  }
  unselect();

  setModeTx();
  const uint32_t txStart = millis();
  while ((readReg(kRegIrqFlags2) & kIrq2PacketSent) == 0 &&
         millis() - txStart < kPacketSentTimeoutMs) {
    delay(1);
  }
  const bool sent = (readReg(kRegIrqFlags2) & kIrq2PacketSent) != 0;
  setModeStandby();
  restartRx();
  return sent ? Status::Ok : Status::Timeout;
}

}  // namespace akashvani
