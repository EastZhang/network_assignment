#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include "rtp_opt.h"
#include "util.h"

#define DEBUG_SENDER 1
#define DEBUG_RECEIVER 2
#define ACKRANGE (2 * rcb->window_size + 2) // This ensures the ACKED to work correctly
#define ACKFLAG(i) ((rcb->ACKED >> (i % ACKRANGE)) & 1) 
#define SETFLAG(i) (rcb->ACKED |= (1 << (i % ACKRANGE)))
#define ZEROFLAG(i) (rcb->ACKED &= ~(1 << (i % ACKRANGE)))

// for debug only
void print_rcb(int mode){
    int sz = rcb->window_size;
    printf("window size: %d\n", sz);
    switch(mode){
        case 1:{
            printf("sender buffer:\n");
            for(int i = rcb->sender_base; i < rcb->sender_front; ++i){
                printf("%d ", i);
            }
            break;
        }
        case 2:{
            printf("\nreceiver buffer:\n");
            for(int i = rcb->receiver_expect; i < rcb->receiver_expect + rcb->window_size; ++i){
                int k = i % rcb->window_size;
                if(!rcb->receiver_buf[k]){
                    printf("%d: NA ", i);
                }
                else{
                    rtp_header_t * rtp = (rtp_header_t *)rcb->receiver_buf[k];
                    if(rtp->checksum != 0){
                        printf("%d: NA ", i);
                    }
                    else{
                        printf("%d: buffed ", i);
                    }
                }
            }
            break;
        } 
    }

    printf("\n\n");
}

void rcb_init(uint32_t window_size) {
    if (rcb == NULL) {
        rcb = (rcb_t *) calloc(1, sizeof(rcb_t));
    } else {
        perror("The current version of the rtp protocol only supports a single connection");
        exit(EXIT_FAILURE);
    }
    rcb->window_size = window_size;
    // rcb->seq_size = 2 * window_size + 1;
    rcb->sender_base = 0;
    rcb->sender_front = 0;
    rcb->receiver_expect = 0;
    rcb->sender_buf = (char **)calloc(window_size + 1, sizeof(char *));
    rcb->receiver_buf = (char **)calloc(window_size + 1, sizeof(char *));
    rcb->ACKED = 0;

    // rcb->act_size = 0;
    // rcb->acked = (uint32_t *) calloc(window_size, sizeof(uint32_t));
    // TODO: you can initialize your RTP-related fields here
    rcb->debug = 0;
}


void set_max_seq_num(uint32_t num){
    rcb->max_seq_num = num;
    printf("max_seq_num:%d\n", num);
}

/*********************** Note ************************/
/* RTP in Assignment 2 only supports single connection.
/* Therefore, we can initialize the related fields of RTP when creating the socket.
/* rcb is a global varialble, you can directly use it in your implementatyion.
/*****************************************************/
int rtp_socket(uint32_t window_size) {
    rcb_init(window_size); 
    // create UDP socket
    return socket(AF_INET, SOCK_DGRAM, 0);  
}


int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}


int rtp_listen(int sockfd, int backlog) {
    // TODO: listen for the START message from sender and send back ACK
    // In standard POSIX API, backlog is the number of connections allowed on the incoming queue.
    // For RTP, backlog is always 1
    // listen(sockfd, backlog);
    struct sockaddr_in client;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    char buffer[BUFFER_SIZE];
    while(recvfrom(sockfd, buffer, 2048, 0, (struct sockaddr_in *)&client, &addrlen)){
        printf("received START\n");
        rtp_header_t *rtp = (rtp_header_t *)buffer;
        if(rtp->type != RTP_START){
            continue;
        }
        else{
            uint64_t checksum = rtp->checksum;
            rtp->checksum = 0;
            if(checksum != compute_checksum(buffer, sizeof(rtp_header_t))){
                return -1;
            }
            printf("confirmed START\n");
            rtp->type = RTP_ACK;
            rtp->checksum = compute_checksum(rtp, sizeof(rtp_header_t));
            // printf("[%s:%d]\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port));
            int size = sendto(sockfd, (void *)rtp, sizeof(rtp_header_t), 0, (struct sockaddr *)&client, addrlen);
            if(size > 0){
                printf("sent ACK\n");
            }
            // rcb->receiver_expect++;
            break;
        }
    }
    return 1;
}


