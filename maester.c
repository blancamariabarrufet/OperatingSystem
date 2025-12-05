/**********************************************
*
* @File : maester.c
* @Purpose : Main application logic, command loop, signal handling, and trade offer management for the Maester.
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 26/10/2025
*
***********************************************/

#define _GNU_SOURCE

// System Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>

// Project Includes
#include "config.h"
#include "inventory.h"
#include "console.h"
#include "commands.h"
#include "network.h"
#include "client_handler.h"

#define MAX_MISSIONS 1

// quit flag Declarations
volatile int flag = 0;

// Forward declarations
void freeThreadData(ThreadData *data);

void freeAndExit(maesterConfig *config, Inventory *inv, ThreadData *threadData, char *buf, char *arg1, char *arg2, pthread_t thread, pthread_t server_thread, int *running, ProtocolState *protocol_state, int *server_fd){
    broadcast_disconnection(config);

    write(1, "The maester of ", 15);
    write(1, config->name, strlen(config->name));
    write(1, " signed off. The reavens rest.\n\n", 33);

    *running = 0;
    if (*server_fd >= 0) {
        close(*server_fd);  
        *server_fd = -1;
    }
    
    if (threadData->ongoingMission == 1) pthread_join(thread, NULL);
    
    pthread_join(server_thread, NULL);
    
    pthread_mutex_destroy(&protocol_state->mutex);
    
    free(buf);
    free(arg1);
    free(arg2);
    freeThreadData(threadData);
    freeConfig(config);
    freeInventory(inv);
    
    exit(1);
}


void initThreadData(ThreadData *data, maesterConfig *config, Inventory *inventory, pthread_t thread, pthread_t server_thread, int *running, int *server_fd) {
    data->config = config;
    data->inventory = inventory;
    data->protocol_state = NULL;
    data->pending_list = NULL;
    data->pending_mutex = NULL;
    data->arg1 = NULL;
    data->arg2 = NULL;
    data->thread = thread;
    data->server_thread = server_thread;
    data->running = running;
    data->server_fd = server_fd;
    data->ongoingMission = 0;
}


void updateThreadDataArgs(ThreadData *data, char *arg1, char *arg2) {
    // Free old copies if they exist
    if (data->arg1 != NULL) {
        free(data->arg1);
        data->arg1 = NULL;
    }
    if (data->arg2 != NULL) {
        free(data->arg2);
        data->arg2 = NULL;
    }
    
    // Make copies of the arguments for the thread
    if (arg1 != NULL) {
        data->arg1 = malloc(strlen(arg1) + 1);
        if (data->arg1) {
            strcpy(data->arg1, arg1);
        }
    }
    if (arg2 != NULL) {
        data->arg2 = malloc(strlen(arg2) + 1);
        if (data->arg2) {
            strcpy(data->arg2, arg2);
        }
    }
}

void freeThreadData(ThreadData *data) {
    if (data == NULL) return;
    
    // Free the copied arguments
    if (data->arg1 != NULL) {
        free(data->arg1);
        data->arg1 = NULL;
    }
    if (data->arg2 != NULL) {
        free(data->arg2);
        data->arg2 = NULL;
    }
}

void quitHandler(int signum __attribute__((unused)) ){
    write(1,"\n",sizeof("\n"));
    flag = 1;
}






// Thread function for CMD_PLEDGE
void *thread_pledge(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    if(network_pledge_alliance(data->arg1, data->arg2, data) == 0){
    }else{
        write(1, "Alliance pledge failed\n$ ", 25);
    }
    
    data->ongoingMission = 0;
    return NULL;
}

// Thread function for CMD_PLEDGE_RESP
void *thread_pledgeResp(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    if (strcmp(data->arg2, "accept") != 0 && strcmp(data->arg2, "reject") != 0) {
        write(1, "Invalid Second Argument, insert ACCEPT or REJECT\n", 49);
        write(1, "Usage: PLEDGE RESPOND <REALM> ACCEPT / REJECT\n\n$ ", 50);
        data->ongoingMission = 0;
        return NULL;
    }
    
    if(network_pledge_respond(data->arg1, data->arg2, data->config, data->pending_list, data->pending_mutex) == 0){
        if(strcmp(data->arg2, "accept") == 0){
            write(1, "Alliance with ", 14);
            write(1, data->arg1, strlen(data->arg1));
            write(1, " established\n$ ", 15);
        }else{
            write(1, "Alliance with ", 14);
            write(1, data->arg1, strlen(data->arg1));
            write(1, " not established\n$ ", 19);
        }
    }else{
        write(1, "Failed to send response\n$ ", 26);
    }
    
    data->ongoingMission = 0;
    return NULL;
}



// Thread function for CMD_LIST_PROD_R
void *thread_listProductsR(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    write(1, "Command OK\n", 11);
    data->ongoingMission = 0;
    return NULL;
}

// Thread function for CMD_START_TRADE
void *thread_startTrade(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    write(1,"Entering trade mode with ",sizeof("Entering trade mode with "));
    write(1,data->arg1,strlen(data->arg1));
    write(1,".\n",sizeof(".\n"));
    tradeFunction(data);
    data->ongoingMission = 0;
    return NULL;
}









