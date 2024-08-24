#pragma once
#include <stddef.h>
#include <stdint.h>
struct client_t;
struct message {
  char *content;
  size_t size;
  char *err;
};
struct client_t *create_easymq(const char *address);
int close_easymq(struct client_t *);
struct message* publish(struct client_t *client, const char *topic, size_t topic_size,
            const char *content, size_t content_size);
struct message *read_latest(struct client_t *client, const char *topic,
                            size_t topic_size);
void close_message(struct message *msg);
// int init_easymq(struct client_t*,const char*addr);