#include "DemoMessage_.hpp"
#include "dds/dds.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

constexpr int kDefaultSampleCount = 5;
constexpr const char *kTopicName = "rt/demo_message";

int sample_count_from_args(int argc, char **argv)
{
  if (argc < 2) {
    return kDefaultSampleCount;
  }

  const int value = std::stoi(argv[1]);
  if (value <= 0) {
    throw std::invalid_argument("sample count must be positive");
  }
  return value;
}

}  // namespace

int main(int argc, char **argv)
{
  try {
    const int expected_count = sample_count_from_args(argc, argv);
    dds::domain::DomainParticipant participant(
        org::eclipse::cyclonedds::domain::default_id());
    dds::topic::qos::TopicQos topic_qos = participant.default_topic_qos();
    topic_qos << dds::core::policy::Reliability::Reliable()
              << dds::core::policy::Durability::Volatile()
              << dds::core::policy::History::KeepLast(10);
    dds::topic::Topic<myproject::msg::dds_::DemoMessage_> topic(
        participant, kTopicName, topic_qos);
    dds::sub::Subscriber subscriber(participant);
    dds::sub::qos::DataReaderQos reader_qos =
        subscriber.default_datareader_qos();
    reader_qos = topic.qos();
    dds::sub::DataReader<myproject::msg::dds_::DemoMessage_> reader(
        subscriber, topic, reader_qos);

    std::cout << "[C++ subscriber] Waiting for " << expected_count
              << " sample(s) on \"" << kTopicName << "\"..." << std::endl;

    int received_count = 0;
    while (received_count < expected_count) {
      const auto samples = reader.take();
      for (const auto &sample : samples) {
        if (!sample.info().valid()) {
          continue;
        }

        const myproject::msg::dds_::DemoMessage_ &message = sample.data();
        std::cout << "[C++ subscriber] Received id=" << message.id_()
                  << ", text=\"" << message.text_() << "\"" << std::endl;
        ++received_count;
      }

      if (samples.length() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
  } catch (const dds::core::Exception &error) {
    std::cerr << "[C++ subscriber] DDS error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << "Usage: " << argv[0] << " [positive-sample-count]\n"
              << "[C++ subscriber] Error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
