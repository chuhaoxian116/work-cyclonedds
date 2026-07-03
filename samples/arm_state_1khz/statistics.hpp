#ifndef MYPROJECT_SAMPLES_ARM_STATE_1KHZ_STATISTICS_HPP_
#define MYPROJECT_SAMPLES_ARM_STATE_1KHZ_STATISTICS_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

namespace arm_state_demo {

class MicrosecondStatistics
{
public:
  void record(double value_us)
  {
    values_.push_back(value_us);
    sum_ += value_us;
    min_ = std::min(min_, value_us);
    max_ = std::max(max_, value_us);
  }

  std::size_t count() const
  {
    return values_.size();
  }

  double mean() const
  {
    return values_.empty() ? 0.0 : sum_ / static_cast<double>(values_.size());
  }

  double min() const
  {
    return values_.empty() ? 0.0 : min_;
  }

  double max() const
  {
    return values_.empty() ? 0.0 : max_;
  }

  double percentile(double percentile_value) const
  {
    if (values_.empty()) {
      return 0.0;
    }

    std::vector<double> sorted(values_);
    std::sort(sorted.begin(), sorted.end());
    const double position =
        percentile_value * static_cast<double>(sorted.size() - 1);
    const std::size_t index =
        static_cast<std::size_t>(std::ceil(position));
    return sorted[index];
  }

  void clear()
  {
    values_.clear();
    sum_ = 0.0;
    min_ = std::numeric_limits<double>::infinity();
    max_ = 0.0;
  }

private:
  std::vector<double> values_;
  double sum_ = 0.0;
  double min_ = std::numeric_limits<double>::infinity();
  double max_ = 0.0;
};

inline void print_statistics(
    std::ostream &output,
    const std::string &label,
    double elapsed_seconds,
    const MicrosecondStatistics &statistics)
{
  output << std::fixed << std::setprecision(3)
         << "[" << label << "] elapsed=" << elapsed_seconds << "s"
         << " count=" << statistics.count()
         << " mean=" << statistics.mean() << "us"
         << " min=" << statistics.min() << "us"
         << " p50=" << statistics.percentile(0.50) << "us"
         << " p90=" << statistics.percentile(0.90) << "us"
         << " p99=" << statistics.percentile(0.99) << "us"
         << " max=" << statistics.max() << "us"
         << std::endl;
}

}  // namespace arm_state_demo

#endif  // MYPROJECT_SAMPLES_ARM_STATE_1KHZ_STATISTICS_HPP_
