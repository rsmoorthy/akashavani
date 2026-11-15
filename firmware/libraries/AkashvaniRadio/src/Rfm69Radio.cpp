#include "Rfm69Radio.h"

#include <cstdio>
#include <ctime>
#include <cstring>

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

uint32_t currentEpochSeconds() {
  const time_t now = time(nullptr);
  return now > 946684800 ? static_cast<uint32_t>(now) : 0;
}

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

Rfm69Radio* Rfm69Radio::instance_ = nullptr;

Rfm69Radio::Rfm69Radio(SPIClass& spi, const RadioPins& pins) : spi_(spi), pins_(pins) {
  instance_ = this;
}

Rfm69Radio::~Rfm69Radio() {
  if (backgroundRx_) {
    detachInterrupt(digitalPinToInterrupt(pins_.dio0));
  }
  if (rxTask_ != nullptr) {
    vTaskDelete(rxTask_);
    rxTask_ = nullptr;
  }
  if (rxQueue_ != nullptr) {
    vQueueDelete(rxQueue_);
    rxQueue_ = nullptr;
  }
  if (radioMutex_ != nullptr) {
    vSemaphoreDelete(radioMutex_);
    radioMutex_ = nullptr;
  }
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

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
  lastHopMs_ = millis();
  lastValidMs_ = 0;
  missStreak_ = 0;
  searchMode_ = false;
  if (config_.backgroundRx && !startBackgroundRx()) {
    AK_LOG_WARN("radio", "interrupt RX unavailable; using polling receive");
  }
  AK_LOG_INFO("radio",
              "RFM69 ready: version=0x%02X ch=%u freq=%.3fMHz bitrate=19.2k dev=4.8k "
              "sync=CB89 power=%ddBm%s rx=%s",
              version, static_cast<unsigned>(channel_), davisChannelFrequencyMhz(channel_),
              static_cast<int>(powerDbm_), highPower_ ? " high-power" : "",
              backgroundRx_ ? "interrupt-task" : "polling");
  return Status::Ok;
}

bool Rfm69Radio::startBackgroundRx() {
  if (payloadLength_ > 8) {
    AK_LOG_WARN("radio", "payload length %u exceeds RX queue record; polling",
                static_cast<unsigned>(payloadLength_));
    return false;
  }
  const int interrupt = digitalPinToInterrupt(pins_.dio0);
  if (interrupt < 0) {
    return false;
  }
  radioMutex_ = xSemaphoreCreateMutex();
  rxQueue_ = xQueueCreate(kRxQueueDepth, sizeof(RxRecord));
  if (radioMutex_ == nullptr || rxQueue_ == nullptr) {
    if (radioMutex_ != nullptr) {
      vSemaphoreDelete(radioMutex_);
      radioMutex_ = nullptr;
    }
    if (rxQueue_ != nullptr) {
      vQueueDelete(rxQueue_);
      rxQueue_ = nullptr;
    }
    return false;
  }
  if (xTaskCreatePinnedToCore(taskEntry, "rfm69-rx", 3072, this, 10, &rxTask_, 1) != pdPASS) {
    rxTask_ = nullptr;
    vQueueDelete(rxQueue_);
    rxQueue_ = nullptr;
    vSemaphoreDelete(radioMutex_);
    radioMutex_ = nullptr;
    return false;
  }
  attachInterrupt(static_cast<uint8_t>(interrupt), isrTrampoline, RISING);
  backgroundRx_ = true;
  // Drain anything already waiting in the FIFO.
  xTaskNotifyGive(rxTask_);
  return true;
}

Status Rfm69Radio::setChannel(uint8_t channel) {
  if (!ready_) {
    return Status::NotReady;
  }
  if (channel >= kDavisChannelCount) {
    return Status::InvalidArgument;
  }
  lockRadio();
  channel_ = channel;
  lastHopMs_ = millis();
  missStreak_ = 0;
  searchMode_ = false;
  applyChannel(channel_);
  restartRx();
  unlockRadio();
  return Status::Ok;
}

Status Rfm69Radio::hop() { return setChannel(nextDavisChannel(channel_)); }

Status Rfm69Radio::hopBack() { return setChannel(previousDavisChannel(channel_)); }

Status Rfm69Radio::receive(RadioFrame& frame, uint32_t timeoutMs) {
  if (!ready_) {
    return Status::NotReady;
  }
  if (backgroundRx_ && rxQueue_ != nullptr) {
    RxRecord record;
    if (xQueueReceive(rxQueue_, &record, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
      return Status::Timeout;
    }
    std::memcpy(frame.data, record.data, record.length);
    frame.length = record.length;
    frame.rssiDbm = record.rssiDbm;
    frame.receivedAtMs = millis();
    frame.receivedAtEpoch = record.receivedAtEpoch;
    frame.channel = record.channel;
    return Status::Ok;
  }
  return pollReceive(frame, timeoutMs);
}

Status Rfm69Radio::pollReceive(RadioFrame& frame, uint32_t timeoutMs) {
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
      frame.receivedAtEpoch = currentEpochSeconds();
      frame.channel = channel_;
      restartRx();
      return Status::Ok;
    }
    delay(1);
  } while (millis() - start < timeoutMs);

  return Status::Timeout;
}