void executeCommand(pthread_t thread, int state, ThreadData *data){
    switch (state){

        case CMD_LIST_REALMS:
            for (int i = 1; i < data->config->route_count; i++){
                write(1,"- ",sizeof("- "));
                write(1,data->config->routes[i].realm_name,strlen(data->config->routes[i].realm_name));
                write(1,"\n",sizeof("\n"));
            }
            break;  

        case CMD_PLEDGE: 
            if (data->ongoingMission == MAX_MISSIONS){
                write(1, "Error: Another mission is ongoing. Please wait until it finishes.\n", sizeof("Error: Another mission is ongoing. Please wait until it finishes.\n"));
                break;
            }
            data->ongoingMission = 1;
            pthread_create(&thread, NULL, thread_pledge, data);
            break;

        case CMD_PLEDGE_RESP: 
            if (data->ongoingMission == MAX_MISSIONS){
                write(1, "Error: Another mission is ongoing. Please wait until it finishes.\n", sizeof("Error: Another mission is ongoing. Please wait until it finishes.\n"));
                break;
            }
            data->ongoingMission = 1;
            pthread_create(&thread, NULL, thread_pledgeResp, data);
            break;

        case CMD_LIST_PROD_L: 
            printInventory(data->inventory);
            break;

        case CMD_LIST_PROD_R: 
            if (data->ongoingMission == MAX_MISSIONS){
                write(1, "Error: Another mission is ongoing. Please wait until it finishes.\n", sizeof("Error: Another mission is ongoing. Please wait until it finishes.\n"));
                break;
            }
            data->ongoingMission = 1;
            pthread_create(&thread, NULL, thread_listProductsR, data);
            break;

        case CMD_START_TRADE: 
            if (data->ongoingMission == MAX_MISSIONS){
                write(1, "Error: Another mission is ongoing. Please wait until it finishes.\n", sizeof("Error: Another mission is ongoing. Please wait until it finishes.\n"));
                break;
            }
            data->ongoingMission = 1;
            pthread_create(&thread, NULL, thread_startTrade, data);
            pthread_join(thread, NULL);
            break;

        case CMD_PLEDGE_STAT:
            pledgeStatus(data);
            break;

        case CMD_ENVOY_STAT:
            write(1, "Command OK\n", 11);
            break;

        default:
            break;
    }
}


/****************************************************************************************************************
*
* @Name: main
* @Def: Entry point of the Maester application; loads config and inventory from command-line arguments, sets up signal handling, initializes thread data, and runs the interactive command loop until EXIT or SIGINT.
* @Arg: In: argc = number of command-line arguments (expects 3: program, config file, inventory file).
*        In: argv = array of argument strings.
* @Ret: Returns 0 on normal exit (though exit(0) is called, so not reached).
*
****************************************************************************************************************/
int main (int argc, char *argv[]){

    maesterConfig config ;
    Inventory inventory ;
    

    if (argc != 3) {
        write(1, "Invalid  Number of arguments\n", 28);
        write(1, "Usage: maester <config.data> <inventory.data>\n", 45);
        exit(1);
    }

    int checkFile = loadMaesterConfig(argv[1], &config);     
    if (0 == checkFile){
        write(1, "Error loading config\n", 21);
        exit(1);
    }

    checkFile = loadInventory(&inventory, argv[2]);
    if (-1 == checkFile){
        write(1, "Error loading inventory\n", 24);
        freeConfig(&config);
        exit(1);
    }


    write(1, "\nMaester of ", sizeof("Maester of "));
    write(1, config.name, strlen(config.name));
    write(1, " initialized. The board is set.\n", sizeof(" initialized. The board is set.\n"));


    char *arg1 = NULL;
    char *arg2 = NULL;

    char *buf = NULL;

    pthread_t thread = 0; 
    
    struct sigaction sa;
    sa.sa_handler = quitHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    PendingAllianceList pending_list = {.entries = NULL, .count = 0, .capacity = 0};
    pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;
    int running = 1;
    int server_fd = -1;  
    
    ProtocolState protocol_state;
    pthread_mutex_init(&protocol_state.mutex, NULL);
    protocol_state.expect_ack = 0;
    protocol_state.ack_received = 0;
    protocol_state.expect_md5 = 0;
    protocol_state.md5_received = 0;
    protocol_state.alliance_response_received = 0;
    
    ServerThreadData server_data;
    server_data.config = &config;
    server_data.pending_list = &pending_list;
    server_data.pending_mutex = &pending_mutex;
    server_data.protocol_state = &protocol_state;
    server_data.running = &running;
    server_data.server_fd = &server_fd;
    
    pthread_t server_thread;
    if(pthread_create(&server_thread, NULL, network_server_thread, &server_data) != 0){
        write(1, "Error starting server thread\n", 29);
        freeConfig(&config);
        freeInventory(&inventory);
        exit(1);
    }
    
    ThreadData threadData;
    initThreadData(&threadData, &config, &inventory, thread, server_thread, &running, &server_fd);
    threadData.protocol_state = &protocol_state;
    threadData.pending_list = &pending_list;
    threadData.pending_mutex = &pending_mutex;
    
    


    while(1){
        
        free(buf);
        free(arg1);
        free(arg2);

        buf = NULL;
        arg1 = NULL;
        arg2 = NULL;


        int state =commandMotor(&buf, &arg1, &arg2);
        
        if (flag == 1 || state == CMD_EXIT) {
            freeAndExit(&config, &inventory, &threadData, buf, arg1, arg2, thread, server_thread, &running, &protocol_state, &server_fd);
        }
        
        updateThreadDataArgs(&threadData, arg1, arg2);

        if (state == CMD_START_TRADE || state == CMD_PLEDGE || state == CMD_PLEDGE_RESP || state == CMD_LIST_PROD_R) {
            if(checkRealmExists(&config, arg1)==0) {
                write(1, "Realm does not exist, Please introduce a valid one.\n", sizeof("Realm does not exist, Please introduce a valid one.\n"));
                state = -1;
            }
        }

        executeCommand(thread, state, &threadData);
        

    }

    return 0;
}
