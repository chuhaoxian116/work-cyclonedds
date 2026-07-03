#ifndef MYPROJECT_SAMPLES_ARM_STATE_1KHZ_COMMON_HPP_
#define MYPROJECT_SAMPLES_ARM_STATE_1KHZ_COMMON_HPP_

#include "dds/dds.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace arm_state_demo {

constexpr const char *kTopicName = "rt/arm/state_1khz";
constexpr std::uint64_t kNanosecondsPerMillisecond = 1000000ULL;

struct Options
{
  int duration_seconds = 60;
  int period_us = 1000;
  int deadline_ms = 2;
  int lifespan_ms = 5;
};

inline int parse_positive(const char *text, const char *name)
{
  const int value = std::stoi(text);
  if (value <= 0) {
    throw std::invalid_argument(std::string(name) + " must be positive");
  }
  return value;
}

inline Options parse_options(int argc, char **argv)
{
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
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline dds::topic::qos::TopicQos make_topic_qos(
    dds::domain::DomainParticipant &participant,
    const Options &options)
{
  dds::topic::qos::TopicQos qos = participant.default_topic_qos();
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

inline dds::pub::qos::DataWriterQos make_writer_qos(
    dds::pub::Publisher &publisher,
    const Options &options)
{
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
