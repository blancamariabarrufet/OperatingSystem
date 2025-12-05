/**********************************************
*
* @File : console.c
* @Purpose : Implementation of console input reading, command parsing with fuzzy matching, and parameter extraction.
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 18/10/2025
*
***********************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>


#include "console.h"

// External global variable for signal handling
extern volatile int flag;


void addCharToBuf(char **buf, char ch, int newSize) {

    char *pointer;

    if (*buf) {
        pointer = (char*)realloc(*buf, newSize);
    } else {
        pointer = (char*)malloc(newSize);
    }

    if (!pointer) {
        write(1, "Error: memory allocation for buffer failed.\n", 44);
        exit(1);
    }

    *buf = pointer;
    (*buf)[newSize - 1] = ch;
}

/**********************************
 functions for command recognition
 **********************************/


int countCorrectLetters(char *buf, char *command, int length){
    int correctLetters = 0;

    for (int i = 0; i < length; i++){
        if (buf[i] == command[i]){
            correctLetters++;
        }
    }
    if((length - correctLetters) < 3){
        if(length == correctLetters){
            return 1;
        }
        return 2;
    }else{
        return 0;
    }
}


int getParameters(char *buf, char **arg1_out, char **arg2_out, int commandLength){
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

    int firstArg = 0;   // 0 = filling arg1, 1 = switched to arg2, 2 = filling arg2, 3 = arg2 complete

    for (int i = currentPosition; buf[i] != '\0' && buf[i] != '\n'; i++){
        if (buf[i] == ' '){
            if (firstArg == 0 && len1 > 0) {
                firstArg = 1;
                len1 += 1;
                addCharToBuf(arg1_out, '\0', len1);
            } else if (firstArg == 1) {
                continue;
            } else if (firstArg == 2 && len2 > 0) {
                firstArg = 3;
                len2 += 1;
                addCharToBuf(arg2_out, '\0', len2);
            } else if (firstArg == 3) {
                continue;
            }
        } else {
            if (firstArg == 0){
                len1 += 1;
                addCharToBuf(arg1_out, buf[i], len1);
                parameters = 1;
            } else if (firstArg == 1) {
                firstArg = 2;
                parameters = 2;
                len2 += 1;
                addCharToBuf(arg2_out, buf[i], len2);
            } else if (firstArg == 2) {
                len2 += 1;
                addCharToBuf(arg2_out, buf[i], len2);
            } else if (firstArg == 3) {
                return 3;
            }
        }
        currentPosition = i;
    }

    if (len1 > 0 && *arg1_out != NULL && (*arg1_out)[len1 - 1] != '\0') {
        len1 += 1;
        addCharToBuf(arg1_out, '\0', len1);
    }

    if (len2 > 0 && *arg2_out != NULL && (*arg2_out)[len2 - 1] != '\0') {
        len2 += 1;
        addCharToBuf(arg2_out, '\0', len2);
    } else if (len2 == 0) {
        len2 += 1;
        addCharToBuf(arg2_out, '\0', len2);
    }
    
    return parameters;
}


void sendUsageMessage(int state){
    switch(state){
        case CMD_LIST_REALMS:
            write(1, "Usage: LIST REALMS\n", 19);
            break;
        case CMD_LIST_PROD_R:
            write(1, "Usage: LIST PRODUCTS <REALM>\n", 29);
            break;
        case CMD_PLEDGE:
            write(1, "Usage: PLEDGE <REALM> <sigil.jpg>\n", 34);
            break;
        case CMD_PLEDGE_RESP:
            write(1, "Usage: PLEDGE RESPOND <REALM> ACCEPT / REJECT\n", 47);
            break;
        case CMD_START_TRADE:
            write(1, "\nIncorrect number of arguments, can't start a trade. plese review the syntax.\n", sizeof("\nIncorrect number of arguments, can't start a trade. plese review the syntax.\n"));
            write(1, "Usage: START TRADE <REALM>\n", 28);
            break;
        case CMD_PLEDGE_STAT:
            write(1, "Usage: PLEDGE STATUS\n", 22);
            break;
        case CMD_ENVOY_STAT:
            write(1, "Usage: ENVOY STATUS\n", 20);
            break;
        case CMD_EXIT:
            write(1, "Usage: EXIT\n", 13);
            break;

        case 10:
            write(1, "Incorrect PLEDGE command\n", sizeof ("Incorrect PLEDGE command\n"));
            write(1, "Usage: PLEDGE STATUS\n", sizeof ("Usage: PLEDGE STATUS\n"));
            write(1, "Usage: PLEDGE <REALM> <sigil.jpg>\n", sizeof("Usage: PLEDGE <REALM> <sigil.jpg>\n"));
            write(1, "Usage: PLEDGE RESPOND <REALM> ACCEPT / REJECT\n", sizeof ("Usage: PLEDGE RESPOND <REALM> ACCEPT / REJECT\n"));

            break;

        default:
            write(1, "Unknown command\n", 17);
            break;
    }
}

int checkCommand(char *buf, char *command, char **arg1_out, char **arg2_out, int nArguments, int stateToUpdate, int commandSize){


    int isCorrect = countCorrectLetters(buf, command, commandSize);

    if (isCorrect == 1){
        int paramCount = getParameters(buf, arg1_out, arg2_out, commandSize);


        if (nArguments == 0){

            if (paramCount == 0){
                return stateToUpdate;

            }
            else{
                sendUsageMessage(stateToUpdate);
                return -3;
            }


        }else{

            if (paramCount == nArguments){
                return stateToUpdate;

            }else{
                sendUsageMessage(stateToUpdate);
                return -3;
            }
        }

    }else if (isCorrect == 2){

        write(1, "Did you mean to send  ", 21);
        write(1, command, strlen(command));
        write(1, "?\n", 2);

        return -2;

    }else{
        write(1, "Unknown command\n", 17);
        return 0;
    }
}