int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    // Since RTP in Assignment 2 only supports one connection,
    // there is no need to implement accpet function.
    // You don???t need to make any changes to this function.
    return 1;
}

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    // TODO: send START message and wait for its ACK
    // connect(sockfd, addr, addrlen);
    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = 0;
    rtp->type = RTP_START;
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));
    int sent_bytes = sendto(sockfd, buffer, sizeof(rtp_header_t), 0, addr, addrlen);
    printf("START sent\n");
    while(1){
        fd_set rdfds;
        struct timeval tv;
        int ret;
        FD_ZERO(&rdfds);
        FD_SET(sockfd, &rdfds);
        tv.tv_sec = 0;
        tv.tv_usec = 500;
        ret = select(sockfd + 1, &rdfds, NULL, NULL, &tv);
        if(ret == 0){ // time-out, ?????????????????????
            return -1;
        }
        int recv_bytes = recvfrom(sockfd, buffer, 2048, 0, NULL, 0);
        printf("received\n");
        if (recv_bytes < 0) {
            perror("receive error");
            exit(EXIT_FAILURE);
        }
        buffer[recv_bytes] = '\0';
        rtp_header_t *rtp = (rtp_header_t *)buffer;

        if(rtp->type != RTP_ACK || rtp->seq_num != 0){
            printf("ACK error\n");
            return -1;
        }
        else{
            uint64_t checksum = rtp->checksum;
            rtp->checksum = 0; 
            // printf("checksum")
            if (checksum != compute_checksum((void *)buffer, recv_bytes)){ // ACK???????????????????????????
                printf("ACK checksum error\n");
                return -1;
            }
            printf("connected\n");
            break;
        }
    }   
    return 1;
}


int rtp_close_sender(int sockfd, const struct sockaddr *to, socklen_t tolen) {
    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = rcb->sender_front;
    rtp->type = RTP_END;
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));
    sendto(sockfd, buffer, sizeof(rtp_header_t), 0, to, tolen);
    // fd_set rdfds;
    // struct timeval tv;
    // int ret;
    // FD_ZERO(&rdfds);
    // FD_SET(sockfd, &rdfds);
    // tv.tv_sec = 1;
    // tv.tv_usec = 500;
    // ret = select(sockfd + 1, &rdfds, NULL, NULL, &tv);
    // if(ret == 0){ // time-out
    //     resend_all(sockfd, to, tolen);
    // }
    return close(sockfd);
}

int rtp_close_receiver(int sockfd) {
    return close(sockfd);
}

int resend_all(int sockfd, const struct sockaddr *to, socklen_t tolen){ 
    int start = rcb->sender_base, end = rcb->sender_front;
    for(int i = start; i < end; ++i){
        int k = i % rcb->window_size;
        if(rcb->sender_buf[k] && !ACKFLAG(i)){ // ?????????????????????ACK??????
            printf("resend pack No.%d\n", i);
            rtp_header_t* rtp = (rtp_header_t*)rcb->sender_buf[k];
            if (!sendto(sockfd, rcb->sender_buf[k], rtp->length + sizeof(rtp_header_t), 0, to, tolen)){
                return -1;
            }
        }
    }
    return 0;
}

// int recv_all_ack(int sockfd){
//     while(1){
//         char buffer[BUFFER_SIZE];
//         fd_set rdfds;
//         struct timeval tv;
//         int ret;
//         FD_ZERO(&rdfds);
//         FD_SET(sockfd, &rdfds);
//         tv.tv_sec = 0;
//         tv.tv_usec = 2;
//         ret = select(sockfd + 1, &rdfds, NULL, NULL, &tv);
//         if(ret == 0){ // no more acks
//             return 0;
//         }
//         int recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, 0);
//         rtp_header_t* rtp = (rtp_header_t*)buffer;
//         if(rtp->seq_num <= rcb->sender_base || rtp->seq_num >= rcb->sender_base + rcb->window_size){
//             continue;
//         }
//         rcb->ACKED &= (1 << rtp->seq_num); 
//         if(rtp->seq_num == rcb->sender_base){
//             rcb->sender_base++;
//         }
//         printf("received ACK No.%d\n", rtp->seq_num);
//     }
// }

// for debug only
void printACKED(){
    printf("ACKED: ");
    for(int i = 0; i < 10; ++i){
        printf("%d:%d ", i, ACKFLAG(i));
    }
    printf("\n");
}


