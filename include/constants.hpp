#pragma once

#include <cstdint>

const std::uint16_t num_literal_length_symbols = 288;
const std::uint16_t num_distance_symbols = 30;
const std::uint16_t num_length_symbols = 29;
const std::uint16_t eob_symbol = 256;
const std::uint16_t minimum_back_reference_length = 3;
const std::uint16_t minimum_back_reference_distance = 1;
const std::uint16_t maximum_look_back_size = 1U << 15U;
const std::uint16_t maximum_look_ahead_size = 258;
const std::uint8_t maximum_prefix_code_length = 15;
