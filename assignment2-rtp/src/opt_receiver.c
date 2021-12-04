#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#include "util.h"
#include "rtp_opt.h"

#define RECV_BUFFER_SIZE 32768  // 32KB

int receiver(char *receiver_port, int window_size, char* file_name) {

  char * buffer = calloc(window_size + 1, 2048);

  // create rtp socket file descriptor
  int receiver_fd = rtp_socket(window_size);
  if (receiver_fd == 0) {
    perror("create rtp socket failed");
    exit(EXIT_FAILURE);
  }

  // create socket address
  // forcefully attach socket to the port
  struct sockaddr_in address;
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(atoi(receiver_port));

  // bind rtp socket to address
  if (rtp_bind(receiver_fd, (struct sockaddr *)&address, sizeof(struct sockaddr))<0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  int recv_bytes;
  struct sockaddr_in sender;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  // listen to incoming rtp connection
  rtp_listen(receiver_fd, 1);
  // accept the rtp connection
  rtp_accept(receiver_fd, (struct sockaddr*)&sender, &addr_len);
  printf("accepted\n");

  // open file
  FILE * fp;
  int len = strlen(file_name);

    fp = fopen(file_name, "w+b");
  if (fp == NULL) {
    perror("receiver: open file error");
    exit(EXIT_FAILURE);
  }
  printf("start to receive pack\n");
 // receive packet
 while(1){

    recv_bytes = rtp_recvfrom(receiver_fd, (void *)buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &addr_len);
    if(recv_bytes == 0){
      continue;
    }
    else if(recv_bytes == -2){ // received RTP_END
      rtp_close_receiver(receiver_fd);
      return 0;
    }
    else if(recv_bytes < 0){
      perror("receiver error");
    }
    else{
      buffer[recv_bytes] = '\0';
      // printf("receive msg: %s\n", buffer);
      fwrite(buffer, recv_bytes, 1, fp);
      printf("%d bytes written to file\n", recv_bytes);
    }
    // if(buffer[recv_bytes - 1] == EOF){
    //   break;
    // }
 }


  rtp_close_receiver(receiver_fd);
  free_rcb();
  free(buffer);

  return 0;
}

/*
 * main():
 * Parse command-line arguments and call receiver function
*/
int main(int argc, char **argv) {
    char *receiver_port;
    int window_size;
    char *file_name;

    if (argc != 4) {
        fprintf(stderr, "Usage: ./receiver [Receiver Port] [Window Size] [File Name]\n");
        exit(EXIT_FAILURE);
    }

    receiver_port = argv[1];
    window_size = atoi(argv[2]);
    file_name = argv[3];
    return receiver(receiver_port, window_size, file_name);
}
