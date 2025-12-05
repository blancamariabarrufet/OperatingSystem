/**********************************************
*
* @File : inventory.h
* @Purpose : Header file defining Product and Inventory structures and declaring related functions.
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 18/10/2025
*
***********************************************/

#ifndef INVENTORY_H
#define INVENTORY_H

typedef struct
{
    char  name[100];  /* stays fixed-size here, matches your INVENTORY.C */
    int   amount;
    float weight;
} Product;

typedef struct
{
    Product *products;
    int      nProducts;
} Inventory;

/****************************************************************************************************************
*
* @Name: invInit
* @Def: Initializes an Inventory struct to empty state, setting products pointer to NULL and count to 0.
* @Arg: Out: inv = pointer to Inventory struct to initialize.
* @Ret: Void (no return value).
*
****************************************************************************************************************/
void invInit(Inventory *inv);

/****************************************************************************************************************
*
* @Name: freeInventory
* @Def: Frees the dynamic products array in the Inventory struct and resets it to empty state.
* @Arg: In/Out: inv = pointer to Inventory to clean up.
* @Ret: Void (no return value).
*
****************************************************************************************************************/
void freeInventory(Inventory *inv);

/****************************************************************************************************************
*
* @Name: loadInventory
* @Def: Reads a binary inventory file, loading Product records into a dynamic array in the Inventory struct.
* @Arg: Out: inv = pointer to Inventory to populate (initialized first).
*        In: path = file path to the binary inventory file.
* @Ret: Returns 0 on success, -1 on file open/read/realloc error.
*
****************************************************************************************************************/
int loadInventory(Inventory *inv, char *path);

/****************************************************************************************************************
*
* @Name: saveInventory
* @Def: Writes the Inventory's products array to a binary file, overwriting any existing content.
* @Arg: In: inv = pointer to loaded Inventory.
*        In: path = file path to write the binary data.
* @Ret: Returns 0 on success, -1 on file open/write/close error.
*
****************************************************************************************************************/
int saveInventory(Inventory *inv, char *path);

/****************************************************************************************************************
*
* @Name: invFindIndex
* @Def: Searches the inventory's products array for the first product whose name matches the given string (case-insensitive).
* @Arg: In: inv = pointer to loaded Inventory.
*        In: name = NUL-terminated string of product name to find.
* @Ret: Returns the zero-based index if found, -1 if not found or invalid inputs.
*
****************************************************************************************************************/
int invFindIndex( Inventory *inv,  char *name);

/****************************************************************************************************************
*
* @Name: printInventory
* @Def: Prints a formatted table of the inventory's products to stdout, including name, amount (as "Value (Gold)"), weight, and total count.
* @Arg: In: inv = pointer to loaded Inventory.
* @Ret: Void (no return value).
*
****************************************************************************************************************/
void printInventory(Inventory *inv);

#endif /* INVENTORY_H */