void Rfm69Radio::taskEntry(void* context) {
  static_cast<Rfm69Radio*>(context)->taskLoop();
}

void IRAM_ATTR Rfm69Radio::isrTrampoline() {
  if (instance_ == nullptr || instance_->rxTask_ == nullptr) {
    return;
  }
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(instance_->rxTask_, &woken);
  portYIELD_FROM_ISR(woken);
}

void Rfm69Radio::taskLoop() {
  for (;;) {
    const uint32_t now = millis();
    const uint32_t interval =
        searchMode_ ? config_.searchHopIntervalMs : config_.hopIntervalMs;
    const uint32_t elapsed = now - lastHopMs_;
    uint32_t waitMs = 0;
    if (interval == 0) {
      waitMs = portMAX_DELAY;
    } else if (elapsed < interval) {
      waitMs = interval - elapsed;
    }
    const uint32_t notified =
        ulTaskNotifyTake(pdTRUE, waitMs == portMAX_DELAY ? portMAX_DELAY
                                                         : pdMS_TO_TICKS(waitMs));
    if (!ready_) {
      continue;
    }
    if (notified > 0) {
      drainFifo();
    }
    handleHopTimer();
  }
}

void Rfm69Radio::drainFifo() {
  if (rxQueue_ == nullptr) {
    return;
  }
  lockRadio();
  // Usually one packet per interrupt. If the task was delayed by the shared
  // SPI bus, keep reading whole packets until the FIFO is empty (bounded).
  const uint8_t first = readReg(kRegIrqFlags2);
  if ((first & kIrq2PayloadReady) == 0) {
    unlockRadio();
    return;
  }
  setModeStandby();
  const uint8_t rxChannel = channel_;
  RxRecord record;
  record.rssiDbm = readRssiDbm();
  record.receivedAtEpoch = currentEpochSeconds();
  record.channel = rxChannel;
  record.length = payloadLength_;

  select();
  spi_.transfer(kRegFifo & 0x7F);
  for (uint8_t i = 0; i < payloadLength_; ++i) {
    record.data[i] = reverseBits(spi_.transfer(0));
  }
  unselect();

  const bool valid =
      config_.frameValidator == nullptr ||
      config_.frameValidator(record.data, payloadLength_);
  if (valid) {
    const uint32_t now = millis();
    if (lastValidMs_ != 0 && now - lastValidMs_ > 6000) {
      resyncCount_ = resyncCount_ + 1;
      AK_LOG_WARN("sync", "RESYNC silence_ms=%lu ch=%u",
                  static_cast<unsigned long>(now - lastValidMs_),
                  static_cast<unsigned>(rxChannel));
    }
    lastValidMs_ = now;
    validFrames_ = validFrames_ + 1;
    missStreak_ = 0;
    searchMode_ = false;
  } else {
    badFrames_ = badFrames_ + 1;
    badRssiSum_ = badRssiSum_ + record.rssiDbm;
    badDeltaCount_ = badDeltaCount_ + 1;
    badDeltaRssiSum_ = badDeltaRssiSum_ + record.rssiDbm;
    if (badDeltaCount_ == 1 || record.rssiDbm < badDeltaRssiMin_) {
      badDeltaRssiMin_ = record.rssiDbm;
    }
    if (badDeltaCount_ == 1 || record.rssiDbm > badDeltaRssiMax_) {
      badDeltaRssiMax_ = record.rssiDbm;
    }
    if (badFrames_ == 1 || record.rssiDbm < badRssiMin_) {
      badRssiMin_ = record.rssiDbm;
    }
    if (badFrames_ == 1 || record.rssiDbm > badRssiMax_) {
      badRssiMax_ = record.rssiDbm;
    }
    if (record.rssiDbm >= config_.badFrameLogRssi) {
      char raw[3 * sizeof(record.data) + 1] = {};
      size_t used = 0;
      for (uint8_t i = 0; i < record.length && used + 4 <= sizeof(raw); ++i) {
        const int written = std::snprintf(raw + used, sizeof(raw) - used, "%02X%s",
                                          record.data[i],
                                          i + 1 == record.length ? "" : " ");
        if (written > 0) {
          used += static_cast<size_t>(written);
        }
      }
      AK_LOG_WARN("rx", "RX_BAD epoch=%lu ch=%u rssi=%d raw=%s",
                  static_cast<unsigned long>(record.receivedAtEpoch),
                  static_cast<unsigned>(record.channel), static_cast<int>(record.rssiDbm),
                  raw);
    }
  }

  if (valid && xQueueSend(rxQueue_, &record, 0) != pdTRUE) {
    droppedFrames_ = droppedFrames_ + 1;
    if (droppedFrames_ == 1 || (droppedFrames_ % 50U) == 0) {
      AK_LOG_WARN("radio", "RX queue full; dropped=%lu",
                  static_cast<unsigned long>(droppedFrames_));
    }
  }
  if (valid) {
    hopLocked(false, "rx");
  } else {
    restartRx();
  }
  unlockRadio();
}

