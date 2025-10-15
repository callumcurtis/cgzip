#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "block_type.hpp"
#include "deflate.hpp"
#include "gz.hpp"
#include "lzss.hpp"
#include "package_merge.hpp"
#include "prefix_codes.hpp"
#include "types.hpp"

namespace block_type_2 {

template <std::uint16_t LookBackSize = maximum_look_back_size,
          std::uint16_t LookAheadSize = maximum_look_ahead_size>
class Stream final : public BlockStream {
private:
  deflate::BufferedBitStream buffered_out_;
  Lzss<LookBackSize, LookAheadSize> lzss_;
  std::array<std::size_t, num_ll_symbols + num_distance_symbols>
      count_by_symbol_{};
  std::vector<std::variant<Symbol, Offset>> block_;
  bool is_last_and_buffered_ = false;

public:
  explicit Stream(gz::BitStream &bit_stream) : buffered_out_{bit_stream} {}

  [[nodiscard]] auto bits(bool is_last) -> std::uint64_t override {
    buffer(is_last);
    return buffered_out_.bits();
  }

  auto reset() -> void override {
    count_by_symbol_.fill(0);
    block_.clear();
    buffered_out_.reset();
    is_last_and_buffered_ = false;
  }

  auto put(std::uint8_t byte) -> void override {
    lzss_.put(byte);
    if (!lzss_.is_full()) {
      return;
    }
    step();
  }

  auto commit(bool is_last) -> void override {
    buffer(is_last);
    buffered_out_.commit();
  }

private:
  auto buffer(bool is_last) {
    if (buffered_out_.bits() > 0) {
      // We have already buffered.
      if (is_last_and_buffered_ != is_last) {
        throw std::logic_error(
            "Cannot re-buffer with different last block flag");
      }
      return;
    }

    is_last_and_buffered_ = is_last;

    buffered_out_.push_bit(is_last ? 1 : 0);
    buffered_out_.push_bits(2, 2);

    while (!lzss_.is_empty()) {
      step();
    }

    push_symbol(eob);

    flush_block();
  }

  auto push_symbol(const Symbol symbol) {
    count_by_symbol_.at(symbol)++;
    block_.emplace_back(symbol);
  }

  auto push_offset(const Offset &offset) { block_.emplace_back(offset); }

  auto push_back_reference() {
    const auto back_reference = lzss_.back_reference();
    const auto length_symbol_with_offset =
        symbol_with_offset_from_length(back_reference.length);
    const auto distance_symbol_with_offset =
        symbol_with_offset_from_distance(back_reference.distance);
    push_symbol(length_symbol_with_offset.symbol);
    push_offset(length_symbol_with_offset.offset);
    push_symbol(distance_symbol_with_offset.symbol + num_ll_symbols);
    push_offset(distance_symbol_with_offset.offset);
    // Add the literals in the back reference to the block after the back
    // reference so that we can decide between using the literals or the back
    // reference when flushing the block.
    for (auto i = lzss_.literals_in_back_reference_begin();
         i != lzss_.literals_in_back_reference_end(); ++i) {
      block_.emplace_back(*i);
    }
  }

  struct CodeLengthOffset {
    std::uint8_t bits;
    std::uint8_t num_bits;
  };

  struct CodeLengthSymbolWithOffset {
    std::uint8_t symbol;
    CodeLengthOffset offset;
  };

  struct CodeLengthSymbolBatch {
    std::uint8_t symbol;
    std::uint8_t offset_num_bits;
    std::uint16_t min;
    std::uint16_t max;
  };

