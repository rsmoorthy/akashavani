#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE_VERSION="${AKASHAVANI_ESP32_CORE:-3.0.2}"
URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli is required (>=1.0). See https://arduino.github.io/arduino-cli/" >&2
  exit 1
fi

echo "Installing ESP32 core ${CORE_VERSION}..."
arduino-cli core update-index --additional-urls "$URL"
arduino-cli core install "esp32:esp32@${CORE_VERSION}" --additional-urls "$URL"

echo "Installing Arduino libraries..."
arduino-cli lib install "ArduinoJson@7.1.0"
arduino-cli lib install "RTClib@2.1.4"
arduino-cli lib install "Adafruit BME280 Library@2.2.4"
arduino-cli lib install "Adafruit SSD1306@2.5.13"

echo
echo "Firmware toolchain ready. Local libraries are passed at build time from:"
echo "  $ROOT/firmware/libraries"
arduino-cli version
