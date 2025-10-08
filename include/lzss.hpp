#pragma once

#include <cstdint>
#include <unordered_map>

#include "ring_buffer.hpp"
#include "prefix_codes.hpp"

struct BackReference {
  std::size_t distance;
  std::size_t length;
};

const std::uint16_t num_ll_symbols = 288;
const std::uint16_t num_distance_symbols = 30;
const std::uint16_t num_length_symbols = 29;
const std::uint16_t eob = 256;
const std::uint16_t minimum_back_reference_length = 3;
const std::uint16_t minimum_back_reference_distance = 1;
const std::uint16_t maximum_look_back_size = 1 << 15;
const std::uint16_t maximum_look_ahead_size = 258;
const std::uint8_t maximum_prefix_code_length = 15;

template <std::size_t LookBackSize = maximum_look_back_size, std::size_t LookAheadSize = maximum_look_ahead_size>
class Lzss {
private:
  using LookAheadRingBuffer = RingBuffer<std::uint8_t, LookAheadSize>;
  using ChainRingBuffer = RingBuffer<std::uint64_t, LookBackSize>;

  RingBuffer<std::uint8_t, LookBackSize> look_back_{};
  LookAheadRingBuffer look_ahead_{};
  ChainRingBuffer chain_{};
  std::unordered_map<std::uint32_t, std::uint64_t> start_absolute_by_length_three_pattern_;
  BackReference back_reference_{.distance = 0, .length = 0};
  std::uint64_t absolute_position_{1}; // Start at 1 to reserve 0 for end of chain

  static constexpr std::uint64_t end_of_chain = 0;
  
  // Create a 3-byte pattern key from a pattern
  auto create_pattern_key(std::uint8_t a, std::uint8_t b, std::uint8_t c) const -> std::uint32_t {
    return (static_cast<std::uint32_t>(a) << 16) |
           (static_cast<std::uint32_t>(b) << 8) |
           static_cast<std::uint32_t>(c);
  }

  auto absolute_start_of_look_back() const -> std::uint64_t {
    return absolute_position_ - 1 - look_back_.size();
  }

  auto is_absolute_in_lookback(std::uint64_t absolute) const -> bool {
    return absolute != end_of_chain && absolute >= absolute_start_of_look_back();
  }

  // Convert absolute position to relative position
  auto absolute_to_relative(std::uint64_t absolute) const -> std::uint16_t {
    return absolute - absolute_start_of_look_back();
  }

  // Convert relative position to absolute position
  auto relative_to_absolute(std::uint16_t relative) const -> std::uint64_t {
    return absolute_start_of_look_back() + relative;
  }

  // Index pattern in hash map and chain
  auto add_pattern() -> void {
    if (look_back_.is_empty() || look_ahead_.size() < minimum_back_reference_length - 1) {
      chain_.enqueue(end_of_chain);
      return;
    }

    const auto start_relative = look_back_.size() - 1;
    auto pattern_key = create_pattern_key(look_back_[start_relative], look_ahead_[0], look_ahead_[1]);

    // Check if this pattern already exists in hash map
    auto it = start_absolute_by_length_three_pattern_.find(pattern_key);
    if (it != start_absolute_by_length_three_pattern_.end()) {
      // Pattern exists, store the previous occurrence in chain
      chain_.enqueue(it->second);
    } else {
      // New pattern, no previous occurrence
      chain_.enqueue(end_of_chain);
    }

    // Update the hash map with the new position
    start_absolute_by_length_three_pattern_[pattern_key] = relative_to_absolute(start_relative);
  }

  // Remove pattern from hash map and chain
  auto remove_pattern() -> void {
    if (!look_back_.is_full()) {
      return;
    }
    chain_.dequeue();
    auto pattern_key = create_pattern_key(look_back_[0], look_back_[1], look_back_[2]);
    auto it = start_absolute_by_length_three_pattern_.find(pattern_key);
    if (it == start_absolute_by_length_three_pattern_.end()) {
      // TODO: remove after allowing look-ahead into next block
      return;
    }
    if (absolute_to_relative(it->second) == 0) {
      start_absolute_by_length_three_pattern_.erase(it);
    }
  }

  // Find best back reference using hash map with chain
  auto find_best_back_reference() -> BackReference {
    // TODO
    auto longest_backref = BackReference{.distance = 0, .length = 0};

    if (look_ahead_.size() < minimum_back_reference_length) {
      return longest_backref;
    }

    auto pattern_key = create_pattern_key(look_ahead_[0], look_ahead_[1], look_ahead_[2]);
    auto it = start_absolute_by_length_three_pattern_.find(pattern_key);

    if (it == start_absolute_by_length_three_pattern_.end()) {
      return longest_backref;
    }

    auto start_absolute = it->second;

    if (!is_absolute_in_lookback(start_absolute)) {
      return longest_backref;
    }

    auto start_relative = absolute_to_relative(start_absolute);
    longest_backref = BackReference{.distance = look_back_.size() - start_relative, .length = minimum_back_reference_length};

    // Follow the chain of all occurrences of this pattern
    while (is_absolute_in_lookback(start_absolute)) {

      // Match as many characters as possible
      for (auto current_lookahead = minimum_back_reference_length; current_lookahead < look_ahead_.size(); ++current_lookahead) {
        const auto current_relative = (
          look_back_.size() - start_relative < look_ahead_.size()
          ? start_relative + (current_lookahead % (look_back_.size() - start_relative))
          : start_relative + current_lookahead
        );

        if (look_back_[current_relative] != look_ahead_[current_lookahead]) {
          break;
        }
        if (longest_backref.length >= current_lookahead + 1) {
          continue;
        }
        longest_backref.distance = look_back_.size() - start_relative;
        longest_backref.length = current_lookahead + 1;
      }

      start_absolute = chain_[start_relative];
      start_relative = absolute_to_relative(start_absolute);
    }

    return longest_backref;
  }

