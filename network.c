/**********************************************
*
* @File : network.c
* @Purpose : Main network module - includes client and server handlers.
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





ssize_t read_exact(int fd, void *buf, size_t len){
    size_t total = 0;
    while(total < len){
        ssize_t n = read(fd, (char *)buf + total, len - total);
        if(n <= 0){
            return n;
        }
        total += n;
    }
    return (ssize_t)total;
}

ssize_t write_exact(int fd, const void *buf, size_t len){
    size_t total = 0;
    while(total < len){
        ssize_t n = write(fd, (const char *)buf + total, len - total);
        if(n <= 0){
            return n;
        }
        total += n;
    }
    return (ssize_t)total;
}


uint16_t compute_checksum(Frame *f){
    uint32_t sum = 0;
    uint16_t checksum_value = f->checksum;
    f->checksum = 0;

    uint8_t *bytes = (uint8_t *)f;
    for(int i = 0; i < FRAME_SIZE; i++){
        sum += bytes[i];
    }

    f->checksum = checksum_value;
    return (uint16_t)(sum % 65536);
}

void set_checksum(Frame *f){
    f->checksum = compute_checksum(f);
}

int verify_checksum(Frame *f){
    uint16_t calculated = compute_checksum(f);
    uint16_t received = f->checksum;
    return (calculated == received);
}
