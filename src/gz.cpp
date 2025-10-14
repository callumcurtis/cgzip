#include "gz.hpp"
#include "size.hpp"

auto gz::BitStreamMixin::push_header() -> void {
  push(static_cast<std::uint8_t>(0x1f),
       static_cast<std::uint8_t>(0x8b),  // Magic Number
       static_cast<std::uint8_t>(0x08),  // Compression (0x08 = DEFLATE)
       static_cast<std::uint8_t>(0x00),  // Flags
       static_cast<std::uint32_t>(0x00), // MTIME (little endian)
       static_cast<std::uint8_t>(0x00),  // Extra flags
       static_cast<std::uint8_t>(0x03)   // OS (Linux)
  );
}

auto gz::BitStreamMixin::push_footer(std::uint32_t crc_on_uncompressed,
                                     std::uint32_t num_bytes_uncompressed)
    -> void {
  push(crc_on_uncompressed);
  push(num_bytes_uncompressed);
}

gz::BitStream::BitStream(std::ostream &stream)
    : bits_(0), num_bits_(0), out_(stream) {}

gz::BitStream::~BitStream() { flush_byte(); }

auto gz::BitStream::push_bit(std::uint8_t b) -> void {
  bits_ |= (b & 1U) << num_bits_;
  num_bits_++;
  if (num_bits_ == size_of_in_bits(bits_)) {
    flush_byte();
  }
}

auto gz::BitStream::flush_byte() -> void {
  if (num_bits_ == 0) {
    return;
  }
  out_.put(static_cast<unsigned char>(bits_));
  bits_ = 0;
  num_bits_ = 0;
}

gz::BufferedBitStream::BufferedBitStream(BitStream &bit_stream)
    : bits_(0), num_bits_(0), wrapped_(bit_stream) {}

auto gz::BufferedBitStream::push_bit(std::uint8_t b) -> void {
  bits_ |= (b & 1U) << num_bits_;
  num_bits_++;
  if (num_bits_ == size_of_in_bits(bits_)) {
    flush_byte();
  }
}

auto gz::BufferedBitStream::flush_byte() -> void {
  if (num_bits_ == 0) {
    return;
  }
  buffer_.emplace_back(bits_);
  bits_ = 0;
  num_bits_ = 0;
}

auto gz::BufferedBitStream::bits() const -> std::size_t {
  return (buffer_.size() * size_of_in_bits<decltype(buffer_)::value_type>()) +
         num_bits_;
}

auto gz::BufferedBitStream::commit() -> void {
  for (const auto &byte : buffer_) {
    wrapped_.push(byte);
  }
  if (num_bits_ > 0) {
    wrapped_.push_bits(bits_, num_bits_);
  }
}

auto gz::BufferedBitStream::reset() -> void {
  buffer_.clear();
  bits_ = 0;
  num_bits_ = 0;
}

auto gz::BufferedBitStream::unbuffered() -> BitStream & { return wrapped_; }
