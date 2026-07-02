#include "DemoMessage_.hpp"
#include "dds/dds.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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
    const int sample_count = sample_count_from_args(argc, argv);
    dds::domain::DomainParticipant participant(
        org::eclipse::cyclonedds::domain::default_id());
    dds::topic::qos::TopicQos topic_qos = participant.default_topic_qos();
    topic_qos << dds::core::policy::Reliability::Reliable()
              << dds::core::policy::Durability::Volatile()
              << dds::core::policy::History::KeepLast(10);
    dds::topic::Topic<myproject::msg::dds_::DemoMessage_> topic(
        participant, kTopicName, topic_qos);
    dds::pub::Publisher publisher(participant);
    dds::pub::qos::DataWriterQos writer_qos =
        publisher.default_datawriter_qos();
    writer_qos = topic.qos();
    dds::pub::DataWriter<myproject::msg::dds_::DemoMessage_> writer(
        publisher, topic, writer_qos);

    std::cout << "[C++ publisher] Waiting for a subscriber on \""
              << kTopicName << "\"..." << std::endl;
    while (writer.publication_matched_status().current_count() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    for (std::int32_t id = 1; id <= sample_count; ++id) {
      myproject::msg::dds_::DemoMessage_ message(
          id, "Hello from the C++ publisher #" + std::to_string(id));
      writer.write(message);
      std::cout << "[C++ publisher] Sent id=" << message.id_()
                << ", text=\"" << message.text_() << "\"" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  } catch (const dds::core::Exception &error) {
    std::cerr << "[C++ publisher] DDS error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << "Usage: " << argv[0] << " [positive-sample-count]\n"
              << "[C++ publisher] Error: " << error.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
