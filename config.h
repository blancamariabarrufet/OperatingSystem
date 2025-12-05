/**********************************************
*
* @File : config.h
* @Purpose : Header file defining structures and functions for Maester configuration handling.
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 18/10/2025
*
***********************************************/

#ifndef CONFIG_H
#define CONFIG_H

typedef struct
{
    char *realm_name;   
    char *ip;           
    int   port;
    int   is_known;        /* 0 if ip == "*.*.*.*", else 1 */
    int   is_default;      /* 1 if realm_name == "DEFAULT", else 0 */
    int   active_alliance; /* 1 if we have an alliance with this realm, 0 if not */
    int   is_active;       /* 1 if the realm is active, 0 if not */
} routeEntry;

typedef struct
{
    char *name;         
    char *folder_path;  
    int   envoy_count;
    char *listen_ip;    
    int   listen_port;
    routeEntry *routes; 
    int   route_count;
} maesterConfig;

int loadMaesterConfig(char *filename, maesterConfig *config);

void freeConfig(maesterConfig *config);

int checkRealmExists(maesterConfig *config, char *realm);


int parseInt(char *s, int *out);

int parseOriginAddress(const char *origin, char *ip_buf, int *port);
routeEntry* findDefaultRoute(maesterConfig *config);
int updateRouteEntry(maesterConfig *config, const char *realm_name, const char *ip, int port);


#endif /* CONFIG_H */
