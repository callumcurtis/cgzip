#include "third_party/output_stream.hpp"

#include "gz.hpp"

namespace gz {

auto push_header(OutputBitStream& stream) -> void {
    stream.push_bytes(
        0x1f,
        0x8b, // Magic Number
        0x08, // Compression (0x08 = DEFLATE)
        0x00, // Flags
        0x00, 0x00, 0x00, 0x00, // MTIME (little endian)
        0x00, // Extra flags
        0x03 // OS (Linux)
    );
}

auto push_footer(OutputBitStream& stream, std::uint32_t crc, std::uint32_t num_bytes_uncompressed) -> void {
    stream.push_u32(crc);
    stream.push_u32(num_bytes_uncompressed);
}

}
