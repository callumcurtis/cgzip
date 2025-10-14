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

  [[nodiscard]] virtual auto bits(bool is_last) -> std::uint64_t = 0;
  virtual auto reset() -> void = 0;
  virtual auto put(std::uint8_t byte) -> void = 0;
  virtual auto commit(bool is_last) -> void = 0;
};
