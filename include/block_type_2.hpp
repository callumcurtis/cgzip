#pragma once

#include "third_party/output_stream.hpp"

#include "package_merge.hpp"
#include "lzss.hpp"
#include "types.hpp"

template <std::size_t LookBackSize = maximum_look_back_size, std::size_t LookAheadSize = maximum_look_ahead_size>
class BlockType2Stream {
private:
  BufferedOutputBitStream out_;
  Lzss<LookBackSize, LookAheadSize> lzss_;
  std::array<std::size_t, num_ll_symbols + num_distance_symbols> count_by_symbol{};
  std::vector<std::variant<Symbol, Offset>> block;

public:
  explicit BlockType2Stream(OutputBitStream& output_bit_stream) : out_{BufferedOutputBitStream(output_bit_stream)} {}

  [[nodiscard]] auto bits() -> std::size_t {
    buffer_all_except_last_block_flag();
    return out_.bits() + 1;
  }

  auto reset() {
    count_by_symbol.fill(0);
    block.clear();
    out_.reset();
  }

  auto put(std::uint8_t byte) {
    lzss_.put(byte);
    if (!lzss_.is_full()) {
      return;
    }
    step();
  }

  auto commit(bool is_last) {
    buffer_all_except_last_block_flag();
    out_.unbuffered().push_bit(is_last ? 1 : 0);
    out_.commit();
    reset();
  }

private:

  auto buffer_all_except_last_block_flag() {
    if (out_.bits() > 0) {
      // We have already buffered
      return;
    }

    out_.push_bits(2, 2); // Two bit block type (in this case, block type 2)

    // TODO: allow look-ahead into next block
    while (!lzss_.is_empty()) {
      step();
    }

    push_symbol(eob);

    flush_block();
  }

  auto push_symbol(const Symbol symbol) {
    count_by_symbol.at(symbol)++;
    block.emplace_back(symbol);
  }

  auto push_offset(const Offset& offset) {
    block.emplace_back(offset);
  }

  auto push_back_reference(const BackReference& back_reference) {
    const auto length_symbol_with_offset = symbol_with_offset_from_length(back_reference.length);
    const auto distance_symbol_with_offset = symbol_with_offset_from_distance(back_reference.distance);
    push_symbol(length_symbol_with_offset.symbol);
    push_offset(length_symbol_with_offset.offset);
    push_symbol(distance_symbol_with_offset.symbol + num_ll_symbols);
    push_offset(distance_symbol_with_offset.offset);
  }

  struct CodeLengthOffset {
    std::uint8_t bits;
    std::uint8_t num_bits;
  };

  struct CodeLengthSymbolWithOffset {
    std::uint8_t symbol;
    CodeLengthOffset offset;
  };

