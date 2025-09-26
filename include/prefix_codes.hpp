#pragma once

#include "package_merge.hpp"
#include <cstdint>

using code_bits_t = std::uint16_t;

struct PrefixCode {
  code_bits_t bits;
  length_t length;
};

template <lengths_size_t N>
constexpr auto prefix_codes(std::span<length_t, N> lengths) -> std::array<PrefixCode, N> {
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
  std::vector<lengths_size_t> count_by_length(max_length+1);
  for (auto length : lengths) {
    count_by_length[length]++;
  }

  // Step 2.
  code_bits_t code_bits = 0;
  count_by_length[0] = 0;
  std::vector<code_bits_t> next_code_bits(max_length+1);
  for (auto bits = 1; bits < max_length+1; ++bits) {
    code_bits = (code_bits + count_by_length[bits-1]) << (unsigned int) 1;
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

