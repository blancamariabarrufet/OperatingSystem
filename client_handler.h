/**********************************************

* @File : client_handler.h
* @Purpose : Function declarations for client-side network operations in the Maester alliance protocol
* @Author : Miquel Pla and Blanca Maria Barrufet
* @Date : 21/11/2025

***********************************************/

#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"
#include "network.h"

/****************************************************************************************************************

* @Name: connectToServer
* @Def: Establishes a TCP connection to a server at the specified IP address and port number.
* @Arg: In: ip = IP address string of the target server.
*        In: port = port number to connect to.
* @Ret: Returns socket file descriptor on success, -1 on failure (connection errors logged).

****************************************************************************************************************/
int connectToServer(const char *ip, int port);

/****************************************************************************************************************

* @Name: waitForDirectAck
* @Def: Waits for an ACK (0x31) response directly from the server on the listening port, using condition variables for synchronization.
* @Arg: In: state = ProtocolState structure containing synchronization primitives.
*        In: timeout_sec = maximum time to wait in seconds.
* @Ret: Returns 0 on successful ACK receipt, -1 on timeout.

****************************************************************************************************************/
int waitForDirectAck(ProtocolState *state, int timeout_sec);

/****************************************************************************************************************

* @Name: waitForDirectMD5
* @Def: Waits for an MD5 Check (0x32) response directly from the server on the listening port, using condition variables for synchronization.
* @Arg: In: state = ProtocolState structure containing synchronization primitives.
*        In: timeout_sec = maximum time to wait in seconds.
* @Ret: Returns 0 on successful MD5 receipt, -1 on timeout.

****************************************************************************************************************/
int waitForDirectMD5(ProtocolState *state, int timeout_sec);

/****************************************************************************************************************

* @Name: sendAllianceRequest
* @Def: Sends an alliance request frame (0x01) containing realm name, sigil file info, and MD5 checksum to the specified realm.
* @Arg: In: sockfd = connected socket file descriptor.
*        In: config = Maester configuration containing local realm information.
*        In: realm_name = name of the target realm for alliance.
*        In: sigil_file = name of the sigil file being offered (unused in current implementation).
* @Ret: Returns 0 on success, -1 on failure.

****************************************************************************************************************/
int sendAllianceRequest(int sockfd, maesterConfig *config, const char *realm_name, const char *sigil_file);

/****************************************************************************************************************

* @Name: sendSigilData
* @Def: Sends sigil data frame (0x02) containing dummy sigil content to complete the alliance request protocol.
* @Arg: In: sockfd = connected socket file descriptor.
*        In: config = Maester configuration containing local realm information.
*        In: realm_name = name of the target realm for alliance.
*        In: sigil_file = name of the sigil file (unused in current implementation).
* @Ret: Returns 0 on success, -1 on failure.

****************************************************************************************************************/
int sendSigilData(int sockfd, maesterConfig *config, const char *realm_name, const char *sigil_file __attribute__((unused)));

/****************************************************************************************************************

* @Name: network_pledge_alliance
* @Def: Main client function to initiate alliance pledge with another realm through the complete protocol sequence: connect, send request, send sigil, wait for responses.
* @Arg: In: realm_name = name of the target realm.
*        In: sigil_file = sigil file name for the alliance offer.
*        In: threadData = thread context containing configuration and protocol state.
* @Ret: Returns 0 on successful alliance acceptance, -1 on rejection or error.

****************************************************************************************************************/
int network_pledge_alliance(char *realm_name, char *sigil_file, ThreadData *threadData);

/****************************************************************************************************************

* @Name: send_product_list_request
* @Def: Sends a PRODUCT LIST REQUEST frame (0x11) to request product list from an ally realm through the routing system.
* @Arg: In: realm_name = name of the requesting realm (to include in DATA field).
*        In: destination_realm = name of the destination realm (for routing and DEST field).
*        In: threadData = thread context containing configuration and protocol state.
* @Ret: Returns 0 on successful ACK receipt, -1 on routing/connection/timeout error.

****************************************************************************************************************/
int send_product_list_request(const char *realm_name, const char *destination_realm, ThreadData *threadData);

/****************************************************************************************************************

* @Name: send_order_header
* @Def: Sends an ORDER REQUEST HEADER frame (0x14) containing file metadata for a trade order to an ally realm.
* @Arg: In: file_name = name of the file being ordered.
*        In: file_size = size of the file in bytes (as string).
*        In: md5sum = MD5 checksum of the file (32-char hex string).
*        In: destination_realm = name of the ally realm (for routing and DEST field).
*        In: threadData = thread context containing configuration and protocol state.
* @Ret: Returns 0 on successful ACK receipt (0x31), -1 on routing/connection/timeout error.

****************************************************************************************************************/
int send_order_header(const char *file_name, const char *file_size, const char *md5sum, const char *destination_realm, ThreadData *threadData);

/****************************************************************************************************************
*
* @Name: broadcast_disconnection
* @Def: Sends a DISCONNECT frame (0x27) to all active allies to notify them of the maester's shutdown.
* @Arg: In: config = Maester configuration containing routing table and local realm info.
* @Ret: Void (no return value; errors are logged but execution continues).
*
****************************************************************************************************************/
void broadcast_disconnection(maesterConfig *config);

#endif // CLIENT_HANDLER_H
