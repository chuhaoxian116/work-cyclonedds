#ifndef MYPROJECT_SAMPLES_ARM_STATE_1KHZ_COMMON_HPP_
#define MYPROJECT_SAMPLES_ARM_STATE_1KHZ_COMMON_HPP_

#include "dds/dds.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace arm_state_demo {

// DDS Topic 名称。rt/ 前缀沿用 ROS 2 在 DDS 层的数据 Topic 命名习惯。
constexpr const char *kTopicName = "rt/arm/state_1khz";
constexpr std::uint64_t kNanosecondsPerMillisecond = 1000000ULL;

// Publisher 与 Subscriber 共用的命令行参数。
//
// 默认含义：
//   duration_seconds = 60：测试持续 60 秒
//   period_us = 1000：每 1 ms 发送一次，即 1 kHz
//   deadline_ms = 2：连续 2 ms 没有更新时产生 Deadline Miss 状态
//   lifespan_ms = 5：样本发布超过 5 ms 后视为过期
//
// Deadline 和 Lifespan 都不会让接收端等待，它们分别是“更新周期监控”
// 和“样本最大有效年龄”。
struct Options
{
  int duration_seconds = 60;
  int period_us = 1000;
  int deadline_ms = 2;
  int lifespan_ms = 5;
};

inline int parse_positive(const char *text, const char *name)
{
  // 所有时间参数必须为正数，避免 0 周期忙循环等无效配置。
  const int value = std::stoi(text);
  if (value <= 0) {
    throw std::invalid_argument(std::string(name) + " must be positive");
  }
  return value;
}

inline Options parse_options(int argc, char **argv)
{
  // 参数顺序：
  //   [运行秒数] [发送周期 us] [Deadline ms] [Lifespan ms]
  Options options;
  if (argc > 1) {
    options.duration_seconds = parse_positive(argv[1], "duration");
  }
  if (argc > 2) {
    options.period_us = parse_positive(argv[2], "period");
  }
  if (argc > 3) {
    options.deadline_ms = parse_positive(argv[3], "deadline");
  }
  if (argc > 4) {
    options.lifespan_ms = parse_positive(argv[4], "lifespan");
  }
  if (argc > 5) {
    throw std::invalid_argument("too many arguments");
  }
  return options;
}

inline std::uint64_t monotonic_time_ns()
{
  // steady_clock 不受系统时间校准或人工修改影响，适合测量周期和时间差。
  // 它在不同主机之间没有共同时间原点，不能直接用于计算单向网络延迟。
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline dds::topic::qos::TopicQos make_topic_qos(
    dds::domain::DomainParticipant &participant,
    const Options &options)
{
  // Topic QoS 用于统一声明这类数据的公共策略。
  // 真正控制发送、接收行为的是下方分别创建的 Writer QoS 和 Reader QoS；
  // 仅设置 Topic QoS 并不能代替端点 QoS。
  dds::topic::qos::TopicQos qos = participant.default_topic_qos();

  // BestEffort：状态流优先追求低延迟和新鲜度，不重传已经失去价值的旧状态。
  // Volatile：Reader 加入之前的数据不补发，避免启动后收到旧机械臂状态。
  // KeepLast(1)：每个实例只保留最新状态，Reader 处理慢时允许跳过中间帧。
  // ResourceLimits(1,1,1)：类型无 Key，只有一个实例且最多缓存一个样本。
  qos << dds::core::policy::Reliability::BestEffort()
      << dds::core::policy::Durability::Volatile()
      << dds::core::policy::History::KeepLast(1)
      << dds::core::policy::ResourceLimits(1, 1, 1)
      // Deadline 是周期性更新的监控阈值，不会延迟数据交付。
      << dds::core::policy::Deadline(
             dds::core::Duration::from_millisecs(options.deadline_ms))
      // Lifespan 由 Writer 执行：超过该年龄的样本不再作为当前状态交付。
      << dds::core::policy::Lifespan(
             dds::core::Duration::from_millisecs(options.lifespan_ms));
  return qos;
}

inline dds::pub::qos::DataWriterQos make_writer_qos(
    dds::pub::Publisher &publisher,
    const Options &options)
{
  // Writer 提供的 QoS。Reader 的要求必须与 Writer 提供的能力兼容。
  dds::pub::qos::DataWriterQos qos = publisher.default_datawriter_qos();
  qos << dds::core::policy::Reliability::BestEffort()
      << dds::core::policy::Durability::Volatile()
      << dds::core::policy::History::KeepLast(1)
      << dds::core::policy::ResourceLimits(1, 1, 1)
      << dds::core::policy::Deadline(
             dds::core::Duration::from_millisecs(options.deadline_ms))
      << dds::core::policy::Lifespan(
             dds::core::Duration::from_millisecs(options.lifespan_ms));
  return qos;
}

inline dds::sub::qos::DataReaderQos make_reader_qos(
    dds::sub::Subscriber &subscriber,
    const Options &options)
{
  // Reader 端不配置 Lifespan，因为 Lifespan 是 Writer/Topic 侧策略。
  // Reader 通过 Deadline 监控数据是否按预期周期持续到达。
  dds::sub::qos::DataReaderQos qos = subscriber.default_datareader_qos();
  qos << dds::core::policy::Reliability::BestEffort()
      << dds::core::policy::Durability::Volatile()
      << dds::core::policy::History::KeepLast(1)
      << dds::core::policy::ResourceLimits(1, 1, 1)
      << dds::core::policy::Deadline(
             dds::core::Duration::from_millisecs(options.deadline_ms));
  return qos;
}

}  // namespace arm_state_demo

#endif  // MYPROJECT_SAMPLES_ARM_STATE_1KHZ_COMMON_HPP_
