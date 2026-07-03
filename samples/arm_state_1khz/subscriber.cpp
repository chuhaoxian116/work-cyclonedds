#include "ArmState_.hpp"
#include "common.hpp"
#include "statistics.hpp"

#include "dds/dds.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t kMaxRecordedAnomalies = 128;
constexpr std::int64_t kMaximumWaitMicroseconds = 100000;

struct ReceiveAnomaly
{
  std::uint64_t sequence;
  std::uint64_t sequence_gap;
  double interval_us;
  double take_cost_us;
};

}  // namespace

int main(int argc, char **argv)
{
  try {
    // Subscriber 使用与 Publisher 相同的周期和 Deadline 参数。
    const arm_state_demo::Options options =
        arm_state_demo::parse_options(argc, argv);
    const auto duration = std::chrono::seconds(options.duration_seconds);

    // Topic 名称和数据类型必须与 Publisher 完全一致。
    dds::domain::DomainParticipant participant(
        org::eclipse::cyclonedds::domain::default_id());
    const auto topic_qos =
        arm_state_demo::make_topic_qos(participant, options);
    dds::topic::Topic<myproject::msg::dds_::ArmState_> topic(
        participant, arm_state_demo::kTopicName, topic_qos);
    // Reader QoS 单独配置；Topic QoS 不能代替 Reader QoS。
    dds::sub::Subscriber subscriber(participant);
    const auto reader_qos =
        arm_state_demo::make_reader_qos(subscriber, options);
    dds::sub::DataReader<myproject::msg::dds_::ArmState_> reader(
        subscriber, topic, reader_qos);

    // ReadCondition 仅在有未读且有效的数据时触发。
    // WaitSet 让线程阻塞等待 DDS 事件，避免 20 ms 轮询带来的延迟。
    dds::sub::cond::ReadCondition read_condition(
        reader, dds::sub::status::DataState::new_data());
    dds::core::cond::WaitSet waitset;
    waitset += read_condition;

    std::cout << "[subscriber] topic=" << arm_state_demo::kTopicName
              << " expected_period=" << options.period_us << "us"
              << " measurement_duration=" << options.duration_seconds << "s"
              << " qos=BestEffort/Volatile/KeepLast(1)"
              << " deadline=" << options.deadline_ms << "ms"
              << " lifespan=" << options.lifespan_ms
              << "ms (enforced by writer)"
              << std::endl;
    std::cout << "[subscriber] waiting for first sample..." << std::endl;

    // started=false 时不启动测量倒计时，确保测试时长从第一帧开始计算。
    bool started = false;
    // 序号统计用于区分丢帧、重复帧和乱序帧。
    std::uint64_t active_session = 0;
    std::uint64_t last_sequence = 0;
    std::uint64_t received = 0;
    std::uint64_t sequence_gaps = 0;
    std::uint64_t duplicates = 0;
    std::uint64_t out_of_order = 0;
    std::uint64_t session_changes = 0;
    // 接收间隔和抖动均使用 Subscriber 本机 steady_clock 计算。
    // 这表示“应用可见的到达周期”，不是跨主机单向网络延迟。
    auto first_receive = std::chrono::steady_clock::time_point();
    auto previous_receive = std::chrono::steady_clock::time_point();
    auto stop = std::chrono::steady_clock::time_point::max();
    arm_state_demo::MicrosecondStatistics interval_total;
    arm_state_demo::MicrosecondStatistics jitter_total;
    const std::size_t expected_samples =
        arm_state_demo::expected_sample_count(options);

    // 在收到第一帧之前完成内存分配，避免接收期间因 Vector 扩容停顿。
    interval_total.reserve(expected_samples);
    jitter_total.reserve(expected_samples);

    std::vector<ReceiveAnomaly> anomalies;
    anomalies.reserve(kMaxRecordedAnomalies);
    std::uint64_t dropped_anomalies = 0;
    double take_cost_sum_us = 0.0;
    double take_cost_max_us = 0.0;
    std::uint64_t take_calls = 0;

    while (true) {
      const auto before_wait = std::chrono::steady_clock::now();
      if (started && before_wait >= stop) {
        break;
      }

      // 首帧前最多等待 100 ms；开始测量后，最后一次等待不会超过剩余时间。
      std::int64_t wait_microseconds = kMaximumWaitMicroseconds;
      if (started) {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::microseconds>(
                stop - before_wait).count();
        wait_microseconds =
            std::max<std::int64_t>(
                1, std::min(kMaximumWaitMicroseconds, remaining));
      }

      try {
        waitset.wait(
            dds::core::Duration::from_microsecs(wait_microseconds));
      } catch (const dds::core::TimeoutError &) {
        continue;
      }

      // take() 取出并移除当前 Reader 缓存中的所有可用样本。
      // KeepLast(1) 下通常只有最新一帧。
      const auto take_begin = std::chrono::steady_clock::now();
      const auto samples = reader.take();
      const auto take_end = std::chrono::steady_clock::now();
      const double take_cost_us =
          std::chrono::duration<double, std::micro>(
              take_end - take_begin).count();
      take_cost_sum_us += take_cost_us;
      take_cost_max_us = std::max(take_cost_max_us, take_cost_us);
      ++take_calls;

      for (const auto &sample : samples) {
        if (!sample.info().valid()) {
          continue;
        }

        // 在应用真正取到样本的位置记录接收时间。
        const auto receive_time = std::chrono::steady_clock::now();
        const auto &message = sample.data();

        if (!started) {
          // 第一帧建立会话、序号和测量时间基准，本身没有前一帧可比较。
          started = true;
          active_session = message.session_id_();
          last_sequence = message.sequence_();
          first_receive = receive_time;
          previous_receive = receive_time;
          stop = first_receive + duration;
          ++received;
          std::cout << "[subscriber] first_sample"
                    << " session=" << active_session
                    << " sequence=" << last_sequence
                    << std::endl;
          continue;
        }

        if (message.session_id_() != active_session) {
          // Publisher 重启后会话 ID 改变。此时重置序号基准，
          // 避免把新会话的序号 1 错判为乱序。
          ++session_changes;
          active_session = message.session_id_();
          last_sequence = message.sequence_();
          previous_receive = receive_time;
          ++received;
          continue;
        }

        std::uint64_t gap_this_sample = 0;

        // 对同一会话进行序号连续性检查。
        if (message.sequence_() == last_sequence) {
          ++duplicates;
        } else if (message.sequence_() < last_sequence) {
          ++out_of_order;
        } else {
          if (message.sequence_() > last_sequence + 1) {
            // sequence 的缺口既可能来自网络丢包/覆盖，
            // 也可能来自 Publisher 主动跳过已经错失的调度周期。
            gap_this_sample = message.sequence_() - last_sequence - 1;
            sequence_gaps += gap_this_sample;
          }
          last_sequence = message.sequence_();
        }

        // interval_us 是相邻两次应用接收的时间差。
        const double interval_us =
            std::chrono::duration<double, std::micro>(
                receive_time - previous_receive).count();
        previous_receive = receive_time;
        // 使用绝对偏差表示抖动：|实际接收间隔 - 期望周期|。
        const double jitter_us =
            std::abs(interval_us - static_cast<double>(options.period_us));
        interval_total.record(interval_us);
        jitter_total.record(jitter_us);
        ++received;

        const bool is_anomaly =
            gap_this_sample > 0 ||
            interval_us >=
                static_cast<double>(options.deadline_ms) * 1000.0 ||
            take_cost_us >= 500.0;
        if (is_anomaly) {
          if (anomalies.size() < kMaxRecordedAnomalies) {
            anomalies.push_back(ReceiveAnomaly{
                message.sequence_(),
                gap_this_sample,
                interval_us,
                take_cost_us});
          } else {
            ++dropped_anomalies;
          }
        }
      }
    }

    // 所有百分位排序和日志输出均在接收循环结束后执行。
    // Deadline Miss、Sample Lost 和 Sample Rejected 是 DDS 层状态；
    // sequence_gaps 等是应用层统计，两类数据需要结合判断。
    const auto deadline_status = reader.requested_deadline_missed_status();
    const auto lost_status = reader.sample_lost_status();
    const auto rejected_status = reader.sample_rejected_status();
    const double elapsed =
        std::chrono::duration<double>(
            std::chrono::steady_clock::now() - first_receive).count();
    arm_state_demo::print_statistics(
        std::cout, "subscriber interval total", elapsed, interval_total);
    arm_state_demo::print_statistics(
        std::cout, "subscriber absolute jitter total", elapsed, jitter_total);
    std::cout << "[subscriber summary]"
              << " received=" << received
              << " sequence_gaps=" << sequence_gaps
              << " duplicates=" << duplicates
              << " out_of_order=" << out_of_order
              << " session_changes=" << session_changes
              << " requested_deadline_missed=" << deadline_status.total_count()
              << " sample_lost=" << lost_status.total_count()
              << " sample_rejected=" << rejected_status.total_count()
              << " take_cost_mean="
              << (take_cost_sum_us / static_cast<double>(take_calls))
              << "us"
              << " take_cost_max=" << take_cost_max_us << "us"
              << " anomalies=" << anomalies.size()
              << " dropped_anomalies=" << dropped_anomalies
              << std::endl;

    for (const auto &anomaly : anomalies) {
      std::cout << "[subscriber anomaly]"
                << " sequence=" << anomaly.sequence
                << " sequence_gap=" << anomaly.sequence_gap
                << " interval=" << anomaly.interval_us << "us"
                << " take=" << anomaly.take_cost_us << "us"
                << std::endl;
    }
  } catch (const dds::core::Exception &error) {
    std::cerr << "[subscriber] DDS error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << "Usage: " << argv[0]
              << " [duration-seconds] [period-us] [deadline-ms] [lifespan-ms]\n"
              << "[subscriber] Error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
