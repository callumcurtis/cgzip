#pragma once

#include <vector>
#include <cmath>
#include <utility>
#include <algorithm>

constexpr int NUM_CLASSES = 256;

// References:
// https://staff.math.su.se/hoehle/pubs/hoehle2010-preprint.pdf
// https://sarem-seitz.com/posts/probabilistic-cusum-for-change-point-detection.html
// https://medium.com/@baw_H1/bayesian-approach-to-time-series-change-point-detection-613bf9376568
class CusumDistributionDetector {
public:
    CusumDistributionDetector(int t_warmup, double h_threshold)
        : _t_warmup(t_warmup), _h_threshold(h_threshold) {
        reset();
    }

    void reset() {
        current_t = 0;
        current_obs_count = 0;

        // CUSUM state variable: S_t = max(0, S_{t-1} + LLR_t - drift_k)
        cusum_statistic = 0.0;

        std::fill(baseline_counts.begin(), baseline_counts.end(), 0.0);
        std::fill(baseline_probs.begin(), baseline_probs.end(), 0.0);
        std::fill(current_counts.begin(), current_counts.end(), 0.0);
    }

    std::pair<double, bool> step(int y) {
        if (y < 0 || y >= NUM_CLASSES) {
            return std::make_pair(cusum_statistic, false);
        }

        _update_data(y);

        if (current_t == _t_warmup) {
            _init_params();
        }

        if (current_t >= _t_warmup) {
            std::pair<double, bool> result = _check_for_changepoint(y);
            double current_cusum = result.first;
            bool is_changepoint = result.second;

            if (is_changepoint) {
                reset();
            }
            return std::make_pair(current_cusum, is_changepoint);
        } else {
            return std::make_pair(0.0, false);
        }
    }

private:
    int _t_warmup;
    double _h_threshold;

    int current_t;
    int current_obs_count;
    double cusum_statistic;

    std::vector<double> baseline_counts = std::vector<double>(NUM_CLASSES);
    std::vector<double> baseline_probs = std::vector<double>(NUM_CLASSES);

    std::vector<double> current_counts = std::vector<double>(NUM_CLASSES);

    void _update_data(int y) {
        current_t++;
        current_counts[y] += 1.0;
        current_obs_count++;
    }

    void _init_params() {
        if (current_obs_count == 0) return;

        double sum_counts = 0.0;
        for (int i = 0; i < NUM_CLASSES; ++i) {
            baseline_counts[i] = current_counts[i];
            sum_counts += baseline_counts[i];
        }

        for (int i = 0; i < NUM_CLASSES; ++i) {
            baseline_probs[i] = (baseline_counts[i] + 1.0) / (sum_counts + NUM_CLASSES);
        }

        std::fill(current_counts.begin(), current_counts.end(), 0.0);
        current_obs_count = 0;
    }

    std::pair<double, bool> _check_for_changepoint(int y) {
        double current_sum_counts = current_obs_count;

        if (current_sum_counts == 0) {
            return std::make_pair(0.0, false);
        }

        double p1_y = (current_counts[y] + 1.0) / (current_sum_counts + NUM_CLASSES);

        double p0_y = baseline_probs[y];

        double llr_t = std::log(p1_y) - std::log(p0_y);

        cusum_statistic = std::max(0.0, cusum_statistic + llr_t);

        bool is_changepoint = cusum_statistic > _h_threshold;

        return std::make_pair(cusum_statistic, is_changepoint);
    }
};