int checkListProducts(char *buf, char **arg1_out, char **arg2_out){

    int isCorrect = countCorrectLetters(buf, "list products" , 13);

    if (isCorrect == 1){

        int paramCount = getParameters(buf, arg1_out, arg2_out, 13);

        if (paramCount == 1){ //LIST PRODUCTS <REALM>
            return CMD_LIST_PROD_R;
        }else if (paramCount == 0){ //LIST PRODUCTS
            return CMD_LIST_PROD_L;

        }else{
            write(1, "Usage: LIST PRODUCTS <REALM>\n", 29);
        }
    }else if (isCorrect == 2){
        write(1, "Did you mean to send a LIST PRODUCTS? \n", 39);
    }else{
        return checkCommand(buf, "list realms", arg1_out, arg2_out, 0, CMD_LIST_REALMS, 11);
    }
    return 0;
}

int checkPledge(char *buf, char **arg1_out, char **arg2_out){

    int isCorrect;

    /*
     * PLEDGE <REALM> <sigil.jpg>
     * PLEDGE RESPOND <REALM> ACCEPT / REJECT
     * PLEDGE STATUS
     */


    isCorrect = countCorrectLetters(buf, "pledge", 6);


    if (isCorrect==0){
        write(1, "Unknown command\n", 17);
        return 0;
    }else if(isCorrect == 2){
        sendUsageMessage(10);
        return -2;
    }else{
        getParameters(buf, arg1_out, arg2_out, 6);

        if (0==strcmp(*arg1_out, "respond")){
            return checkCommand(buf, "pledge respond", arg1_out, arg2_out, 2, CMD_PLEDGE_RESP, 14);
        }else if (0==strcmp(*arg1_out, "status")) {
            return checkCommand(buf, "pledge status", arg1_out, arg2_out, 0, CMD_PLEDGE_STAT, 13);
        }else{
            return checkCommand(buf, "pledge", arg1_out, arg2_out, 2, CMD_PLEDGE, 6);
        }
    }

}

int compareCommand(char *buf, char **arg1_out, char **arg2_out, int bufSize){


    switch (buf[0]){

        case 'l':
            // LIST PRODUCTS <REALM>
            // LIST PRODUCTS
            // LIST REALMS
            if (bufSize <= 13){ //case of List Realms (11 chars + \n + \0 = 13)
                return checkCommand(buf, "list realms", arg1_out, arg2_out, 0, CMD_LIST_REALMS, 11);
            }
            else{
                return checkListProducts(buf, arg1_out, arg2_out);
            }
            break;

        case 'p':
            // PLEDGE <REALM> <sigil.jpg>
            // PLEDGE RESPOND <REALM> ACCEPT / REJECT
            // PLEDGE STATUS
            return checkPledge(buf, arg1_out, arg2_out);
            break;

        case 's':
            // START TRADE <REALM>
            return checkCommand(buf, "start trade", arg1_out, arg2_out, 1, CMD_START_TRADE, 11);
            break;

        case 'e':
            // ENVOY STATUS
            // EXIT
            if (bufSize <= 6){ // "exit" = 4 chars + \n + \0 = 6
                return checkCommand(buf, "exit", arg1_out, arg2_out, 0, CMD_EXIT, 4);
            }else{
                return checkCommand(buf, "envoy status", arg1_out, arg2_out, 0, CMD_ENVOY_STAT, 12);
            }
            break;

        default:
            write(1, "Unknown command\n", 17);
            return 0;

    }
}

/******************************
 functions for command reading
 ******************************/




void addToBuff(char c, char **buf, int *bufSize){
    if (c != '&'){
        if (c >= 'A' && c <= 'Z') {
            c += ('a' - 'A');
        }
        size_t need = (size_t)(*bufSize) + 1;
        char *pointer;

        if (*buf) {
            pointer = (char*)realloc(*buf, need);
        } else {
            pointer = (char*)malloc(need);
        }

        if (!pointer) {
            write(1, "Error: memory allocation for command failed.\n", 45);
            exit(1);
        }

        *buf = pointer;
        (*buf)[ (*bufSize) - 1 ] = c;

    }else {
        (*bufSize)--;
    }
}



char *read_until(int fd, char end) {
    char *string = NULL;
    char c = '\0';
    int i = 0, size;

    while (1) {
        size = read(fd, &c, sizeof(char));
        
        // Check if signal was received
        if (size <= 0) {
            if (flag == 1) {
                // Signal received, return empty string to allow cleanup
                if (string == NULL) {
                    string = (char *)malloc(2);
                }
                string[0] = ' ';
                string[1] = '\0';
                return string;
            }
            break;
        }
        
        if (string == NULL) {
            string = (char *)malloc(sizeof(char));
        }
        if (c != end && size > 0) {
            // Skip '&' character
            if (c == '&') {
                continue;
            }
            // Convert uppercase to lowercase
            if (c >= 'A' && c <= 'Z') {
                c += ('a' - 'A');
            }
            string = (char *)realloc(string, sizeof(char) * (i + 2));
            string[i++] = c;
        } else {
            break;
        }
    }

    string = (char *)realloc(string, sizeof(char) * (i + 2));
    string[i] = ' ';
    string[i + 1] = '\0';

    return string;
}





int commandMotor(char **buf_out, char **arg1, char **arg2){

    write(1, "$ ", sizeof("$ "));
    char *buf = read_until(0, '\n');
    
    // Check if SIGINT was received
    if (flag == 1) {
        free(buf);
        return CMD_EXIT;
    }
    
    // Store the buffer for the caller
    *buf_out = buf;
    
    return compareCommand(buf, arg1, arg2, strlen(buf));
}




