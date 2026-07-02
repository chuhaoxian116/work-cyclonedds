#include "DemoMessage.h"
#include "dds/dds.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_SAMPLE_COUNT 5
#define MAX_SAMPLES 1
#define TOPIC_NAME "Demo_Message"

static int sample_count_from_args(int argc, char **argv)
{
  if (argc < 2) {
    return DEFAULT_SAMPLE_COUNT;
  }

  const long value = strtol(argv[1], NULL, 10);
  if (value <= 0 || value > INT32_MAX) {
    fprintf(stderr, "Usage: %s [positive-sample-count]\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  return (int)value;
}

int main(int argc, char **argv)
{
  const int expected_count = sample_count_from_args(argc, argv);
  const dds_entity_t participant =
      dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0) {
    DDS_FATAL("dds_create_participant: %s\n",
              dds_strretcode(-participant));
  }

  const dds_entity_t topic = dds_create_topic(
      participant, &Demo_Message_desc, TOPIC_NAME, NULL, NULL);
  if (topic < 0) {
    DDS_FATAL("dds_create_topic: %s\n", dds_strretcode(-topic));
  }

  dds_qos_t *qos = dds_create_qos();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(2));
  const dds_entity_t reader =
      dds_create_reader(participant, topic, qos, NULL);
  dds_delete_qos(qos);
  if (reader < 0) {
    DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-reader));
  }

  void *samples[MAX_SAMPLES] = {NULL};
  samples[0] = Demo_Message__alloc();
  dds_sample_info_t sample_info[MAX_SAMPLES];
  int received_count = 0;

  printf("[C subscriber] Waiting for %d sample(s) on \"%s\"...\n",
         expected_count, TOPIC_NAME);
  fflush(stdout);

  while (received_count < expected_count) {
    const dds_return_t rc =
        dds_take(reader, samples, sample_info, MAX_SAMPLES, MAX_SAMPLES);
    if (rc < 0) {
      DDS_FATAL("dds_take: %s\n", dds_strretcode(-rc));
    }

    if (rc > 0 && sample_info[0].valid_data) {
      const Demo_Message *message = samples[0];
      printf("[C subscriber] Received id=%" PRId32 ", text=\"%s\"\n",
             message->id, message->text);
      fflush(stdout);
      ++received_count;
    } else {
      dds_sleepfor(DDS_MSECS(20));
    }
  }

  Demo_Message_free(samples[0], DDS_FREE_ALL);
  const dds_return_t rc = dds_delete(participant);
  if (rc != DDS_RETCODE_OK) {
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-rc));
  }
  return EXIT_SUCCESS;
}
