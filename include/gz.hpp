#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "size.hpp"

namespace gz {

class BitStreamMixin {
public:
  BitStreamMixin() = default;

  virtual ~BitStreamMixin() = default;

  BitStreamMixin(const BitStreamMixin &) = delete;
  BitStreamMixin(BitStreamMixin &&) = delete;
  auto operator=(const BitStreamMixin &) -> BitStreamMixin & = delete;
  auto operator=(BitStreamMixin &&) -> BitStreamMixin & = delete;

  template <typename Integral>
  auto push_bits(Integral b, std::uint8_t num_bits) -> void {
    for (std::uint8_t i = 0; i < num_bits; i++) {
      push_bit((b >> i) & 1);
    }
  }

  template <typename Integral> auto push(Integral b) -> void {
    push_bits(b, size_of_in_bits(b));
  }

  template <typename Integral, typename... Integrals>
  auto push(Integral b, Integrals... rest) -> void {
    push(b);
    push(rest...);
  }

  virtual auto push_bit(std::uint8_t b) -> void = 0;
  virtual auto flush_byte() -> void = 0;

  auto push_header() -> void;
  auto push_footer(std::uint32_t crc_on_uncompressed,
                   std::uint32_t num_bytes_uncompressed) -> void;
};

class BitStream final : public BitStreamMixin {
public:
  explicit BitStream(std::ostream &stream);

  ~BitStream() final;

  BitStream(const BitStream &) = delete;
  BitStream(BitStream &&) = delete;
  auto operator=(const BitStream &) -> BitStream & = delete;
  auto operator=(BitStream &&) -> BitStream & = delete;

  auto push_bit(std::uint8_t b) -> void override;
  auto flush_byte() -> void override;

private:
  std::uint8_t bits_;
  std::uint8_t num_bits_;
  std::ostream &out_;
};

class BufferedBitStream final : public BitStreamMixin {
public:
  explicit BufferedBitStream(BitStream &bit_stream);

  auto push_bit(std::uint8_t b) -> void override;
  auto flush_byte() -> void override;

  [[nodiscard]] auto bits() const -> std::size_t;
  auto commit() -> void;
  auto reset() -> void;
  auto unbuffered() -> BitStream &;

private:
  std::uint8_t bits_;
  std::uint8_t num_bits_;
  BitStream &wrapped_;
  std::vector<std::uint8_t> buffer_;
};

} // namespace gz
