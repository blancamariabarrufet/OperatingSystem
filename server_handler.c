/**********************************************
*
* @File : server_handler.c
* @Purpose : Server-side implementation for handling incoming frames and forwarding.
* @Author : Miquel Pla and Blanca Maria Barrufet
* @Date : 21/11/2025
*
***********************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>

#include "commands.h"
#include "network.h"
#include "server_handler.h"
#include "client_handler.h"

/* ========= HELPER FUNCTIONS ========= */

int openListenConnection(int port, const char *ip){
    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        write(1, "Socket creation failed\n", 23);
        return -1;
    }
    
    int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        write(1, "Setsockopt failed\n", 18);
        close(sockfd);
        return -1;
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);
    
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0){
        write(1, "Socket bind failed\n", 19);
        close(sockfd);
        return -1;
    }
    
    if(listen(sockfd, MAX_CONN) != 0){
        write(1, "Listen failed\n", 14);
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

void sendAck(int client_fd, maesterConfig *config, const char *msg){
    Frame ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = TYPE_ACK;
    
    /* Build origin as "IP:PORT" */
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(ack.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    const char *data = msg ? msg : "OK";
    size_t len = strlen(data);
    if(len > DATA_MAX){
        len = DATA_MAX;
    }
    
    memcpy(ack.data, data, len);
    ack.data_len = (uint16_t)len;
    set_checksum(&ack);
    
    
    write_exact(client_fd, &ack, sizeof(ack));
}

void sendAckDirect(const char *origin_ip_port, maesterConfig *config, const char *msg){
    Frame ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = TYPE_ACK;
    
    /* Build origin as "IP:PORT" */
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(ack.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    const char *data = msg ? msg : "OK";
    size_t len = strlen(data);
    if(len > DATA_MAX){
        len = DATA_MAX;
    }
    
    memcpy(ack.data, data, len);
    ack.data_len = (uint16_t)len;
    set_checksum(&ack);
    
    /* Parse origin to get IP and port */
    char ip_buf[20];
    int port;
    if(!parseOriginAddress(origin_ip_port, ip_buf, &port)){
        write(1, "[ERROR] Failed to parse origin address for direct ACK\n", 55);
        return;
    }
    
    //char pstr[16];
    //int plen = snprintf(pstr, sizeof(pstr), "%d", port);
    
    int direct_fd = connectToServer(ip_buf, port);
    if(direct_fd < 0){
        write(1, "[ERROR] Failed to connect directly for ACK\n", 44);
        return;
    }
    
    write_exact(direct_fd, &ack, sizeof(ack));
    close(direct_fd);
}

void sendMD5Check(int client_fd, maesterConfig *config, const char *md5){
    Frame md5_frame;
    memset(&md5_frame, 0, sizeof(md5_frame));
    md5_frame.type = TYPE_MD5_CHECK;
    
    /* Build origin as "IP:PORT" */
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(md5_frame.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    const char *data = md5 ? md5 : "CHECK_OK";
    size_t len = strlen(data);
    if(len > DATA_MAX){
        len = DATA_MAX;
    }
    
    memcpy(md5_frame.data, data, len);
    md5_frame.data_len = (uint16_t)len;
    set_checksum(&md5_frame);
    
    
    write_exact(client_fd, &md5_frame, sizeof(md5_frame));
}

void sendMD5CheckDirect(const char *origin_ip_port, maesterConfig *config, const char *md5){
    Frame md5_frame;
    memset(&md5_frame, 0, sizeof(md5_frame));
    md5_frame.type = TYPE_MD5_CHECK;
    
    /* Build origin as "IP:PORT" */
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(md5_frame.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    const char *data = md5 ? md5 : "CHECK_OK";
    size_t len = strlen(data);
    if(len > DATA_MAX){
        len = DATA_MAX;
    }
    
    memcpy(md5_frame.data, data, len);
    md5_frame.data_len = (uint16_t)len;
    set_checksum(&md5_frame);
    
    /* Parse origin to get IP and port */
    char ip_buf[20];
    int port;
    if(!parseOriginAddress(origin_ip_port, ip_buf, &port)){
        write(1, "[ERROR] Failed to parse origin address for direct MD5\n", 55);
        return;
    }
    
    //char pstr[16];
    //int plen = snprintf(pstr, sizeof(pstr), "%d", port);
    
    int direct_fd = connectToServer(ip_buf, port);
    if(direct_fd < 0){
        write(1, "[ERROR] Failed to connect directly for MD5 Check\n", 50);
        return;
    }
    
    write_exact(direct_fd, &md5_frame, sizeof(md5_frame));
    close(direct_fd);
}

void sendNack(int client_fd, const char *reason){
    Frame nack;
    memset(&nack, 0, sizeof(nack));
    nack.type = TYPE_NACK;
    
    const char *data = reason ? reason : "ERROR";
    size_t len = strlen(data);
    if(len > DATA_MAX){
        len = DATA_MAX;
    }
    
    memcpy(nack.data, data, len);
    nack.data_len = (uint16_t)len;
    set_checksum(&nack);
    
    
    write_exact(client_fd, &nack, sizeof(nack));
}

/* ========= SERVER THREAD ========= */

void* network_server_thread(void *arg){
    ServerThreadData *data = (ServerThreadData *)arg;
    maesterConfig *config = data->config;
    PendingAllianceList *pending_list = data->pending_list;
    pthread_mutex_t *pending_mutex = data->pending_mutex;
    ProtocolState *protocol_state_sync = data->protocol_state;
    int *running = data->running;
    
    int server_fd = openListenConnection(config->listen_port, config->listen_ip);
    if(server_fd < 0){
        write(1, "Failed to start server\n", 23);
        return NULL;
    }
    
    /* Store server_fd so main thread can close it if needed */
    *(data->server_fd) = server_fd;
    
    while(*running){
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if(client_fd < 0){
            if(*running){
                continue;
            }
            break;
        }
        
        
        /* Temporary storage for this connection's alliance data */
        PendingAllianceEntry temp_entry;
        memset(&temp_entry, 0, sizeof(temp_entry));
        temp_entry.client_fd = -1;  // Mark as uninitialized
        
        /* Protocol state tracking */
        int protocol_state = 0;  // 0=waiting for request, 1=waiting for sigil, 2=waiting for final ACK
        
        /* Handle frames from this client */
        int protocol_complete = 0;
        while(*running && !protocol_complete){
            Frame f;
            ssize_t n = read_exact(client_fd, &f, sizeof(f));
            
            if(n <= 0){
                break;
            }
            
            /* Verify checksum */
            if(!verify_checksum(&f)){
                sendNack(client_fd, "CHECKSUM_ERROR");
                close(client_fd);
                break;
            }
            
            
            /* Response frames (ACK, MD5_CHECK) and DISCONNECT are ALWAYS for me - don't forward */
            if(f.type == TYPE_ACK || f.type == TYPE_MD5_CHECK || f.type == TYPE_DISCONNECT){
                /* These are responses/notifications - process locally, don't forward */
                /* Fall through to local processing below */
            }
            /* Check if this frame is destined for ME or should be FORWARDED */
            else if(strcasecmp(f.dest, config->name) != 0){
                /* Frame is NOT for me - forward it */
                
                /* Extract origin realm name from IP:PORT */
                char origin_ip[20];
                int origin_port;
                char *origin_realm = "UNKNOWN";
                if(parseOriginAddress(f.origin, origin_ip, &origin_port)){
                    /* Find realm name by IP:Port */
                    for(int i = 0; i < config->route_count; i++){
                        if(config->routes[i].realm_name != NULL &&
                           config->routes[i].is_known &&
                           strcmp(config->routes[i].ip, origin_ip) == 0 &&
                           config->routes[i].port == origin_port){
                            origin_realm = config->routes[i].realm_name;
                            break;
                        }
                    }
                }
                
                /* Display hop message */
                write(1, ">>> Received hop: ", 18);
                write(1, origin_realm, strlen(origin_realm));
                write(1, " -> ", 4);
                write(1, f.dest, strnlen(f.dest, DEST_SIZE));
                write(1, "\n", 1);
                
                /* Look up destination in routing table */
                const char *next_hop_ip = NULL;
                int next_hop_port = 0;
                int found = 0;
                char *route_name = NULL;
                
                for(int i = 0; i < config->route_count; i++){
                    if(config->routes[i].realm_name != NULL &&
                       strcasecmp(config->routes[i].realm_name, f.dest) == 0){
                        next_hop_ip = config->routes[i].ip;
                        next_hop_port = config->routes[i].port;
                        route_name = config->routes[i].realm_name;
                        found = 1;
                        break;
                    }
                }
                
                if(!found){
                    /* Destination not in routing table - use DEFAULT */
                    routeEntry *default_route = findDefaultRoute(config);
                    if(default_route != NULL){
                        next_hop_ip = default_route->ip;
                        next_hop_port = default_route->port;
                        route_name = default_route->realm_name;
                        found = 1;
                    }
                }
                
                if(found){
                    /* Display route found */
                    write(1, "Found route: ", 13);
                    write(1, route_name, strlen(route_name));
                    write(1, " -> ", 4);
                    write(1, next_hop_ip, strlen(next_hop_ip));
                    write(1, ":", 1);
                    char *pstr;
                    asprintf(&pstr, "%d", next_hop_port);
                    write(1, pstr, strlen(pstr));
                    free(pstr);
                    write(1, "\n", 1);
                    write(1, "Forwarding PLEDGE request...\n$ ", 32);
                    
                    /* Forward frame to next hop and relay responses DIRECTLY to origin */
                    
                    /* Connect to next hop */
                    int forward_sock = connectToServer(next_hop_ip, next_hop_port);
                    if(forward_sock < 0){
                        write(1, "Error: Failed to connect to next hop\n$ ", 40);
                        close(client_fd);
                        break;
                    }
                    
                    /* Send initial frame to next hop */
                    write_exact(forward_sock, &f, sizeof(f));
                    
                    /* Relay loop: forward client frames to next hop */
                    while(*running){
                        /* Read next frame from client */
                        Frame client_frame;
                        ssize_t client_n = read_exact(client_fd, &client_frame, sizeof(client_frame));
                        
                        if(client_n <= 0){
                            break;
                        }
                        
                        
                        write_exact(forward_sock, &client_frame, sizeof(client_frame));
                    }
                    
                    close(forward_sock);
               }else{
                    /* No route found */
                    write(1, "Error: No route found for destination ", 38);
                    write(1, f.dest, strnlen(f.dest, DEST_SIZE));
                    write(1, "\n$ ", 3);
                }
                
                /* Close connection after forwarding */
                close(client_fd);
                break;
            }
            
            /* Frame IS for me - process it locally */
            
            /* Special case: Alliance RESPONSE (0x03) - response to OUR outgoing pledge */
            if(f.type == TYPE_ALLIANCE_RESPONSE){
                
                int is_accept = (strncmp(f.data, "ACCEPT", 6) == 0);
                
                //int lock_retries = 0;
                while(pthread_mutex_trylock(&protocol_state_sync->mutex) != 0){
                     usleep(100000); // 0.1s
                }
                
                if(is_accept){
                    protocol_state_sync->alliance_response_received = 1;
                    
                    /* Update routing table with IP:Port from response origin */
                    char ip_buf[20];
                    int port;
                    if(parseOriginAddress(f.origin, ip_buf, &port)){
                        
                        
                        /* Find which realm in routing table matches this IP:Port 
                         * OR find a realm with unknown IP that we're expecting alliance from */
                        char *realm_name = NULL;
                        for(int i = 0; i < config->route_count; i++){
                            if(config->routes[i].realm_name != NULL){
                                
                                /* First try: Match by IP and Port (for known realms) */
                                if(config->routes[i].is_known && 
                                   strcmp(config->routes[i].ip, ip_buf) == 0 && 
                                   config->routes[i].port == port){
                                    realm_name = config->routes[i].realm_name;
                                    break;
                                }
                                
                                /* Second try: Match unknown realms (is_known=0) with no active alliance
                                 * This handles the case where we're receiving response for a realm
                                 * we just pledged to but haven't updated yet */
                                if(!config->routes[i].is_known && 
                                   !config->routes[i].active_alliance &&
                                   config->routes[i].is_default == 0){
                                    /* This could be the realm we're pledging to */
                                    realm_name = config->routes[i].realm_name;
                                    break;
                                }
                            }
                        }
                        
                        if(realm_name != NULL){
                            if(updateRouteEntry(config, realm_name, ip_buf, port)){
                            }else{
                            }
                        }else{
                            //char pstr2[16];
                            //int plen2 = snprintf(pstr2, sizeof(pstr2), "%d", port);
                        }
                    }
                }else{
                    protocol_state_sync->alliance_response_received = -1;
                }
                pthread_mutex_unlock(&protocol_state_sync->mutex);
                
                /* Send ACK back to acknowledge */
                Frame ack;
                memset(&ack, 0, sizeof(ack));
                ack.type = TYPE_ACK;
                const char *msg = "OK";
                memcpy(ack.data, msg, strlen(msg));
                ack.data_len = strlen(msg);
                set_checksum(&ack);
                
                if(write_exact(client_fd, &ack, sizeof(ack)) <= 0){
                }else{
                }
                
                usleep(100000); /* Wait 100ms to ensure ACK is flushed */
                protocol_complete = 1; /* Prevent double close */
                close(client_fd);
                break;
            }
            
            /* Special case: ACK (0x31) - response to OUR outgoing request/sigil */
            else if(f.type == TYPE_ACK){
                pthread_mutex_lock(&protocol_state_sync->mutex);
                if(protocol_state_sync->expect_ack){
                    protocol_state_sync->ack_received = 1;
                    protocol_state_sync->expect_ack = 0;
                    pthread_mutex_unlock(&protocol_state_sync->mutex);
                    
                    /* Close connection as it was a direct response */
                    close(client_fd);
                    break;
                }
                pthread_mutex_unlock(&protocol_state_sync->mutex);
                
                if(protocol_state == 2){
                     /* This is the final ACK for an INCOMING request - handled below */
                     /* Fall through to protocol state machine */
                }else{
                    close(client_fd);
                    break;
                }
            }
            
            /* Special case: MD5_CHECK (0x32) - response to OUR outgoing sigil */
            else if(f.type == TYPE_MD5_CHECK){
                pthread_mutex_lock(&protocol_state_sync->mutex);
                if(protocol_state_sync->expect_md5){
                    
                    /* Copy MD5 data */
                    uint16_t dlen = f.data_len;
                    if(dlen > 32) dlen = 32;
                    memcpy(protocol_state_sync->md5_data, f.data, dlen);
                    protocol_state_sync->md5_data[dlen] = '\0';
                    
                    protocol_state_sync->md5_received = 1;
                    protocol_state_sync->expect_md5 = 0;
                    pthread_mutex_unlock(&protocol_state_sync->mutex);
                    
                    /* Close connection as it was a direct response */
                    close(client_fd);
                    break;
                }
                pthread_mutex_unlock(&protocol_state_sync->mutex);
                
                close(client_fd);
                break;
            }
            
            /* Handle incoming alliance REQUEST protocol frames */
            if(f.type == TYPE_ALLIANCE_REQUEST && protocol_state == 0){
                /* Step 1: Receive alliance request - store temporarily */
                uint16_t dlen = f.data_len;
                if(dlen > DATA_MAX) dlen = DATA_MAX;
                
                char data_buf[DATA_MAX + 1];
                memcpy(data_buf, f.data, dlen);
                data_buf[dlen] = '\0';
                
                char *realm_name = strtok(data_buf, "&");
                char *sigil_name = strtok(NULL, "&");
                char *file_size = strtok(NULL, "&");
                char *md5sum = strtok(NULL, "&");
                
                (void)sigil_name;
                (void)file_size;
                
                
                /* Store in temporary entry */
                temp_entry.client_fd = client_fd;
                
                if(realm_name != NULL){
                    char *temp_realm;
                    asprintf(&temp_realm, "%s", realm_name);
                    snprintf(temp_entry.realm_name, sizeof(temp_entry.realm_name), "%s", temp_realm);
                    free(temp_realm);
                }else{
                    char *temp_realm;
                    asprintf(&temp_realm, "UNKNOWN");
                    snprintf(temp_entry.realm_name, sizeof(temp_entry.realm_name), "%s", temp_realm);
                    free(temp_realm);
                }
                
                /* Extract IP and port from frame origin */
                char *origin_copy;
                asprintf(&origin_copy, "%s", f.origin);
                char *ip_part = strtok(origin_copy, ":");
                char *port_part = strtok(NULL, ":");
                
                if(ip_part != NULL){
                    char *temp_ip;
                    asprintf(&temp_ip, "%s", ip_part);
                    snprintf(temp_entry.ip, sizeof(temp_entry.ip), "%s", temp_ip);
                    free(temp_ip);
                }else{
                    char *temp_ip;
                    asprintf(&temp_ip, "unknown");
                    snprintf(temp_entry.ip, sizeof(temp_entry.ip), "%s", temp_ip);
                    free(temp_ip);
                }
                
                if(port_part != NULL){
                    temp_entry.port = atoi(port_part);
                }else{
                    temp_entry.port = 0;
                }
                
                free(origin_copy);
                
                if(md5sum != NULL){
                    char *temp_md5;
                    asprintf(&temp_md5, "%s", md5sum);
                    snprintf(temp_entry.md5sum, sizeof(temp_entry.md5sum), "%s", temp_md5);
                    free(temp_md5);
                }else{
                    char *temp_md5;
                    asprintf(&temp_md5, "0123456789abcdef0123456789abcdef");
                    snprintf(temp_entry.md5sum, sizeof(temp_entry.md5sum), "%s", temp_md5);
                    free(temp_md5);
                }
                
                memcpy(&temp_entry.received_frame, &f, sizeof(Frame));
                
                /* Send ACK - ready for sigil - DIRECTLY to origin */
                sendAckDirect(f.origin, config, "OK");
                
                protocol_state = 1;  // Now waiting for sigil data
                
            }else if(f.type == TYPE_SIGIL_DATA && protocol_state == 1){
                /* Step 2: Receive sigil data */
                uint16_t dlen = f.data_len;
                if(dlen > DATA_MAX) dlen = DATA_MAX;
                
                char data_buf[DATA_MAX + 1];
                memcpy(data_buf, f.data, dlen);
                data_buf[dlen] = '\0';
                
                
                /* Send MD5 check - DIRECTLY to origin */
                if(temp_entry.client_fd != -1){
                    sendMD5CheckDirect(temp_entry.received_frame.origin, config, temp_entry.md5sum);
                }else{
                    sendMD5CheckDirect(f.origin, config, "0123456789abcdef0123456789abcdef");
                }
                
                protocol_state = 2;  // Now waiting for final ACK
                
            }else if(f.type == TYPE_ACK && protocol_state == 2){
                /* Step 3: Receive final ACK after MD5 - NOW add to pending list */
                
                if(temp_entry.client_fd != -1){
                    /* Add to pending alliance list */
                    pthread_mutex_lock(pending_mutex);
                    
                    /* Resize array if needed */
                    if(pending_list->count >= pending_list->capacity){
                        int new_capacity = pending_list->capacity == 0 ? 4 : pending_list->capacity * 2;
                        PendingAllianceEntry *new_entries = realloc(pending_list->entries, 
                                                                     new_capacity * sizeof(PendingAllianceEntry));
                        if(new_entries == NULL){
                            write(1, "Error: Failed to allocate memory for pending alliances\n", 55);
                            pthread_mutex_unlock(pending_mutex);
                            sendNack(client_fd, "SERVER_ERROR");
                            close(client_fd);
                            break;
                        }
                        pending_list->entries = new_entries;
                        pending_list->capacity = new_capacity;
                    }
                    
                    /* Copy temp entry to list */
                    pending_list->entries[pending_list->count] = temp_entry;
                    pending_list->count++;
                    
                    pthread_mutex_unlock(pending_mutex);
                    
                    write(1, ">>> Alliance request received from ", 36);
                    write(1, temp_entry.realm_name, strlen(temp_entry.realm_name));
                    write(1, "\n$ ", 3);
                    
                    protocol_complete = 1;  // Keep connection open for response
                }else{
                    write(1, "Error: Received ACK but no alliance request data\n", 49);
                    close(client_fd);
                    break;
                }
                
            }
            /* Handle PRODUCT LIST REQUEST (0x11) */
            else if(f.type == TYPE_PRODUCT_LIST_REQ){
                //uint16_t dlen = f.data_len;
                //if(dlen > DATA_MAX) dlen = DATA_MAX;
                
                /* Send ACK directly to origin */
                sendAckDirect(f.origin, config, "PRODUCT_LIST_OK");
                
                close(client_fd);
                break;
            }
            /* Handle ORDER REQUEST HEADER (0x14) */
            else if(f.type == TYPE_ORDER_HEADER){
                
                /* Parse order data: FileName&FileSize&MD5SUM */
                uint16_t dlen = f.data_len;
                if(dlen > DATA_MAX) dlen = DATA_MAX;
                char data_buf[DATA_MAX + 1];
                memcpy(data_buf, f.data, dlen);
                data_buf[dlen] = '\0';
                
                char *file_name = strtok(data_buf, "&");
                char *file_size = strtok(NULL, "&");
                char *md5sum = strtok(NULL, "&");
                
                //if(file_name) write(1, file_name, strlen(file_name));
                //if(file_size) write(1, file_size, strlen(file_size));
                //if(md5sum) write(1, md5sum, strlen(md5sum));
                
                
                (void)file_name;
                (void)file_size;
                (void)md5sum;
                
                /* Extract realm name from origin */
                char ip_buf[20];
                int port;
                if(parseOriginAddress(f.origin, ip_buf, &port)){
                    /* Find realm in routing table by IP:Port */
                    for(int i = 0; i < config->route_count; i++){
                        if(config->routes[i].realm_name != NULL &&
                           config->routes[i].is_known &&
                           strcmp(config->routes[i].ip, ip_buf) == 0 &&
                           config->routes[i].port == port){
                            write(1, ">>> Trade request received from ", 32);
                            write(1, config->routes[i].realm_name, strlen(config->routes[i].realm_name));
                            write(1, "\n", 1);
                            break;
                        }
                    }
                }
                
                write(1, "Order processed successfully. Stock updated.\n$ ", 47);
                
                /* Send ACK FITXER directly to origin */
                sendAckDirect(f.origin, config, "ORDER_RECEIVED");
                
                close(client_fd);
                break;
            }
            /* Handle DISCONNECT (0x27) */
            else if(f.type == TYPE_DISCONNECT){
                /* Parse origin to get IP and port */
                char ip_buf[20];
                int port;
                int found_index = -1;
                
                if(parseOriginAddress(f.origin, ip_buf, &port)){
                    /* First try: Find realm by IP:Port */
                    for(int i = 0; i < config->route_count; i++){
                        if(config->routes[i].realm_name != NULL &&
                           config->routes[i].is_known && 
                           strcmp(config->routes[i].ip, ip_buf) == 0 && 
                           config->routes[i].port == port &&
                           config->routes[i].is_default == 0){  // Don't match DEFAULT
                            found_index = i;
                            break;
                        }
                    }
                }
                
                /* Second try: If not found by IP:Port, try matching by destination (ally realm looking up sender) */
                if(found_index == -1 && f.dest[0] != '\0'){
                    for(int i = 0; i < config->route_count; i++){
                        if(config->routes[i].realm_name != NULL &&
                           strcasecmp(config->routes[i].realm_name, config->name) == 0){
                            /* This disconnect is addressed to me, find the disconnecting realm */
                            /* The origin should be the disconnecting realm's name from their perspective */
                            /* We need to search for active alliances and match somehow */
                            continue;
                        }
                    }
                    /* Try to find any realm with active alliance that matches expected pattern */
                    for(int i = 0; i < config->route_count; i++){
                        if(config->routes[i].realm_name != NULL && config->routes[i].active_alliance == 1 && config->routes[i].is_default == 0){
                            found_index = i;
                            break;
                        }
                    }
                }
                
                if(found_index >= 0){
                    config->routes[found_index].active_alliance = 0;
                }
                
                close(client_fd);
                break;
            }
            else{
                write(1, "[RECEIVED] Unexpected frame type: 0x", 36);
                char *type_str;
                asprintf(&type_str, "%02X", f.type);
                write(1, type_str, strlen(type_str));
                free(type_str);
                write(1, " (state=", 8);
                char *state_str;
                asprintf(&state_str, "%d", protocol_state);
                write(1, state_str, strlen(state_str));
                free(state_str);
                write(1, ")\n", 2);
            }
        }
        
        /* Don't close connection yet - wait for user to accept/reject */
        if(!protocol_complete){
            close(client_fd);
        }
    }
    
    close(server_fd);
    return NULL;
}

/* ========= PLEDGE RESPOND (ACCEPT/REJECT) ========= */

int network_pledge_respond(char *realm_name, char *decision, maesterConfig *config, PendingAllianceList *pending_list, pthread_mutex_t *pending_mutex){
    
    pthread_mutex_lock(pending_mutex);
    
    /* Check if there are any pending alliances */
    if(pending_list->count == 0){
        write(1, "No pending alliance requests.\n", 30);
        pthread_mutex_unlock(pending_mutex);
        return -1;
    }
    
    /* Search for the realm in the pending list */
    int found_index = -1;
    for(int i = 0; i < pending_list->count; i++){
        if(strcasecmp(pending_list->entries[i].realm_name, realm_name) == 0){
            found_index = i;
            break;
        }
        return -1;
    }
    
    PendingAllianceEntry *entry = &pending_list->entries[found_index];
    
    /* Determine ACCEPT or REJECT */
    int accept = 0;
    if(strcasecmp(decision, "accept") == 0){
        accept = 1;
    }else if(strcasecmp(decision, "reject") != 0){
        write(1, "Error: Decision must be 'accept' or 'reject'\n", 45);
        pthread_mutex_unlock(pending_mutex);
        return -1;
    }
    
    /* Send alliance response (0x03) DIRECTLY to client's IP:Port */
    Frame resp;
    memset(&resp, 0, sizeof(resp));
    resp.type = TYPE_ALLIANCE_RESPONSE;
    
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(resp.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    /* Destination is the realm NAME, not IP:Port */
    char *temp_dest;
    asprintf(&temp_dest, "%s", entry->realm_name);
    snprintf(resp.dest, DEST_SIZE, "%s", temp_dest);
    free(temp_dest);
    
    const char *response_text = accept ? "ACCEPT" : "REJECT";
    size_t len = strlen(response_text);
    memcpy(resp.data, response_text, len);
    resp.data_len = (uint16_t)len;
    set_checksum(&resp);
    
    
    /* Extract IP:Port from stored origin to connect directly */
    char client_ip[20];
    int client_port;
    if(!parseOriginAddress(entry->received_frame.origin, client_ip, &client_port)){
        write(1, "Error: Failed to parse client address from origin\n", 51);
        pthread_mutex_unlock(pending_mutex);
        return -1;
    }
    
    
    int direct_fd = connectToServer(client_ip, client_port);
    if(direct_fd < 0){
        write(1, "Error: Failed to connect directly to client\n", 44);
        pthread_mutex_unlock(pending_mutex);
        return -1;
    }
    
    
    write_exact(direct_fd, &resp, sizeof(resp));
    
    /* Wait for ACK (0x31) from client on this direct connection */
    Frame ack;
    ssize_t n = read_exact(direct_fd, &ack, sizeof(ack));
    if(n > 0){
        
        if(ack.type == TYPE_ACK){
        }
    }
    
    close(direct_fd);
    
    if(accept){
        updateRouteEntry(config, entry->realm_name, client_ip, client_port);
    }
    
    for(int i = found_index; i < pending_list->count - 1; i++){
        pending_list->entries[i] = pending_list->entries[i+1];
    }
    pending_list->count--;
    
    pthread_mutex_unlock(pending_mutex);
    return 0;
}
