#include "DemoMessage.hpp"
#include "dds/dds.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

constexpr int kDefaultSampleCount = 5;
constexpr const char *kTopicName = "Demo_Message";

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
    dds::topic::Topic<Demo::Message> topic(participant, kTopicName);
    dds::sub::Subscriber subscriber(participant);
    dds::sub::DataReader<Demo::Message> reader(subscriber, topic);

    std::cout << "[C++ subscriber] Waiting for " << expected_count
              << " sample(s) on \"" << kTopicName << "\"..." << std::endl;

    int received_count = 0;
    while (received_count < expected_count) {
      const auto samples = reader.take();
      for (const auto &sample : samples) {
        if (!sample.info().valid()) {
          continue;
        }

        const Demo::Message &message = sample.data();
        std::cout << "[C++ subscriber] Received id=" << message.id()
                  << ", text=\"" << message.text() << "\"" << std::endl;
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