  auto flush_block_metadata(
      const std::array<PrefixCode, num_ll_symbols> &literal_length_prefix_codes,
      const std::array<PrefixCode, num_distance_symbols>
          &distance_prefix_codes) {

    auto count_trailing_zero_length_prefix_codes = [](auto &prefix_codes) {
      std::uint16_t count = 0;
      for (const auto &prefix_code : prefix_codes) {
        if (prefix_code.length > 0) {
          count = 0;
        } else {
          count++;
        }
      }
      return count;
    };

    auto count_leading_nonzero_prefix_codes = [](const std::uint16_t min,
                                                 const std::uint16_t max,
                                                 const std::uint16_t trailing) {
      return std::max(min, std::uint16_t(max - trailing));
    };

    constexpr auto min_leading_literal_length_prefix_codes = 257;
    constexpr auto min_leading_distance_prefix_codes = 1;
    constexpr auto min_leading_code_length_prefix_codes = 4;
    constexpr auto literal_length_header_num_bits = 5;
    constexpr auto distance_header_num_bits = 5;
    constexpr auto code_length_header_num_bits = 4;
    constexpr auto code_length_num_bits = 3;

    constexpr std::uint8_t maximum_code_length = 7;
    constexpr std::uint8_t num_code_length_symbols = 19;

    const auto num_leading_literal_length_prefix_codes =
        count_leading_nonzero_prefix_codes(
            min_leading_literal_length_prefix_codes, num_ll_symbols,
            count_trailing_zero_length_prefix_codes(
                literal_length_prefix_codes));
    const auto num_leading_distance_prefix_codes =
        count_leading_nonzero_prefix_codes(
            min_leading_distance_prefix_codes, num_distance_symbols,
            count_trailing_zero_length_prefix_codes(distance_prefix_codes));

    std::array<std::uint16_t, num_code_length_symbols>
        count_by_code_length_symbol{};
    std::vector<std::variant<std::uint8_t, CodeLengthSymbolWithOffset>>
        cl_symbols;
    std::uint8_t prev_prefix_code_length = maximum_prefix_code_length + 1;
    std::uint16_t num_prev_prefix_code_length = 0;

    auto add_code_length_symbol_batch =
        [&prev_prefix_code_length, &num_prev_prefix_code_length, &cl_symbols,
         &count_by_code_length_symbol](
            CodeLengthSymbolBatch code_length_batch_symbol) {
          while (num_prev_prefix_code_length >= code_length_batch_symbol.min) {
            const std::uint16_t size = std::min(code_length_batch_symbol.max,
                                                num_prev_prefix_code_length);
            num_prev_prefix_code_length -= size;
            count_by_code_length_symbol.at(code_length_batch_symbol.symbol)++;
            cl_symbols.emplace_back(CodeLengthSymbolWithOffset{
                .symbol = code_length_batch_symbol.symbol,
                .offset = CodeLengthOffset{
                    .bits = std::uint8_t(size - code_length_batch_symbol.min),
                    .num_bits = code_length_batch_symbol.offset_num_bits}});
          }
          for (auto i = 0; std::cmp_less(i, num_prev_prefix_code_length); ++i) {
            cl_symbols.emplace_back(prev_prefix_code_length);
          }
          count_by_code_length_symbol.at(prev_prefix_code_length) +=
              num_prev_prefix_code_length;
          num_prev_prefix_code_length = 0;
        };

    auto add_consecutive_prefix_codes_to_code_length_symbols =
        [&prev_prefix_code_length, &num_prev_prefix_code_length, &cl_symbols,
         &count_by_code_length_symbol, &add_code_length_symbol_batch]() {
          if (num_prev_prefix_code_length <= 0) {
            return;
          }

          constexpr auto zero_11_or_more_times = CodeLengthSymbolBatch{
              .symbol = 18, .offset_num_bits = 7, .min = 11, .max = 138};
          if (prev_prefix_code_length == 0 &&
              num_prev_prefix_code_length >= zero_11_or_more_times.min) {
            add_code_length_symbol_batch(zero_11_or_more_times);
            return;
          }

          constexpr auto zero_3_or_more_times = CodeLengthSymbolBatch{
              .symbol = 17, .offset_num_bits = 3, .min = 3, .max = 10};
          if (prev_prefix_code_length == 0 &&
              num_prev_prefix_code_length >= zero_3_or_more_times.min) {
            add_code_length_symbol_batch(zero_3_or_more_times);
            return;
          }

          constexpr auto previous_3_or_more_times = CodeLengthSymbolBatch{
              .symbol = 16, .offset_num_bits = 2, .min = 3, .max = 6};
          cl_symbols.emplace_back(prev_prefix_code_length);
          count_by_code_length_symbol.at(prev_prefix_code_length)++;
          num_prev_prefix_code_length--;
          if (num_prev_prefix_code_length > 0) {
            add_code_length_symbol_batch(previous_3_or_more_times);
          }
        };

    auto add_prefix_codes_to_code_length_symbols =
        [&prev_prefix_code_length, &num_prev_prefix_code_length,
         &add_consecutive_prefix_codes_to_code_length_symbols](const auto start,
                                                               const auto end) {
          for (auto prefix_code_ptr = start; prefix_code_ptr < end;
               ++prefix_code_ptr) {
            const auto prefix_code_length = prefix_code_ptr->length;
            if (prefix_code_length == prev_prefix_code_length) {
              num_prev_prefix_code_length++;
            } else {
              add_consecutive_prefix_codes_to_code_length_symbols();
              prev_prefix_code_length = prefix_code_length;
              num_prev_prefix_code_length = 1;
            }
          }
        };

    add_prefix_codes_to_code_length_symbols(
        literal_length_prefix_codes.begin(),
        literal_length_prefix_codes.begin() +
            num_leading_literal_length_prefix_codes);
    add_prefix_codes_to_code_length_symbols(
        distance_prefix_codes.begin(),
        distance_prefix_codes.begin() + num_leading_distance_prefix_codes);
    add_consecutive_prefix_codes_to_code_length_symbols();

    const auto code_length_lengths =
        package_merge(std::span<std::uint16_t, num_code_length_symbols>(
                          count_by_code_length_symbol.begin(),
                          count_by_code_length_symbol.end()),
                      maximum_code_length);
    const auto code_length_prefix_codes =
        prefix_codes(std::span<const std::uint8_t, num_code_length_symbols>(
            code_length_lengths));
    const auto reordered_code_length_prefix_codes =
        std::array<PrefixCode, num_code_length_symbols>{
            code_length_prefix_codes[16], code_length_prefix_codes[17],
            code_length_prefix_codes[18], code_length_prefix_codes[0],
            code_length_prefix_codes[8],  code_length_prefix_codes[7],
            code_length_prefix_codes[9],  code_length_prefix_codes[6],
            code_length_prefix_codes[10], code_length_prefix_codes[5],
            code_length_prefix_codes[11], code_length_prefix_codes[4],
            code_length_prefix_codes[12], code_length_prefix_codes[3],
            code_length_prefix_codes[13], code_length_prefix_codes[2],
            code_length_prefix_codes[14], code_length_prefix_codes[1],
            code_length_prefix_codes[15]};
    const auto num_leading_code_length_prefix_codes =
        count_leading_nonzero_prefix_codes(
            min_leading_code_length_prefix_codes, num_code_length_symbols,
            count_trailing_zero_length_prefix_codes(
                reordered_code_length_prefix_codes));

    buffered_out_.push_bits(num_leading_literal_length_prefix_codes -
                                min_leading_literal_length_prefix_codes,
                            literal_length_header_num_bits);
    buffered_out_.push_bits(num_leading_distance_prefix_codes -
                                min_leading_distance_prefix_codes,
                            distance_header_num_bits);
    buffered_out_.push_bits(num_leading_code_length_prefix_codes -
                                min_leading_code_length_prefix_codes,
                            code_length_header_num_bits);

    for (std::uint8_t i = 0; i < num_leading_code_length_prefix_codes; ++i) {
      buffered_out_.push_bits(reordered_code_length_prefix_codes.at(i).length,
                              code_length_num_bits);
    }

    for (const auto &symbol_or_symbol_with_offset : cl_symbols) {
      if (std::holds_alternative<CodeLengthSymbolWithOffset>(
              symbol_or_symbol_with_offset)) {
        const CodeLengthSymbolWithOffset &symbol_with_offset =
            std::get<CodeLengthSymbolWithOffset>(symbol_or_symbol_with_offset);
        buffered_out_.push_prefix_code(
            code_length_prefix_codes.at(symbol_with_offset.symbol));
        buffered_out_.push_bits(symbol_with_offset.offset.bits,
                                symbol_with_offset.offset.num_bits);
      } else {
        const std::uint8_t symbol =
            std::get<std::uint8_t>(symbol_or_symbol_with_offset);
        buffered_out_.push_prefix_code(code_length_prefix_codes.at(symbol));
      }
    }
  }

