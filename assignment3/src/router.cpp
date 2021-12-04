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
#include <map>

#include "common.h"

int respond(int sockfd, int type, int fromid, void * msg, struct sockaddr * from){
    switch(type){
        case(AGENT_DV):{
            return propagate();
        }
        case(ROUTER_DV):{
            int from_dv[MAX_ROUTERN][MAX_ROUTERN];
            memcpy(from_dv, msg, sizeof(from_dv));
            break;
        }
        case(AGENT_SHOW):{
            printf("received cmd SHOW\n");
            char buffer[BUFFER_SIZE];
            int ret = show_dv(buffer);
            int size = sendto(sockfd, buffer, ret, 0, from, sizeof(sockaddr));
            return size;
        }
    }
}


int router(){
    int sockfd = rp_socket();
    // create socket address
    // forcefully attach socket to the port
    // printf("%d\n", atoi(host->port));
    // printf("here\n");
    // int i = host->id;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(get_host_port()));
    // bind rp socket to address
    if (rp_bind(sockfd, (struct sockaddr *)&address, sizeof(struct sockaddr))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    set_socket(sockfd);

    int fromid, type;
    char msg[BUFFER_SIZE];
    struct sockaddr from;
    while(rp_recvfrom(sockfd, &fromid, &type, msg, &from) > 0){
        // printf("here\n");
        if(respond(sockfd, type, fromid, msg, &from) < 0){
            perror("fail to respond command");
            exit(-1);
        }
        // return 0;
    }
    return 0;
}

int main(int argc, char **argv){
    if(argc < 4){
        printf("error: too less args\n");
        exit(-1);
    }
    else if(argc > 4){
        printf("error: too much args\n");
        exit(-1);
    }
    char * loc_file_name = argv[1], *top_file_name = argv[2];
    int rid = atoi(argv[3]);
    if(router_init(loc_file_name, top_file_name, rid) < 0){
        printf("error: router init failed\n");
        exit(-1);
    }
    return router();
}