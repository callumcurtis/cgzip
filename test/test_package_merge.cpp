#include <catch2/catch_all.hpp>
#include <cmath>

#include "package_merge.hpp"

template <typename Container> auto kraft_mcmillan(Container container) {
  double km = 0;
  for (auto value : container) {
    if (value == 0) {
      continue;
    }
    km += pow(2, -value);
  }
  return km;
}

TEST_CASE("package merge") {
  SECTION("throws if inadequate max length") {
    std::array<std::size_t, 17> weights = {1, 3, 2, 5, 8,  10, 12, 3, 5,
                                           7, 8, 2, 3, 67, 23, 5,  3}; // NOLINT
    REQUIRE_THROWS(package_merge<17, std::size_t>(weights, 1));
  }

  SECTION("kraft-mcmillan inequality") {
    std::array<std::size_t, 17> weights = {1, 3, 2, 5, 8,  10, 12, 3, 5,
                                           7, 8, 2, 3, 67, 23, 5,  3}; // NOLINT
    auto lengths = package_merge<17, std::size_t>(weights, 15);
    REQUIRE_THAT(kraft_mcmillan(lengths),
                 Catch::Matchers::WithinAbs(1.0, 1e-6));
  }

  SECTION("zero lengths for zero weights") {
    std::array<std::size_t, 5> weights = {1, 3, 0, 5, 0};
    auto lengths = package_merge<5, std::size_t>(weights, 15);
    REQUIRE(lengths[0] > 0);
    REQUIRE(lengths[1] > 0);
    REQUIRE(lengths[2] == 0);
    REQUIRE(lengths[3] > 0);
    REQUIRE(lengths[4] == 0);
    REQUIRE_THAT(kraft_mcmillan(lengths),
                 Catch::Matchers::WithinAbs(1.0, 1e-6));
  }

  SECTION("only one symbol") {
    std::array<std::size_t, 5> weights = {0, 0, 5, 0, 0};
    auto lengths = package_merge<5, std::size_t>(weights, 15);
    REQUIRE(lengths[0] == 0);
    REQUIRE(lengths[1] == 0);
    REQUIRE(lengths[2] == 1);
    REQUIRE(lengths[3] == 0);
    REQUIRE(lengths[4] == 0);
    REQUIRE_THAT(kraft_mcmillan(lengths),
                 Catch::Matchers::WithinAbs(0.5, 1e-6));
  }
}
