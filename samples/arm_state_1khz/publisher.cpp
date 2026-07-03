#include "ArmState_.hpp"
#include "common.hpp"
#include "statistics.hpp"

#include "dds/dds.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace {

// 每次 Publisher 启动都生成新的会话 ID。
// 接收端可据此区分“同一进程内的连续序号”和“进程重启后的新序号”。
std::uint64_t make_session_id()
{
  const auto wall_clock = std::chrono::system_clock::now().time_since_epoch();
  const auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(wall_clock).count();
  return static_cast<std::uint64_t>(nanoseconds) ^
         (static_cast<std::uint64_t>(::getpid()) << 32U);
}

}  // namespace

int main(int argc, char **argv)
{
  try {
    // 解析测试时长、周期、Deadline 和 Lifespan。
    const arm_state_demo::Options options =
        arm_state_demo::parse_options(argc, argv);
    const auto period = std::chrono::microseconds(options.period_us);
    const auto duration = std::chrono::seconds(options.duration_seconds);

    // Participant 是当前进程在 DDS Domain 中的入口。
    dds::domain::DomainParticipant participant(
        org::eclipse::cyclonedds::domain::default_id());

    // Topic 只定义数据类型、名称和公共策略；Writer QoS 仍需单独配置。
    const auto topic_qos =
        arm_state_demo::make_topic_qos(participant, options);
    dds::topic::Topic<myproject::msg::dds_::ArmState_> topic(
        participant, arm_state_demo::kTopicName, topic_qos);
    // Publisher 是发布容器，DataWriter 才是真正写入 ArmState_ 数据的端点。
    dds::pub::Publisher publisher(participant);
    const auto writer_qos =
        arm_state_demo::make_writer_qos(publisher, options);
    dds::pub::DataWriter<myproject::msg::dds_::ArmState_> writer(
        publisher, topic, writer_qos);

    // 测试消息在循环外创建并复用，避免 1 kHz 循环内反复分配内存。
    myproject::msg::dds_::ArmState_ message;
    const std::uint64_t session_id = make_session_id();
    message.session_id_(session_id);
    // 固定填充 100 字节负载，使序列化后的消息大小接近 ddsperf 的 128 B。
    std::fill(message.payload_().begin(), message.payload_().end(), 0x5a);

    std::cout << "[publisher] topic=" << arm_state_demo::kTopicName
              << " rate=" << (1000000.0 / options.period_us) << "Hz"
              << " period=" << options.period_us << "us"
              << " duration=" << options.duration_seconds << "s"
              << " qos=BestEffort/Volatile/KeepLast(1)"
              << " deadline=" << options.deadline_ms << "ms"
              << " lifespan=" << options.lifespan_ms << "ms"
              << " session=" << session_id
              << std::endl;

    // scheduled_cycles：理论上应经历的 1 ms 周期数，同时作为消息序号。
    // samples_written：实际调用 writer.write() 的次数。
    // skipped_cycles：线程唤醒过晚，已经失去发送意义而跳过的周期数。
    std::uint64_t scheduled_cycles = 0;
    std::uint64_t samples_written = 0;
    std::uint64_t skipped_cycles = 0;
    // 运行期间只采集数据，不做排序和周期日志输出。
    // interval_total 和 lateness_total 在发送循环结束后统一汇总。
    arm_state_demo::MicrosecondStatistics interval_total;
    arm_state_demo::MicrosecondStatistics lateness_total;

    // steady_clock 是单调时钟，不受 NTP 或系统时间修改影响。
    const auto start = std::chrono::steady_clock::now();
    const auto stop = start + duration;
    auto next_release = start;
    auto previous_write = start;

    while (next_release < stop) {
      // 使用绝对释放时刻，而不是 sleep_for(1ms)。
      // sleep_for 会把每次调度误差累积到下一周期，长时间运行会发生漂移。
      next_release += period;
      ++scheduled_cycles;
      std::this_thread::sleep_until(next_release);

      auto now = std::chrono::steady_clock::now();
      if (now >= next_release + period) {
        // 如果醒来时已经错过一个或多个完整周期，不进行“追赶式连发”。
        // 对机械臂最新状态而言，旧周期已经失效；跳过它们并保留序号缺口，
        // 让 Subscriber 能明确统计发送端错过了多少周期。
        const auto behind = now - next_release;
        const auto skipped =
            static_cast<std::uint64_t>(behind / period);
        skipped_cycles += skipped;
        scheduled_cycles += skipped;
        next_release += period * skipped;
      }

      now = std::chrono::steady_clock::now();
      // lateness_us：实际执行时刻相对计划释放时刻晚了多久。
      const double lateness_us =
          std::chrono::duration<double, std::micro>(now - next_release).count();
      // interval_us：相邻两次实际 write() 之间的时间差。
      const double interval_us =
          std::chrono::duration<double, std::micro>(now - previous_write).count();
      previous_write = now;

      // sequence 使用理论周期号；跳过周期后会产生序号缺口。
      message.sequence_(scheduled_cycles);

      // scheduled_time_ns_ 记录发送计划时刻。因为是本机单调时钟，
      // 只能与发送端自身日志比较；未做 PTP 同步时不能用于跨主机单向延迟。
      message.scheduled_time_ns_(
          static_cast<std::uint64_t>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  next_release.time_since_epoch()).count()));
      // BestEffort 写入不会等待远端应用确认。
      writer.write(message);
      ++samples_written;

      interval_total.record(interval_us);
      lateness_total.record(std::max(0.0, lateness_us));
    }

    // 以下排序和日志均在 1 kHz 发送循环结束后执行，不再干扰实时发送。
    // DDS 状态与应用统计一起输出，便于区分调度问题和 DDS 周期违约。
    const auto deadline_status = writer.offered_deadline_missed_status();
    arm_state_demo::print_statistics(
        std::cout, "publisher interval total",
        std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count(),
        interval_total);
    arm_state_demo::print_statistics(
        std::cout, "publisher schedule lateness total",
        std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count(),
        lateness_total);
    std::cout << "[publisher summary]"
              << " scheduled_cycles=" << scheduled_cycles
              << " written=" << samples_written
              << " skipped_cycles=" << skipped_cycles
              << " offered_deadline_missed=" << deadline_status.total_count()
              << std::endl;
  } catch (const dds::core::Exception &error) {
    std::cerr << "[publisher] DDS error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << "Usage: " << argv[0]
              << " [duration-seconds] [period-us] [deadline-ms] [lifespan-ms]\n"
              << "[publisher] Error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
