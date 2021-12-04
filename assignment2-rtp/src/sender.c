#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.h"
#include "rtp.h"

#define LOAD_SIZE (1470 - sizeof(rtp_header_t))
#define FILE_TEXT 0
#define FILE_BINARY 1

int send_file(FILE *fp, int sock, struct sockaddr *addr, socklen_t addrlen, int file_type){
  fseek(fp, 0L, SEEK_END);
  long size = ftell(fp), cnt = 0;
  fseek(fp, 0L, SEEK_SET);
  char buf[LOAD_SIZE];

  int plusone = ((size % LOAD_SIZE) != 0);
  // printf("%d\n", plusone);
  printf("start to send data\n");
  set_max_seq_num(size / LOAD_SIZE + plusone);

  // send data in chunks
  while(cnt != size){
    int len;
    // if(file_type == FILE_TEXT){
    //   fgets(buf, LOAD_SIZE, fp);
    //   len = strlen(buf);
    //   printf("sent bytes %d\n", len);
    //   cnt += len;
    // }
    // else{
      int sz = LOAD_SIZE;
      if(size - cnt < sz){
        sz = size - cnt;
      }
      fread(buf, sz, 1, fp);
      cnt += sz;
      len = sz;
    // }
    if(!rtp_sendto(sock, (void *)buf, len, 0, addr, addrlen)){
      return -1;
    }

  }


  // buf[0] = EOF;
  // buf[1] = '\0';
  // rtp_sendto(sock, (void *)buf, strlen(buf), 0, addr, addrlen);
  fclose(fp);
}

int sender(char *receiver_ip, char* receiver_port, int window_size, char* message){

  // create socket
  int sock = 0;
  if ((sock = rtp_socket(window_size)) < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // create receiver address
  struct sockaddr_in receiver_addr;
  memset(&receiver_addr, 0, sizeof(receiver_addr));
  receiver_addr.sin_family = AF_INET;
  receiver_addr.sin_port = htons(atoi(receiver_port));

  // convert IPv4 or IPv6 addresses from text to binary form
  if(inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr)<=0) {
    perror("address failed");
    exit(EXIT_FAILURE);
  }

  // connect to server
  int ret = rtp_connect(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr));
  if(ret < 0){
    rtp_close_sender(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr));
    return 0;
  }
  // printf("connected\n");

  // send data
  char test_data[] = "Hello, world!\n";
  // TODO: if message is filename, open the file and send its content
  // rtp_sendto(sock, (void *)test_data, strlen(test_data), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
  // rtp_sendto(sock, (void *)message, strlen(message), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
  FILE * fp = fopen(message, "rb");
  if(fp){
    int filetype = FILE_BINARY, len = strlen(message);
    // printf("message-3:%s\n", message + len - 3);
    // if(strcmp(message + len - 3, "txt")){
    //   filetype = FILE_BINARY;
    //   fclose(fp);
    //   fp = fopen(message, "rb");
    // }
    send_file(fp, sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr), filetype);
    // fclose(fp);
  }
  else{
    rtp_sendto(sock, (void *)message, strlen(message), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
  }

  // close rtp socket
  rtp_close_sender(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr));
  free_rcb();

  return 0;
}



/*
 * main()
 * Parse command-line arguments and call sender function
*/
int main(int argc, char **argv) {
  char *receiver_ip;
  char *receiver_port;
  int window_size;
  char *message;

  if (argc != 5) {
    fprintf(stderr, "Usage: ./sender [Receiver IP] [Receiver Port] [Window Size] [message]");
    exit(EXIT_FAILURE);
  }

  receiver_ip = argv[1];
  receiver_port = argv[2];
  window_size = atoi(argv[3]);
  message = argv[4];
  return sender(receiver_ip, receiver_port, window_size, message);
}