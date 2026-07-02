#include "DemoMessage.h"
#include "dds/dds.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_SAMPLE_COUNT 5
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
  const int sample_count = sample_count_from_args(argc, argv);
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

  const dds_entity_t writer =
      dds_create_writer(participant, topic, NULL, NULL);
  if (writer < 0) {
    DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-writer));
  }

  dds_return_t rc =
      dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);
  if (rc != DDS_RETCODE_OK) {
    DDS_FATAL("dds_set_status_mask: %s\n", dds_strretcode(-rc));
  }

  printf("[C publisher] Waiting for a subscriber on \"%s\"...\n", TOPIC_NAME);
  fflush(stdout);

  uint32_t status = 0;
  while ((status & DDS_PUBLICATION_MATCHED_STATUS) == 0) {
    rc = dds_get_status_changes(writer, &status);
    if (rc != DDS_RETCODE_OK) {
      DDS_FATAL("dds_get_status_changes: %s\n", dds_strretcode(-rc));
    }
    dds_sleepfor(DDS_MSECS(20));
  }

  for (int32_t id = 1; id <= sample_count; ++id) {
    char text[128];
    (void)snprintf(text, sizeof(text), "Hello from the C publisher #%" PRId32,
                   id);

    Demo_Message message = {.id = id, .text = text};
    rc = dds_write(writer, &message);
    if (rc != DDS_RETCODE_OK) {
      DDS_FATAL("dds_write: %s\n", dds_strretcode(-rc));
    }

    printf("[C publisher] Sent id=%" PRId32 ", text=\"%s\"\n", id, text);
    fflush(stdout);
    dds_sleepfor(DDS_MSECS(500));
  }

  rc = dds_delete(participant);
  if (rc != DDS_RETCODE_OK) {
    DDS_FATAL("dds_delete: %s\n", dds_strretcode(-rc));
  }
  return EXIT_SUCCESS;
}
