#include "filedata.h"
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>    
#include <netdb.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include "wrapsock.h"
#include "dbclient.h"
#ifndef PORT
#define PORT 54099
#endif
ssize_t Readn(int fd, void *ptr, size_t nbytes);
void Writen(int fd, void *ptr, size_t nbytes);
void login(struct client_info *client);
void server_sync(struct client_info *client);
void server_get(struct client_info *client);
int is_in_server (struct file_info *file, char *filename);
void change_mtime(char *dirname, char *filename, long int mtime);
int find_file_from_server(struct file_info *files, char *filename);
void path_name(char *buf, char *dir, char *filename) ;
void server_synchronize_send(int soc, char* path, int size);
void send_from_server(struct client_info *client);
void server_getfile(struct client_info *client);

char *root = "server_dir/";

int main(){

    int i, maxi, maxfd, listenfd, connfd, sockfd;
    int nready;
    ssize_t n;
    fd_set rset, allset;
    int buf;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    int yes = 1;    

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(PORT);

    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))
       == -1) {
        perror("setsockopt");
    }

    Bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    Listen(listenfd, LISTENQ);

    maxfd = listenfd;
    maxi = -1;
    for (i = 0; i < MAXCLIENTS; i++) {
        clients[i].sock = -1;
        clients[i].STATE = LOGIN;
    } 

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    if(mkdir(root, S_IRWXU) == -1){
        printf("Server root (%s) already exists\n", root);
    }

    for ( ; ; ) {
    rset = allset;
    nready = Select(maxfd+1, &rset, NULL, NULL, NULL);
    
    if (FD_ISSET(listenfd, &rset)) {    
        clilen = sizeof(cliaddr);
        connfd = Accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
        printf("accepted a new client\n");

        for (i = 0; i < MAXCLIENTS; i++) {
          if (clients[i].STATE == LOGIN) {
              clients[i].sock = connfd;
                struct login_message login;
                Readn(clients[i].sock, &login, sizeof(struct login_message));
                strncpy(clients[i].userid, login.userid, MAXNAME);
                strncpy(clients[i].dirname, root, MAXNAME);
                strncat(clients[i].dirname, login.dir, strlen(login.dir));
                if(mkdir(clients[i].dirname, S_IRWXU) == -1){
                    printf("Directory %s already exists\n", clients[i].dirname);
                }
                clients[i].STATE = SYNC;
              break;
          }
        }
        if (i == MAXCLIENTS) {
          printf("too many clients\n");
        }
        
        FD_SET(connfd, &allset);
        if (connfd > maxfd)
          maxfd = connfd;
        if (i > maxi)
          maxi = i;
        
        if (--nready <= 0)
          continue;
    }

    for (i = 0; i <= maxi; i++) {   
        if ( (sockfd = clients[i].sock) < 0)
          continue;
        if (FD_ISSET(sockfd, &rset)) {
            if ( (n = Readn(sockfd, &buf, 1)) <= 0) {
                Close(sockfd);
                FD_CLR(sockfd, &allset);
                clients[i].STATE = LOGIN;
                clients[i].sock = -1;
                clear_client(i);
                printf("connection closed by client\n");
            } else {
              switch(clients[i].STATE){
                  case SYNC:
                             server_sync(&clients[i]);
                             break;
                  case GETFILE:
                             server_getfile(&clients[i]);
                             break;
                  default: break;
              }
            }
            
            if (--nready <= 0)
                break;
        }
    }
    }
    return 0;
}

void server_sync(struct client_info *client) {
    struct sync_message sync;
    Readn(client->sock, &sync, sizeof(struct sync_message));
    if ((strcmp(sync.filename, "") == 0) && (sync.mtime == 0) && (sync.size == 0)) {
        // empty message from client to check server files
        DIR *dir;
        if ((dir = opendir(client->dirname)) == NULL) {
            perror("Open Directory");
            exit(1);
        }
        struct dirent *dir_file;
        while ((dir_file = readdir(dir)) != NULL) {
            if (dir_file->d_type != 4) {
                char path[563];
                path_name(path, client->dirname, dir_file->d_name);

                if(is_in_server(client->files, dir_file->d_name) == 1) {
                    struct stat filestat;
                    stat(path, &filestat);
                    struct sync_message sync, response;
                    strncpy(sync.filename, dir_file->d_name, MAXNAME);
                    sync.mtime = filestat.st_mtime;
                    sync.size = htonl(filestat.st_size);
                    Writen(client->sock, &sync, sizeof(struct sync_message));
                    fprintf(stdout, "Sending %s to the client...\n", sync.filename);
                    server_synchronize_send(client->sock, path, filestat.st_size);
                    Readn(client->sock, &response, sizeof(struct sync_message));

                    int i = find_file_from_server(client->files, dir_file->d_name);
                    client->files[i].mtime = filestat.st_mtime;

                }

            }
        }

        struct sync_message emp;
        emp.filename[0] = '\0';
        emp.size = 0;
        emp.mtime = 0;
        Writen(client->sock, &emp, sizeof(struct sync_message));

        if (closedir(dir) == -1) {
            perror("closedir");
        }
    } else {
        struct sync_message response;
        struct stat filestat;
        char path[513];
        if(is_in_server(client->files, sync.filename) == 0) {
            path_name(path, client->dirname, sync.filename);
            if (stat(path, &filestat)) {
                perror("stat");
            }
            int i = find_file_from_server(client->files, sync.filename);
            strncpy(response.filename, sync.filename, MAXNAME);
            struct stat st;
            stat(path, &st);
            response.size = st.st_size;
            if (client->files[i].mtime2 > 0 && client->files[i].mtime2 < filestat.st_mtime) {
                response.mtime = filestat.st_mtime;
            } else {
                response.mtime = client->files[i].mtime;
            }

            Writen(client->sock, &response, sizeof(struct sync_message));
            if(sync.mtime > response.mtime) {
                fprintf(stdout, "Getting %s from the client...\n", sync.filename);
                client->files[i].mtime = sync.mtime;
                change_mtime(client->dirname, sync.filename, sync.mtime);
                client->STATE = GETFILE;
                strncpy(client->currFilename, sync.filename, MAXNAME);
                client->expected_size = ntohl(sync.size);
                client->received_so_far = 0;
                client->mtime = sync.mtime;

            } else if (client->files[i].mtime2 > 0 && client->files[i].mtime2 < filestat.st_mtime) {
                fprintf(stdout, "Sending %s to the client...\n", sync.filename);
                server_synchronize_send(client->sock, path, st.st_size);
                client->files[i].mtime = filestat.st_mtime;
                write(client->sock, "", 1);

            } else {
            }
        } else {
            fprintf(stdout, "Getting %s to from client...\n", sync.filename);
            int i = find_file_from_server(client->files, sync.filename);
            client->files[i].mtime = sync.mtime;
            client->STATE = GETFILE;
            strncpy(client->currFilename, sync.filename, MAXNAME);
            client->expected_size = ntohl(sync.size);
            client->received_so_far = 0;
            client->mtime = sync.mtime;
            response.mtime = 0;
            Writen(client->sock, &response, sizeof(struct sync_message));
        }
    }
}

