#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "filedata.h"


struct client_info clients[MAXCLIENTS];

void clear_files(struct file_info *files) {
    int j;
    for (j = 0; j < MAXFILES; j++) {
        files[j].filename[0] = '\0';
        files[j].mtime = 0;
    }
}

void clear_client(int i) {
    clients[i].userid[0] = '\0';
    clients[i].dirname[0]= '\0';
    clients[i].sock = -1;
    clients[i].dirname[0] = '\0';
    clear_files(clients[i].files);
}

/* initialize dirs and clients */
void init(){
    int i;
    for(i = 0; i < MAXCLIENTS; i++) {
        clients[i].userid[0] = '\0';
        clients[i].dirname[0]= '\0';
        clients[i].sock = -1;
        
        clients[i].dirname[0] = '\0';
        clear_files(clients[i].files);
    }
}

/* Adds a client to the client array.
 * If the client is already in the array, update the dirname field with the 
 * dir field from s.  If an empty slot is available add the client userid and
 * dir.  
 * Return 0 if it was able to add or update the client, and -1 if there 
 * was no slot available*/
int add_client(struct login_message s) {
    int i;
    for(i = 0; i < MAXCLIENTS; i++) {
        if(clients[i].userid[0] != '\0') {
            if(strcmp(clients[i].userid, s.userid) == 0) {
                // client is already in array so check if dir matches
                if(strcmp(clients[i].dirname, s.dir) != 0) {
                    // if no match then update dirname and clear files
                    strncpy(clients[i].dirname, s.dir, MAXNAME);
                    clear_files(clients[i].files);
                }
                return 0;
            } 
        } else {
            // an empty slot to place the client
            strncpy(clients[i].userid, s.userid, MAXNAME);
            strncpy(clients[i].dirname, s.dir, MAXNAME);
            clear_files(clients[i].files);
            return 0;
        }
    }
    fprintf(stderr, "Error: Too many clients\n");
    return -1;
}


/* check_file - check if filename is in dirname's list
 *
 * - return a pointer to the file info struct for filename
 *   The caller will use this pointer to check mtime for this file
 * - add filename and return pointer to the file info struct if filename 
 *   was not found in dirname 
 * - return NULL if there is no more space in contents to add filename
 */

struct file_info *check_file(struct file_info *files, char *filename) {
    int i;
    for(i = 0; i < MAXFILES; i++) {
        if(strcmp(files[i].filename, filename) == 0) {
            return &files[i];
        } else if(files[i].filename[0] == '\0') {
            strncpy(files[i].filename, filename, MAXNAME);
            return &files[i];
        }
    }
    return NULL;
}

void display_clients() {
    int i = 0;
    while(clients[i].userid[0] != '\0') {
        printf("%s -  %s\n", clients[i].userid, clients[i].dirname);
        int j = 0;
        while(clients[i].files[j].filename[0] != '\0') {
            printf("    %s %ld\n", clients[i].files[j].filename, 
                (long int)clients[i].files[j].mtime);
            j++;
        }
        i++;
    }
}