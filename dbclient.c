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

int main(int argc, char* argv[])
{ 
    int soc;
    struct hostent *hp;
    struct sockaddr_in peer;

    peer.sin_family = PF_INET;
    peer.sin_port = htons(PORT); 
    printf("PORT = %d\n", PORT);


    if ( argc != 4 )
    {  
        fprintf(stderr, "Usage: dbclient host userid directory\n");
        exit(1);
    }

    /* fill in peer address */
    hp = gethostbyname(argv[1]);                
    if ( hp == NULL ) {  
        fprintf(stderr, "%s: %s unknown host\n",
            argv[0], argv[1]);
        exit(1);
    }

    peer.sin_addr = *((struct in_addr *)hp->h_addr);

    /* create socket */
    if((soc = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
	}
    /* request connection to server */
    if (connect(soc, (struct sockaddr *)&peer, sizeof(peer)) == -1) {  
        perror("client:connect"); close(soc);
        exit(1); 
    }

    struct login_message login;
    strncpy(login.userid, argv[2], MAXNAME);
    strncpy(login.dir, basename(argv[3]), MAXNAME);
    Writen(soc, &login, sizeof(struct login_message));

    while (1) {
    	printf("\n------------------------------------\n");
    	printf("synchronizing.....\n\n");
    	synchronize(soc, argv[3]);
    	printf("\nAll files are up to date now!\n");
    	printf("--------------------------------------\n");
    	sleep(8);
    }

    Close(soc);
    return 0;


}

void synchronize(int soc, char* path) {

	// Open Directory
	DIR *dir;
	if ((dir = opendir(path)) == NULL) {
		perror("opendir");
		exit(1);
	}

	// look for all files except for directories
	struct dirent *files;
	while ((files = readdir(dir)) != NULL) {
		// dirctory not allowed!!
		if(files->d_type != 4) {

			struct stat filestat;
			struct sync_message sync;
			struct sync_message response;
			char file_path_name[MAXNAME];
			strncpy(file_path_name, basename(path), MAXNAME);
			strcat(file_path_name, "/");
			strncat(file_path_name, files->d_name, strlen(files->d_name));

			// get file stats
			if(stat(file_path_name, &filestat) == -1) {
				perror("stat");
			}

			// send server stats of the file
			strncpy(sync.filename, files->d_name, strlen(files->d_name)+1);
			sync.mtime = filestat.st_mtime;
			sync.size = htonl((int)filestat.st_size);
			write(soc, "", 1);
			Writen(soc, &sync, sizeof(struct sync_message));
			// read response from server
			Readn(soc, &response, sizeof(struct sync_message));
			int server_time = response.mtime;

			// server file is older! send client file
			if (server_time < filestat.st_mtime) {
				printf("%s is being updated to server...\n", sync.filename);
				synchronize_send(soc, file_path_name, filestat.st_size);
			} 
			// client file is older! send server file
			else if (server_time > filestat.st_mtime) {
				printf("%s is newer on server. Getting newer version from server...\n", sync.filename);
				synchronize_get(soc, file_path_name, response.size);

				struct utimbuf mod_time;
				mod_time.modtime = server_time;
				mod_time.actime = filestat.st_atime;
				utime(file_path_name, &mod_time);
				return;
			} 
			// file is up to date.
			else {

			}
		}
	}


	// check if there's any new files on the server
	struct sync_message emp_mess;
	emp_mess.filename[0] = '\0';
	emp_mess.size = 0;
	emp_mess.mtime = 0;
	write(soc, "", 1);
	Writen(soc, &emp_mess, sizeof(struct sync_message));
	struct sync_message response;
	Readn(soc, &response, sizeof(struct sync_message));
	while(!((strcmp(response.filename, "") == 0) && (response.mtime == 0) && (response.size == 0))) {
		printf("Updating %s from server...\n", response.filename);
		char new_file_path[513];
		strncpy(new_file_path, path, MAXNAME);
		strncat(new_file_path, "/", 1);
		strncat(new_file_path, response.filename, MAXNAME);
		int new_file_size = ntohl(response.size);

		synchronize_get(soc, new_file_path, new_file_size);

		struct stat new_file_stat;
		stat(new_file_path, &new_file_stat);
		struct utimbuf new_file_time;
		new_file_time.modtime = response.mtime;
		new_file_time.actime = new_file_stat.st_atime;
		utime(new_file_path, &new_file_time);
		Writen(soc, &emp_mess, sizeof(struct sync_message));
		Readn(soc, &response, sizeof(struct sync_message));

	}
	// error checking closedir
	if(closedir(dir) == -1) {
		perror("closedir");
	}
}

void synchronize_send(int soc, char* path, int size) {
	FILE *file;
	if ((file = fopen(path, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char buffer[CHUNKSIZE];
	if (size < CHUNKSIZE) {
		fread(&buffer, size, 1, file);
		write(soc, "", 1);
		Writen(soc, &buffer, size);
	} else {
		int total_chunk;
		while(fread(&buffer, CHUNKSIZE, 1, file) != 0) {
			write(soc, "", 1);
			Writen(soc, &buffer, CHUNKSIZE);
			total_chunk += CHUNKSIZE;
		}

		// remaining bits
		write(soc, "", 1);
		Writen(soc, &buffer, size - total_chunk);
	}

	if(fclose(file) == -1) {
		perror("fclose");
	}
}

void synchronize_get(int soc, char* path, int size) {
	FILE *file;
	if ((file = fopen(path, "w")) == NULL) {
		perror("fopen");
		exit(1);
	}
	char buffer[CHUNKSIZE];

	if (size < CHUNKSIZE) {
		Readn(soc, &buffer, size);
		fwrite(&buffer, size, 1, file);
	} else {
		int total_chunk = 0;
		while(total_chunk < size) {
			Readn(soc, &buffer, CHUNKSIZE);
			fwrite(&buffer, CHUNKSIZE, 1, file);
			total_chunk += CHUNKSIZE;
		}

		// remaining bits
		Readn(soc, &buffer, size - total_chunk);
		fwrite(&buffer, total_chunk, 1, file);
	}

	if(fclose(file) == -1) {
		perror("fclose");
	}
}






































