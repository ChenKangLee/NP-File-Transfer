#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> // files
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h> // directory entries

#define MAX_SEG 2048
#define MAX_CONNECTION 5

char local_path[MAX_SEG] = "serverFolder";

void interaction_handler(int); // cd should be implement here?
int make_directory(char *);
int change_directory(int, char *);
void list_file(int);
void client_upload_handler(int , char *);
void client_download_handler(int, char *);

int main(int argc, char ** argv)
{
    int listen_fd, connect_fd;
    struct sockaddr_in listen_addr, connect_addr;
    socklen_t addr_len;
    pid_t child_pid;

    char buff[MAX_SEG];
    char path[MAX_SEG];

    if( (listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Listen socket create failed");
        exit(1);
    }

    // initialize address
    bzero(&listen_addr, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_port = htons(strtol(argv[1], NULL, 10));

    bzero(&connect_addr, sizeof(connect_addr));

    // bind listen socket
    if(bind(listen_fd, (const struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0 ) {
        perror("Listen Socket failed to bind");
        exit(1);
    }

    // listen
    listen(listen_fd, MAX_CONNECTION);

    printf("Listening on %s:%d\n", inet_ntoa(listen_addr.sin_addr), ntohs(listen_addr.sin_port));
    printf("Waiting for clients...\n\n");

    addr_len = sizeof(connect_addr);

    while(1) {
        if( (connect_fd = accept(listen_fd, (struct sockaddr *)&connect_addr, (socklen_t *)&addr_len)) < 0) {
            perror("Failed to accept");
            exit(1);
        }

        printf("Client accepted (%d)\n", connect_fd);
        printf("Client from %s:%d\n", inet_ntoa(connect_addr.sin_addr), ntohs(connect_addr.sin_port));

        if( (child_pid = fork()) == 0) { // in child process
            close(listen_fd);

            sprintf(path, "./%s", local_path);
            mkdir(path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

            interaction_handler(connect_fd);

            // client operation done
            close(connect_fd);
            return 0;
        } else { // in parent process
            close(connect_fd);
        }
    }
}

void interaction_handler(int socket_fd) {
    char buff[MAX_SEG];
    char *charPtr;
    char command[10];
    char path[MAX_SEG - 10];

    // write conncetion success message
    printf("(%d) sent success message to client\n", socket_fd);
    memset(buff, '\0', MAX_SEG);
    sprintf(buff, "%s", "[✓] Successfully connected to server.\n -------- \n");
    if(write(socket_fd, buff, MAX_SEG) < 0) {
        perror("Connection success message write failed");
    }

    // write server working path
    memset(buff, '\0', MAX_SEG);
    sprintf(buff, "%s", local_path);
    write(socket_fd, buff, strlen(buff));

    while(1) {
        // initialize
        charPtr = NULL;

        // read raw buff
        memset(buff, '\0', MAX_SEG);
        if(read(socket_fd, buff, MAX_SEG) == 0) {
            printf("Client (%d), terminated\n", socket_fd);
            break;
        } else {
            if((charPtr = strrchr(buff, '\n')) != NULL) *charPtr = '\0';

            // parse buff
            charPtr = strtok(buff, " \t\0");
            strcpy(command, charPtr);

            if(strcmp(command, "mkdir") == 0) {
                printf("Client (%d) makes new directory.\n", socket_fd);

                strcpy(path, buff + strlen(command) + 1);
                if(strlen(path) == 0) continue;

                make_directory(path);
            } else if(strcmp(command, "cd") == 0) {
                printf("Client (%d) changes directory.\n", socket_fd);

                strcpy(path, buff + strlen(command) + 1);
                if(strlen(path) == 0) continue;

                change_directory(socket_fd, path);
            } else if(strcmp(command, "ls") == 0) {
                printf("Client (%d) request listing file.\n", socket_fd);

                list_file(socket_fd);
            } else if(strcmp(command, "get") == 0) {
                printf("Client (%d) downloads file ", socket_fd);

                strcpy(path, buff + strlen(command) + 1);
                if(strlen(path) == 0) continue;

                printf("%s\n", path);
                client_download_handler(socket_fd, path);
            } else if(strcmp(command, "put") == 0) {
                printf("Client (%d) uploads file ", socket_fd);

                strcpy(path, buff + strlen(command) + 1);
                if(strlen(path) == 0) continue;

                printf("%s\n", path);
                client_upload_handler(socket_fd, path);
            } else if(strcmp(command, "exit") == 0) {
                printf("Client (%d) closes connection.\n", socket_fd);

                break;
            }
        }
    } // end of while(1)

    return;
}

int make_directory(char dirname[]) {
    struct stat st = {0};
    char path[MAX_SEG];

    sprintf(path, "./%s/%s", local_path, dirname);

    if(stat(path, &st) == -1) {
        mkdir(path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
        return 1;
    } else return 0;
}

int change_directory(int socket_fd, char dirname[]) {
    struct stat st = {0};
    char path[MAX_SEG];

    sprintf(path, "./%s/%s", local_path, dirname);

    if(stat(path, &st) == -1) {
        return 0;
    } else {
        sprintf(local_path, "%s/%s", local_path, dirname);
        // write server working path
        write(socket_fd, local_path, strlen(local_path));
        return 1;
    }
}

void list_file(int socket_fd) {
    DIR *dirPtr;
    struct dirent *direntPtr = NULL;
    char buff[MAX_SEG];
    char path[MAX_SEG];

    int write_byte;

    printf("Listing file to client... \n");

    // generate path
    sprintf(path, "./%s", local_path);

    if((dirPtr = opendir(path)) == NULL) {
        perror("Failed to open directory\n");
    } else {
        memset(buff, '\0', MAX_SEG);

        while((direntPtr = readdir(dirPtr)) != NULL) {
            // we did not ignore the current and parent directory entry
            strcat(buff, "\n");
            strcat(buff, direntPtr->d_name);
        }

        if( write(socket_fd, buff, strlen(buff)) < 0) {
            perror("Writing file list failed.\n");
            exit(1);
        }
    }

    closedir(dirPtr);

    return;
}

void client_upload_handler(int socket_fd, char filename[]) {
    char buff[MAX_SEG];
    char path[MAX_SEG];

    int total_file_size = 0;
    int read_byte = 0;
    int read_sum = 0;

    FILE *filePtr;
    char *pEnd;

    // get file path
    memset(path, '\0', MAX_SEG);
    sprintf(path, "./%s/%s", local_path, filename);

    // get start message
    memset(buff, '\0', MAX_SEG);
    read(socket_fd, buff, MAX_SEG);
    printf("%s\n", buff);

    // get file size
    memset(buff, '\0', MAX_SEG);
    read(socket_fd, buff, MAX_SEG);
    total_file_size = strtol (buff, &pEnd, 10);
    printf("File size: %d\n", total_file_size);

    // allocate memory for file
    read_sum = 0;
    filePtr = fopen(path, "wb");
    if(filePtr) {
        while (read_sum < total_file_size) {
        memset(buff, '\0', MAX_SEG);

        read_byte = read(socket_fd, buff, sizeof(buff));

        /* write file to local disk*/
        fwrite(&buff, sizeof(char), read_byte, filePtr);
        read_sum += read_byte;
      }
      fclose(filePtr);

      // recieve download complete message
      memset(buff, '\0', MAX_SEG);
      read(socket_fd, buff, MAX_SEG);
      printf("%s\n", buff);
    } else {
        printf("ERROR: client upload failed.\n");
    }

    return;
}

void client_download_handler(int socket_fd, char filename[]) {
    char buff[MAX_SEG];
    char path[MAX_SEG];

    int total_file_size = 0;
    int write_byte = 0;
    int write_sum = 0;

    FILE *filePtr;

    // get file path
    memset(path, '\0', MAX_SEG);
    sprintf(path, "./%s/%s", local_path, filename);

    filePtr = fopen(path, "rb");
    if(filePtr) { // file found
        // send starting message
        printf("Client downloading \"%s\"\n", path);
        memset(buff, '\0', MAX_SEG);
        sprintf(buff, "[SERVER] transfering: %s", filename);
        if(write(socket_fd, buff, strlen(buff)) < 0) {
            perror("upload: start message failed.");
            exit(1);
        }


        // get file size, store in file_size
        fseek(filePtr, 0, SEEK_END);
        total_file_size = ftell(filePtr);
        rewind(filePtr);

        printf("%d\n", total_file_size);

        // send file size
        memset(buff, '\0', MAX_SEG);
        sprintf(buff, "%d", total_file_size);
        if(write(socket_fd, buff, MAX_SEG) < 0) {
            perror("upload: send file size failed");
            exit(1);
        }

        // start upload
        while(write_sum < total_file_size) {
            memset(buff, '\0', MAX_SEG);
            write_byte = fread(&buff, sizeof(char), MAX_SEG, filePtr);
            write_byte = write(socket_fd, buff, write_byte);
            write_sum += write_byte;
        }

        fclose(filePtr);

        // sleep -- not sure if neccessary
        sleep(2);

        // send upload complete message
        memset(buff, '\0', MAX_SEG);
        sprintf(buff, "%s", "[SERVER] Download complete!!");
        write(socket_fd, buff, strlen(buff));
    } else {
        printf("ERROR: client download failed.\n");
    }

    return;
}
