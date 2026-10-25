#include "DavisEncoder.h"

namespace akashvani {

const char* toString(DavisEncodeStatus status) {
  switch (status) {
    case DavisEncodeStatus::Ok:
      return "ok";
    case DavisEncodeStatus::BufferTooSmall:
      return "buffer-too-small";
    case DavisEncodeStatus::UnsupportedProtocol:
      return "unsupported-protocol";
    case DavisEncodeStatus::InvalidObservation:
      return "invalid-observation";
  }
  return "unknown";
}

DavisEncodeStatus encodeDavisFrame(const DavisObservation& observation, uint8_t* output,
                                   size_t outputSize, size_t& written) {
  (void)observation;
  (void)output;
  (void)outputSize;
  written = 0;
  // TODO(protocol): mirror the receiver decoder. Keeping encoder and decoder in
  // this library makes loopback tests possible without a second board.
  return DavisEncodeStatus::UnsupportedProtocol;
}

}  // namespace akashvani
