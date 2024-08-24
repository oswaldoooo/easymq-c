#include "client.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define uint16_bigendian_bytes(dst, src)                                       \
  dst[1] = src % 256;                                                          \
  dst[0] = src / 256

#define bytes_bigendian_uint16(src) src[1] + src[0] * 256

struct client_t {
  int fd;
};
int _read_msg(int fd, u_char *buff, size_t size) {
  short _rsize = read(fd, buff, 2);
  if (_rsize < 2) {
    return -1;
  }
  uint16_t rsize = bytes_bigendian_uint16(buff);
  // printf("read msg size %d\n",rsize);
  uint16_t rrsize = 0;
  int ssize;
__readmsg:
  ssize = read(fd, buff, rsize - rrsize);
  if (ssize <= 0) {
    return ssize;
  }
  rrsize += ssize;
  if (rrsize < rsize) {
    goto __readmsg;
  }
  return rsize;
}
// int init_easymq(struct client_t *client, const char *addr) {}
struct client_t *create_easymq(const char *address) {

  struct sockaddr_in si={0};
  si.sin_family = AF_INET;
  char *next = strstr(address, ":");
  if (!next) {
    fprintf(stderr, "not contains port. correct like ip:port");
    return NULL;
  }
  char ip_address[next - address];
  memmove(ip_address, address, next - address);
  int port = atoi(next+1);
  si.sin_port=htons(port);
  if (inet_aton(ip_address, &si.sin_addr) < 0) {
    perror("parse ip address failed");
    return NULL;
  }
  // si.sin_port=htons();
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (connect(fd, (struct sockaddr *)&si, sizeof(si)) < 0) {
    close(fd);
    return NULL;
  }
  struct client_t *result = calloc(1,sizeof(struct client_t));
  result->fd = fd;
  // printf("fd is %d client %p\n",result->fd,result);
  return result;
}

int close_easymq(struct client_t *client) {
  int ans = close(client->fd);
  free(client);
  return ans;
}
const uint8_t PUBLISH = 1;
const uint8_t READ_LATEST = 2;
const uint8_t OK = 1;
const uint8_t INTERNAL_ERROR = 2;
struct message *read_latest(struct client_t *client, const char *topic,
                            size_t topic_size) {
  // send read topic request
  u_char buff[topic_size + 3];
  buff[0] = READ_LATEST;
  uint16_bigendian_bytes((buff + 1), topic_size);
  memmove(buff + 3, topic, topic_size);
  long size = send(client->fd, buff, topic_size + 3, 0);
  if (size <= 0)
    return NULL;
  u_char rdbuff[1500];
  // read status code
  int vsize = read(client->fd, rdbuff, 1);
  if (vsize <= 0)
    return NULL;
  uint8_t status_code = rdbuff[0];
  vsize = _read_msg(client->fd, rdbuff, 1500);
  struct message *ans = malloc(sizeof(struct message));
  ans->size = vsize;
  char *content = (vsize > 0) ? malloc(vsize) : NULL;
  if (content) {
    memmove(content, rdbuff, vsize);
  }
  memmove(content, rdbuff, vsize);
  if (status_code == OK) {
    ans->err = NULL;
    ans->content = content;
  } else if (status_code == INTERNAL_ERROR) {
    ans->err = content;
    ans->content = NULL;
  } else {
    fprintf(stderr, "unknown code %d\n", status_code);
  }
  return ans;
}
struct message *publish(struct client_t *client, const char *topic,
                        size_t topic_size, const char *content,
                        size_t content_size) {
  u_char buff[topic_size + content_size + 5];
  buff[0] = PUBLISH;
  uint16_bigendian_bytes((buff + 1), topic_size);
  memmove(buff + 3, topic, topic_size);
  uint16_bigendian_bytes((buff + 3 + topic_size), content_size);
  memmove(buff + 5 + topic_size, content, content_size);
  int size = send(client->fd, buff, topic_size + content_size + 5, 0);
  if (size <= 0)
    return NULL;
  u_char *temp_buff = calloc(128,1);
  size = recv(client->fd, temp_buff, 1, 0);
  if (size < 1) {
    free(temp_buff);
    return NULL;
  }
  uint8_t status_code = temp_buff[0];
  size = _read_msg(client->fd, temp_buff, 128);
  if (size <= 0) {
    free(temp_buff);
    fprintf(stderr, "publish msg error cause protocol handshake failed");
    return NULL;
  }
  struct message *result = calloc(1,sizeof(struct message));
  result->size = size;
  if (status_code == OK) {
    result->err = NULL;
    result->content = (char *)temp_buff;
  } else if (status_code == INTERNAL_ERROR) {
    printf("internal error\n");
    result->err = (char *)temp_buff;
    result->content = NULL;
  } else {
    fprintf(stderr,"error code %d\n",status_code);
    snprintf((char *)temp_buff, 128,
             "protocol version error unknown status code %d\n", status_code);
    result->err = (char *)temp_buff;
    result->size = strlen((char *)temp_buff);
  }
  return result;
}
void close_message(struct message *msg) {
  if (msg->err)
    free(msg->err);
  if (msg->content)
    free(msg->content);
  free(msg);
}