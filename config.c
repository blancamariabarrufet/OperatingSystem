/**********************************************
*
* @File : config.c
* @Purpose : Configuration loading and management for the Maester application, including parsing routes and settings from config file.
* @Author : Blanca Maria Barrufet and Miquel Pla
* @Date : 18/10/2025
*
***********************************************/


#include <stdio.h>
#include <unistd.h>   /* read, close, write */
#include <fcntl.h>    /* open */
#include <stdlib.h>   /* malloc, realloc, free */
#include <string.h>   /* strcmp, memcpy */

#include "config.h"



// Read one line from fd until '\n' or EOF.
static char *readLine(int fd) {
    char *line = NULL;
    size_t len = 0;
    char ch;
    ssize_t r;

    while (1) {
        r = read(fd, &ch, 1);
        if (0 == r) {
            if (0 == len) { 
                free(line); 
                return NULL; 
            }
            break; // EOF but we have data 
        }
        if (0 > r) {
            free(line); 
            return NULL; 
        } // Error
        if ('\n' == ch) break;

         
        char *tmp = realloc(line, len + 1);
        if (NULL == tmp) { 
            free(line); 
            return NULL; 
        } // Error
        line = tmp;
        line[len] = ch;
        len = len + 1;
    }

    // add \0 
    char *tmp = realloc(line, len + 1);
    if (NULL == tmp) { free(line); return NULL; }
    line = tmp;
    line[len] = '\0';
    return line;
}