int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen) {
    // TODO: send message
    // rcb->sender_front++;
    if(rcb->sender_front - rcb->sender_base + 1 >= rcb->window_size){
        while(1){
            char buffer[BUFFER_SIZE];
            fd_set rdfds;
            struct timeval tv;
            int ret;
            FD_ZERO(&rdfds);
            FD_SET(sockfd, &rdfds);
            tv.tv_sec = 1;
            tv.tv_usec = 500;
            ret = select(sockfd + 1, &rdfds, NULL, NULL, &tv);
            if(ret == 0){ // time-out
                resend_all(sockfd, to, tolen);
                continue;
            }
            int recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, 0);
            rtp_header_t* rtp = (rtp_header_t*)buffer;
            // printf("ACK checksum:%u\n", rtp->checksum);
            // printf("compute checksum:%u\n", compute_checksum((void *)rtp, recv_bytes));
            if(rtp->seq_num < rcb->sender_base || rtp->seq_num >= rcb->sender_base + rcb->window_size){ // ack should be greater than sender_base
                continue;
            }
            else if(rtp->seq_num == rcb->sender_base){
                // rcb->ACKED |= (1 << (rtp->seq_num % (2 * rcb->window_size + 1)));
                SETFLAG(rtp->seq_num);
                while(ACKFLAG(rcb->sender_base)){
                    // rcb->ACKED &= ~(1 << (rtp->seq_num % (2 * rcb->window_size + 1)));
                    ZEROFLAG(rtp->seq_num);
                    rcb->sender_base++;
                }    
                printf("received ACK No.%d, window_base moves to %d\n", rtp->seq_num, rcb->sender_base);
                // printACKED();
                // printf("ACKFLAG %d: %d\n", rcb->sender_base, ACKFLAG(rcb->sender_base));
                // printf("%d, %d\n", rcb->ACKED, rcb->ACKED & (1 << 0));
                break;
            }
            else{ // ?????????????????????????????????????????????????????????????????????????????????????????????
                // rcb->ACKED |= (1 << (rtp->seq_num % (2 * rcb->window_size + 1)));
                SETFLAG(rtp->seq_num);
                printf("received ACK No.%d\n", rtp->seq_num);
                continue;
            }
            // if(recv_all_ack(sockfd) == 0){
            //     break;
            // }
        }
    }

    // Send the first data message sample
    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = len;
    rtp->checksum = 0;
    rtp->seq_num = rcb->sender_front;
    rtp->type = RTP_DATA;
    memcpy((void *)buffer+sizeof(rtp_header_t), msg, len);
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t) + len);

    int sent_bytes = sendto(sockfd, (void*)buffer, sizeof(rtp_header_t) + len, flags, to, tolen);
    if (sent_bytes != (sizeof(struct RTP_header) + len)) {
        perror("send error");
        exit(EXIT_FAILURE);
    }
    int i = rcb->sender_front % rcb->window_size;
    if(!rcb->sender_buf[i]){
        rcb->sender_buf[i] = calloc(BUFFER_SIZE + 1, 1);
    }
    printf("sent pack No.%d\n", rtp->seq_num);
    memcpy(rcb->sender_buf[i], (void *)buffer, sizeof(rtp_header_t) + len);
    rcb->sender_front++;
    // rcb->sender_buf[i][sent_bytes] = '\0'; // for string
    // if(rcb->sender_base < 15)
    //     print_rcb(DEBUG_SENDER);

    // reach the end of file
    if(rcb->sender_front == rcb->max_seq_num){
        printf("yes\n");
        while(rcb->sender_base != rcb->sender_front - 1){
            char buffer[BUFFER_SIZE];
            fd_set rdfds;
            struct timeval tv;
            int ret;
            FD_ZERO(&rdfds);
            FD_SET(sockfd, &rdfds);
            tv.tv_sec = 1;
            tv.tv_usec = 500;
            ret = select(sockfd + 1, &rdfds, NULL, NULL, &tv);
            if(ret == 0){ // time-out
                resend_all(sockfd, to, tolen);
                continue;
            }
            int recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, 0);
            rtp_header_t* rtp = (rtp_header_t*)buffer;
            // printf("ACK checksum:%u\n", rtp->checksum);
            // printf("compute checksum:%u\n", compute_checksum((void *)rtp, recv_bytes));
            if(rtp->seq_num < rcb->sender_base || rtp->seq_num >= rcb->sender_base + rcb->window_size){ // ack should be greater than sender_base
                continue;
            }
            else if(rtp->seq_num == rcb->sender_base){
                // rcb->ACKED |= (1 << (rtp->seq_num % (2 * rcb->window_size + 1)));
                SETFLAG(rtp->seq_num);
                while(ACKFLAG(rcb->sender_base)){
                    // rcb->ACKED &= ~(1 << (rtp->seq_num % (2 * rcb->window_size + 1)));
                    ZEROFLAG(rtp->seq_num);
                    rcb->sender_base++;
                }    
                printf("received ACK No.%d, window_base moves to %d\n", rtp->seq_num, rcb->sender_base);
                // printACKED();
                // printf("ACKFLAG %d: %d\n", rcb->sender_base, ACKFLAG(rcb->sender_base));
                // printf("%d, %d\n", rcb->ACKED, rcb->ACKED & (1 << 0));
                continue;
            }
            else{ // ?????????????????????????????????????????????????????????????????????????????????????????????
                // rcb->ACKED |= (1 << (rtp->seq_num % (2 * rcb->window_size + 1)));
                SETFLAG(rtp->seq_num);
                printf("received ACK No.%d\n", rtp->seq_num);
                continue;
            }
            // if(recv_all_ack(sockfd) == 0){
            //     break;
            // }
        }
    }

    return 1;

}

