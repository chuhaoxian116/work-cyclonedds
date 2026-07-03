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
    const arm_state_demo::Options options =
        arm_state_demo::parse_options(argc, argv);
    const auto period = std::chrono::microseconds(options.period_us);
    const auto duration = std::chrono::seconds(options.duration_seconds);

    dds::domain::DomainParticipant participant(
        org::eclipse::cyclonedds::domain::default_id());
    const auto topic_qos =
        arm_state_demo::make_topic_qos(participant, options);
    dds::topic::Topic<myproject::msg::dds_::ArmState_> topic(
        participant, arm_state_demo::kTopicName, topic_qos);
    dds::pub::Publisher publisher(participant);
    const auto writer_qos =
        arm_state_demo::make_writer_qos(publisher, options);
    dds::pub::DataWriter<myproject::msg::dds_::ArmState_> writer(
        publisher, topic, writer_qos);

    myproject::msg::dds_::ArmState_ message;
    const std::uint64_t session_id = make_session_id();
    message.session_id_(session_id);
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

    std::uint64_t scheduled_cycles = 0;
    std::uint64_t samples_written = 0;
    std::uint64_t skipped_cycles = 0;
    arm_state_demo::MicrosecondStatistics interval_window;
    arm_state_demo::MicrosecondStatistics interval_total;
    arm_state_demo::MicrosecondStatistics lateness_total;

    const auto start = std::chrono::steady_clock::now();
    const auto stop = start + duration;
    auto next_release = start;
    auto previous_write = start;
    auto next_report = start + std::chrono::seconds(1);

    while (next_release < stop) {
      next_release += period;
      ++scheduled_cycles;
      std::this_thread::sleep_until(next_release);

      auto now = std::chrono::steady_clock::now();
      if (now >= next_release + period) {
        const auto behind = now - next_release;
        const auto skipped =
            static_cast<std::uint64_t>(behind / period);
        skipped_cycles += skipped;
        scheduled_cycles += skipped;
        next_release += period * skipped;
      }

      now = std::chrono::steady_clock::now();
      const double lateness_us =
          std::chrono::duration<double, std::micro>(now - next_release).count();
      const double interval_us =
          std::chrono::duration<double, std::micro>(now - previous_write).count();
      previous_write = now;

      message.sequence_(scheduled_cycles);
      message.scheduled_time_ns_(
          static_cast<std::uint64_t>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  next_release.time_since_epoch()).count()));
      writer.write(message);
      ++samples_written;

      interval_window.record(interval_us);
      interval_total.record(interval_us);
      lateness_total.record(std::max(0.0, lateness_us));

      if (now >= next_report) {
        arm_state_demo::print_statistics(
            std::cout,
            "publisher interval",
            std::chrono::duration<double>(now - start).count(),
            interval_window);
        std::cout << "[publisher] written=" << samples_written
                  << " skipped_cycles=" << skipped_cycles
                  << " matched_readers="
                  << writer.publication_matched_status().current_count()
                  << std::endl;
        interval_window.clear();
        do {
          next_report += std::chrono::seconds(1);
        } while (next_report <= now);
      }
    }

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
