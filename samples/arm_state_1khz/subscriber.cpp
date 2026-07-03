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

int main(int argc, char **argv)
{
  try {
    const arm_state_demo::Options options =
        arm_state_demo::parse_options(argc, argv);
    const auto duration = std::chrono::seconds(options.duration_seconds);

    dds::domain::DomainParticipant participant(
        org::eclipse::cyclonedds::domain::default_id());
    const auto topic_qos =
        arm_state_demo::make_topic_qos(participant, options);
    dds::topic::Topic<myproject::msg::dds_::ArmState_> topic(
        participant, arm_state_demo::kTopicName, topic_qos);
    dds::sub::Subscriber subscriber(participant);
    const auto reader_qos =
        arm_state_demo::make_reader_qos(subscriber, options);
    dds::sub::DataReader<myproject::msg::dds_::ArmState_> reader(
        subscriber, topic, reader_qos);

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

    bool started = false;
    std::uint64_t active_session = 0;
    std::uint64_t last_sequence = 0;
    std::uint64_t received = 0;
    std::uint64_t sequence_gaps = 0;
    std::uint64_t duplicates = 0;
    std::uint64_t out_of_order = 0;
    std::uint64_t session_changes = 0;
    auto first_receive = std::chrono::steady_clock::time_point();
    auto previous_receive = std::chrono::steady_clock::time_point();
    auto stop = std::chrono::steady_clock::time_point::max();
    auto next_report = std::chrono::steady_clock::time_point::max();
    arm_state_demo::MicrosecondStatistics interval_window;
    arm_state_demo::MicrosecondStatistics interval_total;
    arm_state_demo::MicrosecondStatistics jitter_total;

    while (!started || std::chrono::steady_clock::now() < stop) {
      try {
        waitset.wait(dds::core::Duration::from_millisecs(100));
      } catch (const dds::core::TimeoutError &) {
        continue;
      }

      const auto samples = reader.take();
      for (const auto &sample : samples) {
        if (!sample.info().valid()) {
          continue;
        }

        const auto receive_time = std::chrono::steady_clock::now();
        const auto &message = sample.data();

        if (!started) {
          started = true;
          active_session = message.session_id_();
          last_sequence = message.sequence_();
          first_receive = receive_time;
          previous_receive = receive_time;
          stop = first_receive + duration;
          next_report = first_receive + std::chrono::seconds(1);
          ++received;
          std::cout << "[subscriber] first_sample"
                    << " session=" << active_session
                    << " sequence=" << last_sequence
                    << std::endl;
          continue;
        }

        if (message.session_id_() != active_session) {
          ++session_changes;
          active_session = message.session_id_();
          last_sequence = message.sequence_();
          previous_receive = receive_time;
          ++received;
          continue;
        }

        if (message.sequence_() == last_sequence) {
          ++duplicates;
        } else if (message.sequence_() < last_sequence) {
          ++out_of_order;
        } else {
          if (message.sequence_() > last_sequence + 1) {
            sequence_gaps += message.sequence_() - last_sequence - 1;
          }
          last_sequence = message.sequence_();
        }

        const double interval_us =
            std::chrono::duration<double, std::micro>(
                receive_time - previous_receive).count();
        previous_receive = receive_time;
        const double jitter_us =
            std::abs(interval_us - static_cast<double>(options.period_us));
        interval_window.record(interval_us);
        interval_total.record(interval_us);
        jitter_total.record(jitter_us);
        ++received;

        if (receive_time >= next_report) {
          arm_state_demo::print_statistics(
              std::cout,
              "subscriber interval",
              std::chrono::duration<double>(
                  receive_time - first_receive).count(),
              interval_window);
          std::cout << "[subscriber] received=" << received
                    << " sequence_gaps=" << sequence_gaps
                    << " duplicates=" << duplicates
                    << " out_of_order=" << out_of_order
                    << std::endl;
          interval_window.clear();
          do {
            next_report += std::chrono::seconds(1);
          } while (next_report <= receive_time);
        }
      }
    }

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
              << std::endl;
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