int send_ack(int seq_num, int sockfd, struct sockaddr * from, socklen_t fromlen){
    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = seq_num;
    rtp->type = RTP_ACK;
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));
    int size = sendto(sockfd, (void *)buffer, sizeof(rtp_header_t), 0, from, fromlen);
    printf("sent ACK No.%d\n", rtp->seq_num);
    return 0;
}

// note: the return refers to bytes copied to buf, not received bytes.
int rtp_recvfrom(int sockfd, void *buf, int len, int flags,  struct sockaddr *from, socklen_t *fromlen) {
    // TODO: recv message

    char buffer[2048];
    
    // if(rcb->receiver_expect > 0){ // already connected
    //     fd_set rdfds;
    //     struct timeval tv;
    //     int ret;
    //     FD_ZERO(&rdfds);
    //     FD_SET(sockfd, &rdfds);
    //     tv.tv_sec = 1;
    //     tv.tv_usec = 500;
    //     ret = select(sockfd + 1, &rdfds, NULL, NULL, &tv);
    //     if(ret == 0){ // time-out
    //         resend_all(sockfd, to, tolen);
    //     }
    // }

    int recv_bytes = recvfrom(sockfd, buffer, 2048, flags, from, fromlen);
    if (recv_bytes < 0) {
        perror("receive error");
        exit(EXIT_FAILURE);
    }
    buffer[recv_bytes] = '\0';

    // extract header
    rtp_header_t *rtp = (rtp_header_t *)buffer;

    // verify checksum
    uint32_t pkt_checksum = rtp->checksum;
    rtp->checksum = 0;
    uint32_t computed_checksum = compute_checksum(buffer, recv_bytes);
    if (pkt_checksum != computed_checksum) {
        perror("checksums not match");
        return -1;
    }
    // for debug only
    // if(rtp->seq_num == 3 && rcb->debug == 0){
    //     rcb->debug = 1;
    //     return 0;
    // }

    int i = rtp->seq_num % rcb->window_size;
    uint32_t cntsz = 0;
    if(rtp->seq_num > rcb->receiver_expect && rtp->seq_num < rcb->receiver_expect + rcb->window_size){
        if(!rcb->receiver_buf[i]){
            rcb->receiver_buf[i] = calloc(BUFFER_SIZE + 1, 1);
        }
        memcpy(rcb->receiver_buf[i], buffer, rtp->length + sizeof(rtp_header_t));
        send_ack(rtp->seq_num, sockfd, from, *fromlen);
    }
    else if(rtp->seq_num == rcb->receiver_expect){
        printf("rcved PAK NO.%d\n", rtp->seq_num);
        // if(rtp->seq_num == 116){
        //     // buffer[rtp->length + sizeof(rtp_header_t)] = '\0';
        //     printf("%s\n", buffer + sizeof(rtp_header_t));
        // }
        if(rtp->type == RTP_END){
            printf("received END\n");
            send_ack(rtp->seq_num, sockfd, from, *fromlen);
            return -2;
        }
        uint32_t k;
        memcpy(buf, buffer+sizeof(rtp_header_t), rtp->length);
        cntsz += rtp->length;
        for (k = rcb->receiver_expect + 1; k < rcb->receiver_expect + rcb->window_size; ++k){
            uint32_t i = k % rcb->window_size;
            if(!rcb->receiver_buf[i]){
                break;
            }
            rtp_header_t *rtp_tmp = (rtp_header_t *)rcb->receiver_buf[i];
            if(rtp_tmp->checksum != 0){
                break;
            }
            memcpy(buf + cntsz, rcb->receiver_buf[i] + sizeof(rtp_header_t), rtp_tmp->length);
            printf("PAK NO.%d copied to buffer\n", k);
            rtp_tmp->checksum = 1; // checksum == 1 refers to trash
            cntsz += rtp_tmp->length;
        }
        
        send_ack(rtp->seq_num, sockfd, from, *fromlen);
        rcb->receiver_expect = k;
    }
    // print_rcb(DEBUG_RECEIVER);


    // memcpy(buf, buffer+sizeof(rtp_header_t), rtp->length);
    // print_rcb();
    return cntsz;
}

int free_rcb(){
    for(int i = 0; i < rcb->window_size; ++i){
        if(rcb->sender_buf[i]){
            free(rcb->sender_buf[i]);
        }
        if(rcb->receiver_buf[i]){
            free(rcb->receiver_buf[i]);
        }
    }
}