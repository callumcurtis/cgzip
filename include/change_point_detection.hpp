#pragma once

#include <algorithm>
#include <vector>
#include <cmath>
#include <utility>
#include <cstdint>

#include "size.hpp"

// References:
// https://staff.math.su.se/hoehle/pubs/hoehle2010-preprint.pdf
// https://sarem-seitz.com/posts/probabilistic-cusum-for-change-point-detection.html
// https://medium.com/@baw_H1/bayesian-approach-to-time-series-change-point-detection-613bf9376568
template <std::size_t N = (1U << size_of_in_bits<std::uint8_t>())>
class CusumDistributionDetector {
public:
    CusumDistributionDetector(int t_warmup, double h_threshold)
        : t_warmup_(t_warmup), h_threshold_(h_threshold) {}

    auto reset() -> void {
        current_t_ = 0;
        current_obs_count_ = 0;
        cusum_statistic_ = 0.0;

        std::ranges::fill(baseline_counts_, 0.0);
        std::ranges::fill(baseline_probs_, 0.0);
        std::ranges::fill(current_counts_, 0.0);
    }

    auto step(int y) -> std::pair<double, bool> {
        if (y < 0 || y >= N) {
            return std::make_pair(cusum_statistic_, false);
        }

        update_data(y);

        if (current_t_ == t_warmup_) {
            init_params();
        }

        if (current_t_ >= t_warmup_) {
            const std::pair<double, bool> result = check_for_changepoint(y);
            const double current_cusum = result.first;
            const bool is_changepoint = result.second;

            if (is_changepoint) {
                reset();
            }

            return std::make_pair(current_cusum, is_changepoint);
        }

        return std::make_pair(0.0, false);
    }

private:
    int t_warmup_;
    double h_threshold_;

    int current_t_{0};
    int current_obs_count_{0};
    double cusum_statistic_{0.0};

    std::vector<double> baseline_counts_ = std::vector<double>(N);
    std::vector<double> baseline_probs_ = std::vector<double>(N);

    std::vector<double> current_counts_ = std::vector<double>(N);

    auto update_data(int y) -> void {
        current_t_++;
        current_counts_[y] += 1.0;
        current_obs_count_++;
    }

    auto init_params() -> void {
        if (current_obs_count_ == 0) {
            return;
        }

        double sum_counts = 0.0;
        for (int i = 0; i < N; ++i) {
            baseline_counts_[i] = current_counts_[i];
            sum_counts += baseline_counts_[i];
        }

        for (int i = 0; i < N; ++i) {
            baseline_probs_[i] = (baseline_counts_[i] + 1.0) / (sum_counts + N);
        }

        std::ranges::fill(current_counts_, 0.0);
        current_obs_count_ = 0;
    }

    auto check_for_changepoint(int y) -> std::pair<double, bool> {
        const double current_sum_counts = current_obs_count_;

        if (current_sum_counts == 0) {
            return std::make_pair(0.0, false);
        }

        const double p1_y = (current_counts_[y] + 1.0) / (current_sum_counts + N);
        const double p0_y = baseline_probs_[y];
        const double llr_t = std::log(p1_y) - std::log(p0_y);

        cusum_statistic_ = std::max(0.0, cusum_statistic_ + llr_t);

        const bool is_changepoint = cusum_statistic_ > h_threshold_;

        return std::make_pair(cusum_statistic_, is_changepoint);
    }
};
