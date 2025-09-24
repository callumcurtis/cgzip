#pragma once

#include <span>
#include <algorithm>
#include <vector>

using length_size_t = int;
using weights_size_t = int;

template <typename T>
struct Package {
  T weight;
  std::vector<weights_size_t> indices;
};

template <typename T>
auto comparePackages(const Package<T>& a, const Package<T>& b) -> bool {
  return a.weight < b.weight;
}

template <typename T, weights_size_t N>
auto package_merge(std::span<T, N> weights, length_size_t max_length) -> std::array<length_size_t, N> {
  // https://people.eng.unimelb.edu.au/ammoffat/abstracts/compsurv19moffat.pdf
  std::vector<std::vector<Package<T>>> packages_by_level{max_length};
  for (weights_size_t i = 0; i < N; ++i) {
    packages_by_level.at(0).push_back({weights[i], {i}});
  }
  std::sort(packages_by_level.at(0).begin(), packages_by_level.at(0).end(), comparePackages<T>);
  for (length_size_t level = 1; level < max_length; ++level) {
    std::vector<Package<T>>& curr_packages = packages_by_level.at(level);
    std::vector<Package<T>>& prev_packages = packages_by_level.at(level-1);
    for (weights_size_t first = 0; first + 1 < prev_packages.size(); first += 2) {
      const Package<T>& first_package = prev_packages.at(first);
      const Package<T>& second_package = prev_packages.at(first+1);
      curr_packages.emplace_back(Package{
        first_package.weight + second_package.weight,
        {}
      });
      Package<T>& merged_package = curr_packages.back();
      merged_package.indices.insert(merged_package.indices.end(), first_package.indices.begin(), first_package.indices.end());
      merged_package.indices.insert(merged_package.indices.end(), second_package.indices.begin(), second_package.indices.end());
    }
    curr_packages.insert(curr_packages.end(), packages_by_level.at(0).begin(), packages_by_level.at(0).end());
    std::sort(curr_packages.begin(), curr_packages.end(), comparePackages<T>);
  }
  std::vector<Package<T>>& packages_at_last_level = packages_by_level.at(max_length-1);
  std::vector<std::vector<Package<T>>> solution_by_level{max_length};
  solution_by_level.at(max_length-1) = {packages_at_last_level.begin(), packages_at_last_level.begin() + (2 * weights.size()) - 2};
  for (length_size_t level = max_length-1; level-- > 0;) {
    weights_size_t num_multi_item_packages_in_prev_layer = 0;
    for (auto package : solution_by_level.at(level+1)) {
      if (package.indices.size() <= 1) {
        continue;
      }
      num_multi_item_packages_in_prev_layer += 1;
    };
    solution_by_level.at(level) = {packages_by_level.at(level).begin(), packages_by_level.at(level).begin() + (2 * num_multi_item_packages_in_prev_layer)};
  }
  std::array<length_size_t, N> lengths{};
  for (length_size_t level = max_length; level-- > 0;) {
    for (auto package : solution_by_level.at(level)) {
      if (package.indices.size() > 1) {
        continue;
      }
      lengths.at(package.indices.at(0))++;
    }
  }
  return lengths;
}

