#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include "constants.hpp"
#include "ring_buffer.hpp"
#include "size.hpp"
#include "types.hpp"

template <std::size_t LookBackSize = maximum_look_back_size,
          std::size_t LookAheadSize = maximum_look_ahead_size>
class Lzss {
private:
  using LookAheadRingBuffer = RingBuffer<std::uint8_t, LookAheadSize>;
  using ChainRingBuffer = RingBuffer<std::uint64_t, LookBackSize>;

  RingBuffer<std::uint8_t, LookBackSize> look_back_{};
  LookAheadRingBuffer look_ahead_{};
  // chain_ is a ring buffer that stores the absolute position of the previous
  // starting point of the same three-byte pattern in the look-back buffer.
  // Together with start_absolute_by_length_three_pattern_, this forms a chain
  // of occurrences for each three-byte pattern in the look-back buffer.
  // Three-byte patterns are used as they are the minimum length pattern
  // that can be represented by a backreference. Using a radix tree would be an
  // alternative approach, but would require reading more cache lines due to its
  // linked-list structure, as well as more complex memory management
  // due to the large branching factor, which otherwise grows in memory quickly
  // if 256 8-byte pointers are naively reserved in each node for possible
  // children.
  ChainRingBuffer chain_{};
  // start_absolute_by_length_three_pattern_ is a hash map that maps three-byte
  // patterns to the absolute position of the most recent occurrence of the
  // pattern in the look-back buffer.
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
      return;
    }
    if (absolute_to_relative(it->second) == 0) {
      start_absolute_by_length_three_pattern_.erase(it);
    }
  }

  // Find best back reference using hash map with chain
  auto find_best_back_reference() -> BackReference {
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
