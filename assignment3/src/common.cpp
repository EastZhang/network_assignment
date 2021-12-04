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
#include <vector>

#include "common.h"

using namespace std;


int set_weight(router_t * host, int rid1, int rid2, int weight){
    if(weight < 0){
        weight = MAX_DIST;
    }
    int id1 = RID2ID(rid1), id2 = RID2ID(rid2);
    host->dv[id1][id2] = host->dv[id2][id1] = weight;
    return 0;
}

int show_dv(char *buffer){
    int hid = host->id, cnt = 0;
    char buf[BUFFER_SIZE];
    for(int i = 0; i < router_num; ++i){
        if(i == hid){
            continue;
        }
        if(host->dv[hid][i] >= MAX_DIST){
            continue;
        }
        char tmp[50];
        sprintf(tmp, "Dest: %d, next: %d, cost: %d\n", ID2RID(i), ID2RID(host->next_hop[i]), host->dv[hid][i]);
        int len = strlen(tmp);
        memcpy(buf + cnt, tmp, len);
        cnt += len;
    }
    buf[cnt] = '\0';
    cnt++;
    printf("%s", buf);
    memcpy(buffer, buf, cnt);
    return cnt;
}


int parse_loc_file(char * filename, int host_rid, int agent){
    FILE * fp;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    if((fp = fopen(filename, "r")) == 0){
        printf("error: fail to open file\n");
        return -1;
    }
    fgets(buffer, 5, fp);
    int num = atoi(buffer);
    router_num = num;
    for(int i = 0; i < num; ++i){

        fgets(buffer, 100, fp);
        // printf("%s\n", buffer);
        const char s[2] = ",";
        char *ip, *port, *rid;
        // char *token;
        
        /* 获取第一个子字符串 */
        ip = strtok(buffer, s);
        port = strtok(NULL, s);
        rid = strtok(NULL, s);
        // strncpy(port, buffer + iplen, k - iplen);
        // port[k - iplen] = '\0';
        // strncpy(rid, buffer + k + 1, len - k - 1);
        int RID = atoi(rid);
        rid2id[RID] = i;
        id2rid[i] = RID;


        router_ip[i] = (char *)calloc(100, 1);
        router_port[i] = (char *)calloc(100, 1);
        strcpy(router_ip[i], ip);
        strcpy(router_port[i], port);

        if(!agent && RID != host_rid){
            continue;
        }
        host = (router_t *)calloc(sizeof(router_t), 1);
        host->ip = (char *)calloc(100, 1);
        host->port = (char *)calloc(100, 1);
        strcpy(host->ip, ip);
        strcpy(host->port, port);
        host->rid = RID;
        host->id = i;
        memset(host->dv, -1, sizeof(host->dv));
        printf("rid:%d, id:%d, ip:%s, port:%s,\n", atoi(rid), i, host->ip, host->port);
        // routers[i].addr = addr;
        // routers[i].rid = RID;
        // memset(routers[i].dv, -1, sizeof(routers[i].dv));

    }
    // if(agent){
    //     struct sockaddr_in addr;
    //     memset(&addr, 0, sizeof(addr));
    //     addr.sin_family = AF_INET;
    //     addr.sin_port = htons(atoi(port));
    //     if(inet_pton(AF_INET, ip, &addr.sin_addr)<=0) {
    //         perror("address failed");
    //         return -1;
    //     }
    // }
    fclose(fp);
    return 0;
}

