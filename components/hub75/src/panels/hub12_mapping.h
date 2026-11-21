// SPDX-License-Identifier: MIT
// Mapping for 32x16, 1/4-scan HUB12 P10 panels with independent latch rows.
#pragma once
#include <stdint.h>

namespace hub75 {
struct Hub12Position {
  uint16_t address;
  uint16_t word;
};

// The I2S backend's shared signal word layout.
inline constexpr uint16_t HUB12_DATA = 1U << 0;
inline constexpr uint16_t HUB12_LAT1 = 1U << 11;
inline constexpr uint16_t HUB12_OE = 1U << 12;
inline constexpr uint16_t HUB12_LAT2 = 1U << 13;

// Each segment has a leading clock and a trailing clock around its payload.
// LAT is asserted on the final payload word; panel observations indicate its
// latch captures that word's clock. The next payload overwrites the harmless
// bits shifted between latch pulses. Edge timing still needs panel validation.
inline constexpr uint32_t hub12_segment_words(uint32_t columns) { return 128U * columns + 2U; }

inline constexpr uint16_t hub12_control_word(unsigned address, unsigned word, unsigned segment, unsigned rows,
                                              bool invert_data) {
  return static_cast<uint16_t>((address << 6) | HUB12_OE | (invert_data ? HUB12_DATA : 0) |
                               (word == segment - 2 ? HUB12_LAT1 : 0) |
                               (rows == 2 && word == 2 * segment - 2 ? HUB12_LAT2 : 0));
}

inline constexpr bool hub12_display_enabled(unsigned word, unsigned segment, unsigned rows, unsigned enabled) {
  const unsigned first = rows * segment + (segment - enabled) / 2;
  return word >= first && word < first + enabled;
}

// Coordinates are physical, before the ESP32 I2S FIFO word-pair swap.
__attribute__((always_inline)) inline constexpr Hub12Position hub12_position(uint16_t x, uint16_t y, uint16_t columns) {
  const uint32_t width = 32U * columns;
  const uint32_t payload = width * 4U;
  const uint32_t segment = hub12_segment_words(columns);
  const uint32_t reversed_x = width - 1U - x;
  const uint32_t byte_offset = (reversed_x / 32U) * 16U + ((reversed_x % 32U) / 8U) * 4U + (y % 16U) / 4U;
  const uint32_t serial_bit = (payload / 8U - 1U - byte_offset) * 8U + 7U - (reversed_x % 8U);
  return {static_cast<uint16_t>(y % 4U), static_cast<uint16_t>((y / 16U) * segment + 1U + serial_bit)};
}
}  // namespace hub75
