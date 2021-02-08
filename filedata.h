/* This is an updated version of the starter code that you may find more
   useful. You are welcome to change this or the original starter code in 
   any way that you like.  Remember that if you use something that you have    
   not seen in lectures, labs, the 209 website or Piazza, you must get 
   instructor permission. 
*/

#include <time.h>
#include "message.h"

struct file_info {
    char filename[MAXNAME];
    time_t mtime;
    time_t mtime2;
};

struct client_info {
    int sock;
    char userid[MAXNAME];
    char dirname[MAXNAME];
    struct file_info files[MAXFILES];
    int STATE;
    /* next items for file this client is currently receiving */
    char currFilename[MAXNAME];
    int expected_size;
    int received_so_far;
    time_t mtime;  
    
};
extern struct client_info clients[MAXCLIENTS];

void init();
int add_client(struct login_message s);
struct file_info *check_file(struct file_info *files, char *filename);
void clear_client(int i);
void display_clients();


