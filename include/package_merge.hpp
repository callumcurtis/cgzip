#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>
#include <limits>

template <typename W>
struct Package {
  W weight;
  std::vector<std::uint16_t> indices;
};

template <typename W>
auto comparePackages(const Package<W>& a, const Package<W>& b) -> bool {
  return a.weight < b.weight;
}

template <std::size_t N, typename W = std::size_t>
auto package_merge(std::span<W, N> weights, std::uint8_t max_length) -> std::array<std::uint8_t, N> {
  // https://people.eng.unimelb.edu.au/ammoffat/abstracts/compsurv19moffat.pdf
  static_assert(N <= std::numeric_limits<std::uint16_t>::max());
  std::vector<std::vector<Package<W>>> packages_by_level(max_length);
  for (std::uint16_t i = 0; i < weights.size(); ++i) {
    if (weights[i] == 0) {
      continue;
    }
    packages_by_level.at(0).push_back({weights[i], {i}});
  }
  const std::uint16_t num_non_zero_weights = packages_by_level.at(0).size();
  std::ranges::sort(packages_by_level.at(0), comparePackages<W>);
  for (std::uint8_t level = 1; level < max_length; ++level) {
    std::vector<Package<W>>& curr_packages = packages_by_level.at(level);
    std::vector<Package<W>>& prev_packages = packages_by_level.at(level-1);
    for (std::uint16_t first = 0; first + 1 < prev_packages.size(); first += 2) {
      const Package<W>& first_package = prev_packages.at(first);
      const Package<W>& second_package = prev_packages.at(first+1);
      curr_packages.emplace_back(Package<W>{
        .weight=W(first_package.weight + second_package.weight),
        .indices={}
      });
      Package<W>& merged_package = curr_packages.back();
      merged_package.indices.insert(merged_package.indices.end(), first_package.indices.begin(), first_package.indices.end());
      merged_package.indices.insert(merged_package.indices.end(), second_package.indices.begin(), second_package.indices.end());
    }
    curr_packages.insert(curr_packages.end(), packages_by_level.at(0).begin(), packages_by_level.at(0).end());
    std::ranges::sort(curr_packages, comparePackages<W>);
  }
  std::vector<Package<W>>& packages_at_last_level = packages_by_level.at(max_length-1);
  std::vector<std::vector<Package<W>>> solution_by_level(max_length);
  solution_by_level.at(max_length-1) = {packages_at_last_level.begin(), packages_at_last_level.begin() + (2 * num_non_zero_weights) - 2};
  for (std::uint8_t level = max_length-1; level-- > 0;) {
    std::uint16_t num_multi_item_packages_in_prev_layer = 0;
    for (const auto& package : solution_by_level.at(level+1)) {
      if (package.indices.size() <= 1) {
        continue;
      }
      num_multi_item_packages_in_prev_layer++;
    };
    solution_by_level.at(level) = {packages_by_level.at(level).begin(), packages_by_level.at(level).begin() + (2 * num_multi_item_packages_in_prev_layer)};
  }
  std::array<std::uint8_t, N> lengths{};
  for (std::uint8_t level = max_length; level-- > 0;) {
    for (const auto& package : solution_by_level.at(level)) {
      if (package.indices.size() > 1) {
        continue;
      }
      lengths.at(package.indices.at(0))++;
    }
  }
  return lengths;
}

