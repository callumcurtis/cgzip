#include <catch2/catch_all.hpp>
#include <cmath>

#include "prefix_codes.hpp"

#define NUM_LL_CODES 288

constexpr auto block_type_1_ll_code_lengths() -> std::array<length_t, NUM_LL_CODES> {
  std::array<length_t, NUM_LL_CODES> lengths{};
  std::fill(lengths.begin(),       lengths.begin() + 144, 8);
  std::fill(lengths.begin() + 144, lengths.begin() + 256, 9);
  std::fill(lengths.begin() + 256, lengths.begin() + 280, 7);
  std::fill(lengths.begin() + 280, lengths.end(),         8);
  return lengths;
}

constexpr auto block_type_1_prefix_codes() -> std::array<PrefixCode, NUM_LL_CODES> {
  auto lengths = block_type_1_ll_code_lengths();
  return prefix_codes<NUM_LL_CODES>(lengths);
}

TEST_CASE("prefix codes") {
  constexpr auto codes = block_type_1_prefix_codes();
  for(auto i = 0; i < 144; ++i) {
    REQUIRE(codes.at(i).length == 8);
    REQUIRE(codes.at(i).bits == static_cast<code_bits_t>(0b00110000 + i));
  }
  for(auto i = 144; i < 256; ++i) {
    REQUIRE(codes.at(i).length == 9);
    REQUIRE(codes.at(i).bits == static_cast<code_bits_t>(0b110010000 + (i-144)));
  }
  for(auto i = 256; i < 280; ++i) {
    REQUIRE(codes.at(i).length == 7);
    REQUIRE(codes.at(i).bits == (i-256));
  }
  for(auto i = 280; i < 288; ++i) {
    REQUIRE(codes.at(i).length == 8);
    REQUIRE(codes.at(i).bits == static_cast<code_bits_t>(0b11000000 + (i-280)));
  }
}

