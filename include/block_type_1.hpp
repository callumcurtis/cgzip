#pragma once

#include "third_party/output_stream.hpp"

#include "lzss.hpp"
#include "prefix_codes.hpp"
#include "ring_buffer.hpp"

const int num_ll_codes = 288;
const int eob = 256;
const int maximum_look_back_size = 1 << 15;
const int maximum_look_ahead_size = 258;
const int minimum_back_reference_length = 3;

struct PrefixCodeWithOffset {
  PrefixCode prefix_code;
  unsigned int num_offset_bits;
  unsigned int offset;
};

struct DistanceRange {
  std::uint8_t bits;
  unsigned int num_offset_bits;
  unsigned int start;
  unsigned int end;
};

struct LengthRange {
  unsigned int prefix_code_ind;
  unsigned int num_offset_bits;
  unsigned int start;
  unsigned int end;
};

template <std::size_t LookBackSize = maximum_look_back_size, std::size_t LookAheadSize = maximum_look_ahead_size>
class BlockType1Stream {
private:
  OutputBitStream& out_;
  RingBuffer<std::uint8_t, LookBackSize> look_back{};
  RingBuffer<std::uint8_t, LookAheadSize> look_ahead{};

  static constexpr auto build_distance_prefix_codes_with_offsets() -> std::array<PrefixCodeWithOffset, maximum_look_back_size+1> {
    const int num_ranges = 30;
    const int num_bits = 5;
    const std::array<DistanceRange, num_ranges> ranges = {
           DistanceRange{0,0,1,1},         DistanceRange{1,0,2,2},          DistanceRange{2,0,3,3},           DistanceRange{3,0,4,4},           DistanceRange{4,1,5,6},
           DistanceRange{5,1,7,8},         DistanceRange{6,2,9,12},         DistanceRange{7,2,13,16},         DistanceRange{8,3,17,24},         DistanceRange{9,3,25,32},
           DistanceRange{10,4,33,48},      DistanceRange{11,4,49,64},       DistanceRange{12,5,65,96},        DistanceRange{13,5,97,128},       DistanceRange{14,6,129,192},
           DistanceRange{15,6,193,256},    DistanceRange{16,7,257,384},     DistanceRange{17,7,385,512},      DistanceRange{18,8,513,768},      DistanceRange{19,8,769,1024},
           DistanceRange{20,9,1025,1536},  DistanceRange{21,9,1537,2048},   DistanceRange{22,10,2049,3072},   DistanceRange{23,10,3073,4096},   DistanceRange{24,11,4097,6144},
           DistanceRange{25,11,6145,8192}, DistanceRange{26,12,8193,12288}, DistanceRange{27,12,12289,16384}, DistanceRange{28,13,16385,24576}, DistanceRange{29,13,24577,32768},
    };
    std::array<PrefixCodeWithOffset, maximum_look_back_size+1> prefix_codes_with_offsets{};
    for (auto range : ranges) {
      for (auto point = range.start; point <= range.end; ++point) {
        prefix_codes_with_offsets.at(point) = PrefixCodeWithOffset{
          .prefix_code = PrefixCode{
            .bits = range.bits,
            .length = num_bits
          },
          .num_offset_bits = range.num_offset_bits,
          .offset = (point - range.start)
        };
      }
    }
    return prefix_codes_with_offsets;
  }

  static constexpr auto build_ll_prefix_codes() -> std::array<PrefixCode, num_ll_codes> {
    std::array<length_t, num_ll_codes> lengths{};
    std::fill(lengths.begin(),       lengths.begin() + 144, 8);
    std::fill(lengths.begin() + 144, lengths.begin() + 256, 9);
    std::fill(lengths.begin() + 256, lengths.begin() + 280, 7);
    std::fill(lengths.begin() + 280, lengths.end(),         8);
    return prefix_codes<num_ll_codes>(lengths);
  }

