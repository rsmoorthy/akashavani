#include "Ssd1306Display.h"

#include <cstdio>
#include <cstring>

#include "Log.h"

namespace akashvani {
namespace {

// 128x64 at text size 1 is 21 characters by 8 rows.
constexpr uint8_t kTextSize = 1;
constexpr uint8_t kRowHeight = 8;
constexpr uint8_t kColumns = 21;

void copyTruncated(char* target, size_t targetSize, const char* source) {
  if (targetSize == 0) {
    return;
  }
  if (source == nullptr) {
    target[0] = '\0';
    return;
  }
  std::strncpy(target, source, targetSize - 1);
  target[targetSize - 1] = '\0';
}

}  // namespace

Ssd1306Display::Ssd1306Display(TwoWire& wire, const Ssd1306Config& config)
    : wire_(wire),
      config_(config),
      driver_(config.width, config.height, &wire, config.resetPin) {}

Status Ssd1306Display::begin() {
  ready_ = false;
  wire_.begin(config_.sdaPin, config_.sclPin);
  wire_.setClock(config_.i2cFrequencyHz);

  // periphBegin=false: Wire is already started with the bench pins above.
  if (!driver_.begin(SSD1306_SWITCHCAPVCC, config_.address, true, false)) {
    AK_LOG_WARN("display", "SSD1306 not found at 0x%02X", config_.address);
    return Status::Unsupported;
  }
  driver_.setTextSize(kTextSize);
  driver_.setTextColor(SSD1306_WHITE);
  driver_.clearDisplay();
  driver_.display();
  ready_ = true;
  AK_LOG_INFO("display", "SSD1306 %ux%u ready at 0x%02X", config_.width,
              config_.height, config_.address);
  return Status::Ok;
}

void Ssd1306Display::drawRow(uint8_t row, const char* text) {
  char line[kColumns + 1];
  copyTruncated(line, sizeof(line), text);
  driver_.setCursor(0, row * kRowHeight);
  driver_.print(line);
}

void Ssd1306Display::drawHeader() {
  // The two-colour panels (GME12864-54 "Y-B") have a fixed 16 px yellow band at
  // the top. Give the title the whole band and keep every data row in the blue
  // area below it.
  driver_.fillRect(0, 0, config_.width, kRowHeight * 2, SSD1306_WHITE);
  driver_.setTextColor(SSD1306_BLACK);
  char title[kColumns + 1];
  copyTruncated(title, sizeof(title), "Akashvani");
  driver_.setCursor(0, kRowHeight / 2);
  driver_.print(title);
  driver_.setTextColor(SSD1306_WHITE);
}

void Ssd1306Display::render(const DisplayState& state) {
  if (!ready_) {
    return;
  }

  char line[kColumns + 1];
  driver_.clearDisplay();
  drawHeader();

  // WiFi line: SSID first, because that is the one thing the bench operator
  // needs to see on both boards. Fall back to the provisioning AP when the
  // station link is down. Rows 2+ are in the blue band of a Y-B panel.
  if (state.wifiConnected && state.wifiSsid[0] != '\0') {
    std::snprintf(line, sizeof(line), "WiFi %s", state.wifiSsid);
  } else if (state.apSsid[0] != '\0') {
    std::snprintf(line, sizeof(line), "AP %s", state.apSsid);
  } else {
    std::snprintf(line, sizeof(line), "WiFi --");
  }
  drawRow(2, line);

  if (state.wifiIp[0] != '\0' && std::strcmp(state.wifiIp, "0.0.0.0") != 0) {
    std::snprintf(line, sizeof(line), "IP %s %ddBm", state.wifiIp, state.rssiDbm);
  } else {
    std::snprintf(line, sizeof(line), "IP --");
  }
  drawRow(3, line);

  drawRow(4, state.line1);
  drawRow(5, state.line2);
  drawRow(6, state.line3);

  std::snprintf(line, sizeof(line), "%s %s %s",
                state.timeValid ? "T" : "-",
                state.radioReady ? "R" : "-",
                state.archiveReady ? "S" : "-");
  drawRow(7, line);

  driver_.display();
}

void Ssd1306Display::loop() {}

}  // namespace akashvani