int is_in_server (struct file_info *file, char *filename) {
    int i;
    for(i = 0; i < MAXFILES; i++) {
        if(strcmp(file[i].filename, filename) == 0) {
            return 0;
        } else if(file[i].filename[0] == '\0') {
            strncpy(file[i].filename, filename, MAXNAME);
            return 1;
        }
    }
    return -1;
}

void change_mtime(char *dirname, char *filename, long int mtime){
    int i;
    for(i = 0; i < MAXCLIENTS; i++){
        if (strcmp(clients[i].dirname, dirname) == 0){
            int file_ind;
            if((file_ind = find_file_from_server(clients[i].files, filename)) != -1){
                clients[i].files[file_ind].mtime = mtime; 
            }
        }
    }
}

int find_file_from_server(struct file_info *files, char *filename){
    int i;
    for(i = 0; i < MAXFILES; i++) {
        if(strcmp(files[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

void send_from_server(struct client_info *client) {
    DIR *dir;
    if ((dir = opendir(client->dirname)) == NULL) {
        perror("Open Directory");
        exit(1);
    }

    struct dirent *dir_file;
    while ((dir_file = readdir(dir)) != NULL) {
        if (dir_file->d_type != 4) {
            char path[563];
            path_name(path, client->dirname, dir_file->d_name);

            if(is_in_server(client->files, dir_file->d_name) == 1) {
                struct stat filestat;
                stat(path, &filestat);
                struct sync_message sync, response;
                strncpy(sync.filename, dir_file->d_name, MAXNAME);
                sync.mtime = filestat.st_mtime;
                sync.size = htonl(filestat.st_size);
                Writen(client->sock, &sync, sizeof(struct sync_message));
                server_synchronize_send(client->sock, path, filestat.st_size);
                Readn(client->sock, &response, sizeof(struct sync_message));

                int i = find_file_from_server(client->files, dir_file->d_name);
                client->files[i].mtime = filestat.st_mtime;

            }

        }
    }

    struct sync_message emp;
    emp.filename[0] = '\0';
    emp.size = 0;
    emp.mtime = 0;
    Writen(client->sock, &emp, sizeof(struct sync_message));

    if (closedir(dir) == -1) {
        perror("closedir");
    }
}

void path_name(char *buf, char *dir, char *filename) {
    strncpy(buf, dir, MAXNAME);
    strcat(buf, "/");
    strncat(buf, filename, MAXNAME);
}

void server_synchronize_send(int soc, char* path, int size) {
    FILE *file;
    if ((file = fopen(path, "r")) == NULL) {
        perror("fopen");
        exit(1);
    }

    char buffer[CHUNKSIZE];
    if (size < CHUNKSIZE) {
        fread(&buffer, size, 1, file);
        Writen(soc, &buffer, size);
    } else {
        int total_chunk = 0;
        while(total_chunk < size) {
            fread(&buffer, CHUNKSIZE,  1, file);
            Writen(soc, &buffer, CHUNKSIZE);
            total_chunk += CHUNKSIZE;
        }

        // remaining bits
        fread(&buffer, CHUNKSIZE,  1, file);
        Writen(soc, &buffer, size - total_chunk);
    }

    if(fclose(file) == -1) {
        perror("fclose");
    }
}

void server_getfile(struct client_info *client) {
    FILE *f;
    char path[563];
    path_name(path, client->dirname, client->currFilename);
    if((f = fopen(path, "w")) == NULL) {
        perror("fopen");
    }

    char buf[CHUNKSIZE];
    if ((client->expected_size - client->received_so_far) > CHUNKSIZE) {
        read(client->sock, &buf, CHUNKSIZE);
        client->received_so_far += CHUNKSIZE;
        fwrite(&buf, CHUNKSIZE, 1, f);
    }
    int i = client->expected_size - client->received_so_far;
    Readn(client->sock, &buf, i);
    fwrite(&buf, i, 1, f);
    client->STATE = SYNC;

    struct stat filestat;
    if (stat(path, &filestat)) {
        perror("stat");
    }
    int file = find_file_from_server(client->files, client->currFilename);
    (client->files)[file].mtime2 = filestat.st_mtime;

    if(fclose(f) == -1){
        perror("fclose");
    }
}













