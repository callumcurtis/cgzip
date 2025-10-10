#pragma once

#include "gz.hpp"
#include "deflate.hpp"
#include "block_type.hpp"
#include "size.hpp"

const std::uint16_t maximum_block_type_0_capacity = (1U << 16U) - 1;

template <std::uint16_t Capacity = maximum_block_type_0_capacity>
class BlockType0Stream final : public BlockStream {
private:
  deflate::BitStream out_;
  std::vector<std::uint8_t> block_;

public:
  explicit BlockType0Stream(gz::BitStream& bit_stream) : out_{bit_stream} {
    block_.reserve(Capacity);
  }

  [[nodiscard]] auto bits(bool  /*is_last*/) -> std::uint64_t override {
    return 40 + (block_.size() * size_of_in_bits<typename decltype(block_)::value_type>());
  }

  auto reset() -> void override {
    block_.clear();
  }

  auto put(std::uint8_t byte) -> void override {
    if (block_.size() == Capacity) {
      throw std::logic_error("Cannot extend a block of type 1 past the maximum length represented by 16 bits");
    }
    block_.emplace_back(byte);
  }

  auto commit(bool is_last) -> void override {
    out_.push_bit(is_last ? 1 : 0); // 1 = last block
    out_.push_bits(0, 2); // Two bit block type (in this case, block type 0)
    out_.flush_byte();
    out_.push(static_cast<std::uint16_t>(block_.size()));
    out_.push(static_cast<std::uint16_t>(~block_.size()));
    for (const auto literal : block_) {
      out_.push(literal);
    }
  }

  [[nodiscard]] static auto capacity() -> std::size_t {
    return Capacity;
  }
};