void Rfm69Radio::handleHopTimer() {
  lockRadio();
  const uint32_t interval =
      searchMode_ ? config_.searchHopIntervalMs : config_.hopIntervalMs;
  if (!ready_ || interval == 0) {
    unlockRadio();
    return;
  }
  const uint32_t now = millis();
  if (now - lastHopMs_ < interval) {
    unlockRadio();
    return;
  }
  if (!searchMode_) {
    missStreak_ = static_cast<uint8_t>(missStreak_ + 1);
    missedSlots_ = missedSlots_ + 1;
    if (missStreak_ >= config_.searchAfterMisses) {
      searchMode_ = true;
      searchCount_ = searchCount_ + 1;
      AK_LOG_WARN("sync", "SEARCH after %u missed slots; reverse scan from ch=%u",
                  static_cast<unsigned>(missStreak_),
                  static_cast<unsigned>(channel_));
    }
  }
  hopLocked(searchMode_, searchMode_ ? "search" : "timeout");
  unlockRadio();
}

void Rfm69Radio::hopLocked(bool backwards, const char* reason) {
  const uint8_t from = channel_;
  channel_ = backwards ? previousDavisChannel(channel_) : nextDavisChannel(channel_);
  applyChannel(channel_);
  restartRx();
  lastHopMs_ = millis();
  AK_LOG_INFO("hop", "HOP ch=%u from=%u reason=%s miss=%u search=%u",
              static_cast<unsigned>(channel_), static_cast<unsigned>(from),
              reason == nullptr ? "-" : reason, static_cast<unsigned>(missStreak_),
              searchMode_ ? 1u : 0u);
}

Rfm69Radio::RadioStats Rfm69Radio::stats() const {
  RadioStats snapshot;
  snapshot.validFrames = validFrames_;
  snapshot.missedSlots = missedSlots_;
  snapshot.resyncCount = resyncCount_;
  snapshot.searchCount = searchCount_;
  snapshot.badFrames = badFrames_;
  snapshot.badRssiMin = badRssiMin_;
  snapshot.badRssiMax = badRssiMax_;
  snapshot.badRssiAvg = badFrames_ > 0
                            ? static_cast<int16_t>(badRssiSum_ / static_cast<int32_t>(badFrames_))
                            : 0;
  snapshot.droppedFrames = droppedFrames_;
  snapshot.channel = channel_;
  snapshot.searchMode = searchMode_;
  return snapshot;
}

Rfm69Radio::BadStats Rfm69Radio::takeBadStats() {
  BadStats snapshot;
  lockRadio();
  snapshot.count = badDeltaCount_;
  snapshot.rssiMin = badDeltaRssiMin_;
  snapshot.rssiSum = badDeltaRssiSum_;
  snapshot.rssiMax = badDeltaRssiMax_;
  badDeltaCount_ = 0;
  badDeltaRssiMin_ = 0;
  badDeltaRssiSum_ = 0;
  badDeltaRssiMax_ = 0;
  unlockRadio();
  return snapshot;
}

uint16_t Rfm69Radio::pendingFrames() const {
  if (rxQueue_ == nullptr) {
    return 0;
  }
  return static_cast<uint16_t>(uxQueueMessagesWaiting(rxQueue_));
}

void Rfm69Radio::lockRadio() {
  if (radioMutex_ != nullptr) {
    xSemaphoreTake(radioMutex_, portMAX_DELAY);
  }
}

void Rfm69Radio::unlockRadio() {
  if (radioMutex_ != nullptr) {
    xSemaphoreGive(radioMutex_);
  }
}

Status Rfm69Radio::transmit(const uint8_t* data, size_t length) {
  if (!ready_) {
    return Status::NotReady;
  }
  if (data == nullptr || length != payloadLength_) {
    return Status::InvalidArgument;
  }

  lockRadio();
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
  unlockRadio();
  return sent ? Status::Ok : Status::Timeout;
}

}  // namespace akashvani
