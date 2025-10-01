#include <iostream>

#include "third_party/output_stream.hpp"
// To compute CRC32 values, we can use this library
// from https://github.com/d-bahr/CRCpp
#define CRCPP_USE_CPP11
#include "third_party/CRC.h"

#include "block_type_1.hpp"
#include "block_type_2.hpp"

int main(){
    // Untie stdout from stdout
    std::cin.tie(nullptr);

    OutputBitStream stream {std::cout};

    auto crc_table = CRC::CRC_32().MakeTable();

    //Push a basic gzip header
    stream.push_bytes( 0x1f, 0x8b, //Magic Number
        0x08, //Compression (0x08 = DEFLATE)
        0x00, //Flags
        0x00, 0x00, 0x00, 0x00, //MTIME (little endian)
        0x00, //Extra flags
        0x03 //OS (Linux)
    );

    //Note that the types u8, u16 and u32 are defined in the output_stream.hpp header
    u32 bytes_read {0};

    char next_byte {}; //Note that we have to use a (signed) char here for compatibility with istream::get()

    //We need to see ahead of the stream by one character (e.g. to know, once we fill up a block,
    //whether there are more blocks coming), so at each step, next_byte will be the next byte from the stream
    //that is NOT in a block.

    //Keep a running CRC of the data we read.
    u32 crc {};

    BlockType2Stream block_type_2_stream(stream);

    if (std::cin.get(next_byte)){
        block_type_2_stream.start();

        bytes_read++;
        //Update the CRC as we read each byte (there are faster ways to do this)
        crc = CRC::Calculate(&next_byte, 1, crc_table); //This call creates the initial CRC value from the first byte read.
        //Read through the input
        while(1){
            block_type_2_stream.put(next_byte);
            if (!std::cin.get(next_byte))
                break;
            bytes_read++;
            crc = CRC::Calculate(&next_byte,1, crc_table, crc); //Add the character we just read to the CRC (even though it is not in a block yet)

            if (block_type_2_stream.block_size() >= 1 << 15) {
                block_type_2_stream.restart();
            }
        }
        block_type_2_stream.end();

        // Pad to byte boundary before returning to gz
        stream.flush_to_byte();
    }

    //Now close out the bitstream by writing the CRC and the total number of bytes stored.
    stream.push_u32(crc);
    stream.push_u32(bytes_read);

    return 0;
}