  auto code_length_symbols(
    const std::array<PrefixCode, num_ll_symbols>& literal_length_prefix_codes,
    const std::array<PrefixCode, num_distance_symbols>& distance_prefix_codes
  ) {
    auto count_trailing_zero_length_prefix_codes = [](auto &prefix_codes) {
      std::uint16_t count = 0;
      for (const auto& prefix_code : prefix_codes) {
        if (prefix_code.length > 0) {
          count = 0;
        } else {
          count++;
        }
      }
      return count;
    };

    auto count_leading_prefix_codes = [](const std::uint16_t min, const std::uint16_t max, const std::uint16_t trailing) {
      return std::max(min, std::uint16_t(max - trailing));
    };

    const auto num_leading_literal_length_prefix_codes = count_leading_prefix_codes(257, num_ll_symbols, count_trailing_zero_length_prefix_codes(literal_length_prefix_codes));
    const auto num_leading_distance_prefix_codes = count_leading_prefix_codes(1, num_distance_symbols, count_trailing_zero_length_prefix_codes(distance_prefix_codes));

    const std::uint8_t num_code_length_symbols = 19;

    std::array<std::uint16_t, num_code_length_symbols> count_by_code_length_symbol{};
    std::vector<std::variant<std::uint8_t, CodeLengthSymbolWithOffset>> cl_symbols;
    std::uint8_t prev_prefix_code_length = maximum_prefix_code_length+1;
    std::uint16_t num_prev_prefix_code_length = 0;

    auto flush_prefix_code_sequence_to_code_length_symbols_batched = [&prev_prefix_code_length, &num_prev_prefix_code_length, &cl_symbols, &count_by_code_length_symbol](
      const std::uint16_t min,
      const std::uint16_t max,
      const std::uint8_t num_bits,
      const std::uint8_t batch_symbol
    ) {
        while (num_prev_prefix_code_length >= min) {
          const std::uint16_t size = std::min(max, num_prev_prefix_code_length);
          num_prev_prefix_code_length -= size;
          count_by_code_length_symbol.at(batch_symbol)++;
          cl_symbols.emplace_back(CodeLengthSymbolWithOffset{
            .symbol = batch_symbol,
            .offset = CodeLengthOffset{
              .bits = std::uint8_t(size - min),
              .num_bits = num_bits
            }
          });
        }
        for (auto i = 0; i < num_prev_prefix_code_length; ++i) {
          cl_symbols.emplace_back(prev_prefix_code_length);
        }
        count_by_code_length_symbol.at(prev_prefix_code_length) += num_prev_prefix_code_length;
        num_prev_prefix_code_length = 0;
    };

    auto flush_prefix_code_sequence_to_code_length_symbols = [&prev_prefix_code_length, &num_prev_prefix_code_length, &cl_symbols, &count_by_code_length_symbol, &flush_prefix_code_sequence_to_code_length_symbols_batched]() {
      if (num_prev_prefix_code_length <= 0) {
        return;
      }

      if (prev_prefix_code_length == 0 && num_prev_prefix_code_length >= 11) {
        flush_prefix_code_sequence_to_code_length_symbols_batched(11, 138, 7, 18);
        return;
      }

      if (prev_prefix_code_length == 0 && num_prev_prefix_code_length >= 3) {
        flush_prefix_code_sequence_to_code_length_symbols_batched(3, 10, 3, 17);
        return;
      }

      cl_symbols.emplace_back(prev_prefix_code_length);
      count_by_code_length_symbol.at(prev_prefix_code_length)++;
      num_prev_prefix_code_length--;
      if (num_prev_prefix_code_length > 0) {
        flush_prefix_code_sequence_to_code_length_symbols_batched(3, 6, 2, 16);
      }
    };

    auto add_prefix_codes_to_code_length_symbols = [&prev_prefix_code_length, &num_prev_prefix_code_length, &flush_prefix_code_sequence_to_code_length_symbols](const auto start, const auto end) {
      for (auto prefix_code_ptr = start; prefix_code_ptr < end; ++prefix_code_ptr) {
        const auto prefix_code_length = prefix_code_ptr->length;
        if (prefix_code_length == prev_prefix_code_length) {
          num_prev_prefix_code_length++;
        } else {
          flush_prefix_code_sequence_to_code_length_symbols();
          prev_prefix_code_length = prefix_code_length;
          num_prev_prefix_code_length = 1;
        }
      }
    };

    add_prefix_codes_to_code_length_symbols(literal_length_prefix_codes.begin(), literal_length_prefix_codes.begin() + num_leading_literal_length_prefix_codes);
    add_prefix_codes_to_code_length_symbols(distance_prefix_codes.begin(), distance_prefix_codes.begin() + num_leading_distance_prefix_codes);
    flush_prefix_code_sequence_to_code_length_symbols();

    const std::uint8_t maximum_code_length = 7;
    const auto code_length_lengths = package_merge(std::span<std::uint16_t, num_code_length_symbols>(count_by_code_length_symbol.begin(), count_by_code_length_symbol.end()), maximum_code_length);
    const auto code_length_prefix_codes = prefix_codes(std::span<const std::uint8_t, num_code_length_symbols>(code_length_lengths));
    const auto reordered_code_length_prefix_codes = std::array<PrefixCode, num_code_length_symbols>{
      code_length_prefix_codes[16],
      code_length_prefix_codes[17],
      code_length_prefix_codes[18],
      code_length_prefix_codes[0],
      code_length_prefix_codes[8],
      code_length_prefix_codes[7],
      code_length_prefix_codes[9],
      code_length_prefix_codes[6],
      code_length_prefix_codes[10],
      code_length_prefix_codes[5],
      code_length_prefix_codes[11],
      code_length_prefix_codes[4],
      code_length_prefix_codes[12],
      code_length_prefix_codes[3],
      code_length_prefix_codes[13],
      code_length_prefix_codes[2],
      code_length_prefix_codes[14],
      code_length_prefix_codes[1],
      code_length_prefix_codes[15]
    };
    const auto num_leading_code_length_prefix_codes = count_leading_prefix_codes(4, num_code_length_symbols, count_trailing_zero_length_prefix_codes(reordered_code_length_prefix_codes));

    out_.push_bits(num_leading_literal_length_prefix_codes - 257, 5);
    out_.push_bits(num_leading_distance_prefix_codes - 1, 5);
    out_.push_bits(num_leading_code_length_prefix_codes - 4, 4);
    for (std::uint8_t i = 0; i < num_leading_code_length_prefix_codes; ++i) {
      out_.push_bits(reordered_code_length_prefix_codes.at(i).length, 3);
    }
    for (const auto& symbol_or_symbol_with_offset : cl_symbols) {
      if (std::holds_alternative<CodeLengthSymbolWithOffset>(symbol_or_symbol_with_offset)) {
        const CodeLengthSymbolWithOffset& symbol_with_offset = std::get<CodeLengthSymbolWithOffset>(symbol_or_symbol_with_offset);
        out_.push_prefix_code(code_length_prefix_codes.at(symbol_with_offset.symbol));
        out_.push_bits(symbol_with_offset.offset.bits, symbol_with_offset.offset.num_bits);
      } else {
        const std::uint8_t symbol = std::get<std::uint8_t>(symbol_or_symbol_with_offset);
        out_.push_prefix_code(code_length_prefix_codes.at(symbol));
      }
    }
  }

