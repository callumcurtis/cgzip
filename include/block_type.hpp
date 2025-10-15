#pragma once

#include <cstdint>

class BlockStream {
public:
  BlockStream() = default;

  virtual ~BlockStream() = default;

  BlockStream(const BlockStream &) = delete;
  BlockStream(BlockStream &&) = delete;
  auto operator=(const BlockStream &) -> BlockStream & = delete;
  auto operator=(BlockStream &&) -> BlockStream & = delete;

  // Return the number of bits in the compressed block.
  [[nodiscard]] virtual auto bits(bool is_last) -> std::uint64_t = 0;

  // Reset the current block in the block stream.
  virtual auto reset() -> void = 0;

  // Add a byte to the current block in the block stream.
  virtual auto put(std::uint8_t byte) -> void = 0;

  // Commit the current block in the block stream.
  virtual auto commit(bool is_last) -> void = 0;
};
