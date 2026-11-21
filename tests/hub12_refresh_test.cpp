// SPDX-License-Identifier: MIT
#include "../components/hub75/src/panels/hub12_refresh.h"
#include <cassert>
#include <initializer_list>

int main() {
  // A clear followed by a redraw cancels the scheduled idle stop. A stale
  // callback cannot stop a later black period before its new deadline.
  hub75::Hub12IdleDeadline idle;
  idle.arm(1000, 100000);
  assert(!idle.due(100999));
  idle.cancel();
  assert(!idle.due(101000));
  idle.arm(50000, 100000);
  assert(!idle.due(101000));
  assert(idle.due(150000));
  idle.cancel();
  assert(!idle.due(160000));
  hub75::Hub12RefreshState state;
  unsigned front = 0;
  unsigned back = 1;
  assert(!state.should_refresh(front, 255));

  // Writing the back buffer does not make a black front buffer visible.
  state.update_pixel(back, false, true);
  state.update_pixel(back, false, true);
  state.update_pixel(back, true, true);
  assert(state.nonblack[back] == 2);
  assert(!state.should_refresh(front, 255));

  front = 1;
  back = 0;
  assert(state.should_refresh(front, 255));
  assert(!state.should_refresh(front, 0));
  // Returning from brightness zero preserves the visible image.
  assert(state.should_refresh(front, 1));

  // A clear of the back buffer must not stop a nonblack front buffer.
  state.nonblack[back] = 0;
  assert(state.should_refresh(front, 255));
  front = 0;
  back = 1;
  assert(!state.should_refresh(front, 255));
  assert(state.nonblack[back] == 2);

  // In single-buffer mode, replacing the last lit pixel makes it black.
  state.update_pixel(back, true, false);
  assert(state.should_refresh(back, 255));
  state.update_pixel(back, true, false);
  assert(!state.should_refresh(back, 255));
  state.update_pixel(back, false, false);
  assert(state.nonblack[back] == 0);

  // OE remains disabled, the clock is idle low, and one full black payload
  // reaches both shared shift-register chains before either latch is pulsed.
  for (unsigned columns : {1U, 6U}) {
    unsigned rising_edges = 0;
    unsigned latch_events = 0;
    bool clock = false;
    bool latches = false;
    hub75::hub12_latch_black(
        columns,
        [&](bool high) {
          assert(!latches && high != clock);
          clock = high;
          if (high) ++rising_edges;
        },
        [&](bool active) {
          assert(!clock && active != latches);
          assert(rising_edges == 128 * columns);
          latches = active;
          ++latch_events;
        },
        [] {});
    assert(rising_edges == 128 * columns && latch_events == 2);
    assert(!clock && !latches);
  }
}