  static constexpr auto build_length_prefix_codes_with_offsets(const std::array<PrefixCode, num_ll_codes> codes) -> std::array<PrefixCodeWithOffset, maximum_look_ahead_size+1> {
    const int num_ranges = 29;
    const std::array<LengthRange, num_ranges> ranges = {
            LengthRange{257,0,3,3},     LengthRange{258,0,4,4},     LengthRange{259,0,5,5},     LengthRange{260,0,6,6},     {261,0,7,7},
            LengthRange{262,0,8,8},     LengthRange{263,0,9,9},     LengthRange{264,0,10,10},   LengthRange{265,1,11,12},   {266,1,13,14},
            LengthRange{267,1,15,16},   LengthRange{268,1,17,18},   LengthRange{269,2,19,22},   LengthRange{270,2,23,26},   {271,2,27,30},
            LengthRange{272,2,31,34},   LengthRange{273,3,35,42},   LengthRange{274,3,43,50},   LengthRange{275,3,51,58},   {276,3,59,66},
            LengthRange{277,4,67,82},   LengthRange{278,4,83,98},   LengthRange{279,4,99,114},  LengthRange{280,4,115,130}, {281,5,131,162},
            LengthRange{282,5,163,194}, LengthRange{283,5,195,226}, LengthRange{284,5,227,257}, LengthRange{285,0,258,258}
    };
    std::array<PrefixCodeWithOffset, maximum_look_ahead_size+1> prefix_codes_with_offsets{};
    for (auto range : ranges) {
      for (auto point = range.start; point <= range.end; ++point) {
        prefix_codes_with_offsets.at(point) = PrefixCodeWithOffset{
          .prefix_code = codes.at(range.prefix_code_ind),
          .num_offset_bits = range.num_offset_bits,
          .offset = (point - range.start)
        };
      }
    }
    return prefix_codes_with_offsets;
  }

  static constexpr std::array<PrefixCode, num_ll_codes> codes_{build_ll_prefix_codes()};
  static constexpr std::array<PrefixCodeWithOffset, maximum_look_ahead_size+1> length_encodings_{build_length_prefix_codes_with_offsets(codes_)};
  static constexpr std::array<PrefixCodeWithOffset, maximum_look_back_size+1> distance_encodings_{build_distance_prefix_codes_with_offsets()};
public:
  explicit BlockType1Stream(OutputBitStream& output_bit_stream) : out_{output_bit_stream} {}

  auto start(bool is_last) {
    out_.push_bit(is_last ? 1 : 0); // 1 = last block
    out_.push_bits(1, 2); // Two bit block type (in this case, block type 1)
  }

  auto put(std::uint8_t byte) {
    look_ahead.enqueue(byte);
    if (!look_ahead.is_full()) {
      return;
    }
    step();
  }

  auto end() {
    while (!look_ahead.is_empty()) {
      step();
    }
    push_code(codes_.at(eob));
  }

private:
  auto push_code(const PrefixCode& code) {
    out_.push_symbol(code.bits, code.length);
  }

  auto push_back_reference(const PrefixCodeWithOffset& length, const PrefixCodeWithOffset& distance) {
   push_code(length.prefix_code);
   out_.push_bits(length.offset, length.num_offset_bits);
   push_code(distance.prefix_code);
   out_.push_bits(distance.offset, distance.num_offset_bits);
  }

  auto step() {
    const auto backref = lzss(look_back, look_ahead);
    const auto byte = look_ahead.dequeue();
    look_back.enqueue(byte);
    const auto& code = codes_.at(byte);
    if (backref.length >= minimum_back_reference_length) {
      const auto& length_encoding = length_encodings_.at(backref.length);
      const auto& distance_encoding = distance_encodings_.at(backref.distance);
      auto num_literal_bits = code.length;
      for (auto i = 0; i < backref.length - 1; ++i) {
        num_literal_bits += codes_.at(look_ahead[i]).length;
      }
      if (num_literal_bits >= length_encoding.prefix_code.length + length_encoding.num_offset_bits + distance_encoding.prefix_code.length + distance_encoding.num_offset_bits) {
        push_back_reference(length_encoding, distance_encoding);
        for (auto i = 1; i < backref.length; ++i) {
          look_back.enqueue(look_ahead.dequeue());
        }
        return;
      }
    }
    push_code(code);
  }
};

