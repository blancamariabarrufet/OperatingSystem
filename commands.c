/**********************************************
*
* @File : commands.c
* @Purpose : Implementation of trade operations and command processing
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 21/11/2025
*
***********************************************/

#define _GNU_SOURCE

#include "commands.h"
#include "console.h"
#include "config.h"
#include "network.h"
#include "client_handler.h"

#include <ctype.h>
#include <signal.h>
#include <sys/select.h>

// External global variable for the signal handler
extern volatile int flag;


void writeTradeOffer(TradeOffer *userOffer){
    if (userOffer->count == 0) {
        write(1, "No products in trade offer.\n", sizeof("No products in trade offer.\n"));
        return;
    }

    int outfd = open("tradeOffer", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (outfd < 0) {
        write(1, "Error: cannot open tradeOffer file\n", 35);
        return;
    }
    
    char *numbuf;
    asprintf(&numbuf, "%d", userOffer->count);
    write(outfd, numbuf, strlen(numbuf));
    free(numbuf);
    write(outfd, "\n", 1);

    for (int i = 0; i < userOffer->count; i++) {
        write(outfd, userOffer->items[i].name, strlen(userOffer->items[i].name));
        write(outfd, ";", 1);
        
        asprintf(&numbuf, "%d", userOffer->items[i].amount);
        write(outfd, numbuf, strlen(numbuf));
        free(numbuf);
        write(outfd, "\n", 1);
    }

    write(outfd, "EOF\n", 4);
    close(outfd);
    
}

void freeTradeOffer(TradeOffer *offer) {
    if (offer == NULL) return;
    if (offer->items != NULL) {
        for (int i = 0; i < offer->count; i++) {
            free(offer->items[i].name);
        }
        free(offer->items);
        offer->items = NULL;
    }
    offer->count = 0;
}

int separateTradeInformation(char *buf, char **arg1_out, char **arg2_out, int commandLength){
    int parameters = 0;
    int currentPosition = commandLength;

    free(*arg1_out);
    *arg1_out = NULL;

    free(*arg2_out);
    *arg2_out = NULL;

    size_t len1 = 0, len2 = 0;

    while (buf[currentPosition] == ' ') currentPosition++;

    if (buf[currentPosition] == '\0' || buf[currentPosition] == '\n'){
        *arg1_out = malloc(sizeof(char));
        if (!*arg1_out) exit(1);
        (**arg1_out) = '\0';

        *arg2_out = malloc(sizeof(char));
        if (!*arg2_out) exit(1);
        (**arg2_out) = '\0';
        return 0;
    }

    int firstArg = 0;   

    for (int i = currentPosition; buf[i] != '\0' && buf[i] != '\n'; i++){
        if (firstArg == 0){
            if (buf[i] >= '0' && buf[i] <= '9'){

                while (len1 > 0 && (*arg1_out)[len1 - 1] == ' ') {
                    len1--;
                }
                
                len1 += 1;
                addCharToBuf(arg1_out, '\0', len1);
                
                firstArg = 1;
                parameters = 2;
                len2 += 1;
                addCharToBuf(arg2_out, buf[i], len2);
            } else {

                len1 += 1;
                addCharToBuf(arg1_out, buf[i], len1);
                parameters = 1;
                
            }
        } else if (firstArg == 1) {
            if (buf[i] >= '0' && buf[i] <= '9'){
                len2 += 1;
                addCharToBuf(arg2_out, buf[i], len2);
            } else if (buf[i] == ' '){
                firstArg = 2;
                len2 += 1;
                addCharToBuf(arg2_out, '\0', len2);
            } else {
                // Free allocated memory on error
                free(*arg1_out);
                *arg1_out = NULL;
                free(*arg2_out);
                *arg2_out = NULL;
                return 3;
            }
        } else {
            if (buf[i] != ' '){
                // Free allocated memory on error
                free(*arg1_out);
                *arg1_out = NULL;
                free(*arg2_out);
                *arg2_out = NULL;
                return 3;
            }
        }
        currentPosition = i;
    }

    // Add null terminator to arg1 if it has content but no null terminator
    if (len1 > 0 && *arg1_out != NULL && (*arg1_out)[len1 - 1] != '\0') {
        len1 += 1;
        addCharToBuf(arg1_out, '\0', len1);
    }

    // Add null terminator to arg2 if it has content but no null terminator
    if (len2 > 0 && *arg2_out != NULL && (*arg2_out)[len2 - 1] != '\0') {
        len2 += 1;
        addCharToBuf(arg2_out, '\0', len2);
    } else if (len2 == 0) {
        len2 += 1;
        addCharToBuf(arg2_out, '\0', len2);
    }
    
    return parameters;
}

void addOption(Inventory *tradeInv,TradeOffer *userOffer, char *arg1, char *arg2){
    char *argProduct = arg1;
    char *argAmount = arg2;
    int amountRequested = 0;

    parseInt(argAmount,&amountRequested);
    int idProduct = invFindIndex(tradeInv, argProduct);
    if (idProduct < 0){
        write(1, "Error: product doesn't exist, so it can't be added\n", sizeof("Error: product doesn't exist, so it can't be added\n"));
        return;
    }
    
    int existingProductIndex = -1;
    for (int i = 0; i < userOffer->count; i++) {
        if (strncasecmp(userOffer->items[i].name, argProduct, strlen(argProduct)) == 0) {
            existingProductIndex = i;
            break;
        }
    }
    
    if (existingProductIndex >= 0) {
        int totalAmount = userOffer->items[existingProductIndex].amount + amountRequested;
        
        if(tradeInv->products[idProduct].amount < totalAmount){
            write(1, "Error: the total amount exceeds what is available in the other realm\n", sizeof("Error: the total amount exceeds what is available in the other realm\n"));
            return;
        }
        
        userOffer->items[existingProductIndex].amount = totalAmount;
        
        return;
    }
    
    if(tradeInv->products[idProduct].amount < amountRequested){
        write(1, "Error: the amount you request is not availabe in the other realm\n", sizeof("Error: the amount you request is not availabe in the other realm\n"));
        return;
    }

    TradeItem *tmp;
    tmp = (TradeItem *)realloc(userOffer->items,(size_t)(userOffer->count + 1) * sizeof(TradeItem));
    if (tmp == NULL)
    {
        write(1, "Error: realloc failed adding trade item\n",sizeof("Error: realloc failed adding trade item\n"));
        return;
    }

    userOffer->items = tmp;

    userOffer->items[userOffer->count].name = NULL;
    userOffer->items[userOffer->count].amount = 0;

    char *nameCopy = (char *)malloc(strlen(argProduct) + 1);
    if (nameCopy == NULL) {
        write(1, "Error: memory allocation failed for product name\n", 50);
        return;
    }
    
    strcpy(nameCopy, argProduct);
    userOffer->items[userOffer->count].name   = nameCopy;
    userOffer->items[userOffer->count].amount = amountRequested;
    userOffer->count = userOffer->count + 1;


}

void removeOption(TradeOffer *userOffer, char *arg1, char *arg2){
    char *argProduct = arg1;
    char *argAmount = arg2;
    int amountToRemove = 0;

    parseInt(argAmount, &amountToRemove);
    
    int idProduct = -1;
    for (int i = 0; i < userOffer->count; i++) {
        if (strncasecmp(userOffer->items[i].name, argProduct, strlen(argProduct)) == 0) {
            idProduct = i;
            break;
        }
    }
    
    if (idProduct < 0) {
        write(1, "Error: product doesn't exist in the trade offer\n", sizeof("Error: product doesn't exist in the trade offer\n"));
        return;
    }
    
    if (userOffer->items[idProduct].amount < amountToRemove) {
        write(1, "Error: the amount you want to remove is greater than the amount offered\n", sizeof("Error: the amount you want to remove is greater than the amount offered\n"));
        return;
    }
    
    if (userOffer->items[idProduct].amount == amountToRemove) {

        // Free the name of the item being removed before overwriting it
        free(userOffer->items[idProduct].name);
        
        userOffer->items[idProduct] = userOffer->items[userOffer->count - 1];
        userOffer->count--;
        
        if (userOffer->count > 0) {
            TradeItem *tmp = (TradeItem *)realloc(userOffer->items, (size_t)userOffer->count * sizeof(TradeItem));
            if (tmp) {
                userOffer->items = tmp;
            }
        } else {
            free(userOffer->items);
            userOffer->items = NULL;
        }
        
        write(1, "Product completely removed from trade offer: ", 45);
        write(1, argProduct, strlen(argProduct));
        write(1, "\n", 1);
    } else {
        userOffer->items[idProduct].amount -= amountToRemove;
        
    }
}


void startTradeMessage(int state, ThreadData *information){
    switch (state){
        
        case 1: //cancel 
            write(1, "Trade Process with ", sizeof("Trade Process with "));
            write(1, information->config->name, sizeof(information->config->name));
            write(1, " has been cancelled\n", sizeof(" has been cancelled\n"));
            break;

        
        case 4: //send
            write(1, "Trade list sent to ", 19);
            write(1, information->arg1, strlen(information->arg1));
            write(1, ". Envoy 1 is on the way.\n", 26);
            break;

        case 0:
            write(1, "Command not found\n", sizeof("Command not found\n"));
            break;

    }
}


int checkLine(char *line){

    switch (line[0]){
        case 'c':
            if (countCorrectLetters(line, "cancel", 6) == 1) return 1;
            else return 0;
            break;

        case 'a':
            if (countCorrectLetters(line, "add", 3) == 1)  return 2;
            else return 0;
            break;

        case 'r':
            if (countCorrectLetters(line, "remove", 6) == 1) return 3;
            else return 0;
            break;

        case 's':
            if (countCorrectLetters(line, "send", 4) == 1) return 4;  
            else return 0;
            break;
        
        default:
            return 0;
        }
} 


void tradeFunction(ThreadData *information){
    int exists = checkRealmExists( information->config, information->arg1);    
    if (exists == 0) return;
    
    routeEntry *target = NULL;
    for(int i = 0; i < information->config->route_count; i++){
        if(information->config->routes[i].realm_name != NULL &&
           strcasecmp(information->config->routes[i].realm_name, information->arg1) == 0){
            target = &information->config->routes[i];
            break;
        }
    }
    
    if(target != NULL && target->active_alliance == 0){
        write(1, "\n[ERROR] No active alliance with realm '", 40);
        write(1, information->arg1, strlen(information->arg1));
        write(1, "'\n", 2);
        write(1, "[ERROR] Cannot start trade without alliance\n", 45);
        write(1, "[HINT] First establish alliance with: PLEDGE ALLIANCE ", 54);
        write(1, information->arg1, strlen(information->arg1));
        write(1, " <sigil_file>\n", 14);
        return;
    }
    
    if(target == NULL){
        write(1, "[ERROR] Realm not found in routing table\n", 42);
        return;
    }
    
    write(1, "[TRADE] Alliance confirmed with ", 32);
    write(1, information->arg1, strlen(information->arg1));
    write(1, "\n", 1);
    
    int exit_loop = 0;

    TradeOffer userOffer;
    userOffer.items = NULL;
    userOffer.count = 0;
    
    // Load local inventory for validation (Phase 2) 
    Inventory invTrade;
    int checkFileTrade = loadInventory(&invTrade, "Files/strong_stock.db");
    if (1 == checkFileTrade){
        write(1, "Error loading trade inventory\n", 31);
        exit(1);
    }
    
    write(1,"Available products: ",sizeof("Available products: "));
    for (int i = 0; i < invTrade.nProducts; ++i) {
        write(1," ",sizeof(" "));
        write(1,invTrade.products[i].name,strlen(invTrade.products[i].name));
        if(i < invTrade.nProducts - 1){
            write(1,",",sizeof(","));
        }
        else{
            write(1,".",sizeof("."));
        }
        }
    write(1,"\n",sizeof("\n"));

    char *arg1 = NULL; //prod
    char *arg2 = NULL; //amount

    while (exit_loop == 0)
    {
        char ch = '\0';  // Initialize to avoid undefined behavior
        char *line = NULL;
        int bufSize=0;

        free(line);
        line = NULL;


        free(arg1);
        arg1 = NULL;

        free(arg2);
        arg2 = NULL;

        bufSize = 0;

        write(1, "(trade)> ", strlen("(trade)> "));

        do{
            // Check flag before attempting to read
            if (flag == 1) {
                freeInventory(&invTrade);
                freeTradeOffer(&userOffer);
                free(line);
                free(arg1);
                free(arg2);
                freeAndExit(information->config, information->inventory, information, NULL, NULL, NULL, information->thread, information->server_thread, information->running, information->protocol_state, information->server_fd);
            }
            
            // Use select to check if input is available with timeout
            fd_set readfds;
            struct timeval tv;
            FD_ZERO(&readfds);
            FD_SET(0, &readfds);
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100ms timeout
            
            int ret = select(1, &readfds, NULL, NULL, &tv);
            if (ret < 0) {
                // Select interrupted by signal
                if (flag == 1) {
                    freeInventory(&invTrade);
                    freeTradeOffer(&userOffer);
                    free(line);
                    free(arg1);
                    free(arg2);
                    freeAndExit(information->config, information->inventory, information, NULL, NULL, NULL, information->thread, information->server_thread, information->running, information->protocol_state, information->server_fd);
                }
                continue;
            } else if (ret == 0) {
                // Timeout - no input available, check flag and loop again
                continue;
            }
            
            // Input is available, read it
            if (read(0, &ch, 1) <= 0) {
                if (flag == 1) {
                    freeInventory(&invTrade);
                    freeTradeOffer(&userOffer);
                    free(line);
                    free(arg1);
                    free(arg2);
                    freeAndExit(information->config, information->inventory, information, NULL, NULL, NULL, information->thread, information->server_thread, information->running, information->protocol_state, information->server_fd);
                }
                continue;
            }
            bufSize++;
            addToBuff(ch, &line, &bufSize);

        }while (ch != '\n' && flag == 0);
        
        // Check if flag was set during the loop
        if (flag == 1) {
            freeInventory(&invTrade);
            freeTradeOffer(&userOffer);
            free(line);
            free(arg1);
            free(arg2);
            freeAndExit(information->config, information->inventory, information, NULL, NULL, NULL, information->thread, information->server_thread, information->running, information->protocol_state, information->server_fd);
        }

        bufSize++;
        char *temp = (char*)realloc(line, bufSize);
        if (!temp) {
            write(1, "Error: memory allocation failed.\n", 33);
            freeInventory(&invTrade);
            exit(1);
        }
        line = temp;
        line[bufSize - 1] = '\0';
        
        // Skip leading spaces
        char *trimmed_line = line;
        while (*trimmed_line == ' ') {
            trimmed_line++;
        }

        if (trimmed_line != NULL && *trimmed_line != '\0')
        {
            switch (checkLine(trimmed_line)){
                case 1: //exit trade
                
                    startTradeMessage(1, information);
                    exit_loop = 1;
                    freeTradeOffer(&userOffer);
                    break;
                    
                case 2: //add product
                    if (separateTradeInformation(trimmed_line, &arg1, &arg2, 3) != 3) {
                        addOption(&invTrade, &userOffer, arg1, arg2);
                    }
                    break;

                case 3: //remove product
                    if (separateTradeInformation(trimmed_line, &arg1, &arg2, 6) != 3) {
                        removeOption(&userOffer, arg1, arg2);
                    }
                    break;

                case 4: //send trade
                
                    startTradeMessage(4, information);
                    writeTradeOffer(&userOffer);
                    
                    
                    int list_result = send_product_list_request(
                        information->config->name,  
                        information->arg1,           
                        information                 
                    );
                    
                    if (list_result != 0) {
                        write(1, "Failed to send trade request.\n", 31);
                        freeTradeOffer(&userOffer);
                        exit_loop = 1;
                        break;
                    }
                    

                    char *file_size_str;
                    asprintf(&file_size_str, "%d", 512); 
                    
                    int order_result = send_order_header(
                        "tradeOffer",                              
                        file_size_str,                             
                        "0123456789abcdef0123456789abcdef",       
                        information->arg1,                         
                        information                                
                    );
                    
                    free(file_size_str);
                    
                    if (order_result == 0) {
                        write(1, "Trade completed successfully!\n", 31);
                    } else {
                        write(1, "Failed to send order request.\n", 31);
                    }
                    
                    freeTradeOffer(&userOffer);
                    exit_loop = 1;
                    
                    break;
                    
                case 0: //error
                    startTradeMessage(0, information);
                    break;

            }
        }
        
        free(line);
    }
    
    // Free arg1 and arg2 before exiting
    free(arg1);
    free(arg2);
    freeInventory(&invTrade);
}

void pledgeStatus(ThreadData *information){
    if(information == NULL || information->config == NULL || 
       information->pending_list == NULL || information->pending_mutex == NULL){
        write(1, "Error: Invalid parameters for pledge status\n", 45);
        return;
    }
    
    pthread_mutex_lock(information->pending_mutex);
    int has_pending = 0;
    for(int i = 0; i < information->pending_list->count; i++){
        write(1, "- ", 2);
        write(1, information->pending_list->entries[i].realm_name, 
              strlen(information->pending_list->entries[i].realm_name));
        write(1, ": PENDING\n", 10);
        has_pending = 1;
    }
    pthread_mutex_unlock(information->pending_mutex);
    
    int has_allied = 0;
    for(int i = 0; i < information->config->route_count; i++){
        if(information->config->routes[i].realm_name != NULL &&
           information->config->routes[i].is_default == 0 &&
           information->config->routes[i].active_alliance == 1){
            write(1, "- ", 2);
            write(1, information->config->routes[i].realm_name,
                  strlen(information->config->routes[i].realm_name));
            write(1, ": ALLIED\n", 9);
            has_allied = 1;
        }
    }
    
    if(!has_pending && !has_allied){
        write(1, "No pending alliance requests or active alliances.\n", 50);
    }
}