// Put \0 into end of item
static char *putTermination(char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (NULL == out) return NULL;
    if (n > 0) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

// Pas (string)Port to (int)Port
int parseInt(char *s, int *out){
    int v = 0;
    if (NULL == s || NULL == out) return 0;

    while (*s >= '0' && *s <= '9')
    {
        v = v * 10 + (*s - '0');
        s++;
    }

    *out = v;
    return 1;
}

/****************************************************************************************************************
*
* @Name: checkRealmExists
* @Def: Checks if the given realm name exists in the config's routes array, excluding the current realm's name (prints a message if it matches the current name).
* @Arg: In: config = pointer to loaded maesterConfig.
*        In: realm = NUL-terminated string of the realm name to check.
* @Ret: Returns 1 if the realm exists in routes, 0 otherwise (including if it matches config->name or invalid inputs).
*
****************************************************************************************************************/
int checkRealmExists(maesterConfig *config, char *realm){
    int i;
    if (NULL == config || NULL == realm){
        write(1, "Files nules... Exiting\n", sizeof("Files nules... Exiting\n"));
        return 0;
    }    
    

    if (config->name != NULL && 0 == strcasecmp(config->name, realm)){
        write(1, "This is the name of the actual realm\n", strlen("This is the name of the actual realm\n"));
        return 0;
    }

    for (i = 0; i < config->route_count; i++){
        char *realmName = config->routes[i].realm_name;

        if (realmName != NULL && 0 == strcasecmp(realmName, realm))
        {
            return 1;
        }
    }

    return 0;
}


/****************************************************************************************************************
*
* @Name: parseRouteLines
* @Def: Parses a single route line from the config file in the format "REALM IP PORT", allocating memory for realm_name and ip fields, parsing the port as an integer, and setting flags for known and default routes.
* @Arg: In: line = NUL-terminated string containing the route line to parse (e.g., "KingsLanding 192.168.1.5 9002").
*        Out: e = pointer to routeEntry struct to populate with parsed data (allocated strings must be freed by caller).
* @Ret: Returns 1 if parsing succeeds (e is populated), 0 on failure (invalid format, allocation error, or non-numeric port).
*
****************************************************************************************************************/
static int parseRouteLines(char *line, routeEntry *e){
    char *p = line;
    char *start;
    size_t n;
    char *realm = NULL;
    char *ip = NULL;
    char *portStr = NULL;
    int ok;

    if (NULL == line || NULL == e) return 0;

    // Real Realm
    while (*p == ' ') p++;
    start = p;
    while (*p != '\0' && *p != ' ') p++;
    n = (size_t)(p - start);
    if (0 == n) return 0;
    realm = putTermination(start, n);
    if (NULL == realm) return 0;

    // Read IP
    while (*p == ' ') p++;
    start = p;
    while (*p != '\0' && *p != ' ') p++;
    n = (size_t)(p - start);
    if (0 == n) { 
        free(realm); 
        return 0; 
    }

    ip = putTermination(start, n);
    if (NULL == ip) { 
        free(realm); 
        return 0; 
    }

    // Read port
    while (*p == ' ') p++;
    start = p;
    while (*p != '\0' && *p != ' ') p++;
    n = (size_t)(p - start);
    if (0 == n) { 
        free(realm); 
        free(ip); 
        return 0; 
    }

    portStr = putTermination(start, n);
    if (NULL == portStr) { 
        free(realm); 
        free(ip); 
        return 0; 
    }

    e->realm_name = realm;
    e->ip = ip;
    e->port = 0;
    e->is_known = 1;
    e->is_default = 0;
    e->active_alliance = 0;
    e->is_active = 0;

    ok = parseInt(portStr, &e->port);
    free(portStr);
    if (0 == ok) {
        free(e->realm_name);
        free(e->ip);
        e->realm_name = NULL;
        e->ip = NULL;
        return 0;
    }

    if (0 == strcmp(e->ip, "*.*.*.*")) e->is_known = 0;
    if (0 == strcmp(e->realm_name, "DEFAULT")) e->is_default = 1;

    return 1;
}

/****************************************************************************************************************
*
* @Name: freeConfig
* @Def: Releases all dynamically allocated memory in the maesterConfig struct, including name, paths, IP, and the routes array (freeing each route's realm_name and ip).
* @Arg: In/Out: config = pointer to maesterConfig to clean up (sets pointers to NULL after freeing).
* @Ret: Void (no return value).
*
****************************************************************************************************************/
// Free all dynamic memory inside config
void freeConfig(maesterConfig *config){
    int i;
    if (NULL == config) return;

    free(config->name);
    free(config->folder_path);
    free(config->listen_ip);
    config->name = NULL;
    config->folder_path = NULL;
    config->listen_ip = NULL;

    if (config->routes != NULL) {
        for (i = 0; i < config->route_count; i++) {
            free(config->routes[i].realm_name);
            free(config->routes[i].ip);
        }
        free(config->routes);
        config->routes = NULL;
    }
    config->route_count = 0;
}

/****************************************************************************************************************
*
* @Name: loadMaesterConfig
* @Def: Loads and parses the Maester configuration from the specified file, populating the config struct with realm name, folder path, envoy count, listen IP/port, and an array of routes until EOF.
* @Arg: In: filename = path to the config file to read.
*        Out: config = pointer to maesterConfig struct to initialize and populate (dynamic fields allocated; caller must free with freeConfig).
* @Ret: Returns 1 on successful loading and parsing, 0 on failure (file open/read error, parse error, or memory allocation failure).
*
****************************************************************************************************************/
// Load config file Returns 1 on success, 0 on failure.
int loadMaesterConfig(char *filename, maesterConfig *config) {
    int fd;
    char *line = NULL;
    int ok;
    int tmp;

    if (NULL == config){
        return 0;
    } 

    // Init of struct
    config->name = NULL;
    config->folder_path = NULL;
    config->envoy_count = 0;
    config->listen_ip = NULL;
    config->listen_port = 0;
    config->routes = NULL;
    config->route_count = 0;

    fd = open(filename, O_RDONLY);
    if (0 > fd) return 0;

    // Read Real name
    line = readLine(fd);
    if (NULL == line) { 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }
    config->name = putTermination(line, strlen(line));
    free(line); 
    line = NULL;
    if (NULL == config->name) { 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }

    // Read folder path
    line = readLine(fd);
    if (NULL == line) { 
        close(fd); 
        freeConfig(config); 
        return 0;
    }
    
    config->folder_path = putTermination(line, strlen(line));
    free(line); 
    line = NULL;
    if (NULL == config->folder_path) { 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }

    // Read num Envoys
    line = readLine(fd);
    if (NULL == line) { 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }
    ok = parseInt(line, &tmp);
    if (0 == ok) { 
        free(line); 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }
    config->envoy_count = tmp;
    free(line); 
    line = NULL;

    // Read IP
    line = readLine(fd);
    if (NULL == line) { 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }

    config->listen_ip = putTermination(line, strlen(line));
    free(line); line = NULL;
    if (NULL == config->listen_ip) { 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }

    // read port
    line = readLine(fd);
    if (NULL == line) { 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }
    ok = parseInt(line, &tmp);
    if (0 == ok) { 
        free(line); 
        close(fd); 
        freeConfig(config); 
        return 0; 
    }
    config->listen_port = tmp;
    free(line); 
    line = NULL;

    // Read  --- ROUTES --- and discard */
    line = readLine(fd);
    if (line != NULL) { 
        free(line); 
        line = NULL; 
    }

    // Read routes until EOF
    while (1){
        routeEntry entry;
        char *routeLine;

        routeLine = readLine(fd);

        if (NULL == routeLine)
        {
            break; //EOF
        }

        if (1 == parseRouteLines(routeLine, &entry))
        {
            routeEntry *tmpArray;

            tmpArray = (routeEntry *)realloc(
                config->routes,
                (size_t)(config->route_count + 1) * sizeof(*config->routes)
            );

            if (NULL == tmpArray)
            {
                free(routeLine);
                close(fd);
                free(entry.realm_name);
                free(entry.ip);
                freeConfig(config);
                return 0;
            }

            config->routes = tmpArray;
            config->routes[config->route_count] = entry;
            config->route_count = config->route_count + 1;
    }

    free(routeLine);
}

close(fd);
return 1;
}

/****************************************************************************************************************
*
* @Name: parseOriginAddress
* @Def: Parses an origin address string in format "IP:PORT" and extracts IP and port separately.
* @Arg: In: origin = origin string like "127.0.0.1:8665"
*        Out: ip_buf = buffer to store IP string (must be at least 20 bytes)
*        Out: port = pointer to store parsed port number
* @Ret: Returns 1 on success, 0 on failure
*
****************************************************************************************************************/
int parseOriginAddress(const char *origin, char *ip_buf, int *port){
    const char *colon;
    size_t ip_len;
    
    if(origin == NULL || ip_buf == NULL || port == NULL){
        return 0;
    }
    
    /* Find colon separator */
    colon = strchr(origin, ':');
    if(colon == NULL){
        return 0;
    }
    
    /* Extract IP */
    ip_len = (size_t)(colon - origin);
    if(ip_len >= 20){
        return 0;  /* IP too long */
    }
    
    memcpy(ip_buf, origin, ip_len);
    ip_buf[ip_len] = '\0';
    
    /* Parse port */
    if(!parseInt((char *)(colon + 1), port)){
        return 0;
    }
    
    return 1;
}

/****************************************************************************************************************
*
* @Name: findDefaultRoute
* @Def: Finds the DEFAULT routing entry in the config's routes array.
* @Arg: In: config = pointer to maesterConfig
* @Ret: Returns pointer to routeEntry if DEFAULT found, NULL otherwise
*
****************************************************************************************************************/
routeEntry* findDefaultRoute(maesterConfig *config){
    int i;
    
    if(config == NULL || config->routes == NULL){
        return NULL;
    }
    
    for(i = 0; i < config->route_count; i++){
        if(config->routes[i].is_default == 1){
            return &config->routes[i];
        }
    }
    
    return NULL;
}

/****************************************************************************************************************
*
* @Name: updateRouteEntry
* @Def: Updates a routing table entry with new IP, port, and alliance flags.
* @Arg: In: config = pointer to maesterConfig
*        In: realm_name = name of realm to update
*        In: ip = new IP address string
*        In: port = new port number
* @Ret: Returns 1 on success, 0 if realm not found
*
****************************************************************************************************************/
int updateRouteEntry(maesterConfig *config, const char *realm_name, const char *ip, int port){
    int i;
    
    if(config == NULL || realm_name == NULL || ip == NULL){
        return 0;
    }
    
    /* Find the realm in routing table */
    for(i = 0; i < config->route_count; i++){
        if(config->routes[i].realm_name != NULL && 
           strcasecmp(config->routes[i].realm_name, realm_name) == 0){
            
            /* Free old IP and allocate new one */
            free(config->routes[i].ip);
            config->routes[i].ip = putTermination((char *)ip, strlen(ip));
            
            if(config->routes[i].ip == NULL){
                return 0;  /* Allocation failed */
            }
            
            /* Update port and flags */
            config->routes[i].port = port;
            config->routes[i].is_known = 1;
            config->routes[i].active_alliance = 1;  /* Alliance is now active */
            config->routes[i].is_active = 1;
            
            return 1;
        }
    }
    
    return 0;  /* Realm not found */
}

