/**********************************************

* @File : server_handler.h
* @Purpose : Function declarations for server-side network operations in the Maester alliance protocol
* @Author : Miquel Pla and Blanca Maria Barrufet
* @Date : 21/11/2025

***********************************************/

#ifndef SERVER_HANDLER_H
#define SERVER_HANDLER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"
#include "network.h"

/****************************************************************************************************************

* @Name: openListenConnection
* @Def: Creates and configures a TCP server socket bound to the specified IP and port, ready to accept incoming connections.
* @Arg: In: port = port number to bind the server socket to.
*        In: ip = IP address string to bind the server socket to.
* @Ret: Returns socket file descriptor on success, -1 on failure (bind/listen errors logged).

****************************************************************************************************************/
int openListenConnection(int port, const char *ip);

/****************************************************************************************************************

* @Name: sendAck
* @Def: Sends an ACK frame (0x31) with optional message data through the specified client socket connection.
* @Arg: In: client_fd = connected client socket file descriptor.
*        In: config = Maester configuration containing local realm information.
*        In: msg = optional message string to include in ACK data (defaults to "OK" if NULL).
* @Ret: Void (no return value; sends frame directly).

****************************************************************************************************************/
void sendAck(int client_fd, maesterConfig *config, const char *msg);

/****************************************************************************************************************

* @Name: sendAckDirect
* @Def: Sends an ACK frame (0x31) directly to a specific IP:port address, establishing a new connection for the response.
* @Arg: In: origin_ip_port = IP:port string of the destination (format "IP:PORT").
*        In: config = Maester configuration containing local realm information.
*        In: msg = optional message string to include in ACK data (defaults to "OK" if NULL).
* @Ret: Void (no return value; establishes connection and sends frame).

****************************************************************************************************************/
void sendAckDirect(const char *origin_ip_port, maesterConfig *config, const char *msg);

/****************************************************************************************************************

* @Name: sendMD5Check
* @Def: Sends an MD5 Check frame (0x32) with MD5 checksum data through the specified client socket connection.
* @Arg: In: client_fd = connected client socket file descriptor.
*        In: config = Maester configuration containing local realm information.
*        In: md5 = MD5 checksum string to include (defaults to "CHECK_OK" if NULL).
* @Ret: Void (no return value; sends frame directly).

****************************************************************************************************************/
void sendMD5Check(int client_fd, maesterConfig *config, const char *md5);

/****************************************************************************************************************

* @Name: sendMD5CheckDirect
* @Def: Sends an MD5 Check frame (0x32) directly to a specific IP:port address, establishing a new connection for the response.
* @Arg: In: origin_ip_port = IP:port string of the destination (format "IP:PORT").
*        In: config = Maester configuration containing local realm information.
*        In: md5 = MD5 checksum string to include (defaults to "CHECK_OK" if NULL).
* @Ret: Void (no return value; establishes connection and sends frame).

****************************************************************************************************************/
void sendMD5CheckDirect(const char *origin_ip_port, maesterConfig *config, const char *md5);

/****************************************************************************************************************

* @Name: sendNack
* @Def: Sends a NACK frame (0x69) with error reason data through the specified client socket connection.
* @Arg: In: client_fd = connected client socket file descriptor.
*        In: reason = error reason string to include in NACK data (defaults to "ERROR" if NULL).
* @Ret: Void (no return value; sends frame directly).

****************************************************************************************************************/
void sendNack(int client_fd, const char *reason);

/****************************************************************************************************************

* @Name: network_server_thread
* @Def: Main server thread function that listens for incoming connections and processes alliance protocol frames, managing the pending alliance queue.
* @Arg: In: arg = ServerThreadData structure containing configuration, pending list, mutexes, and runtime flags.
* @Ret: Returns NULL (void* for thread compatibility).

****************************************************************************************************************/
void* network_server_thread(void *arg);

/****************************************************************************************************************

* @Name: network_pledge_respond
* @Def: Processes user decision (accept/reject) for a pending alliance request, sends response directly to client and updates routing table if accepted.
* @Arg: In: realm_name = name of the realm whose alliance request is being processed.
*        In: decision = decision string ("accept" or "reject").
*        In: config = Maester configuration for routing table updates.
*        In/Out: pending_list = list of pending alliance requests to modify.
*        In: pending_mutex = mutex protecting the pending alliance list.
* @Ret: Returns 0 on success, -1 on failure (realm not found, invalid decision, connection errors).

****************************************************************************************************************/
int network_pledge_respond(char *realm_name, char *decision, maesterConfig *config, PendingAllianceList *pending_list, pthread_mutex_t *pending_mutex);

#endif // SERVER_HANDLER_H
