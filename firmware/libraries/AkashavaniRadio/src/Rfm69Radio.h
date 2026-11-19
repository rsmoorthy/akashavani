#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstddef>
#include <cstdint>

#include "IRadio.h"
#include "RadioConfig.h"
#include "RadioFrame.h"

namespace akashavani {

// RFM69 (SX1231/SX1231H) driver for the Davis ISS receive/transmit path.
//
// Configuration is taken from the validated prototype in
// prototype/lib2/DavisRFM69. Keep the modem values, sync bytes and fixed
// eight-byte payload in step with that code and with the transmitter firmware.
// The RFM69 sends/receives the least-significant bit of each byte first; the
// prototype stores Davis bytes reversed on air, so this driver reverses each
// byte on transmit and receive.
class Rfm69Radio final : public IRadio {
 public:
  Rfm69Radio(SPIClass& spi, const RadioPins& pins);
  ~Rfm69Radio() override;

  Status begin(const RadioConfig& config) override;
  Status receive(RadioFrame& frame, uint32_t timeoutMs) override;
  Status transmit(const uint8_t* data, size_t length) override;

  bool isReady() const override { return ready_; }
  const char* name() const override { return "rfm69"; }

  // Frames waiting in the RAM queue, and frames dropped because the queue was
  // full while the main loop was busy (OTA, data replay).
  uint16_t pendingFrames() const;
  uint32_t droppedFrames() const { return droppedFrames_; }

  struct RadioStats {
    uint32_t validFrames = 0;
    uint32_t missedSlots = 0;
    uint32_t resyncCount = 0;
    uint32_t searchCount = 0;
    uint32_t badFrames = 0;
    int16_t badRssiMin = 0;
    int16_t badRssiAvg = 0;
    int16_t badRssiMax = 0;
    uint32_t droppedFrames = 0;
    uint8_t channel = 0;
    bool searchMode = false;
  };
  RadioStats stats() const;

  struct BadStats {
    uint32_t count = 0;
    int16_t rssiMin = 0;
    int32_t rssiSum = 0;
    int16_t rssiMax = 0;
  };
  // Returns invalid-frame RSSI stats accumulated since the previous call and
  // resets the delta. Cumulative totals remain in RadioStats.
  BadStats takeBadStats();

  // Davis channel index (0..50). Writing the channel restarts RX.
  Status setChannel(uint8_t channel);
  uint8_t channel() const { return channel_; }
  Status hop();
  Status hopBack();

  // Diagnostic register access; useful for the bench bring-up sketches.
  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  int16_t readRssiDbm();

 private:
  void select();
  void unselect();
  uint8_t readReg(uint8_t address);
  void writeReg(uint8_t address, uint8_t value);
  void setModeStandby();
  void setModeRx();
  void setModeTx();
  void restartRx();
  void applyChannel(uint8_t channel);
  void configurePower();
  void configureModem();
  static uint8_t reverseBits(uint8_t value);

  // Interrupt-driven RX. The ISR only notifies this task; the task drains the
  // RFM69 FIFO into a fixed RAM queue so frames survive a blocking OTA or
  // replay in the main loop.
  static void taskEntry(void* context);
  static void IRAM_ATTR isrTrampoline();
  void taskLoop();
  void drainFifo();
  void handleHopTimer();
  void hopLocked(bool backwards, const char* reason);
  Status pollReceive(RadioFrame& frame, uint32_t timeoutMs);  bool startBackgroundRx();
  void lockRadio();
  void unlockRadio();

  struct RxRecord {
    uint8_t data[8] = {};
    int16_t rssiDbm = 0;
    uint32_t receivedAtEpoch = 0;
    uint8_t channel = 0;
    uint8_t length = 0;
  };

  static constexpr uint16_t kRxQueueDepth = 128;
  static Rfm69Radio* instance_;

  SPIClass& spi_;
  RadioPins pins_;
  RadioConfig config_;
  bool ready_ = false;
  bool highPower_ = true;
  bool backgroundRx_ = false;
  uint8_t channel_ = 0;
  uint8_t payloadLength_ = 8;
  int8_t powerDbm_ = 17;
  QueueHandle_t rxQueue_ = nullptr;
  TaskHandle_t rxTask_ = nullptr;
  SemaphoreHandle_t radioMutex_ = nullptr;
  volatile uint32_t droppedFrames_ = 0;
  uint32_t lastHopMs_ = 0;
  uint32_t lastValidMs_ = 0;
  volatile uint32_t validFrames_ = 0;
  volatile uint32_t missedSlots_ = 0;
  volatile uint32_t resyncCount_ = 0;
  volatile uint32_t searchCount_ = 0;
  volatile uint32_t badFrames_ = 0;
  volatile int16_t badRssiMin_ = 0;
  volatile int16_t badRssiMax_ = 0;
  volatile int32_t badRssiSum_ = 0;
  uint32_t badDeltaCount_ = 0;
  int16_t badDeltaRssiMin_ = 0;
  int32_t badDeltaRssiSum_ = 0;
  int16_t badDeltaRssiMax_ = 0;
  uint8_t missStreak_ = 0;
  bool searchMode_ = false;
};

}  // namespace akashavani
