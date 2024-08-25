#include "src/client.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
void *child(void *_) {
  // read msg
  struct client_t *client = create_easymq("192.168.200.143:7777");
  if (!client) {
    perror("connect to easymq failed");
    _exit(-1);
  }
  while (1) {
    struct message *msg = read_latest(client, "testC", 5);
    // printf("accept msg %p\n",msg);
    if (!msg)
      break;
    if (msg->err) {
      fprintf(stderr, "error %s\n", msg->err);
      close_message(msg);
      break;
    }
    if (msg->content)
      printf("read msg [%ld]\n",msg->size);
    close_message(msg);
  }
  perror("read msg failed");
  _exit(-1);
}
void publishtest() {
  char buff[100] = {0};
  int i = 0;
  struct client_t *client = create_easymq("192.168.200.143:7777");
  if (!client) {
    perror("connect to easymq failed");
    _exit(-1);
  }
  printf("sender fd is %d\n",*(int*)client);
  
  while (1) {
    i++;
    // printf("sender fd is %d pointer %p\n",*(int*)client,client);
    snprintf(buff, 100, "hello easymq %d", i);
    struct message* msg=publish(client, "testC", 5, buff, strlen(buff));
    if (!msg) {
      perror("publish msg failed\n");
    }else{
      close_message(msg);
    }
    // printf("send msg success client %p\n",client);
    // sleep(1);
  }
}
#include <fcntl.h>
int main() {
  int fd=open("test.pid",O_CREAT|O_WRONLY|O_TRUNC,0644);
  char buff[8]={0};
  snprintf(buff,8,"%d",getpid());
  write(fd, buff,strlen(buff));
  close(fd);
  pthread_t ph;
  if (pthread_create(&ph, NULL, child, NULL) < 0)
    perror("thread create failed");
  pthread_detach(ph);
  publishtest();
}