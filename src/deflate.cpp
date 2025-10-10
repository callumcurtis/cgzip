#include "deflate.hpp"

auto deflate::BitStreamMixin::push_prefix_code(PrefixCode prefix_code) -> void {
    push_symbolic_bits(prefix_code.bits, prefix_code.length);
}

auto deflate::BitStreamMixin::push_offset(Offset offset) -> void {
    push_bits(offset.bits, offset.num_bits);
}

auto deflate::BitStreamMixin::push_back_reference(PrefixCodedBackReference prefix_coded_back_reference) -> void {
    push_prefix_code(prefix_coded_back_reference.length.prefix_code);
    push_offset(prefix_coded_back_reference.length.offset);
    push_prefix_code(prefix_coded_back_reference.distance.prefix_code);
    push_offset(prefix_coded_back_reference.distance.offset);
}

deflate::BitStream::BitStream(gz::BitStream& bit_stream) : wrapped_(bit_stream) {}

auto deflate::BitStream::push_bit(std::uint8_t b) -> void {
    wrapped_.push_bit(b);
}

auto deflate::BitStream::flush_byte() -> void {
    wrapped_.flush_byte();
}

deflate::BufferedBitStream::BufferedBitStream(gz::BitStream& bit_stream) : wrapped_(gz::BufferedBitStream(bit_stream)), unbuffered_(BitStream(bit_stream)) {}

auto deflate::BufferedBitStream::push_bit(std::uint8_t b) -> void {
    wrapped_.push_bit(b);
}

auto deflate::BufferedBitStream::flush_byte() -> void {
    wrapped_.flush_byte();
}

auto deflate::BufferedBitStream::bits() const -> std::size_t {
    return wrapped_.bits();
}

auto deflate::BufferedBitStream::commit() -> void {
    wrapped_.commit();
}

auto deflate::BufferedBitStream::reset() -> void {
    wrapped_.reset();
}

auto deflate::BufferedBitStream::unbuffered() -> BitStream& {
    return unbuffered_;
}
