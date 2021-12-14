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

#define DV 1
#define UPDATE 2
#define SHOW 3
#define RESET 4

// using namespace std;

int parse_cmd(char *cmd, int *argv){
    if(cmd[0] == 'd'){
        if(strlen(cmd) != 3 && strstr(cmd, "dv")){
            return -1;
        }
        return DV;
    }
    else if(cmd[0] == 'u'){
        char tmp[20];
        strcpy(tmp, cmd);
        tmp[7] = '\0';
        if(strcmp(tmp, "update:")){
            return -1;
        }
        strcpy(tmp, cmd + 7);
        const char s[2] = ",";
        char *tmpstr = NULL;
        for (int i = 0; i < 3; ++i){
            if(i == 0)
                tmpstr = strtok(tmp, s);
            else
                tmpstr = strtok(NULL, s);
            if(!tmpstr){
                return -1;
            }
            if(i == 2){
                int len = strlen(tmpstr);
                tmpstr[len - 1] = '\0';
                argv[i] = atoi(tmpstr);
                if(argv[i] <= 0){
                    argv[i] = MAX_DIST;
                }
            }
            else{
                argv[i] = RID2ID(atoi(tmpstr));
            }
            // printf("%d ", argv[i]);
        }
        // printf("\n");
        return UPDATE;
        // argv[0] = atoi(strtok(tmp, s));
        // argv[1] = atoi(strtok(NULL, s));
        // argv[2] = atoi(strtok(NULL, s));
    }
    else if(cmd[0] == 's'){
        char tmp[20];
        strcpy(tmp, cmd);
        tmp[5] = '\0';
        if(strcmp(tmp, "show:")){
            return -1;
        }
        strcpy(tmp, cmd + 5);
        int len = strlen(tmp);
        tmp[len - 1] = '\0';

        // for(int i = 0; i < 4; ++i){
        //     printf("id: %d, rid: %d\n", i, ID2RID(i));
        // }

        argv[0] = RID2ID(atoi(tmp));
        // printf("%d\n", argv[0]);
        // printf("%s\n", tmp);

        // printf("\n");
        return SHOW;
    }
    else if(cmd[0] == 'r'){
        char tmp[20];
        strcpy(tmp, cmd);
        tmp[6] = '\0';
        if(strcmp(tmp, "reset:")){
            return -1;
        }
        strcpy(tmp, cmd + 6);
        int len = strlen(tmp);
        tmp[len - 1] = '\0';
        argv[0] = RID2ID(atoi(tmp));
        printf("%d\n", argv[0]);
        // printf("\n");
        return RESET;
    }
    return -1;
}

int agent(){
    int sockfd = rp_socket();
    // rp_bind(sockfd, (sockaddr *)&host->addr, sizeof(sockaddr_in));
    int fromid;
    size_t len = 0;
    char * cmd = NULL;
    while(getline(&cmd, &len, stdin) != -1){
        // struct sockaddr_in addr;
        // memset(&addr, 0, sizeof(addr));
        // addr.sin_family = AF_INET;
        // addr.sin_port = htons(atoi("13001"));
        // if(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr)<=0) {
        //     perror("address failed");
        //     return -1;
        // }
        // sendto(sockfd, "hello", 6, 0, (sockaddr *)&addr, sizeof(sockaddr_in));

        // int id = atoi(cmd);
        // int ret = rp_sendto(sockfd, -1, id, "hello", 10, AGENT_SHOW);
        // printf("sent %d bytes to router %d\n", ret, id);
        int argv[4];
        char msg[BUFFER_SIZE];
        switch(parse_cmd(cmd, argv)){
            case(DV):{
                // rp_sendto(sockfd, -1, 0, msg, 0, AGENT_DV);
                int k, n = get_routernum();
                for(k = 0; k < n; ++k){
                    rp_sendto(sockfd, -1, k, msg, 0, AGENT_DV);
                }
                printf("cmd dv ends\n");
                break;
            }
            case(UPDATE):{
                uint32_t m[3];
                for(int i = 0; i < 3; ++i){
                    m[i] = htonl((uint32_t) argv[i]);
                }
                rp_sendto(sockfd, -1, argv[0], (void *)m, 3 * sizeof(uint32_t), AGENT_UPDATE);
                break;
            }
            case(SHOW):{
                rp_sendto(sockfd, -1, argv[0], msg, 0, AGENT_SHOW);
                char buffer[BUFFER_SIZE];
                // int fromid, type;
                recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, 0);
                printf("%s", buffer);
                break;
            }
            case(RESET):{
                rp_sendto(sockfd, -1, argv[0], msg, 0, AGENT_RESET);
                break;
            }
            default:{
                printf("error: no such command\n");
            }
        }

    }
    free(cmd);
    return 0;
}

int main(int argc, char **argv){
    setvbuf(stdout, NULL, _IONBF, 0);
    if(argc < 2){
        printf("error: too less args\n");
        exit(-1);
    }
    else if(argc > 2){
        printf("error: too much args\n");
        exit(-1);
    }
    char * loc_file_name = argv[1];
    if(agent_init(loc_file_name) < 0){
        printf("error: agent init failed\n");
        exit(-1);
    }
        //     for(int i = 0; i < 4; ++i){
        //     printf("id: %d, rid: %d\n", i, ID2RID(i));
        // }
    return agent();
}