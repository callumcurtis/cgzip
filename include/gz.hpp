#pragma once

#include "third_party/output_stream.hpp"

namespace gz {

auto push_header(OutputBitStream&) -> void;
auto push_footer(OutputBitStream&, std::uint32_t, std::uint32_t) -> void;
}
