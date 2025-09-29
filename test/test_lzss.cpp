#include <catch2/catch_all.hpp>

#include "lzss.hpp"

TEST_CASE("lzss basic matching") {

    SECTION("Throws when lookahead is empty") {
        std::vector<char> lookback = {'a', 'b', 'c'};
        std::vector<char> lookahead = {};
        REQUIRE_THROWS_AS(lzss(lookback, lookahead), std::invalid_argument);
    }

    SECTION("No match, default is distance 0, length 0") {
        std::vector<char> lookback = {'a', 'b', 'c'};
        std::vector<char> lookahead = {'d', 'e', 'f'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 0);
        REQUIRE(result.length == 0);
    }

    SECTION("Single character match") {
        std::vector<char> lookback = {'a', 'b', 'c'};
        std::vector<char> lookahead = {'c', 'd', 'e'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 1);
        REQUIRE(result.length == 1);
    }

    SECTION("Match at the start of lookback buffer") {
        std::vector<char> lookback = {'a', 'b', 'c', 'd'};
        std::vector<char> lookahead = {'a', 'b', 'x'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 4);
        REQUIRE(result.length == 2);
    }

    SECTION("Match a substring within the lookback buffer") {
        std::vector<char> lookback = {'x', 'y', 'a', 'b', 'c', 'z'};
        std::vector<char> lookahead = {'a', 'b', 'c', 'd'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 4);
        REQUIRE(result.length == 3);
    }

    SECTION("Longest match takes precedence over shorter match") {
        std::vector<char> lookback = {'a', 'b', 'c', 'a', 'b', 'c', 'd'};
        std::vector<char> lookahead = {'a', 'b', 'c', 'd', 'e'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 4);
        REQUIRE(result.length == 4);
    }

    SECTION("Closest longest match takes precedence (Lempel-Ziv 77 behavior)") {
        std::vector<char> lookback = {'a', 'b', 'a', 'b'};
        std::vector<char> lookahead = {'a', 'b', 'c'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 2);
        REQUIRE(result.length == 2);
    }

    SECTION("Lookahead longer than lookback portion for the match") {
        std::vector<char> lookback = {'1', '2', '3', '4'};
        std::vector<char> lookahead = {'3', '4', '5', '6'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 2);
        REQUIRE(result.length == 2);
    }

    SECTION("Empty lookback buffer") {
        std::vector<char> lookback = {};
        std::vector<char> lookahead = {'a', 'b', 'c'};
        auto result = lzss(lookback, lookahead);
        REQUIRE(result.distance == 0);
        REQUIRE(result.length == 0);
    }

    SECTION("Overlapping with future") {
      std::vector<char> lookback = {'y', 'f', 'a', 'b', 'c'};
      std::vector<char> lookahead = {'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'f', 'g'};
      auto result = lzss(lookback, lookahead);
      REQUIRE(result.distance == 3);
      REQUIRE(result.length == 12);
    }

    SECTION("RLE") {
      std::vector<char> lookback = {'y', 'f', 'a'};
      std::vector<char> lookahead = {'a','a', 'a', 'a', 'a', 'a', 'a', 'c', 'd'};
      auto result = lzss(lookback, lookahead);
      REQUIRE(result.distance == 1);
      REQUIRE(result.length == 7);
    }
}