  auto cache_back_reference() {
    if (back_reference_.length > 0) {
      return;
    }
    back_reference_ = find_best_back_reference();
  }

  auto clear_cached_back_reference() {
    if (back_reference_.length == 0) {
      return;
    }
    back_reference_ = BackReference{.distance = 0, .length = 0};
  }

  auto take_literal_() {
    remove_pattern();
    look_back_.enqueue(look_ahead_.dequeue());
    absolute_position_++;
    add_pattern();
  }

public:
  auto is_empty() const -> bool {
    return look_ahead_.is_empty();
  }

  auto is_full() const -> bool {
    return look_ahead_.is_full();
  }

  auto literal() const -> std::uint8_t {
    return look_ahead_.peek();
  }

  auto back_reference() -> BackReference {
    cache_back_reference();
    return back_reference_;
  }

  auto literals_in_back_reference_begin() const -> LookAheadRingBuffer::const_iterator {
    return look_ahead_.begin();
  }

  auto literals_in_back_reference_end() -> LookAheadRingBuffer::const_iterator {
    cache_back_reference();
    return look_ahead_.begin() + back_reference_.length;
  }

  auto take_back_reference() {
    cache_back_reference();
    for (auto i = 0; i < back_reference_.length; ++i) {
      take_literal_();
    }
    clear_cached_back_reference();
  }

  auto take_literal() {
    take_literal_();
    clear_cached_back_reference();
  }

  auto put(std::uint8_t literal) {
    look_ahead_.enqueue(literal);
    clear_cached_back_reference();
  }
};

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

template <std::size_t R, std::size_t N>
constexpr auto get_symbols_with_offsets_from_ranges(const std::array<Range, R> ranges) -> std::array<SymbolWithOffset, N> {
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
  return get_symbols_with_offsets_from_ranges<num_distance_symbols, maximum_look_back_size+1>({
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

inline constexpr auto length_ranges = std::array<Range, num_length_symbols>{
  Range{257,0,3,3},     Range{258,0,4,4},     Range{259,0,5,5},     Range{260,0,6,6},     {261,0,7,7},
  Range{262,0,8,8},     Range{263,0,9,9},     Range{264,0,10,10},   Range{265,1,11,12},   {266,1,13,14},
  Range{267,1,15,16},   Range{268,1,17,18},   Range{269,2,19,22},   Range{270,2,23,26},   {271,2,27,30},
  Range{272,2,31,34},   Range{273,3,35,42},   Range{274,3,43,50},   Range{275,3,51,58},   {276,3,59,66},
  Range{277,4,67,82},   Range{278,4,83,98},   Range{279,4,99,114},  Range{280,4,115,130}, {281,5,131,162},
  Range{282,5,163,194}, Range{283,5,195,226}, Range{284,5,227,257}, Range{285,0,258,258}
};

constexpr auto get_symbol_with_offset_by_length() -> std::array<SymbolWithOffset, maximum_look_ahead_size+1> {
    return get_symbols_with_offsets_from_ranges<num_length_symbols, maximum_look_ahead_size+1>(length_ranges);
}

inline constexpr std::array<SymbolWithOffset, maximum_look_ahead_size+1> symbol_with_offset_by_length{get_symbol_with_offset_by_length()};

constexpr auto symbol_with_offset_from_length(std::uint16_t length) -> const SymbolWithOffset& {
  return symbol_with_offset_by_length.at(length);
}

constexpr auto get_length_starts_by_symbol() -> std::array<std::uint16_t, num_length_symbols> {
  std::array<std::uint16_t, num_length_symbols> length_starts_by_symbol{};
  for (auto i = 0; i < num_length_symbols; ++i) {
    length_starts_by_symbol.at(i) = length_ranges.at(i).start;
  }
  return length_starts_by_symbol;
}

inline constexpr std::array<std::uint16_t, num_length_symbols> length_starts_by_symbol{get_length_starts_by_symbol()};

constexpr auto length_from_symbol_with_offset(SymbolWithOffset symbol_with_offset) -> std::uint16_t {
  return length_starts_by_symbol.at(symbol_with_offset.symbol - length_ranges.at(0).symbol) + symbol_with_offset.offset.bits;
}
