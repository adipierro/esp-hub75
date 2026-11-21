// SPDX-License-Identifier: MIT
#include "../components/hub75/src/panels/hub12_mapping.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
using namespace hub75;
int main() {
  for (unsigned cols : {1U, 6U, 8U, 16U}) for (unsigned rows : {1U, 2U})
    for (unsigned bits : {1U, 2U, 3U, 4U, 8U}) for (bool invert : {false, true}) {
      const unsigned seg = hub12_segment_words(cols), width = seg * (rows + 1), payload = seg * rows;
      assert(seg % 2 == 0 && payload % 2 == 0);
      for (unsigned address = 0; address < 4; ++address) for (unsigned enabled : {0U, 1U, seg / 2, seg - 4}) {
        std::vector<std::vector<uint16_t>> planes(bits, std::vector<uint16_t>(width));
        for (unsigned bit = 0; bit < bits; ++bit) for (unsigned i = 0; i < width; ++i) {
          auto word = hub12_control_word(address, i, seg, rows, invert);
          if (hub12_display_enabled(i, seg, rows, enabled)) word &= ~HUB12_OE;
          if (i < payload && ((i * 31 + bit * 17 + address * 7) % 11 < 5)) word ^= HUB12_DATA;
          planes[bit][i ^ 1U] = word;
        }
        std::vector<uint16_t> shared(planes[0].begin() + payload, planes[0].end());
        for (unsigned bit = 0; bit < bits; ++bit) {
          assert(std::equal(shared.begin(), shared.end(), planes[bit].begin() + payload));
          std::vector<uint16_t> out;
          for (unsigned offset = 0; offset < payload * 2; offset += 4092) {
            unsigned len = std::min(4092U, payload * 2 - offset);
            assert(offset % 4 == 0 && len % 4 == 0);
            out.insert(out.end(), planes[bit].begin() + offset / 2, planes[bit].begin() + (offset + len) / 2);
          }
          for (unsigned offset = 0; offset < seg * 2; offset += 4092) {
            unsigned len = std::min(4092U, seg * 2 - offset);
            assert(offset % 4 == 0 && len % 4 == 0);
            out.insert(out.end(), shared.begin() + offset / 2, shared.begin() + (offset + len) / 2);
          }
          assert(out == planes[bit]);
        }
      }
    }
}
