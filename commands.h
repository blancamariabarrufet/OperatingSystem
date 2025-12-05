/**********************************************
*
* @File : commands.h
* @Purpose : Function declarations for trade operations and command processing
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 21/11/2025
*
***********************************************/

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <pthread.h>
#include "inventory.h"
#include "config.h"

/* Forward declarations - full definitions in network.h */
typedef struct ProtocolState ProtocolState;
typedef struct PendingAllianceList PendingAllianceList;

// Type definitions for trade operations
typedef struct
{
    char *name;
    int   amount;
} TradeItem;

typedef struct
{
    TradeItem *items;
    int        count;
} TradeOffer;

typedef struct
{
    maesterConfig *config;
    Inventory *inventory;
    ProtocolState *protocol_state;
    PendingAllianceList *pending_list;
    pthread_mutex_t *pending_mutex;
    char *arg1;
    char *arg2;
    int ongoingMission; 
    pthread_t thread;
    pthread_t server_thread;
    int *running;
    int *server_fd;
} ThreadData;

// Forward declaration for function defined in maester.c
void freeAndExit(maesterConfig *config, Inventory *inv, ThreadData *threadData, char *buf, char *arg1, char *arg2, pthread_t thread, pthread_t server_thread, int *running, ProtocolState *protocol_state, int *server_fd);

/****************************************************************************************************************
*
* @Name: writeTradeOffer
* @Def: Serializes the TradeOffer to a text file "tradeOffer" in a readable format (header, item list with names/amounts, footer); creates/truncates the file.
* @Arg: In: userOffer = pointer to populated TradeOffer (non-empty assumed; prints message if empty).
* @Ret: Void (no return value; errors printed on file open failure).
*
****************************************************************************************************************/
void writeTradeOffer(TradeOffer *userOffer);

/****************************************************************************************************************
*
* @Name: freeTradeOffer
* @Def: Frees all allocated memory in TradeOffer: each item's name string and the items array itself, resetting count to 0.
* @Arg: In/Out: offer = pointer to TradeOffer to deallocate (sets pointers to NULL).
* @Ret: Void (no return value).
*
****************************************************************************************************************/
void freeTradeOffer(TradeOffer *offer);

/****************************************************************************************************************
*
* @Name: separateTradeInformation
* @Def: Parses trade subcommand arguments from the buffer (e.g., "add product amount"), extracting product name and amount string, handling numbers to switch args.
* @Arg: In: buf = command buffer.
*        In: commandLength = length of subcommand prefix (e.g., 3 for "add").
*        Out: arg1_out = product name string (allocated).
*        Out: arg2_out = amount string (allocated; empty if none).
* @Ret: Returns number of parameters (0-2); 3 if invalid (more than two or malformed).
*
****************************************************************************************************************/
int separateTradeInformation(char *buf, char **arg1_out, char **arg2_out, int commandLength);

/****************************************************************************************************************
*
* @Name: addOption
* @Def: Adds or increments a product amount in the TradeOffer, validating against target realm's inventory availability; allocates new item if not present.
* @Arg: In: tradeInv = Inventory of target realm for validation.
*        In/Out: userOffer = TradeOffer to update.
*        In: arg1 = product name.
*        In: arg2 = amount string (parsed to int).
* @Ret: Void (no return value; prints errors on invalid product/amount/availability).
*
****************************************************************************************************************/
void addOption(Inventory *tradeInv, TradeOffer *userOffer, char *arg1, char *arg2);

/****************************************************************************************************************
*
* @Name: removeOption
* @Def: Decrements a product amount in the TradeOffer or removes the item if amount reaches zero; validates amount doesn't exceed offered.
* @Arg: In/Out: userOffer = TradeOffer to update.
*        In: arg1 = product name.
*        In: arg2 = amount string (parsed to int).
* @Ret: Void (no return value; prints errors on invalid product/excess amount; message on full removal).
*
****************************************************************************************************************/
void removeOption(TradeOffer *userOffer, char *arg1, char *arg2);

/****************************************************************************************************************
*
* @Name: startTradeMessage
* @Def: Prints status messages for trade actions (cancel, send, or error) using the realm name from config.
* @Arg: In: state = action code (0=error, 1=cancel, 4=send).
*        In: information = ThreadData with config for realm name.
* @Ret: Void (no return value).
*
****************************************************************************************************************/
void startTradeMessage(int state, ThreadData *information);

/****************************************************************************************************************
*
* @Name: checkLine
* @Def: Validates trade subcommands (cancel/add/remove/send) using fuzzy matching on the first letter and exact length check.
* @Arg: In: line = NUL-terminated subcommand string (lowercase).
* @Ret: Returns action code (0=unknown, 1=cancel, 2=add, 3=remove, 4=send); prints error on unknown.
*
****************************************************************************************************************/
int checkLine(char *line);

/****************************************************************************************************************
*
* @Name: tradeFunction
* @Def: Enters interactive trade mode for a specific realm: validates realm, loads target inventory, reads user commands (add/remove/send/cancel), manages TradeOffer, and exits on cancel/send.
* @Arg: In: information = pointer to ThreadData (config, inventory, arg1=realm, arg2=unused).
* @Ret: Void (no return value; updates offer and prints interactions).
*
****************************************************************************************************************/
void tradeFunction(ThreadData *information);

/****************************************************************************************************************
*
* @Name: pledgeStatus
* @Def: Displays the current status of alliance requests (pending and allied realms) by scanning the pending alliance list and routing table.
* @Arg: In: information = pointer to ThreadData containing config, pending_list, and pending_mutex.
* @Ret: Void (no return value; prints status to stdout).
*
****************************************************************************************************************/
void pledgeStatus(ThreadData *information);

#endif // COMMANDS_H

