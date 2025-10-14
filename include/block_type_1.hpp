#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "block_type.hpp"
#include "deflate.hpp"
#include "gz.hpp"
#include "lzss.hpp"
#include "prefix_codes.hpp"
#include "types.hpp"

template <std::size_t LookBackSize = maximum_look_back_size,
          std::size_t LookAheadSize = maximum_look_ahead_size>
class BlockType1Stream final : public BlockStream {
private:
  deflate::BufferedBitStream buffered_out_;
  Lzss<LookBackSize, LookAheadSize> lzss_;

  static constexpr auto build_distance_prefix_codes_with_offsets()
      -> std::array<PrefixCodeWithOffset, maximum_look_back_size + 1> {
    std::array<PrefixCodeWithOffset, maximum_look_back_size + 1>
        prefix_codes_with_offsets{};
    for (auto distance = minimum_back_reference_distance;
         distance <= maximum_look_back_size; ++distance) {
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
      const std::array<PrefixCode, num_ll_symbols> codes)
      -> std::array<PrefixCodeWithOffset, maximum_look_ahead_size + 1> {
    std::array<PrefixCodeWithOffset, maximum_look_ahead_size + 1>
        prefix_codes_with_offsets{};
    for (auto length = minimum_back_reference_length;
         length <= maximum_look_ahead_size; ++length) {
      const auto &symbol_with_offset = symbol_with_offset_from_length(length);
      prefix_codes_with_offsets.at(length) = PrefixCodeWithOffset{
          .prefix_code = codes.at(symbol_with_offset.symbol),
          .offset = symbol_with_offset.offset};
    }
    return prefix_codes_with_offsets;
  }

  static constexpr std::array<PrefixCode, num_ll_symbols> codes_{
      build_ll_prefix_codes()};
  static constexpr std::array<PrefixCodeWithOffset, maximum_look_ahead_size + 1>
      length_encodings_{build_length_prefix_codes_with_offsets(codes_)};
  static constexpr std::array<PrefixCodeWithOffset, maximum_look_back_size + 1>
      distance_encodings_{build_distance_prefix_codes_with_offsets()};

public:
  explicit BlockType1Stream(gz::BitStream &bit_stream)
      : buffered_out_{bit_stream} {}

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
    const auto block_type = 1;
    const auto num_block_type_bits = 2;
    auto &unbuffered_out = buffered_out_.unbuffered();
    unbuffered_out.push_bit(is_last ? 1 : 0);
    unbuffered_out.push_bits(block_type, num_block_type_bits);
    while (!lzss_.is_empty()) {
      step();
    }
    buffered_out_.commit();
    unbuffered_out.push_prefix_code(codes_.at(eob));
  }

private:
  auto step() {
    const auto backref = lzss_.back_reference();
    const auto byte = lzss_.literal();
    const auto &code = codes_.at(byte);
    if (backref.length >= minimum_back_reference_length) {
      const auto &length_encoding = length_encodings_.at(backref.length);
      const auto &distance_encoding = distance_encodings_.at(backref.distance);
      auto num_literal_bits = 0;
      for (auto i = lzss_.literals_in_back_reference_begin();
           i != lzss_.literals_in_back_reference_end(); ++i) {
        num_literal_bits += codes_.at(*i).length;
      }
      const auto num_back_reference_bits =
          length_encoding.prefix_code.length + length_encoding.offset.num_bits +
          distance_encoding.prefix_code.length +
          distance_encoding.offset.num_bits;
      if (num_literal_bits >= num_back_reference_bits) {
        buffered_out_.push_back_reference(PrefixCodedBackReference{
            .length = length_encoding, .distance = distance_encoding});
        lzss_.take_back_reference();
        return;
      }
    }
    buffered_out_.push_prefix_code(code);
    lzss_.take_literal();
  }
};