int parse_top_file(char * filename, int rid){
    FILE * fp;
    // router * host = &routers[RID2ID(rid)];
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    for(int i = 0; i < MAX_ROUTERN; ++i){
        for(int j = 0; j < MAX_ROUTERN; ++j){
            set_weight(host, i, j, -1);
        }
    }

    if((fp = fopen(filename, "r")) == 0){
        printf("error: fail to open file\n");
        return -1;
    }
    fgets(buffer, 5, fp);
    int num = atoi(buffer);
    // router_num = num;
    for(int i = 0; i < num; ++i){
        fgets(buffer, 100, fp);
        // printf("%s\n", buffer);
        const char s[2] = ",";
        char * rid1 = strtok(buffer, s);
        char * rid2 = strtok(NULL, s);
        char * weight = strtok(NULL, s);
        int id1 = RID2ID(atoi(rid1)), id2 = RID2ID(atoi(rid2));

        if(atoi(rid1) == rid){
            set_weight(host, rid, atoi(rid2), atoi(weight));
            host->next_hop[id2] = id2;
            int w = atoi(weight);
            if(w > 0 && w < MAX_DIST)
                host->neighbor.push_back(RID2ID(atoi(rid2)));
        }
        if(atoi(rid2) == rid){
            set_weight(host, rid, atoi(rid1), atoi(weight));
            // host->next_hop[id1] = id1;
            // host->neighbor.push_back(atoi(rid1));
        }
        
        printf("rid1:%d, rid2:%d, weight:%d\n", atoi(rid1), atoi(rid2), atoi(weight));
    }
    printf("neighbor ids:");
    for(auto it = host->neighbor.begin(); it != host->neighbor.end(); ++it){
        printf("%d ", *it);
    }
    printf("\n");
    fclose(fp);
    return 0;
}

int router_init(char * loc_file_name, char * top_file_name, int rid){
    if(parse_loc_file(loc_file_name, rid, 0) < 0){
        perror("parse_loc_file error");
        return -1;
    }
    if(parse_top_file(top_file_name, rid) < 0){
        perror("parse_top_file error");
        return -1;
    }
    return 0;
}

int agent_init(char * loc_file_name){
    if(parse_loc_file(loc_file_name, 0, 1) < 0){
        perror("agent: parse_loc_file error");
        return -1;
    }
    return 0;
}


// the network part
int rp_socket(){
    return socket(AF_INET, SOCK_DGRAM, 0); 
}

int rp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}


int rp_sendto(int sockfd, int from_id, int to_id, const void * msg, int len, int type){
    char buffer[BUFFER_SIZE];
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(atoi(router_port[to_id]));

    // convert IPv4 or IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, router_ip[to_id], &receiver_addr.sin_addr)<=0) {
        perror("address failed");
        exit(EXIT_FAILURE);
    }
    
    rp_header_t * header = (rp_header_t *)buffer;
    header->fromid = from_id;
    header->len = len;
    header->type = type;
    memcpy((void *)buffer + sizeof(rp_header_t), msg, len);
    return sendto(sockfd, (void *)buffer, len + sizeof(rp_header_t), 0, (struct sockaddr *)&receiver_addr, sizeof(sockaddr_in));
}

int rp_recvfrom(int sockfd, int * from_id, int * type, void * buf, struct sockaddr *from){
    char buffer[BUFFER_SIZE];
    int bytes;
    socklen_t socklen;
    if((bytes = recvfrom(sockfd, (void *)buffer, BUFFER_SIZE, 0, from, &socklen)) < 0){
        printf("recvfrom error\n");
        return -1;
    }
    printf("received %d bytes\n", bytes);
    buffer[bytes] = '\0';
    rp_header_t * header = (rp_header_t *)buffer;
    *from_id = (int)header->fromid;
    *type = (int)header->type;
    memcpy((void *)buf, buffer + sizeof(rp_header_t), header->len);
    // printf("here\n");

    return bytes;
}

char * get_host_ip(){
    return host->ip;
}

char * get_host_port(){
    return host->port;
}

void set_socket(int sockfd){
    host->sockfd = sockfd;
}

int propagate(){
    int id = host->id;
    char msg[BUFFER_SIZE];
    int len = sizeof(host->dv);
    memcpy(msg, host->dv, len);
    for(auto it = host->neighbor.begin(); it != host->neighbor.end(); ++it){
        int nid = *it;
        if(rp_sendto(host->sockfd, id, nid, msg, len, ROUTER_DV) < 0){
            return -1;
        }
    }
    return 0;
}

int update_dv(int fromid, int **fromdv){
    memcpy(host->dv[fromid], fromdv[fromid], MAX_ROUTERN * sizeof(int));
}

int bellman_ford(){
    
}