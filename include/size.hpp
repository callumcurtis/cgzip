#pragma once

#include <climits>
#include <cstddef>

template <typename T> constexpr auto size_of_in_bits(T t) -> std::size_t {
  return sizeof(t) * CHAR_BIT;
}

template <typename T> constexpr auto size_of_in_bits() -> std::size_t {
  return sizeof(T) * CHAR_BIT;
}
