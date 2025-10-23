#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "size.hpp"

// References:
// https://staff.math.su.se/hoehle/pubs/hoehle2010-preprint.pdf
// https://sarem-seitz.com/posts/probabilistic-cusum-for-change-point-detection.html
// https://medium.com/@baw_H1/bayesian-approach-to-time-series-change-point-detection-613bf9376568
template <std::size_t N = (1U << size_of_in_bits<std::uint8_t>())>
class CusumDistributionDetector {
public:
  struct CusumDistributionDetectorParams {
    int warmup;
    double threshold;
  };

  explicit CusumDistributionDetector(CusumDistributionDetectorParams params)
      : warmup_steps_(params.warmup), threshold_(params.threshold) {}

  auto reset() -> void {
    current_step_ = 0;
    current_counts_total_ = 0;
    cusum_ = 0.0;

    std::ranges::fill(baseline_counts_, 0.0);
    std::ranges::fill(baseline_probs_, 0.0);
    std::ranges::fill(current_counts_, 0.0);
  }

  auto step(int y) -> bool {
    if (y < 0 || y >= N) {
      return false;
    }

    update_counters(y);

    if (current_step_ == warmup_steps_) {
      init_params();
    }

    if (current_step_ >= warmup_steps_) {
      update_cusum(y);

      const bool is_changepoint = cusum_ > threshold_;

      if (is_changepoint) {
        reset();
      }

      return is_changepoint;
    }

    return false;
  }

private:
  int warmup_steps_;
  double threshold_;

  int current_step_{0};
  int current_counts_total_{0};
  double cusum_{0.0};

  std::vector<double> baseline_counts_ = std::vector<double>(N);
  std::vector<double> baseline_probs_ = std::vector<double>(N);

  std::vector<double> current_counts_ = std::vector<double>(N);

  auto update_counters(int y) -> void {
    current_step_++;
    current_counts_[y] += 1.0;
    current_counts_total_++;
  }

  auto init_params() -> void {
    if (current_counts_total_ == 0) {
      return;
    }

    baseline_counts_.swap(current_counts_);

    for (int i = 0; i < N; ++i) {
      const auto count = baseline_counts_[i];
      baseline_probs_[i] = count > 0 ? count / current_counts_total_ : 1.0 / N;
    }

    current_counts_total_ = 0;
  }

  auto update_cusum(int y) -> void {
    const auto count = current_counts_[y];
    const double p1_y = count > 0 ? count / current_counts_total_ : 1.0 / N;
    const double p0_y = baseline_probs_[y];
    const double llr_t = std::log(p1_y) - std::log(p0_y);
    cusum_ = std::max(0.0, cusum_ + llr_t);
  }
};
