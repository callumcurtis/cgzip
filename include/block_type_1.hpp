#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "block_type.hpp"
#include "deflate.hpp"
#include "gz.hpp"
#include "lzss.hpp"
#include "prefix_codes.hpp"
#include "types.hpp"

namespace block_type_1 {

template <std::uint16_t LookBackSize = maximum_look_back_size,
          std::uint16_t LookAheadSize = maximum_look_ahead_size>
class Stream final : public BlockStream {
private:
  deflate::BufferedBitStream buffered_out_;
  Lzss<LookBackSize, LookAheadSize> lzss_;

  static constexpr auto build_distance_prefix_codes_with_offsets()
      -> std::array<PrefixCodeWithOffset, LookBackSize + 1> {
    std::array<PrefixCodeWithOffset, LookBackSize + 1>
        prefix_codes_with_offsets{};
    for (auto distance = minimum_back_reference_distance;
         distance <= LookBackSize; ++distance) {
      const auto &symbol_with_offset =
          symbol_with_offset_from_distance(distance);
      prefix_codes_with_offsets.at(distance) = PrefixCodeWithOffset{
          .prefix_code =
              PrefixCode{
                  .bits = symbol_with_offset.symbol,
                  .length =
                      5}, // NOLINT (cppcoreguidelines-avoid-magic-numbers)
          .offset = symbol_with_offset.offset};
    }
    return prefix_codes_with_offsets;
  }

  static constexpr auto build_ll_prefix_codes()
      -> std::array<PrefixCode, num_ll_symbols> {
    std::array<std::uint8_t, num_ll_symbols> lengths{};
    // NOLINTBEGIN (cppcoreguidelines-avoid-magic-numbers)
    std::fill(lengths.begin(), lengths.begin() + 144, 8);
    std::fill(lengths.begin() + 144, lengths.begin() + 256, 9);
    std::fill(lengths.begin() + 256, lengths.begin() + 280, 7);
    std::fill(lengths.begin() + 280, lengths.end(), 8);
    // NOLINTEND (cppcoreguidelines-avoid-magic-numbers)
    return prefix_codes<num_ll_symbols>(lengths);
  }

  static constexpr auto build_length_prefix_codes_with_offsets(
      const std::array<PrefixCode, num_ll_symbols> literal_length_prefix_codes)
      -> std::array<PrefixCodeWithOffset, LookAheadSize + 1> {
    std::array<PrefixCodeWithOffset, LookAheadSize + 1>
        prefix_codes_with_offsets{};
    for (auto length = minimum_back_reference_length; length <= LookAheadSize;
         ++length) {
      const auto &symbol_with_offset = symbol_with_offset_from_length(length);
      prefix_codes_with_offsets.at(length) =
          PrefixCodeWithOffset{.prefix_code = literal_length_prefix_codes.at(
                                   symbol_with_offset.symbol),
                               .offset = symbol_with_offset.offset};
    }
    return prefix_codes_with_offsets;
  }

  static constexpr std::array<PrefixCode, num_ll_symbols>
      literal_length_prefix_codes_{build_ll_prefix_codes()};
  static constexpr std::array<PrefixCodeWithOffset, LookAheadSize + 1>
      length_prefix_codes_with_offsets_{
          build_length_prefix_codes_with_offsets(literal_length_prefix_codes_)};
  static constexpr std::array<PrefixCodeWithOffset, LookBackSize + 1>
      distance_prefix_codes_with_offsets_{
          build_distance_prefix_codes_with_offsets()};

public:
  explicit Stream(gz::BitStream &bit_stream) : buffered_out_{bit_stream} {}

  [[nodiscard]] auto bits(bool /*is_last*/) -> std::uint64_t override {
    return buffered_out_.bits();
  }

  auto reset() -> void override {
    while (!lzss_.is_empty()) {
      step();
    }
    buffered_out_.reset();
  }

  auto put(std::uint8_t byte) -> void override {
    lzss_.put(byte);
    if (!lzss_.is_full()) {
      return;
    }
    step();
  }

  auto commit(bool is_last) -> void override {
    auto &unbuffered_out = buffered_out_.unbuffered();
    unbuffered_out.push_bit(is_last ? 1 : 0);
    unbuffered_out.push_bits(1, 2);
    while (!lzss_.is_empty()) {
      step();
    }
    buffered_out_.commit();
    unbuffered_out.push_prefix_code(literal_length_prefix_codes_.at(eob));
  }

private:
  auto step() {
    const auto backref = lzss_.back_reference();
    const auto byte = lzss_.literal();
    const auto &prefix_code = literal_length_prefix_codes_.at(byte);
    if (backref.length >= minimum_back_reference_length) {
      const auto &length_prefix_code_with_offset =
          length_prefix_codes_with_offsets_.at(backref.length);
      const auto &distance_prefix_code_with_offset =
          distance_prefix_codes_with_offsets_.at(backref.distance);
      auto num_literal_bits = 0;
      for (auto i = lzss_.literals_in_back_reference_begin();
           i != lzss_.literals_in_back_reference_end(); ++i) {
        num_literal_bits += literal_length_prefix_codes_.at(*i).length;
      }
      const auto num_back_reference_bits =
          length_prefix_code_with_offset.prefix_code.length +
          length_prefix_code_with_offset.offset.num_bits +
          distance_prefix_code_with_offset.prefix_code.length +
          distance_prefix_code_with_offset.offset.num_bits;
      if (num_literal_bits >= num_back_reference_bits) {
        buffered_out_.push_back_reference(PrefixCodedBackReference{
            .length = length_prefix_code_with_offset,
            .distance = distance_prefix_code_with_offset});
        lzss_.take_back_reference();
        return;
      }
    }
    buffered_out_.push_prefix_code(prefix_code);
    lzss_.take_literal();
  }
};

} // namespace block_type_1
