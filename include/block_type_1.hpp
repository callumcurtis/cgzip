#pragma once

#include "prefix_codes.hpp"
#include "third_party/output_stream.hpp"

const int num_ll_codes = 288;
const int eob = 256;
const int look_back_size = 1 << 15;
const int look_ahead_size = 258;

template <std::size_t LookBackSize = look_back_size, std::size_t LookAheadSize = look_ahead_size>
class BlockType1Stream {
private:
  OutputBitStream& out_;

  static constexpr auto build_ll_prefix_codes() -> std::array<PrefixCode, num_ll_codes> {
    std::array<length_t, num_ll_codes> lengths{};
    std::fill(lengths.begin(),       lengths.begin() + 144, 8);
    std::fill(lengths.begin() + 144, lengths.begin() + 256, 9);
    std::fill(lengths.begin() + 256, lengths.begin() + 280, 7);
    std::fill(lengths.begin() + 280, lengths.end(),         8);
    return prefix_codes<num_ll_codes>(lengths);
  }

  static constexpr std::array<PrefixCode, num_ll_codes> codes_{build_ll_prefix_codes()};
public:
  explicit BlockType1Stream(OutputBitStream& output_bit_stream) : out_{output_bit_stream} {}

  auto start(bool is_last) {
    out_.push_bit(is_last ? 1 : 0); // 1 = last block
    out_.push_bits(1, 2); // Two bit block type (in this case, block type 1)
  }

  auto put(std::uint8_t byte) {
    push_code(codes_.at(byte));
  }

  auto end() {
    push_code(codes_.at(eob));
  }

private:
  auto push_code(const PrefixCode& code) {
    out_.push_symbol(code.bits, code.length);
  }
};

