#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include "ring_buffer.hpp"
#include "size.hpp"
#include "types.hpp"

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
const std::uint16_t maximum_look_back_size = 1U << 15U;
const std::uint16_t maximum_look_ahead_size = 258;
const std::uint8_t maximum_prefix_code_length = 15;

template <std::size_t LookBackSize = maximum_look_back_size,
          std::size_t LookAheadSize = maximum_look_ahead_size>
class Lzss {
private:
  using LookAheadRingBuffer = RingBuffer<std::uint8_t, LookAheadSize>;
  using ChainRingBuffer = RingBuffer<std::uint64_t, LookBackSize>;

  RingBuffer<std::uint8_t, LookBackSize> look_back_{};
  LookAheadRingBuffer look_ahead_{};
  ChainRingBuffer chain_{};
  std::unordered_map<std::uint32_t, std::uint64_t>
      start_absolute_by_length_three_pattern_;
  BackReference back_reference_{.distance = 0, .length = 0};
  std::uint64_t absolute_position_{
      1}; // Start at 1 to reserve 0 for end of chain

  static constexpr std::uint64_t end_of_chain = 0;

  // Create a 3-byte pattern key from a pattern
  auto create_pattern_key(std::uint8_t a, std::uint8_t b, std::uint8_t c) const
      -> std::uint32_t {
    return (static_cast<std::uint32_t>(a) << size_of_in_bits<std::uint16_t>()) |
           (static_cast<std::uint32_t>(b) << size_of_in_bits<std::uint8_t>()) |
           static_cast<std::uint32_t>(c);
  }

  auto absolute_start_of_look_back() const -> std::uint64_t {
    return absolute_position_ - 1 - look_back_.size();
  }

  auto is_absolute_in_lookback(std::uint64_t absolute) const -> bool {
    return absolute != end_of_chain &&
           absolute >= absolute_start_of_look_back();
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
    if (look_back_.is_empty() ||
        look_ahead_.size() < minimum_back_reference_length - 1) {
      chain_.enqueue(end_of_chain);
      return;
    }

    const auto start_relative = look_back_.size() - 1;
    auto pattern_key = create_pattern_key(look_back_[start_relative],
                                          look_ahead_[0], look_ahead_[1]);

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
    start_absolute_by_length_three_pattern_[pattern_key] =
        relative_to_absolute(start_relative);
  }

  // Remove pattern from hash map and chain
  auto remove_pattern() -> void {
    if (!look_back_.is_full()) {
      return;
    }
    chain_.dequeue();
    auto pattern_key =
        create_pattern_key(look_back_[0], look_back_[1], look_back_[2]);
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

    auto pattern_key =
        create_pattern_key(look_ahead_[0], look_ahead_[1], look_ahead_[2]);
    auto it = start_absolute_by_length_three_pattern_.find(pattern_key);

    if (it == start_absolute_by_length_three_pattern_.end()) {
      return longest_backref;
    }

    auto start_absolute = it->second;

    if (!is_absolute_in_lookback(start_absolute)) {
      return longest_backref;
    }

    auto start_relative = absolute_to_relative(start_absolute);
    longest_backref =
        BackReference{.distance = look_back_.size() - start_relative,
                      .length = minimum_back_reference_length};

    // Follow the chain of all occurrences of this pattern
    while (is_absolute_in_lookback(start_absolute)) {

      // Match as many characters as possible
      for (auto current_lookahead = minimum_back_reference_length;
           current_lookahead < look_ahead_.size(); ++current_lookahead) {
        const auto current_relative =
            (look_back_.size() - start_relative < look_ahead_.size()
                 ? start_relative + (current_lookahead %
                                     (look_back_.size() - start_relative))
                 : start_relative + current_lookahead);

        if (look_back_[current_relative] != look_ahead_[current_lookahead]) {
          break;
        }
        if (std::cmp_greater_equal(longest_backref.length,
                                   current_lookahead + 1)) {
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
  auto is_empty() const -> bool { return look_ahead_.is_empty(); }

  auto is_full() const -> bool { return look_ahead_.is_full(); }

  auto literal() const -> std::uint8_t { return look_ahead_.peek(); }

  auto back_reference() -> BackReference {
    cache_back_reference();
    return back_reference_;
  }

  auto literals_in_back_reference_begin() const
      -> LookAheadRingBuffer::const_iterator {
    return look_ahead_.begin();
  }

  auto literals_in_back_reference_end() -> LookAheadRingBuffer::const_iterator {
    cache_back_reference();
    return look_ahead_.begin() + back_reference_.length;
  }

  auto take_back_reference() {
    cache_back_reference();
    for (auto i = 0; std::cmp_less(i, back_reference_.length); ++i) {
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

inline constexpr std::array<SymbolWithOffset, maximum_look_back_size + 1>
    symbol_with_offset_by_distance{get_symbol_with_offset_by_distance()};

constexpr auto symbol_with_offset_from_distance(std::uint16_t distance)
    -> const SymbolWithOffset & {
  return symbol_with_offset_by_distance.at(distance);
}

inline constexpr auto length_ranges = std::array<Range, num_length_symbols>{
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

inline constexpr std::array<SymbolWithOffset, maximum_look_ahead_size + 1>
    symbol_with_offset_by_length{get_symbol_with_offset_by_length()};

constexpr auto symbol_with_offset_from_length(std::uint16_t length)
    -> const SymbolWithOffset & {
  return symbol_with_offset_by_length.at(length);
}

constexpr auto get_length_starts_by_symbol()
    -> std::array<std::uint16_t, num_length_symbols> {
  std::array<std::uint16_t, num_length_symbols> length_starts_by_symbol{};
  for (auto i = 0; std::cmp_less(i, num_length_symbols); ++i) {
    length_starts_by_symbol.at(i) = length_ranges.at(i).start;
  }
  return length_starts_by_symbol;
}

inline constexpr std::array<std::uint16_t, num_length_symbols>
    length_starts_by_symbol{get_length_starts_by_symbol()};

constexpr auto
length_from_symbol_with_offset(SymbolWithOffset symbol_with_offset)
    -> std::uint16_t {
  return length_starts_by_symbol.at(symbol_with_offset.symbol -
                                    length_ranges.at(0).symbol) +
         symbol_with_offset.offset.bits;
}
