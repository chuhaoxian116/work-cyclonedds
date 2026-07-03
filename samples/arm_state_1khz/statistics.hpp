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

// 简单的微秒级统计容器。
// 测试期间只追加原始数值，所有排序都在实时循环结束后执行。
// 长时间测试会持续占用内存，生产监控可替换为固定大小直方图。
class MicrosecondStatistics
{
public:
  // 记录一个以微秒为单位的样本，并在线更新总和、最小值和最大值。
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

  // 百分位计算使用 nearest-rank 风格：复制并排序全部样本。
  // 该函数只在最终汇总时调用，不处于逐帧热路径。
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

  // 可用于复用统计对象；vector 保留容量，减少后续重复分配。
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
  // 输出格式保持固定，便于后续脚本直接解析 mean/P50/P90/P99/max。
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
