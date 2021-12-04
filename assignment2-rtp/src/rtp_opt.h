#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RTP_START 0
#define RTP_END   1
#define RTP_DATA  2
#define RTP_ACK   3

#define BUFFER_SIZE 2048

typedef struct __attribute__ ((__packed__)) RTP_header {
    uint8_t type;       // 0: START; 1: END; 2: DATA; 3: ACK
    uint16_t length;    // Length of data; 0 for ACK, START and END packets
    uint32_t seq_num;
    uint32_t checksum;  // 32-bit CRC
} rtp_header_t;


typedef struct RTP_control_block {
    uint32_t window_size;
    // uint32_t seq_size;
    uint32_t sender_base;
    uint32_t sender_front;
    uint32_t receiver_expect;
    uint32_t max_seq_num;

    char **sender_buf; // 注：sender buffer 保存包括RTP头的全部内容
    char **receiver_buf; // 注：receiver buffer 保存包括RTP头的全部内容，其中checksum=0表示有效，checksum=1表示无效
    uint64_t ACKED; // 记录sender端收到的ACK。注:在opt模式下，window_size不能超过31。
    // uint32_t act_size;
    // uint32_t * acked;
    // TODO: you can add your RTP-related fields here
    int debug;
} rcb_t;

static rcb_t* rcb = NULL;

// different from the POSIX
int rtp_socket(uint32_t window_size);

int rtp_listen(int sockfd, int backlog);

int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int rtp_close_sender(int sockfd,  const struct sockaddr *addr, socklen_t addrlen);

int rtp_close_receiver(int sockfd);

int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen);

int rtp_recvfrom(int sockfd, void *buf, int len, int flags, struct sockaddr *from, socklen_t *fromlen);

int send_ack(int seq_num, int sockfd, struct sockaddr * from, socklen_t fromlen);

int free_rcb();

void set_max_seq_num(uint32_t num);

#endif //RTP_H
