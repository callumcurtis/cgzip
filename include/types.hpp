#pragma once

#include <cstdint>

using Symbol = std::uint16_t;

struct Offset {
    std::uint16_t bits;
    std::uint8_t num_bits;
};

struct PrefixCode {
    std::uint16_t bits;
    std::uint8_t length;
};

struct PrefixCodeWithOffset {
    PrefixCode prefix_code;
    Offset offset;
};

struct PrefixCodedBackReference {
    PrefixCodeWithOffset length;
    PrefixCodeWithOffset distance;
};