  auto flush_block() {
    const auto ll_lengths = package_merge(std::span<std::size_t, num_ll_symbols>(count_by_symbol.begin(), count_by_symbol.begin() + num_ll_symbols), maximum_prefix_code_length);
    const auto distance_lengths = package_merge(std::span<std::size_t, num_distance_symbols>(count_by_symbol.begin() + num_ll_symbols, count_by_symbol.end()), maximum_prefix_code_length);
    const auto ll_codes = prefix_codes(std::span<const std::uint8_t, num_ll_symbols>(ll_lengths));
    const auto distance_codes = prefix_codes(std::span<const std::uint8_t, num_distance_symbols>(distance_lengths));
    code_length_symbols(ll_codes, distance_codes);
    for (auto symbol_or_offset : block) {
      if (std::holds_alternative<Symbol>(symbol_or_offset)) {
        const Symbol& symbol = std::get<Symbol>(symbol_or_offset);
        if (symbol < num_ll_symbols) {
          out_.push_prefix_code(ll_codes.at(symbol));
        } else {
          out_.push_prefix_code(distance_codes.at(symbol - num_ll_symbols));
        }
      } else {
        const Offset& offset = std::get<Offset>(symbol_or_offset);
        out_.push_offset(offset);
      }
    }
  }

  auto step() {
    const auto backref = lzss_.back_reference();
    const auto byte = lzss_.literal();
    if (backref.length >= minimum_back_reference_length) {
      // TODO: check if back-reference is advantageous over just encoding the plain literal
      push_back_reference(backref);
      lzss_.take_back_reference();
      return;
    }
    push_symbol(byte);
    lzss_.take_literal();
  }
};

