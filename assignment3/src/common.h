#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <map>
#include <vector>

#define ROUTER_DV     0
#define AGENT_DV      1
#define AGENT_UPDATE  2
#define AGENT_SHOW    3
#define AGENT_RESET   4
#define ROUTER_SHOW   5

#define RID2ID(rid)  (rid2id[rid])
#define ID2RID(id)   (id2rid[id])

#define BUFFER_SIZE 2048
#define MAX_ROUTERN 10
#define MAX_DIST    1000  // 1000+ means not able to reach

typedef struct __attribute__ ((__packed__)) RP_header {
    uint8_t type;       // 0: ROUTER_DV; 1: AGENT_DV; 2: AGENT_UPDATE; 3: AGENT_SHOW; 4: AGENT_RESET
    uint32_t fromid;
    uint32_t len;
} rp_header_t;

typedef struct RP_router{ 
    int sockfd;
    uint32_t id; // note: all ids inside the protocol are IDs, RIDs are only used when interacting with the outside.
    uint32_t rid; // rid: the real id; id: the mapped rid(0-10)
    // struct sockaddr_in addr; // socket address
    char * ip;
    char * port;
    int dv[MAX_ROUTERN][MAX_ROUTERN]; // distance vectors
    int next_hop[MAX_ROUTERN]; // next hsop vector 
    std::vector<int> neighbor; // neighbors' id. never change.
} router_t;

static std::map<int, int> rid2id, id2rid;
static router_t * host = NULL; // for router only
static char * router_ip[MAX_ROUTERN], *router_port[MAX_ROUTERN]; // for agent only
// static int dist[MAX_ROUTERN][MAX_ROUTERN];
static int router_num;

int parse_loc_file(char * filename, int rid, int agent);

int parse_top_file(char * filename, int rid);

int router_init(char * loc_file_name, char * top_file_name, int rid);

int agent_init(char * loc_file_name);

int rp_socket();

int rp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int rp_sendto(int sockfd, int from_id, int to_id, const void * msg, int len, int type);

int rp_recvfrom(int sockfd, int * from_id, int * type, void * buf, struct sockaddr * from);

char * get_host_ip();

char * get_host_port();

int propagate();

int show_dv(char* buffer);

void set_socket(int sockfd);
// int print_router_info(router_t r);

#endif