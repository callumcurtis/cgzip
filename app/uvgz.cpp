/* uvgz.cpp

   Starter code for Assignment 2 (in C++).
   This basic implementation generates a fully compliant .gz output stream,
   using block mode 0 (store only) for all DEFLATE data.

   B. Bird - 2023-05-12
*/
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <third_party/output_stream.hpp>

// To compute CRC32 values, we can use this library
// from https://github.com/d-bahr/CRCpp
#define CRCPP_USE_CPP11
#include <third_party/CRC.h>




int main(){

    //See output_stream.hpp for a description of the OutputBitStream class
    OutputBitStream stream {std::cout};

    //Pre-cache the CRC table
    auto crc_table = CRC::CRC_32().MakeTable();

    //Push a basic gzip header
    stream.push_bytes( 0x1f, 0x8b, //Magic Number
        0x08, //Compression (0x08 = DEFLATE)
        0x00, //Flags
        0x00, 0x00, 0x00, 0x00, //MTIME (little endian)
        0x00, //Extra flags
        0x03 //OS (Linux)
    );


    //This starter implementation writes a series of blocks with type 0 (store only)
    //Each store-only block can contain up to 2**16 - 1 bytes of data.
    //(This limit does NOT apply to block types 1 and 2)
    //Since we have to keep track of how big each block is (and whether any more blocks
    //follow it), we have to save up the data for each block in an array before writing it.


    //Note that the types u8, u16 and u32 are defined in the output_stream.hpp header
    std::array< u8, (1<<16)-1 > block_contents {};
    u32 block_size {0};
    u32 bytes_read {0};

    char next_byte {}; //Note that we have to use a (signed) char here for compatibility with istream::get()

    //We need to see ahead of the stream by one character (e.g. to know, once we fill up a block,
    //whether there are more blocks coming), so at each step, next_byte will be the next byte from the stream
    //that is NOT in a block.

    //Keep a running CRC of the data we read.
    u32 crc {};


    if (!std::cin.get(next_byte)){
        //Empty input?

    }else{

        bytes_read++;
        //Update the CRC as we read each byte (there are faster ways to do this)
        crc = CRC::Calculate(&next_byte, 1, crc_table); //This call creates the initial CRC value from the first byte read.
        //Read through the input
        while(1){
            block_contents.at(block_size++) = next_byte;
            if (!std::cin.get(next_byte))
                break;

            bytes_read++;
            crc = CRC::Calculate(&next_byte,1, crc_table, crc); //Add the character we just read to the CRC (even though it is not in a block yet)

            //If we get to this point, we just added a byte to the block AND there is at least one more byte in the input waiting to be written.
            if (block_size == block_contents.size()){
                //The block is full, so write it out.
                //We know that there are more bytes left, so this is not the last block
                stream.push_bit(0); //0 = not last block
                stream.push_bits(0, 2); //Two bit block type (in this case, block type 0)
                stream.push_bits(0, 5); //Pad the stream to the next byte boundary.
                //Now write the block size (as a pair (X, ~X) where X is the 16 bit size)
                stream.push_u16(block_size);
                stream.push_u16(~block_size);
                //Now write the actual block data
                for(unsigned int i = 0; i < block_size; i++)
                    stream.push_byte(block_contents.at(i)); //Interesting optimization question: Will the compiler optimize away the bounds checking for .at here?
                block_size = 0;
            }
        }
    }
    //At this point, we've finished reading the input (no new characters remain), and we may have an incomplete block to write.
    if (block_size > 0){
        //Write out any leftover data
        stream.push_bit(1); //1 = last block
        stream.push_bits(0, 2); //Two bit block type (in this case, block type 0)
        stream.push_bits(0, 5); //Pad the stream to the next byte boundary.
        //Now write the block size (as a pair (X, ~X) where X is the 16 bit size)
        stream.push_u16(block_size);
        stream.push_u16(~block_size);
        //Now write the actual block data
        for(unsigned int i = 0; i < block_size; i++)
            stream.push_byte(block_contents.at(i));
        block_size = 0;
    }

    //Now close out the bitstream by writing the CRC and the total number of bytes stored.
    stream.push_u32(crc);
    stream.push_u32(bytes_read);

    return 0;
}
