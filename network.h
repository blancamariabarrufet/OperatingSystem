/**********************************************
*
* @File : network.h
* @Purpose : Public API for Maester network communication and frame protocol.
* @Author : Miquel Pla and Blanca Maria Barrufet
* @Date : 21/11/2025
*
***********************************************/

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <pthread.h>
#include "config.h"
#include "inventory.h"

#include "commands.h"


#define FRAME_SIZE   320
#define ORIGIN_SIZE  20
#define DEST_SIZE    20
#define DATA_MAX     (FRAME_SIZE - 1 - ORIGIN_SIZE - DEST_SIZE - 2 - 2)
#define MAX_CONN     5


#define TYPE_ALLIANCE_REQUEST   0x01
#define TYPE_SIGIL_DATA         0x02
#define TYPE_ALLIANCE_RESPONSE  0x03
#define TYPE_PRODUCT_LIST_REQ   0x11
#define TYPE_ORDER_HEADER       0x14
#define TYPE_ACK                0x31
#define TYPE_MD5_CHECK          0x32
#define TYPE_NACK               0x69
#define TYPE_DISCONNECT         0x27


#pragma pack(push, 1)
typedef struct {
    uint8_t  type;
    char     origin[ORIGIN_SIZE];
    char     dest[DEST_SIZE];
    uint16_t data_len;
    char     data[DATA_MAX];
    uint16_t checksum;
} Frame;
#pragma pack(pop)

_Static_assert(sizeof(Frame) == FRAME_SIZE, "Frame must be 320 bytes");


typedef struct PendingAllianceEntry {
    int client_fd;
    char realm_name[DEST_SIZE];
    char ip[20];
    int port;
    char md5sum[33];
    Frame received_frame;
} PendingAllianceEntry;

typedef struct PendingAllianceList {
    PendingAllianceEntry *entries;
    int count;
    int capacity;
} PendingAllianceList;


typedef struct ProtocolState {
    pthread_mutex_t mutex;
    int expect_ack;
    int ack_received;
    int expect_md5;
    int md5_received;
    char md5_data[33];
    int alliance_response_received;  // 0=waiting, 1=ACCEPT, -1=REJECT
} ProtocolState;


typedef struct {
    maesterConfig *config;
    PendingAllianceList *pending_list;
    pthread_mutex_t *pending_mutex;
    ProtocolState *protocol_state;
    int *running;
    int *server_fd; 
} ServerThreadData;


int network_pledge_alliance(char *realm_name, char *sigil_file, ThreadData *threadData);


void* network_server_thread(void *arg);


int network_pledge_respond(char *realm_name, char *decision, maesterConfig *config, PendingAllianceList *pending_list, pthread_mutex_t *pending_mutex);



void set_checksum(Frame *f);
int verify_checksum(Frame *f);
void printFrame(const char *direction, Frame *f);


ssize_t write_exact(int fd, const void *buf, size_t count);
ssize_t read_exact(int fd, void *buf, size_t count);


routeEntry* findDefaultRoute(maesterConfig *config);
int updateRouteEntry(maesterConfig *config, const char *realm_name, const char *ip, int port);
int parseOriginAddress(const char *origin, char *ip_buf, int *port);

#endif /* NETWORK_H */
