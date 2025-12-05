/**********************************************
*
* @File : console.h
* @Purpose : Header file defining command enums and declaring console-related functions.
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 18/10/2025
*
***********************************************/

#ifndef CONSOLE_H
#define CONSOLE_H



enum {
    CMD_LIST_REALMS = 1,  // LIST REALMS
    CMD_PLEDGE      = 2,  // PLEDGE <REALM> <sigil.jpg>
    CMD_PLEDGE_RESP = 3,  // PLEDGE RESPOND <REALM> ACCEPT / REJECT
    CMD_LIST_PROD_R = 4,  // LIST PRODUCTS <REALM>
    CMD_LIST_PROD_L = 5,  // LIST PRODUCTS
    CMD_START_TRADE = 6,  // START TRADE <REALM>
    CMD_PLEDGE_STAT = 7,  // PLEDGE STATUS
    CMD_ENVOY_STAT  = 8,  // ENVOY STATUS
    CMD_EXIT        = 9   // EXIT
};

/****************************************************************************************************************
*
* @Name: commandMotor
* @Def: Reads user input from stdin, processes the command string, dispatching to appropriate check functions based on the first letter, extracting arguments, and returning the command enum or error code.
* @Arg: Out: buf_out = pointer to char* to store the allocated input buffer (caller must free).
*        Out: arg1_out = pointer to char* for first argument (allocated and set; caller must free).
*        Out: arg2_out = pointer to char* for second argument (allocated and set; caller must free).
* @Ret: Returns the command enum value (e.g., CMD_LIST_REALMS) if valid, -2 for similar command suggestion, -3 for wrong argument count, 0 for unknown.
*
****************************************************************************************************************/
int commandMotor(char **buf_out, char **arg1_out, char **arg2_out);

/****************************************************************************************************************
*
* @Name: countCorrectLetters
* @Def: Compares characters in buf with command, counting matches and determining if they are exact, similar, or not similar.
* @Arg: In: buf = NUL-terminated input string to compare.
*        In: command = NUL-terminated target string to match.
*        In: length = length of the command string.
* @Ret: Returns 1 if exact match, 2 if similar, 0 if not similar.
*
****************************************************************************************************************/
int countCorrectLetters(char *buf, char *command, int length);

char* readCommand(volatile int *flag, int *bufSize_out);

/****************************************************************************************************************/

void addToBuff(char c, char **buf, int *bufSize);

char *read_until(int fd, char end);

void addCharToBuf(char **buf, char ch, int newSize);

int compareCommand(char *buf, char **arg1_out, char **arg2_out, int bufSize);
#endif //CONSOLE_H
