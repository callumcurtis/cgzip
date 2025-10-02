#pragma once

#include "third_party/output_stream.hpp"

#include "lzss.hpp"
#include "prefix_codes.hpp"
#include "ring_buffer.hpp"

template <std::size_t LookBackSize = maximum_look_back_size, std::size_t LookAheadSize = maximum_look_ahead_size>
class BlockType1Stream {
private:
  std::uint64_t bits_;
  OutputBitStream& out_;
  std::vector<std::variant<PrefixCode, Offset>> block;
  RingBuffer<std::uint8_t, LookBackSize> look_back{};
  RingBuffer<std::uint8_t, LookAheadSize> look_ahead{};

  static constexpr auto build_distance_prefix_codes_with_offsets() -> std::array<PrefixCodeWithOffset, maximum_look_back_size+1> {
    std::array<PrefixCodeWithOffset, maximum_look_back_size+1> prefix_codes_with_offsets{};
    for (auto distance = minimum_back_reference_distance; distance <= maximum_look_back_size; ++distance) {
      const auto& symbol_with_offset = symbol_with_offset_from_distance(distance);
      prefix_codes_with_offsets.at(distance) = PrefixCodeWithOffset{
        .prefix_code = PrefixCode{
          .bits = symbol_with_offset.symbol,
          .length = 5
        },
        .offset = symbol_with_offset.offset
      };
    }
    return prefix_codes_with_offsets;
  }

  static constexpr auto build_ll_prefix_codes() -> std::array<PrefixCode, num_ll_symbols> {
    std::array<std::uint8_t, num_ll_symbols> lengths{};
    std::fill(lengths.begin(),       lengths.begin() + 144, 8);
    std::fill(lengths.begin() + 144, lengths.begin() + 256, 9);
    std::fill(lengths.begin() + 256, lengths.begin() + 280, 7);
    std::fill(lengths.begin() + 280, lengths.end(),         8);
    return prefix_codes<num_ll_symbols>(lengths);
  }

  static constexpr auto build_length_prefix_codes_with_offsets(const std::array<PrefixCode, num_ll_symbols> codes) -> std::array<PrefixCodeWithOffset, maximum_look_ahead_size+1> {
    std::array<PrefixCodeWithOffset, maximum_look_ahead_size+1> prefix_codes_with_offsets{};
    for (auto length = minimum_back_reference_length; length <= maximum_look_ahead_size; ++length) {
      const auto& symbol_with_offset = symbol_with_offset_from_length(length);
      prefix_codes_with_offsets.at(length) = PrefixCodeWithOffset{
        .prefix_code = codes.at(symbol_with_offset.symbol),
        .offset = symbol_with_offset.offset
      };
    }
    return prefix_codes_with_offsets;
  }

  static constexpr std::array<PrefixCode, num_ll_symbols> codes_{build_ll_prefix_codes()};
  static constexpr std::array<PrefixCodeWithOffset, maximum_look_ahead_size+1> length_encodings_{build_length_prefix_codes_with_offsets(codes_)};
  static constexpr std::array<PrefixCodeWithOffset, maximum_look_back_size+1> distance_encodings_{build_distance_prefix_codes_with_offsets()};
public:
  explicit BlockType1Stream(OutputBitStream& output_bit_stream) : out_{output_bit_stream} {}

  [[nodiscard]] auto bits() const -> std::uint64_t {
    return bits_;
  }

  auto reset() {
    block.clear();
  }

  auto put(std::uint8_t byte) {
    look_ahead.enqueue(byte);
    if (!look_ahead.is_full()) {
      return;
    }
    step();
  }

  auto commit(bool is_last) {
    out_.push_bit(is_last ? 1 : 0); // 1 = last block
    out_.push_bits(1, 2); // Two bit block type (in this case, block type 1)
    while (!look_ahead.is_empty()) {
      step();
    }
    for (const auto prefix_code_or_offset : block) {
      if (std::holds_alternative<PrefixCode>(prefix_code_or_offset)) {
        out_.push_prefix_code(std::get<PrefixCode>(prefix_code_or_offset));
      } else {
        out_.push_offset(std::get<Offset>(prefix_code_or_offset));
      }
    }
    out_.push_prefix_code(codes_.at(eob));
    reset();
  }

private:
  auto push_prefix_code(const PrefixCode& prefix_code) {
    block.emplace_back(prefix_code);
    bits_ += prefix_code.length;
  }

  auto push_offset(const Offset& offset) {
    block.emplace_back(offset);
    bits_ += offset.num_bits;
  }

  auto push_back_reference(const PrefixCodeWithOffset& length, const PrefixCodeWithOffset& distance) {
    push_prefix_code(length.prefix_code);
    push_offset(length.offset);
    push_prefix_code(distance.prefix_code);
    push_offset(distance.offset);
  }

  auto step() {
    const auto backref = lzss(look_back, look_ahead);
    const auto byte = look_ahead.dequeue();
    look_back.enqueue(byte);
    const auto& code = codes_.at(byte);
    if (backref.length >= minimum_back_reference_length) {
      const auto& length_encoding = length_encodings_.at(backref.length);
      const auto& distance_encoding = distance_encodings_.at(backref.distance);
      auto num_literal_bits = code.length;
      for (auto i = 0; i < backref.length - 1; ++i) {
        num_literal_bits += codes_.at(look_ahead[i]).length;
      }
      if (num_literal_bits >= length_encoding.prefix_code.length + length_encoding.offset.num_bits + distance_encoding.prefix_code.length + distance_encoding.offset.num_bits) {
        push_back_reference(length_encoding, distance_encoding);
        for (auto i = 1; i < backref.length; ++i) {
          look_back.enqueue(look_ahead.dequeue());
        }
        return;
      }
    }
    push_prefix_code(code);
  }
};

