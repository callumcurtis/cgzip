#pragma once

#include <stdexcept>
#include <cstdint>

#include "prefix_codes.hpp"

struct BackReference {
  std::size_t distance;
  std::size_t length;
};

template <typename LookBack, typename LookAhead>
auto lzss(LookBack lookback, LookAhead lookahead) -> BackReference {
  if (lookahead.size() <= 0) {
    throw std::invalid_argument("The lookahead buffer must include at least one element, which corresponds to the next element to be encoded.");
  }
  auto longest_backref = BackReference{.distance = 0, .length = 0};
  for (auto lookback_start = lookback.size(); lookback_start-- > 0;) {
    for (auto lookahead_ind = 0; lookahead_ind < lookahead.size(); ++lookahead_ind) {
      const auto lookback_ind = (
        lookback.size() - lookback_start < lookahead.size()
        ? lookback_start + (lookahead_ind % (lookback.size() - lookback_start))
        : lookback_start + lookahead_ind
      );
      if (lookback[lookback_ind] != lookahead[lookahead_ind]) {
        break;
      }
      if (longest_backref.length >= lookahead_ind + 1) {
        continue;
      }
      longest_backref.distance = lookback.size() - lookback_start;
      longest_backref.length = lookahead_ind + 1;
    }
  }
  return longest_backref;
}

const std::uint16_t num_ll_codes = 288;
const std::uint16_t eob = 256;
const std::uint16_t minimum_back_reference_length = 3;
const std::uint16_t minimum_back_reference_distance = 1;
const std::uint16_t maximum_look_back_size = 1 << 15;
const std::uint16_t maximum_look_ahead_size = 258;

struct Offset {
  std::uint16_t bits;
  std::uint8_t num_bits;
};

struct PrefixCodeWithOffset {
  PrefixCode prefix_code;
  Offset offset;
};

struct SymbolWithOffset {
  std::uint16_t symbol;
  Offset offset;
};

struct Range {
  std::uint16_t symbol;
  std::uint8_t num_offset_bits;
  std::uint16_t start;
  std::uint16_t end;
};

template <std::size_t N>
constexpr auto get_symbols_with_offsets_from_ranges(const std::array<Range, N> ranges) -> std::array<SymbolWithOffset, N> {
  std::array<SymbolWithOffset, N> symbols_with_offsets{};
  for (auto range : ranges) {
    for (auto point = range.start; point <= range.end; ++point) {
      symbols_with_offsets.at(point) = SymbolWithOffset{
        .symbol = range.symbol,
        .offset = Offset{
          .bits = static_cast<std::uint16_t>(point - range.start),
          .num_bits = range.num_offset_bits
        }
      };
    }
  }
  return symbols_with_offsets;
}

constexpr auto get_symbol_with_offset_by_distance() -> std::array<SymbolWithOffset, maximum_look_back_size+1> {
  return get_symbols_with_offsets_from_ranges<maximum_look_back_size+1>({
         Range{0,0,1,1},         Range{1,0,2,2},          Range{2,0,3,3},           Range{3,0,4,4},           Range{4,1,5,6},
         Range{5,1,7,8},         Range{6,2,9,12},         Range{7,2,13,16},         Range{8,3,17,24},         Range{9,3,25,32},
         Range{10,4,33,48},      Range{11,4,49,64},       Range{12,5,65,96},        Range{13,5,97,128},       Range{14,6,129,192},
         Range{15,6,193,256},    Range{16,7,257,384},     Range{17,7,385,512},      Range{18,8,513,768},      Range{19,8,769,1024},
         Range{20,9,1025,1536},  Range{21,9,1537,2048},   Range{22,10,2049,3072},   Range{23,10,3073,4096},   Range{24,11,4097,6144},
         Range{25,11,6145,8192}, Range{26,12,8193,12288}, Range{27,12,12289,16384}, Range{28,13,16385,24576}, Range{29,13,24577,32768},
  });
}

inline constexpr std::array<SymbolWithOffset, maximum_look_back_size+1> symbol_with_offset_by_distance{get_symbol_with_offset_by_distance()};

constexpr auto symbol_with_offset_from_distance(std::uint16_t distance) -> const SymbolWithOffset& {
  return symbol_with_offset_by_distance.at(distance);
}

constexpr auto get_symbol_with_offset_by_length() -> std::array<SymbolWithOffset, maximum_look_ahead_size+1> {
    return get_symbols_with_offsets_from_ranges<maximum_look_ahead_size+1>({
            Range{257,0,3,3},     Range{258,0,4,4},     Range{259,0,5,5},     Range{260,0,6,6},     {261,0,7,7},
            Range{262,0,8,8},     Range{263,0,9,9},     Range{264,0,10,10},   Range{265,1,11,12},   {266,1,13,14},
            Range{267,1,15,16},   Range{268,1,17,18},   Range{269,2,19,22},   Range{270,2,23,26},   {271,2,27,30},
            Range{272,2,31,34},   Range{273,3,35,42},   Range{274,3,43,50},   Range{275,3,51,58},   {276,3,59,66},
            Range{277,4,67,82},   Range{278,4,83,98},   Range{279,4,99,114},  Range{280,4,115,130}, {281,5,131,162},
            Range{282,5,163,194}, Range{283,5,195,226}, Range{284,5,227,257}, Range{285,0,258,258}
    });
}

inline constexpr std::array<SymbolWithOffset, maximum_look_ahead_size+1> symbol_with_offset_by_length{get_symbol_with_offset_by_length()};

constexpr auto symbol_with_offset_from_length(std::uint16_t length) -> const SymbolWithOffset& {
  return symbol_with_offset_by_length.at(length);
}

