#pragma once

#include "third_party/output_stream.hpp"

class BlockType0Stream {
private:
  OutputBitStream& out_;
  std::vector<std::uint8_t> block_;
  static const std::size_t capacity_ = (1 << 16) - 1;

public:
  explicit BlockType0Stream(OutputBitStream& output_bit_stream) : out_{output_bit_stream} {
    block_.reserve(capacity_);
  }

  [[nodiscard]] auto bits() const -> std::uint64_t {
    return 40 + (block_.size() * 8);
  }

  auto reset() {
    block_.clear();
  }

  auto put(std::uint8_t byte) {
    if (block_.size() == capacity_) {
      throw std::logic_error("Cannot extend a block of type 1 past the maximum length represented by 16 bits");
    }
    block_.emplace_back(byte);
  }

  auto commit(bool is_last) {
    out_.push_bit(is_last ? 1 : 0); // 1 = last block
    out_.push_bits(0, 2); // Two bit block type (in this case, block type 0)
    out_.flush_to_byte();
    out_.push_u16(block_.size());
    out_.push_u16(~block_.size());
    for (const auto literal : block_) {
      out_.push_byte(literal);
    }
    reset();
  }

  [[nodiscard]] static auto capacity() -> std::size_t {
    return capacity_;
  }
};

