// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

namespace hub75 {
struct Hub12IdleDeadline {
  bool pending = false;
  int64_t deadline_us = 0;
  void arm(int64_t now_us, int64_t delay_us) { pending = true; deadline_us = now_us + delay_us; }
  void cancel() { pending = false; }
  bool due(int64_t now_us) const { return pending && now_us >= deadline_us; }
};
// Tracks the pixels which survive color conversion in each DMA buffer.
struct Hub12RefreshState {
  uint32_t nonblack[2] = {0, 0};

  __attribute__((always_inline)) inline void update_pixel(unsigned buffer, bool was_lit, bool is_lit) {
    if (was_lit != is_lit) {
      if (is_lit) ++nonblack[buffer];
      else --nonblack[buffer];
    }
  }

  bool should_refresh(unsigned front, uint8_t brightness) const { return brightness != 0 && nonblack[front] != 0; }
};

// Called only while OE is inactive and DATA is already held at physical black.
// All panel rows share CLK and DATA, so one black payload clears every row.
template<typename SetClock, typename SetLatches, typename Delay>
inline void hub12_latch_black(uint32_t columns, SetClock set_clock, SetLatches set_latches, Delay delay) {
  delay();
  for (uint32_t i = 0; i < 128U * columns; ++i) {
    set_clock(true);
    delay();
    set_clock(false);
    delay();
  }
  set_latches(true);
  delay();
  set_latches(false);
}
}  // namespace hub75
