#pragma once

#include <cstdint>

constexpr std::uint16_t num_literal_length_symbols = 288;
constexpr std::uint16_t num_distance_symbols = 30;
constexpr std::uint16_t num_length_symbols = 29;
constexpr std::uint16_t eob_symbol = 256;
constexpr std::uint16_t minimum_back_reference_length = 3;
constexpr std::uint16_t minimum_back_reference_distance = 1;
constexpr std::uint16_t maximum_look_back_size = 1U << 15U;
constexpr std::uint16_t maximum_look_ahead_size = 258;
constexpr std::uint8_t maximum_prefix_code_length = 15;
