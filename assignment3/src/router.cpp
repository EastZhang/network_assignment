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
            printf("received cmd DV\n");
            return propagate();
        }
        case(ROUTER_DV):{
            printf("received DV from router id %d: ", fromid);
            int from_dv[MAX_ROUTERN];
            uint32_t * m = (uint32_t *)msg;
            int n = get_routernum();
            for(int i = 0; i < n; ++i){
                from_dv[i] = (int)ntohl(m[i]);
                printf("%d ", from_dv[i]);
            }
            printf("\n");

            return update_dv(fromid, from_dv);
            // if(bellman_ford() > 0){
            //     return propagate();
            // }
            // return 0;
        }
        case(AGENT_SHOW):{
            printf("received cmd SHOW\n");
            char buffer[BUFFER_SIZE];
            int ret = show_dv(buffer);
            int size = sendto(sockfd, buffer, ret, 0, from, sizeof(sockaddr));
            return size;
        }
        case(AGENT_UPDATE):{
            printf("received cmd UPDATE\n");
            uint32_t *argv = (uint32_t *)msg;
            int toid = (int)ntohl(argv[1]), weight = (int)ntohl(argv[2]);
            return update_to(toid, weight);
            // bellman_ford();
            // return propagate();
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