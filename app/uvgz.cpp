#include <algorithm>
#include <iostream>
#include <memory>
#include <array>

// To compute CRC32 values, we can use this library
// from https://github.com/d-bahr/CRCpp
#define CRCPP_USE_CPP11
#include "third_party/CRC.h"

#include "gz.hpp"
#include "change_point_detection.hpp"
#include "block_type.hpp"
#include "block_type_0.hpp"
#include "block_type_1.hpp"
#include "block_type_2.hpp"

struct BlockStreamWithBreakpoint {
  std::unique_ptr<BlockStream> block_stream;
  std::size_t breakpoint; // The maximum number of uncompressed bytes that can be stored in a block in the stream
};

auto main() -> int {
    // Untie stdout from stdout
    std::cin.tie(nullptr);

    gz::BitStream stream {std::cout};

    auto crc_table = CRC::CRC_32().MakeTable();

    //Push the gzip header
    stream.push_header();

    std::uint32_t bytes_read {0};
    std::uint32_t uncompressed_block_size {0};

    char next_byte {}; //Note that we have to use a (signed) char here for compatibility with istream::get()

    //We need to see ahead of the stream by one character (e.g. to know, once we fill up a block,
    //whether there are more blocks coming), so at each step, next_byte will be the next byte from the stream
    //that is NOT in a block.

    //Keep a running CRC of the data we read.
    std::uint32_t crc {};

    const auto block_streams_with_breakpoints = std::array<BlockStreamWithBreakpoint, 3>{
      BlockStreamWithBreakpoint{.block_stream=std::make_unique<BlockType0Stream>(stream), .breakpoint=BlockType0Stream::capacity()},
      // Block type 1 is only suitable for small blocks, where the overhead of block type 2 is comparatively large.
      // Since the warmup period for the change point detector is larger than the maximum desirable size of a
      // block of type 1, disable block type 1 entirely by setting its breakpoint to 0. This improves speed
      // by reducing the number of LZSS searches.
      BlockStreamWithBreakpoint{.block_stream=std::make_unique<BlockType1Stream<maximum_look_back_size, maximum_look_ahead_size>>(stream), .breakpoint=0},
      BlockStreamWithBreakpoint{.block_stream=std::make_unique<BlockType2Stream<maximum_look_back_size, maximum_look_ahead_size>>(stream), .breakpoint=1 << 30}
    };
    CusumDistributionDetector change_point_detector(1 << 13, 1e3);
    const auto last_breakpoint = std::ranges::max_element(
      block_streams_with_breakpoints,
      [](const auto &a, const auto &b) {
          return a.breakpoint < b.breakpoint;
      }
    )->breakpoint;

    auto commit_smallest = [&block_streams_with_breakpoints, &uncompressed_block_size](bool is_last) {
      std::uint8_t smallest_compressed_block_type = std::numeric_limits<std::uint8_t>::max();
      std::size_t smallest_compressed_block_size = std::numeric_limits<std::size_t>::max();
      for (auto i = 0; i < block_streams_with_breakpoints.size(); ++i) {
        if (uncompressed_block_size > block_streams_with_breakpoints.at(i).breakpoint) {
          continue;
        }
        const auto compressed_block_size = block_streams_with_breakpoints.at(i).block_stream->bits(is_last);
        if (compressed_block_size < smallest_compressed_block_size) {
          smallest_compressed_block_size = compressed_block_size;
          smallest_compressed_block_type = static_cast<std::uint8_t>(i);
        }
      }
      block_streams_with_breakpoints.at(smallest_compressed_block_type).block_stream->commit(is_last);
    };

    if (std::cin.get(next_byte)){
        bytes_read++;
        uncompressed_block_size++;
        //Update the CRC as we read each byte (there are faster ways to do this)
        crc = CRC::Calculate(&next_byte, 1, crc_table); //This call creates the initial CRC value from the first byte read.
        //Read through the input
        while(1){
            for (auto &block_stream_with_breakpoint : block_streams_with_breakpoints) {
              if (uncompressed_block_size >= block_stream_with_breakpoint.breakpoint) {
                continue;
              }
              block_stream_with_breakpoint.block_stream->put(next_byte);
            }
            const auto is_change_point_detected = change_point_detector.step(next_byte).second;
            if (!std::cin.get(next_byte))
                break;
            bytes_read++;
            uncompressed_block_size++;
            crc = CRC::Calculate(&next_byte,1, crc_table, crc); //Add the character we just read to the CRC (even though it is not in a block yet)

            if (is_change_point_detected || uncompressed_block_size >= last_breakpoint) {
                commit_smallest(false);
                for (auto &block_stream_with_breakpoint : block_streams_with_breakpoints) {
                  block_stream_with_breakpoint.block_stream->reset();
                }
                change_point_detector.reset();
                uncompressed_block_size = 0;
            }
        }

        commit_smallest(true);

        // Pad to byte boundary before returning to gz
        stream.flush_byte();
    }

    stream.push_footer(crc, bytes_read);

    return 0;
}
