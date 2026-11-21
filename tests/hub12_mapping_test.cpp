// SPDX-License-Identifier: MIT
#include "../components/hub75/src/panels/hub12_mapping.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>

// Independently model the reference's byte-oriented, MSB-first SPI stream.
static uint32_t reference_word(unsigned x, unsigned y, unsigned columns) {
  const unsigned width = 32 * columns;
  const unsigned mirror_x = width - x - 1;
  const unsigned bytes_per_scan = width / 8 * 4;
  const unsigned byte_from_end = (mirror_x / 8) * 4 + ((y % 16) / 4);
  const unsigned byte_from_start = bytes_per_scan - 1 - byte_from_end;
  return (y / 16) * hub75::hub12_segment_words(columns) + 1 + byte_from_start * 8 + (7 - mirror_x % 8);
}

int main() {
  for (unsigned columns : {1U, 2U, 6U}) {
    const unsigned width = 32 * columns;
    const unsigned payload = width * 4;
    const unsigned segment = hub75::hub12_segment_words(columns);
    for (unsigned rows : {1U, 2U}) {
      std::vector<unsigned> used(4 * segment * rows);
      for (unsigned y = 0; y < 16 * rows; ++y) {
        for (unsigned x = 0; x < width; ++x) {
          auto p = hub75::hub12_position(x, y, columns);
          assert(p.address == y % 4);
          assert(p.word == reference_word(x, y, columns));
          assert(p.word < segment * rows);
          assert(++used[p.address * segment * rows + p.word] == 1);
        }
      }
      for (unsigned address = 0; address < 4; ++address) {
        for (unsigned row = 0; row < rows; ++row) {
          for (unsigned word = 0; word < segment; ++word) {
            assert(used[address * segment * rows + row * segment + word] == (word > 0 && word <= payload ? 1U : 0U));
          }
        }
      }
    }
  }
  for (unsigned rows : {1U, 2U}) {
    constexpr unsigned segment = hub75::hub12_segment_words(6);
    for (bool inverted : {false, true}) {
      for (unsigned address = 0; address < 4; ++address) {
        unsigned latches[2] = {};
        for (unsigned x = 0; x < (rows + 1) * segment; ++x) {
          const auto word = hub75::hub12_control_word(address, x, segment, rows, inverted);
          assert(((word >> 6) & 31) == address);
          assert(word & hub75::HUB12_OE);
          assert(bool(word & hub75::HUB12_DATA) == inverted);
          if (word & hub75::HUB12_LAT1) { assert(x == segment - 2); ++latches[0]; }
          if (word & hub75::HUB12_LAT2) { assert(x == 2 * segment - 2); ++latches[1]; }
        }
        assert(latches[0] == 1 && latches[1] == rows - 1);
        for (unsigned enabled : {0U, 1U, segment - 4}) {
          unsigned count = 0;
          for (unsigned x = 0; x < (rows + 1) * segment; ++x) {
            if (hub75::hub12_display_enabled(x, segment, rows, enabled)) {
              assert(x >= rows * segment);
              const auto word = hub75::hub12_control_word(address, x, segment, rows, inverted);
              assert(!(word & (hub75::HUB12_LAT1 | hub75::HUB12_LAT2)));
              ++count;
            }
          }
          assert(count == enabled);
        }
      }
    }
  }

  // Model both rows' shift registers, including the two extra clocks per
  // segment. Model the panel capturing after the latch word's clock edge.
  constexpr unsigned columns = 6;
  constexpr unsigned payload = 128 * columns;
  constexpr unsigned segment = hub75::hub12_segment_words(columns);
  for (unsigned address = 0; address < 4; ++address) {
    std::vector<bool> stream(segment * 3);
    for (unsigned y = address; y < 32; y += 4) {
      for (unsigned x = 0; x < 32 * columns; ++x) {
        auto pos = hub75::hub12_position(x, y, columns);
        stream[pos.word] = ((x * 17 + y * 31) % 7) < 3;
      }
    }
    std::deque<bool> shifted(payload, false);
    for (unsigned word = 0; word < stream.size(); ++word) {
      const auto controls = hub75::hub12_control_word(address, word, segment, 2, false);
      shifted.pop_front();
      shifted.push_back(stream[word]);
      if (controls & (hub75::HUB12_LAT1 | hub75::HUB12_LAT2)) {
        const unsigned panel_row = (controls & hub75::HUB12_LAT2) ? 1 : 0;
        for (unsigned i = 0; i < payload; ++i) assert(shifted[i] == stream[panel_row * segment + 1 + i]);
      }
    }
  }

  // Exercise the 8x2 configuration with a sparse pixel, ESP32 FIFO word
  // swapping, and the 4092-byte DMA descriptor split. A solid fill would not
  // reveal a stuck data bit, an ordering error, or an offset at that split.
  for (bool invert : {false, true}) {
    constexpr unsigned cols = 8;
    constexpr unsigned pixels = 128 * cols;
    constexpr unsigned words_per_segment = hub75::hub12_segment_words(cols);
    constexpr unsigned words_per_plane = words_per_segment * 3;
    for (unsigned y : {0U, 15U, 16U, 31U}) {
      for (unsigned x : {0U, 1U, 127U, 254U, 255U}) {
        const auto pixel = hub75::hub12_position(x, y, cols);
        std::vector<uint16_t> dma(words_per_plane);
        for (unsigned word = 0; word < words_per_plane; ++word)
          dma[word ^ 1U] = hub75::hub12_control_word(pixel.address, word, words_per_segment, 2, invert);
        auto &pixel_word = dma[pixel.word ^ 1U];
        if (invert) pixel_word &= ~hub75::HUB12_DATA;
        else pixel_word |= hub75::HUB12_DATA;

        std::deque<bool> shift(pixels, false);
        unsigned latches_seen = 0;
        const unsigned total_bytes = 2 * words_per_plane;
        for (unsigned chunk = 0; chunk < total_bytes; chunk += 4092) {
          const unsigned end = std::min(total_bytes, chunk + 4092U);
          assert(chunk % 4 == 0 && end % 4 == 0);
          for (unsigned byte = chunk; byte < end; byte += 2) {
            const unsigned word = byte / 2;
            const uint16_t out = dma[word ^ 1U];
            shift.pop_front();
            shift.push_back(bool(out & hub75::HUB12_DATA) != invert);
            if (out & (hub75::HUB12_LAT1 | hub75::HUB12_LAT2)) {
              const unsigned row = (out & hub75::HUB12_LAT2) ? 1 : 0;
              unsigned lit = 0;
              for (bool state : shift) lit += state;
              assert(lit == (row == y / 16 ? 1U : 0U));
              if (row == y / 16) assert(shift[pixel.word - row * words_per_segment - 1]);
              ++latches_seen;
            }
          }
        }
        assert(latches_seen == 2);
      }
    }
  }

  // Reproduce the reported 8x8 failure using the old payload alignment, and
  // verify the adjusted alignment under the same latch/clock timing model.
  {
    constexpr unsigned cols = 8;
    constexpr unsigned pixels = 128 * cols;
    constexpr unsigned seg = hub75::hub12_segment_words(cols);
    for (bool old_alignment : {false, true}) for (unsigned address = 0; address < 4; ++address) {
      std::vector<bool> stream(seg * 3);
      for (unsigned y = address; y < 8; y += 4)
        for (unsigned x = 0; x < 8; ++x)
          stream[hub75::hub12_position(x, y, cols).word - old_alignment] = true;
      std::deque<bool> shift(pixels, false);
      std::vector<bool> latched;
      for (unsigned word = 0; word < stream.size(); ++word) {
        shift.pop_front();
        shift.push_back(stream[word]);
        if (hub75::hub12_control_word(address, word, seg, 2, false) & hub75::HUB12_LAT1)
          latched.assign(shift.begin(), shift.end());
      }
      assert(latched.size() == pixels);
      for (unsigned y = address; y < 16; y += 4) {
        for (unsigned x = 0; x < 32 * cols; ++x) {
          const bool expected = old_alignment ?
              (y < 4 && x < 7) || (y >= 4 && y < 8 && x < 8) || (y >= 8 && y < 12 && x == 7) :
              y < 8 && x < 8;
          assert(latched[hub75::hub12_position(x, y, cols).word - 1] == expected);
        }
      }
    }
  }
}
