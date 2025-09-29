#pragma once

#include <stdexcept>

struct BackReference {
  std::size_t distance;
  std::size_t length;
};

template <typename LookBack, typename LookAhead>
auto lzss(LookBack lookback, LookAhead lookahead) -> BackReference {
  if (lookahead.size() <= 0) {
    throw std::invalid_argument("The lookahead buffer must include at least one element, which corresponds to the next element to be encoded.");
  }
  auto longest_backreference = BackReference{.distance = 0, .length = 0};
  for (auto lookback_start = lookback.size(); lookback_start-- > 0;) {
    for (auto lookahead_ind = 0; lookahead_ind < lookahead.size(); ++lookahead_ind) {
      const auto lookback_ind = (
        lookback.size() - lookback_start < lookahead.size()
        ? lookback_start + (lookahead_ind % (lookback.size() - lookback_start))
        : lookback_start + lookahead_ind
      );
      if (lookback[lookback_ind] != lookahead[lookahead_ind]) {
        break;
      }
      if (longest_backreference.length >= lookahead_ind + 1) {
        continue;
      }
      longest_backreference.distance = lookback.size() - lookback_start;
      longest_backreference.length = lookahead_ind + 1;
    }
  }
  return longest_backreference;
}