  auto flush_block() {
    const auto literal_length_prefix_code_lengths =
        package_merge(std::span<std::size_t, num_ll_symbols>(
                          count_by_symbol_.begin(),
                          count_by_symbol_.begin() + num_ll_symbols),
                      maximum_prefix_code_length);
    const auto distance_prefix_code_lengths = package_merge(
        std::span<std::size_t, num_distance_symbols>(
            count_by_symbol_.begin() + num_ll_symbols, count_by_symbol_.end()),
        maximum_prefix_code_length);
    const auto literal_length_prefix_codes =
        prefix_codes(std::span<const std::uint8_t, num_ll_symbols>(
            literal_length_prefix_code_lengths));
    const auto distance_prefix_codes =
        prefix_codes(std::span<const std::uint8_t, num_distance_symbols>(
            distance_prefix_code_lengths));
    flush_block_metadata(literal_length_prefix_codes, distance_prefix_codes);
    auto it = block_.begin();
    while (it != block_.end()) {
      auto symbol = std::get<Symbol>(*it++);
      if (symbol > eob) {
        // Is a back reference.
        auto length_prefix_code = literal_length_prefix_codes.at(symbol);
        auto length_offset = std::get<Offset>(*it++);
        auto distance_prefix_code =
            distance_prefix_codes.at(std::get<Symbol>(*it++) - num_ll_symbols);
        auto distance_offset = std::get<Offset>(*it++);
        const auto num_back_reference_bits =
            (length_prefix_code.length + length_offset.num_bits +
             distance_prefix_code.length + distance_offset.num_bits);

        const auto length = length_from_symbol_with_offset(
            {.symbol = symbol, .offset = length_offset});
        auto num_literal_bits = 0;
        for (auto literal_it = it; literal_it != it + length; ++literal_it) {
          const auto &prefix_code =
              literal_length_prefix_codes.at(std::get<Symbol>(*literal_it));
          if (prefix_code.length == 0) {
            // At least one literal does not have a prefix code, so we must use
            // the back reference. Since in cases of ties we prefer the back
            // reference, set the number of literal bits to the number of back
            // reference bits.
            num_literal_bits = num_back_reference_bits;
            break;
          }
          num_literal_bits += prefix_code.length;
        }
        if (num_literal_bits < num_back_reference_bits) {
          // The literals are more efficient than the back reference, so we use
          // the literals.
          for (auto i = 0; std::cmp_less(i, length); ++i) {
            buffered_out_.push_prefix_code(
                literal_length_prefix_codes.at(std::get<Symbol>(*it++)));
          }
        } else {
          // The back reference is more efficient than the literals, so we use
          // the back reference.
          buffered_out_.push_prefix_code(length_prefix_code);
          buffered_out_.push_offset(length_offset);
          buffered_out_.push_prefix_code(distance_prefix_code);
          buffered_out_.push_offset(distance_offset);
          it += length;
        }
      } else {
        // Is a literal.
        buffered_out_.push_prefix_code(literal_length_prefix_codes.at(symbol));
      }
    }
  }

  auto step() {
    if (lzss_.back_reference().length >= minimum_back_reference_length) {
      push_back_reference();
      lzss_.take_back_reference();
      return;
    }
    push_symbol(lzss_.literal());
    lzss_.take_literal();
  }
};

} // namespace block_type_2
