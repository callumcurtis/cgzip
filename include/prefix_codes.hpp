#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "types.hpp"

// prefix_codes generates Huffman prefix codes based on Huffman code lengths.
template <std::size_t N>
constexpr auto prefix_codes(std::span<const std::uint8_t, N> lengths)
    -> std::array<PrefixCode, N> {
  // Based on RFC 1951 (Section 3.2.2):
  // https://www.ietf.org/rfc/rfc1951.txt
  // Reference:
  // https://github.com/billbird/gzstat/blob/master/gzstat.py
  // The steps below are numbered to match the steps in the RFC.
  if (N == 0) {
    return {};
  }

  // Step 1.
  auto max_length = *std::ranges::max_element(lengths);
  std::vector<std::uint16_t> count_by_length(max_length + 1);
  for (auto length : lengths) {
    count_by_length[length]++;
  }

  // Step 2.
  std::uint16_t code_bits = 0;
  count_by_length[0] = 0;
  std::vector<std::uint16_t> next_code_bits(max_length + 1);
  for (auto bits = 1; bits < max_length + 1; ++bits) {
    code_bits =
        static_cast<std::uint16_t>(code_bits + count_by_length[bits - 1]) << 1U;
    next_code_bits[bits] = code_bits;
  }

  // Step 3.
  std::array<PrefixCode, N> codes{};
  for (auto i = 0; i < N; ++i) {
    auto length = lengths[i];
    if (length != 0) {
      codes[i].bits = next_code_bits[length];
      codes[i].length = length;
      next_code_bits[length]++;
    }
  }

  return codes;
}
