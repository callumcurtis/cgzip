#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "constants.hpp"

struct Offset {
  std::uint16_t bits;
  std::uint8_t num_bits;
};

struct PrefixCode {
  std::uint16_t bits;
  std::uint8_t length;
};

struct PrefixCodeWithOffset {
  PrefixCode prefix_code;
  Offset offset;
};

struct PrefixCodedBackReference {
  PrefixCodeWithOffset length;
  PrefixCodeWithOffset distance;
};

struct BackReference {
  std::size_t distance;
  std::size_t length;
};

struct SymbolWithOffset {
  std::uint16_t symbol;
  Offset offset;
  static constexpr auto from_distance(std::uint16_t distance)
      -> const SymbolWithOffset &;
  static constexpr auto from_length(std::uint16_t length)
      -> const SymbolWithOffset &;
  static constexpr auto to_length(SymbolWithOffset symbol_with_offset)
      -> std::uint16_t;
};

namespace detail {

struct Range {
  std::uint16_t symbol;
  std::uint8_t num_offset_bits;
  std::uint16_t start;
  std::uint16_t end;
};

template <std::size_t R, std::size_t N>
constexpr auto
get_symbols_with_offsets_from_ranges(const std::array<Range, R> ranges)
    -> std::array<SymbolWithOffset, N> {
  std::array<SymbolWithOffset, N> symbols_with_offsets{};
  for (auto range : ranges) {
    for (auto point = range.start; point <= range.end; ++point) {
      symbols_with_offsets.at(point) = SymbolWithOffset{
          .symbol = range.symbol,
          .offset =
              Offset{.bits = static_cast<std::uint16_t>(point - range.start),
                     .num_bits = range.num_offset_bits}};
    }
  }
  return symbols_with_offsets;
}

constexpr auto get_symbol_with_offset_by_distance()
    -> std::array<SymbolWithOffset, maximum_look_back_size + 1> {
  return get_symbols_with_offsets_from_ranges<num_distance_symbols,
                                              maximum_look_back_size + 1>({
      // NOLINTBEGIN (cppcoreguidelines-avoid-magic-numbers)
      Range{.symbol = 0, .num_offset_bits = 0, .start = 1, .end = 1},
      Range{.symbol = 1, .num_offset_bits = 0, .start = 2, .end = 2},
      Range{.symbol = 2, .num_offset_bits = 0, .start = 3, .end = 3},
      Range{.symbol = 3, .num_offset_bits = 0, .start = 4, .end = 4},
      Range{.symbol = 4, .num_offset_bits = 1, .start = 5, .end = 6},
      Range{.symbol = 5, .num_offset_bits = 1, .start = 7, .end = 8},
      Range{.symbol = 6, .num_offset_bits = 2, .start = 9, .end = 12},
      Range{.symbol = 7, .num_offset_bits = 2, .start = 13, .end = 16},
      Range{.symbol = 8, .num_offset_bits = 3, .start = 17, .end = 24},
      Range{.symbol = 9, .num_offset_bits = 3, .start = 25, .end = 32},
      Range{.symbol = 10, .num_offset_bits = 4, .start = 33, .end = 48},
      Range{.symbol = 11, .num_offset_bits = 4, .start = 49, .end = 64},
      Range{.symbol = 12, .num_offset_bits = 5, .start = 65, .end = 96},
      Range{.symbol = 13, .num_offset_bits = 5, .start = 97, .end = 128},
      Range{.symbol = 14, .num_offset_bits = 6, .start = 129, .end = 192},
      Range{.symbol = 15, .num_offset_bits = 6, .start = 193, .end = 256},
      Range{.symbol = 16, .num_offset_bits = 7, .start = 257, .end = 384},
      Range{.symbol = 17, .num_offset_bits = 7, .start = 385, .end = 512},
      Range{.symbol = 18, .num_offset_bits = 8, .start = 513, .end = 768},
      Range{.symbol = 19, .num_offset_bits = 8, .start = 769, .end = 1024},
      Range{.symbol = 20, .num_offset_bits = 9, .start = 1025, .end = 1536},
      Range{.symbol = 21, .num_offset_bits = 9, .start = 1537, .end = 2048},
      Range{.symbol = 22, .num_offset_bits = 10, .start = 2049, .end = 3072},
      Range{.symbol = 23, .num_offset_bits = 10, .start = 3073, .end = 4096},
      Range{.symbol = 24, .num_offset_bits = 11, .start = 4097, .end = 6144},
      Range{.symbol = 25, .num_offset_bits = 11, .start = 6145, .end = 8192},
      Range{.symbol = 26, .num_offset_bits = 12, .start = 8193, .end = 12288},
      Range{.symbol = 27, .num_offset_bits = 12, .start = 12289, .end = 16384},
      Range{.symbol = 28, .num_offset_bits = 13, .start = 16385, .end = 24576},
      Range{.symbol = 29, .num_offset_bits = 13, .start = 24577, .end = 32768},
      // NOLINTEND (cppcoreguidelines-avoid-magic-numbers)
  });
}

constexpr std::array<SymbolWithOffset, maximum_look_back_size + 1>
    symbol_with_offset_by_distance{get_symbol_with_offset_by_distance()};

constexpr auto length_ranges = std::array<Range, num_length_symbols>{
    // NOLINTBEGIN (cppcoreguidelines-avoid-magic-numbers)
    Range{.symbol = 257, .num_offset_bits = 0, .start = 3, .end = 3},
    Range{.symbol = 258, .num_offset_bits = 0, .start = 4, .end = 4},
    Range{.symbol = 259, .num_offset_bits = 0, .start = 5, .end = 5},
    Range{.symbol = 260, .num_offset_bits = 0, .start = 6, .end = 6},
    {.symbol = 261, .num_offset_bits = 0, .start = 7, .end = 7},
    Range{.symbol = 262, .num_offset_bits = 0, .start = 8, .end = 8},
    Range{.symbol = 263, .num_offset_bits = 0, .start = 9, .end = 9},
    Range{.symbol = 264, .num_offset_bits = 0, .start = 10, .end = 10},
    Range{.symbol = 265, .num_offset_bits = 1, .start = 11, .end = 12},
    {.symbol = 266, .num_offset_bits = 1, .start = 13, .end = 14},
    Range{.symbol = 267, .num_offset_bits = 1, .start = 15, .end = 16},
    Range{.symbol = 268, .num_offset_bits = 1, .start = 17, .end = 18},
    Range{.symbol = 269, .num_offset_bits = 2, .start = 19, .end = 22},
    Range{.symbol = 270, .num_offset_bits = 2, .start = 23, .end = 26},
    {.symbol = 271, .num_offset_bits = 2, .start = 27, .end = 30},
    Range{.symbol = 272, .num_offset_bits = 2, .start = 31, .end = 34},
    Range{.symbol = 273, .num_offset_bits = 3, .start = 35, .end = 42},
    Range{.symbol = 274, .num_offset_bits = 3, .start = 43, .end = 50},
    Range{.symbol = 275, .num_offset_bits = 3, .start = 51, .end = 58},
    {.symbol = 276, .num_offset_bits = 3, .start = 59, .end = 66},
    Range{.symbol = 277, .num_offset_bits = 4, .start = 67, .end = 82},
    Range{.symbol = 278, .num_offset_bits = 4, .start = 83, .end = 98},
    Range{.symbol = 279, .num_offset_bits = 4, .start = 99, .end = 114},
    Range{.symbol = 280, .num_offset_bits = 4, .start = 115, .end = 130},
    {.symbol = 281, .num_offset_bits = 5, .start = 131, .end = 162},
    Range{.symbol = 282, .num_offset_bits = 5, .start = 163, .end = 194},
    Range{.symbol = 283, .num_offset_bits = 5, .start = 195, .end = 226},
    Range{.symbol = 284, .num_offset_bits = 5, .start = 227, .end = 257},
    Range{.symbol = 285, .num_offset_bits = 0, .start = 258, .end = 258}
    // NOLINTEND (cppcoreguidelines-avoid-magic-numbers)
};

constexpr auto get_symbol_with_offset_by_length()
    -> std::array<SymbolWithOffset, maximum_look_ahead_size + 1> {
  return get_symbols_with_offsets_from_ranges<num_length_symbols,
                                              maximum_look_ahead_size + 1>(
      length_ranges);
}

constexpr std::array<SymbolWithOffset, maximum_look_ahead_size + 1>
    symbol_with_offset_by_length{get_symbol_with_offset_by_length()};

constexpr auto get_length_starts_by_symbol()
    -> std::array<std::uint16_t, num_length_symbols> {
  std::array<std::uint16_t, num_length_symbols> length_starts_by_symbol{};
  for (auto i = 0; std::cmp_less(i, num_length_symbols); ++i) {
    length_starts_by_symbol.at(i) = length_ranges.at(i).start;
  }
  return length_starts_by_symbol;
}

constexpr std::array<std::uint16_t, num_length_symbols> length_starts_by_symbol{
    get_length_starts_by_symbol()};

} // namespace detail

constexpr auto SymbolWithOffset::from_distance(std::uint16_t distance)
    -> const SymbolWithOffset & {
  return detail::symbol_with_offset_by_distance.at(distance);
}

constexpr auto SymbolWithOffset::from_length(std::uint16_t length)
    -> const SymbolWithOffset & {
  return detail::symbol_with_offset_by_length.at(length);
}

constexpr auto SymbolWithOffset::to_length(SymbolWithOffset symbol_with_offset)
    -> std::uint16_t {
  return detail::length_starts_by_symbol.at(
             symbol_with_offset.symbol - detail::length_ranges.at(0).symbol) +
         symbol_with_offset.offset.bits;
}
