#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>

#define CRCPP_USE_CPP11
#include "third_party/CRC.h"

#include "block_type.hpp"
#include "block_type_0.hpp"
#include "block_type_1.hpp"
#include "block_type_2.hpp"
#include "change_point_detection.hpp"
#include "gz.hpp"
#include "lzss.hpp"

struct BlockStreamWithMaximumBlockSize {
  std::unique_ptr<BlockStream> block_stream;
  std::size_t maximum_uncompressed_bytes_in_block;
};

auto main() -> int {
  // Untie stdin from stdout to avoid flushing output before each read
  std::cin.tie(nullptr);

  auto crc_table = CRC::CRC_32().MakeTable();
  std::uint32_t crc{};

  gz::BitStream stream{std::cout};
  stream.push_header();

  // We need to see ahead of the stream by one character (e.g. to know, once we
  // fill up a block, whether there are more blocks coming), so at each step,
  // next_byte will be the next byte from the stream that is NOT in a block.
  char next_byte{};

  std::uint32_t num_uncompressed_bytes_in_file{0};
  std::uint32_t num_uncompressed_bytes_in_block{0};

  const auto block_streams_with_maximum_block_sizes =
      std::array<BlockStreamWithMaximumBlockSize, 3>{
          BlockStreamWithMaximumBlockSize{
              .block_stream = std::make_unique<
                  BlockType0Stream<maximum_block_type_0_capacity>>(stream),
              .maximum_uncompressed_bytes_in_block =
                  maximum_block_type_0_capacity},
          // Block type 1 is only suitable for small blocks, where the overhead
          // of block type 2 is comparatively large. Since the warmup period for
          // the change point detector is larger than the maximum desirable size
          // of a block of type 1, disable block type 1 entirely by setting its
          // breakpoint to 0. This improves speed by reducing the number of LZSS
          // searches.
          BlockStreamWithMaximumBlockSize{
              .block_stream = std::make_unique<BlockType1Stream<
                  maximum_look_back_size, maximum_look_ahead_size>>(stream),
              .maximum_uncompressed_bytes_in_block = 0},
          BlockStreamWithMaximumBlockSize{
              .block_stream = std::make_unique<BlockType2Stream<
                  maximum_look_back_size, maximum_look_ahead_size>>(stream),
              .maximum_uncompressed_bytes_in_block = 1U << 30U}};

  CusumDistributionDetector change_point_detector(
      {.t_warmup = 1U << 13U, // NOLINT (cppcoreguidelines-avoid-magic-numbers)
       .h_threshold = 1e3});  // NOLINT (cppcoreguidelines-avoid-magic-numbers)

  const auto maximum_of_maximum_uncompressed_block_sizes =
      std::ranges::max_element(block_streams_with_maximum_block_sizes,
                               [](const auto &a, const auto &b) {
                                 return a.maximum_uncompressed_bytes_in_block <
                                        b.maximum_uncompressed_bytes_in_block;
                               })
          ->maximum_uncompressed_bytes_in_block;

  auto commit_smallest = [&block_streams_with_maximum_block_sizes,
                          &num_uncompressed_bytes_in_block](bool is_last) {
    std::uint8_t smallest_compressed_block_type =
        std::numeric_limits<std::uint8_t>::max();
    std::size_t smallest_compressed_block_size =
        std::numeric_limits<std::size_t>::max();
    for (auto i = 0; i < block_streams_with_maximum_block_sizes.size(); ++i) {
      if (num_uncompressed_bytes_in_block >
          block_streams_with_maximum_block_sizes.at(i)
              .maximum_uncompressed_bytes_in_block) {
        continue;
      }
      const auto compressed_block_size =
          block_streams_with_maximum_block_sizes.at(i).block_stream->bits(
              is_last);
      if (compressed_block_size < smallest_compressed_block_size) {
        smallest_compressed_block_size = compressed_block_size;
        smallest_compressed_block_type = static_cast<std::uint8_t>(i);
      }
    }
    block_streams_with_maximum_block_sizes.at(smallest_compressed_block_type)
        .block_stream->commit(is_last);
  };

  if (std::cin.get(next_byte)) {
    num_uncompressed_bytes_in_file++;
    num_uncompressed_bytes_in_block++;
    crc = CRC::Calculate(&next_byte, 1, crc_table);
    while (true) {
      for (const auto &block_stream_with_maximum_block_size :
           block_streams_with_maximum_block_sizes) {
        if (num_uncompressed_bytes_in_block >=
            block_stream_with_maximum_block_size
                .maximum_uncompressed_bytes_in_block) {
          continue;
        }
        block_stream_with_maximum_block_size.block_stream->put(next_byte);
      }
      const auto is_change_point_detected =
          change_point_detector.step(next_byte).second;
      if (!std::cin.get(next_byte)) {
        break;
      }
      num_uncompressed_bytes_in_file++;
      num_uncompressed_bytes_in_block++;
      crc = CRC::Calculate(&next_byte, 1, crc_table, crc);

      if (is_change_point_detected ||
          num_uncompressed_bytes_in_block >=
              maximum_of_maximum_uncompressed_block_sizes) {
        commit_smallest(false);
        for (const auto &block_stream_with_maximum_block_size :
             block_streams_with_maximum_block_sizes) {
          block_stream_with_maximum_block_size.block_stream->reset();
        }
        change_point_detector.reset();
        num_uncompressed_bytes_in_block = 0;
      }
    }

    commit_smallest(true);

    // Pad to byte boundary before returning from deflate bitstream to gz
    // bitstream
    stream.flush_byte();
  }

  stream.push_footer(crc, num_uncompressed_bytes_in_file);

  return 0;
}
