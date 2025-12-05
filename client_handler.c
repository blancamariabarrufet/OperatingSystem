/**********************************************
*
* @File : client_handler.c
* @Purpose : Client-side implementation for sending alliance requests.
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
#include <time.h>
#include <pthread.h>

#include "commands.h"
#include "network.h"
#include "client_handler.h"

int connectToServer(const char *ip, int port){
    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        write(1, "Socket creation failed\n", 23);
        return -1;
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);
    
    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1){
        write(1, "Connection failed\n", 18);
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int waitForDirectAck(ProtocolState *state, int timeout_sec){
    
    time_t start_time = time(NULL);
    
    pthread_mutex_lock(&state->mutex);
    state->expect_ack = 1;
    state->ack_received = 0;
    pthread_mutex_unlock(&state->mutex);
    
    while(1){
        sleep(1);  
        
        pthread_mutex_lock(&state->mutex);
        int received = state->ack_received;
        pthread_mutex_unlock(&state->mutex);
        
        if(received){
            pthread_mutex_lock(&state->mutex);
            state->expect_ack = 0;
            pthread_mutex_unlock(&state->mutex);
            return 0;
        }
        
        if(time(NULL) - start_time >= timeout_sec){
            write(1, "Error: Timeout waiting for direct ACK\n", 38);
            pthread_mutex_lock(&state->mutex);
            state->expect_ack = 0;
            pthread_mutex_unlock(&state->mutex);
            return -1;
        }
    }
}

int waitForDirectMD5(ProtocolState *state, int timeout_sec){
    
    time_t start_time = time(NULL);
    
    pthread_mutex_lock(&state->mutex);
    state->expect_md5 = 1;
    state->md5_received = 0;
    pthread_mutex_unlock(&state->mutex);
    
    while(1){
        sleep(1);  
        
        pthread_mutex_lock(&state->mutex);
        int received = state->md5_received;
        pthread_mutex_unlock(&state->mutex);
        
        if(received){
            pthread_mutex_lock(&state->mutex);
            state->expect_md5 = 0;
            pthread_mutex_unlock(&state->mutex);
            return 0;
        }
        
        if(time(NULL) - start_time >= timeout_sec){
            write(1, "Error: Timeout waiting for direct MD5 Check\n", 44);
            pthread_mutex_lock(&state->mutex);
            state->expect_md5 = 0;
            pthread_mutex_unlock(&state->mutex);
            return -1;
        }
    }
}

int sendAllianceRequest(int sockfd, maesterConfig *config, const char *realm_name, const char *sigil_file){
    Frame f;
    memset(&f, 0, sizeof(f));
    f.type = TYPE_ALLIANCE_REQUEST;
    
    // Build origin as "IP:PORT" 
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(f.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    // Destination is the realm name 
    char *temp_dest;
    asprintf(&temp_dest, "%s", realm_name);
    snprintf(f.dest, DEST_SIZE, "%s", temp_dest);
    free(temp_dest);
    
    const char *my_realm = config->name;
    const char *file_size = "1234";  
    const char *md5sum = "0123456789abcdef0123456789abcdef";  
    
    char *data_buf;
    asprintf(&data_buf, "%s&%s&%s&%s",
             my_realm, sigil_file, file_size, md5sum);
    
    size_t dlen = strlen(data_buf);
    if(dlen > DATA_MAX){
        dlen = DATA_MAX;
    }
    
    memcpy(f.data, data_buf, dlen);
    f.data_len = (uint16_t)dlen;
    free(data_buf);
    set_checksum(&f);
    
    
    ssize_t sent = write_exact(sockfd, &f, sizeof(f));
    if(sent <= 0){
        write(1, "Error sending frame\n", 20);
        return -1;
    }
    
    return 0;
}

int sendSigilData(int sockfd, maesterConfig *config, const char *realm_name, const char *sigil_file __attribute__((unused))){
    Frame f;
    memset(&f, 0, sizeof(f));
    f.type = TYPE_SIGIL_DATA;
    
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(f.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    char *temp_dest;
    asprintf(&temp_dest, "%s", realm_name);
    snprintf(f.dest, DEST_SIZE, "%s", temp_dest);
    free(temp_dest);
    
    const char *dummy_sigil = "sigil.jpg";  
    size_t dlen = strlen(dummy_sigil);
    if(dlen > DATA_MAX){
        dlen = DATA_MAX;
    }
    
    memcpy(f.data, dummy_sigil, dlen);
    f.data_len = (uint16_t)dlen;
    set_checksum(&f);
    
    
    ssize_t sent = write_exact(sockfd, &f, sizeof(f));
    if(sent <= 0){
        write(1, "Error sending sigil data\n", 25);
        return -1;
    }
    
    return 0;
}


int network_pledge_alliance(char *realm_name, char *sigil_file, ThreadData *threadData){
    
    // Validate inputs 
    if(realm_name == NULL || sigil_file == NULL || threadData == NULL || threadData->config == NULL){
        write(1, "Error: Invalid arguments to network_pledge_alliance\n", 52);
        return -1;
    }
    
    maesterConfig *config = threadData->config;
    
    int realm_index = -1;
    for(int i = 0; i < config->route_count; i++){
        if(config->routes[i].realm_name != NULL && 
            strcasecmp(config->routes[i].realm_name, realm_name) == 0){
            realm_index = i;
            break;
        }
    }
    
    if(realm_index == -1){
        write(1, "Error: Realm '", 14);
        write(1, realm_name, strlen(realm_name));
        write(1, "' not found in routing table\n", 29);
        return -1;
    }
    
    routeEntry *target = &config->routes[realm_index];
    
    if(target->active_alliance == 1){
        write(1, "\nError: Alliance already exists with realm '", 44);
        write(1, realm_name, strlen(realm_name));
        write(1, "'\n", 2);
        write(1, "Cannot pledge alliance with a realm we are already allied with.\n", 64);
        return -1;
    }
    
    const char *connect_ip;
    int connect_port;
    
    if(target->is_known == 1){
        connect_ip = target->ip;
        connect_port = target->port;
    }else{
        routeEntry *default_route = findDefaultRoute(config);
        if(default_route == NULL){
            write(1, "Error: No DEFAULT route found and destination unknown\n", 54);
            return -1;
        }
        connect_ip = default_route->ip;
        connect_port = default_route->port;
    }
    
    write(1, "Pledge sent to ", 15);
    write(1, realm_name, strlen(realm_name));
    write(1, ". Envoy 1 is on the way.\n$ ", 28);
    
    
    int sockfd = connectToServer(connect_ip, connect_port);
    if(sockfd < 0){
        write(1, "Failed to connect\n", 18);
        return -1;
    }
    
    if(sendAllianceRequest(sockfd, config, realm_name, sigil_file) < 0){
        close(sockfd);
        return -1;
    }
    
    if(waitForDirectAck(threadData->protocol_state, 10) < 0){
        close(sockfd);
        return -1;
    }
    
    if(sendSigilData(sockfd, config, realm_name, sigil_file) < 0){
        close(sockfd);
        return -1;
    }
    
    if(waitForDirectMD5(threadData->protocol_state, 10) < 0){
        close(sockfd);
        return -1;
    }
    
    Frame final_ack;
    memset(&final_ack, 0, sizeof(final_ack));
    final_ack.type = TYPE_ACK;
    
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(final_ack.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    char *temp_dest2;
    asprintf(&temp_dest2, "%s", realm_name);
    snprintf(final_ack.dest, DEST_SIZE, "%s", temp_dest2);
    free(temp_dest2);
    
    const char *ack_msg = "OK";
    size_t ack_len = strlen(ack_msg);
    memcpy(final_ack.data, ack_msg, ack_len);
    final_ack.data_len = (uint16_t)ack_len;
    set_checksum(&final_ack);
    
    
    if(write_exact(sockfd, &final_ack, sizeof(final_ack)) <= 0){
        write(1, "Error sending final ACK\n", 24);
        close(sockfd);
        return -1;
    }
    
    close(sockfd);
        
    time_t start_time = time(NULL);
    
    pthread_mutex_lock(&threadData->protocol_state->mutex);
    threadData->protocol_state->alliance_response_received = 0;
    pthread_mutex_unlock(&threadData->protocol_state->mutex);
    
    while(1){
        sleep(1);  
        
        pthread_mutex_lock(&threadData->protocol_state->mutex);
        int response = threadData->protocol_state->alliance_response_received;
        pthread_mutex_unlock(&threadData->protocol_state->mutex);
        
        if(response != 0){
            // Got response 
            if(response == 1){
                write(1, ">>> Alliance with ", 18);
                write(1, realm_name, strlen(realm_name));
                write(1, " forged successfully!\n$ ", 24);
                target->active_alliance = 1;
                return 0;
            }else{
                write(1, ">>> Alliance with ", 18);
                write(1, realm_name, strlen(realm_name));
                write(1, " was rejected.\n$ ", 18);
                return -1;
            }
        }
        
        if(time(NULL) - start_time >= 120){
            
            write(1, "Timeout: No response received after 120 seconds\n", 49);
            write(1, "Sending NACK to destination realm...\n$ ", 40);
            
            Frame nack;
            memset(&nack, 0, sizeof(nack));
            nack.type = TYPE_NACK;
            char *temp_nack_dest;
            asprintf(&temp_nack_dest, "%s", realm_name);
            snprintf(nack.dest, DEST_SIZE, "%s", temp_nack_dest);
            free(temp_nack_dest);
            
            const char *timeout_msg = "TIMEOUT";
            memcpy(nack.data, timeout_msg, strlen(timeout_msg));
            nack.data_len = strlen(timeout_msg);
            set_checksum(&nack);
            
            const char *nack_ip;
            int nack_port;
            
            if(target->is_known == 1){
                nack_ip = target->ip;
                nack_port = target->port;
            }else{
                routeEntry *default_route = findDefaultRoute(config);
                if(default_route == NULL){
                    return -1;
                }
                nack_ip = default_route->ip;
                nack_port = default_route->port;
            }
            
            
            int nack_fd = connectToServer(nack_ip, nack_port);
            if(nack_fd >= 0){
                close(nack_fd);
            }else{
            }
            
            return -1;
        }
    }
}

int send_product_list_request(const char *realm_name, const char *destination_realm, ThreadData *threadData){
    // Validate inputs 
    if(realm_name == NULL || destination_realm == NULL || threadData == NULL || threadData->config == NULL){
        write(1, "Error: Invalid arguments to send_product_list_request\n", 54);
        return -1;
    }
    
    maesterConfig *config = threadData->config;
    
    
    // Look up destination in routing table 
    routeEntry *target = NULL;
    for(int i = 0; i < config->route_count; i++){
        if(config->routes[i].realm_name != NULL &&
           strcasecmp(config->routes[i].realm_name, destination_realm) == 0){
            target = &config->routes[i];
            break;
        }
    }
    
    if(target == NULL){
    }
    
            // Check if we have an active alliance with the destination 
    if(target != NULL && target->active_alliance == 0){
        write(1, "Error: No active alliance\n", 26);
        return -1;
    }
    
    const char *connect_ip = NULL;
    int connect_port = 0;
    
    if(target != NULL && target->is_known){
        connect_ip = target->ip;
        connect_port = target->port;
    }else{
        routeEntry *default_route = findDefaultRoute(config);
        if(default_route == NULL) return -1;
        connect_ip = default_route->ip;
        connect_port = default_route->port;
    }
    
    int sockfd = connectToServer(connect_ip, connect_port);
    if(sockfd < 0) return -1;
    
    // Set up expectation BEFORE sending the frame 
    pthread_mutex_lock(&threadData->protocol_state->mutex);
    threadData->protocol_state->expect_ack = 1;
    threadData->protocol_state->ack_received = 0;
    pthread_mutex_unlock(&threadData->protocol_state->mutex);
    
    Frame f;
    memset(&f, 0, sizeof(f));
    f.type = TYPE_PRODUCT_LIST_REQ;
    
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(f.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    char *temp_dest3;
    asprintf(&temp_dest3, "%s", destination_realm);
    snprintf(f.dest, DEST_SIZE, "%s", temp_dest3);
    free(temp_dest3);
    
    size_t dlen = strlen(realm_name);
    if(dlen > DATA_MAX) dlen = DATA_MAX;
    memcpy(f.data, realm_name, dlen);
    f.data_len = (uint16_t)dlen;
    set_checksum(&f);
    
    write_exact(sockfd, &f, sizeof(f));
    close(sockfd);
    
    time_t start_time = time(NULL);
    
    while(1){
        sleep(1);  
        
        pthread_mutex_lock(&threadData->protocol_state->mutex);
        int received = threadData->protocol_state->ack_received;
        pthread_mutex_unlock(&threadData->protocol_state->mutex);
        
        if(received){
            pthread_mutex_lock(&threadData->protocol_state->mutex);
            threadData->protocol_state->expect_ack = 0;
            pthread_mutex_unlock(&threadData->protocol_state->mutex);
            return 0;
        }
        
        if(time(NULL) - start_time >= 10){
            write(1, "Error: Timeout waiting for direct ACK\n", 38);
            pthread_mutex_lock(&threadData->protocol_state->mutex);
            threadData->protocol_state->expect_ack = 0;
            pthread_mutex_unlock(&threadData->protocol_state->mutex);
            return -1;
        }
    }
}

int send_order_header(const char *file_name, const char *file_size, const char *md5sum, const char *destination_realm, ThreadData *threadData){
    // Validate inputs 
    if(file_name == NULL || file_size == NULL || md5sum == NULL || destination_realm == NULL || threadData == NULL || threadData->config == NULL){
        write(1, "Error: Invalid arguments to send_order_header\n", 46);
        return -1;
    }
    
    maesterConfig *config = threadData->config;
    
    // Look up destination in routing table 
    routeEntry *target = NULL;
    for(int i = 0; i < config->route_count; i++){
        if(config->routes[i].realm_name != NULL &&
           strcasecmp(config->routes[i].realm_name, destination_realm) == 0){
            target = &config->routes[i];
            break;
        }
    }
    
    if(target == NULL){
    }
    
    // Check if we have an active alliance with the destination 
    if(target != NULL && target->active_alliance == 0){
        write(1, "Error: No active alliance with realm '", 38);
        write(1, destination_realm, strlen(destination_realm));
        write(1, "'\n", 2);
        return -1;
    }
    
    
    const char *connect_ip = NULL;
    int connect_port = 0;
    
    if(target != NULL && target->is_known){
        // Direct connection 
        connect_ip = target->ip;
        connect_port = target->port;
    }else{
        // Route through DEFAULT gateway 
        routeEntry *default_route = findDefaultRoute(config);
        if(default_route == NULL){
            write(1, "Error: No DEFAULT route found\n", 30);
            return -1;
        }
        connect_ip = default_route->ip;
        connect_port = default_route->port;
    }
    
    // Connect to target 
    int sockfd = connectToServer(connect_ip, connect_port);
    if(sockfd < 0){
        return -1;
    }
    
    // Set up expectation BEFORE sending the frame 
    pthread_mutex_lock(&threadData->protocol_state->mutex);
    threadData->protocol_state->expect_ack = 1;
    threadData->protocol_state->ack_received = 0;
    pthread_mutex_unlock(&threadData->protocol_state->mutex);
    
    // Build ORDER REQUEST HEADER frame 
    Frame f;
    memset(&f, 0, sizeof(f));
    f.type = TYPE_ORDER_HEADER;
    
    // Build origin as "IP:PORT" 
    char *my_ip_port;
    asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
    snprintf(f.origin, ORIGIN_SIZE, "%s", my_ip_port);
    free(my_ip_port);
    
    // Destination is the ally realm name 
    char *temp_dest4;
    asprintf(&temp_dest4, "%s", destination_realm);
    snprintf(f.dest, DEST_SIZE, "%s", temp_dest4);
    free(temp_dest4);
    
    // Data: <FileName>&<FileSize>&<MD5SUM> 
    char *data_buf;
    asprintf(&data_buf, "%s&%s&%s", file_name, file_size, md5sum);
    
    size_t dlen = strlen(data_buf);
    if(dlen > DATA_MAX){
        dlen = DATA_MAX;
    }
    
    memcpy(f.data, data_buf, dlen);
    f.data_len = (uint16_t)dlen;
    free(data_buf);
    
    
    set_checksum(&f);
    
    
    ssize_t sent = write_exact(sockfd, &f, sizeof(f));
    if(sent <= 0){
        write(1, "Error: Failed to send frame\n", 28);
        close(sockfd);
        return -1;
    }
    
    close(sockfd);
    
    time_t start_time = time(NULL);
    
    while(1){
        sleep(1);  // Check every 1 second
        
        pthread_mutex_lock(&threadData->protocol_state->mutex);
        int received = threadData->protocol_state->ack_received;
        pthread_mutex_unlock(&threadData->protocol_state->mutex);
        
        if(received){
            pthread_mutex_lock(&threadData->protocol_state->mutex);
            threadData->protocol_state->expect_ack = 0;
            pthread_mutex_unlock(&threadData->protocol_state->mutex);
            return 0;
        }
        
        if(time(NULL) - start_time >= 10){
            write(1, "Error: Timeout waiting for ACK FITXER\n", 38);
            pthread_mutex_lock(&threadData->protocol_state->mutex);
            threadData->protocol_state->expect_ack = 0;
            pthread_mutex_unlock(&threadData->protocol_state->mutex);
            return -1;
        }
    }
}


void broadcast_disconnection(maesterConfig *config){
    if(config == NULL) return;
    
    
    char notified_realms[100][50]; 
    int notified_count = 0;
    
    for(int i = 0; i < config->route_count; i++){
        routeEntry *route = &config->routes[i];
        
        if(route->active_alliance == 1 && route->realm_name != NULL){
            // Check if we've already notified this realm 
            int already_notified = 0;
            for(int j = 0; j < notified_count; j++){
                if(strcasecmp(notified_realms[j], route->realm_name) == 0){
                    already_notified = 1;
                    break;
                }
            }
            
            if(already_notified){
                continue;  // Skip duplicate 
            }
            
            
            // Determine connection details 
            const char *connect_ip;
            int connect_port;
            
            if(route->is_known == 1){
                connect_ip = route->ip;
                connect_port = route->port;
            }else{
                // Route through DEFAULT if unknown 
                routeEntry *default_route = findDefaultRoute(config);
                if(default_route == NULL){
                    continue;
                }
                connect_ip = default_route->ip;
                connect_port = default_route->port;
            }
            
            // Connect 
            int sockfd = connectToServer(connect_ip, connect_port);
            if(sockfd < 0){
                continue;
            }
            
            // Build DISCONNECT frame 
            Frame f;
            memset(&f, 0, sizeof(f));
            f.type = TYPE_DISCONNECT;
            
            // Origin: IP:PORT of disconnecting realm 
            char *my_ip_port;
            asprintf(&my_ip_port, "%s:%d", config->listen_ip, config->listen_port);
            snprintf(f.origin, ORIGIN_SIZE, "%s", my_ip_port);
            free(my_ip_port);
            
            // Destination: Ally realm name 
            char *temp_dest5;
            asprintf(&temp_dest5, "%s", route->realm_name);
            snprintf(f.dest, DEST_SIZE, "%s", temp_dest5);
            free(temp_dest5);
            
            // Data: DISCONNECT 
            const char *msg = "DISCONNECT";
            memcpy(f.data, msg, strlen(msg));
            f.data_len = strlen(msg);
            
            set_checksum(&f);
            
            // Send frame 
            write_exact(sockfd, &f, sizeof(f));
            close(sockfd);
            
            // Mark this realm as notified 
            if(notified_count < 100){
                char *temp_notified;
                asprintf(&temp_notified, "%s", route->realm_name);
                snprintf(notified_realms[notified_count], 50, "%s", temp_notified);
                free(temp_notified);
                notified_count++;
            }
        }
    }
}