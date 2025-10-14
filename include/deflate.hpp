#pragma once

#include "gz.hpp"
#include "types.hpp"

namespace deflate {

class BitStreamMixin : public gz::BitStreamMixin {
public:
  auto push_prefix_code(PrefixCode prefix_code) -> void;
  auto push_offset(Offset offset) -> void;
  auto push_back_reference(PrefixCodedBackReference prefix_coded_back_reference)
      -> void;

private:
  template <typename Integral>
  auto push_symbolic_bits(Integral b, std::uint8_t num_bits) -> void {
    for (std::uint8_t i = num_bits; i-- > 0;) {
      push_bit((b >> i) & 1);
    }
  }
};

class BitStream final : public BitStreamMixin {
public:
  explicit BitStream(gz::BitStream &bit_stream);

  auto push_bit(std::uint8_t b) -> void override;
  auto flush_byte() -> void override;

private:
  gz::BitStream &wrapped_;
};

class BufferedBitStream final : public BitStreamMixin {
public:
  explicit BufferedBitStream(gz::BitStream &bit_stream);

  auto push_bit(std::uint8_t b) -> void override;
  auto flush_byte() -> void override;

  [[nodiscard]] auto bits() const -> std::size_t;
  auto commit() -> void;
  auto reset() -> void;
  auto unbuffered() -> BitStream &;

private:
  gz::BufferedBitStream wrapped_;
  BitStream unbuffered_;
};

} // namespace deflate
