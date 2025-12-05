/**********************************************
*
* @File : inventory.c
* @Purpose : Management of product inventory, including loading/saving binary files, searching, and printing.
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 18/10/2025
*
***********************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "inventory.h"



void invInit(Inventory *inv) {
    inv->products = NULL;
    inv->nProducts = 0;
}

void freeInventory(Inventory *inv) {
    free(inv->products);
    inv->products = NULL;
    inv->nProducts = 0;
}

int invFindIndex( Inventory *inv,  char *name) {
    for (int i = 0; i < inv->nProducts; ++i) {
        if (strncasecmp(inv->products[i].name, name, sizeof(inv->products[i].name)) == 0) {
            return i;
        }
    }
    return -1;
}

static void writeRow(char *name, int amount, float weight){
    char *msg = NULL;
    int n = asprintf(&msg, "%-25s | %12d | %13.1f\n", name, amount, weight);

    if (n < 0 || msg == NULL)
    {
        return;  
    }

    write(1, msg, n);
    free(msg);
}


static void writeStr(int fd,  char *s){
    write(fd, s, strlen(s));
}


int loadInventory(Inventory *inv, char *path) {
    invInit(inv);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        
        return -1;
    }

    Product p;
    ssize_t r;
    while ((r = read(fd, &p, sizeof(Product))) == sizeof(Product)) {
        Product *tmp = realloc(inv->products, (size_t)(inv->nProducts + 1) * sizeof(Product));
        if (!tmp) {
            write(1,"Error reallocating memory\n", 27);
            close(fd);
            freeInventory(inv);
            return -1;
        }
        inv->products = tmp;
        inv->products[inv->nProducts++] = p;
    }

    if (r < 0) {
        write(1, "Error reading file\n", 20);
        close(fd);
        freeInventory(inv);
        return -1;
    }

    close(fd);
    return 0;
}


int saveInventory(Inventory *inv, char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        write(1, "Error opening file for writing\n", 31);
        return -1;
    }

    for (int i = 0; i < inv->nProducts; ++i) {
        ssize_t w = write(fd, &inv->products[i], sizeof(Product));
        if (w != sizeof(Product)) {
            write(1, "Error writing file\n", 20);
            close(fd);
            return -1;
        }
    }

    if (close(fd) != 0) {
        write(1, "Error closing file\n", 20);
        return -1;
    }

    return 0;
}


/* Functionalities for future phases--------------------------------------------------------------
static int invAddProduct(Inventory *inv, char *name, int amount, float weight) {
    if (!name) {
        write(1, "Error: product name is NULL\n",sizeof("Error: product name is NULL\n"));
        return -1;
    }

    
    if (invFindIndex(inv, name) >= 0) {
        write(1, "Error: product already exists\n", sizeof("Error: product already exists\n"));
        return -1;
    }

    
    Product p;                                   
    snprintf(p.name, sizeof p.name, "%s", name);
    if (amount < 0) amount = 0;
    p.amount = amount;
    if (weight < 0) weight = 0;
    p.weight = weight;

    
    Product *tmp = realloc(inv->products, (size_t)(inv->nProducts + 1) * sizeof(Product));
    if (!tmp) {
        write(1, "Error: realloc failed while adding product\n", sizeof("Error: realloc failed while adding product\n"));
        return -1;
    }
    inv->products = tmp;
    inv->products[inv->nProducts] = p;
    inv->nProducts++;
    return 0;
}


static int invModifyProductAmount(Inventory *inv, char *name, int newAmount) {
    int id = invFindIndex(inv, name);
    if (id < 0) return -1;
    if (newAmount < 0) newAmount = 0; 
    inv->products[id].amount = newAmount;
    
    return 0;
}

static int invModifyProductWeight(Inventory *inv, char *name, float newWeight) {
    int id = invFindIndex(inv, name);
    if (id < 0) return -1;
    if (newWeight < 0) newWeight = 0; 
    inv->products[id].weight = newWeight;
    return 0;
}



static int invRemoveProduct(Inventory *inv, char *name) {
    int id = invFindIndex(inv, name);
    if (id < 0){
        write(1, "Error: product doesn't exist, so it can't be removed\n", sizeof("Error: product doesn't exist, so it can't be removed\n"));
        return -1;
    }

    
    inv->products[id] = inv->products[inv->nProducts - 1];
    inv->nProducts--;

    if (inv->nProducts == 0) {
        free(inv->products);
        inv->products = NULL;
    } else {
        Product *tmp = realloc(inv->products, (size_t)inv->nProducts * sizeof(Product));
        if (tmp) inv->products = tmp; 
    }
    return 0;
}
    
---------------------------------------------------------------------------------------------------------
*/

void printInventory(Inventory *inv) {
    writeStr(1, "--- Trade Ledger ---\n");
    writeStr(1, "Item                      | Value (Gold) | Weight (Stone)\n");
    writeStr(1, "---------------------------------------------------------\n");

    for (int i = 0; i < inv->nProducts; ++i) {
        Product *p = &inv->products[i];
        writeRow(p->name, p->amount, p->weight);
    }

    writeStr(1, "---------------------------------------------------------\n");

    char *tail = NULL;
    int n = asprintf(&tail, "Total Entries: %d\n\n", inv->nProducts);

    if (n > 0 && tail != NULL)
    {
        write(1, tail, n);
        free(tail);
    }
}


