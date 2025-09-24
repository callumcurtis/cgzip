#include <catch2/catch_all.hpp>
#include <cmath>

#include "package_merge.hpp"

TEST_CASE("package merge") {
  SECTION("throws if inadequate max length") {
    std::array<int, 17> weights = {1, 3, 2, 5, 8, 10, 12, 3, 5, 7, 8, 2, 3, 67, 23, 5, 3}; // NOLINT
    REQUIRE_THROWS(package_merge<int, 17>(weights, 1));
  }

  SECTION("kraft-mcmillan inequality") {
    std::array<int, 17> weights = {1, 3, 2, 5, 8, 10, 12, 3, 5, 7, 8, 2, 3, 67, 23, 5, 3}; // NOLINT
    auto lengths = package_merge<int, 17>(weights, 15);
    double kmi = 0;
    for (auto length : lengths) {
      kmi += pow(2, -length);
    }
    REQUIRE_THAT(kmi, Catch::Matchers::WithinAbs(1.0, 1e-6));
  }
}

