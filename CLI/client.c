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

char remote_path[MAX_SEG - 100];
char local_path[MAX_SEG] = "localFolder";

void interaction_handler(int);
void print_help();
void recieve_list(int);
void download_handler(int, char *);
void upload_handler(int, char *);

int main (int argc, char **argv)
{
    int connect_fd;
    struct sockaddr_in server_addr;
    char path[MAX_SEG];

    if( (connect_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Listen socket create failed");
        exit(1);
    }

    // initialize addr
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]); // hard set
    server_addr.sin_port = htons(strtol(argv[2], NULL, 10));  // hard set

    if (connect(connect_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        exit(1);
    }

    sprintf(path, "./%s", local_path);
    mkdir(path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

    interaction_handler(connect_fd);

    close(connect_fd);
    return 0;
}

void interaction_handler(int socket_fd) {
    char buff[MAX_SEG];
    char *charPtr;
    char command[10];
    char path[MAX_SEG - 10];

    // read connection success message
    memset(buff, '\0', MAX_SEG);
    read(socket_fd, buff, MAX_SEG);
    printf("%s", buff);

    // first time guide
    print_help();

    // read server working path
    memset(buff, '\0', MAX_SEG);
    read(socket_fd, buff, MAX_SEG);
    sprintf(remote_path, "%s", buff);

    while(1) {
        // initialize
        charPtr = NULL;

        // prompt
        printf("simpleFTP_server:%s>", remote_path);

        memset(buff, '\0', MAX_SEG);
        fgets(buff, MAX_SEG, stdin);

        if(buff[0] == '\n' || buff[0] == ' ' || buff[0] == '\t') continue;

        if((charPtr = strrchr(buff, '\n')) != NULL) *charPtr = '\0';

        // write raw buff to server, both do parsing
        write(socket_fd, buff, strlen(buff));

        // read in commands
        charPtr = strtok(buff, " \t\0");
        strcpy(command, charPtr);

        // note that not all commands are handled, only the ones that involves client itself
        if(strcmp(command, "cd") == 0) {
            strcpy(path, buff + strlen(command) + 1);
            if(strlen(path) == 0) continue;

            // new remote working directory
            memset(buff, '\0', MAX_SEG);
            read(socket_fd, buff, MAX_SEG);

            sprintf(remote_path, "%s", buff);
        } else if(strcmp(command, "ls") == 0) {
            recieve_list(socket_fd);
        } else if(strcmp(command, "get") == 0) {
            strcpy(path, buff + strlen(command) + 1);
            if(strlen(path) == 0) continue;

            download_handler(socket_fd, path);
        } else if(strcmp(command, "put") == 0) {
            strcpy(path, buff + strlen(command) + 1);
            if(strlen(path) == 0) continue;

            upload_handler(socket_fd, path);
        } else if(strcmp(command, "help") == 0) {
            print_help();
        } else if(strcmp(command, "exit") == 0) {
            break;
        }
    }

    return;
}

void print_help() {
    printf("\nls\n  displays contents of remote current working directory.");
	  printf("\ncd <path>\n  changes the remote current working directory to the specified <path>.");
	  printf("\nget <filename>\n  downloads <filename> in remote current working directory to local directory.");
	  printf("\nput <filename>\n  uploads the local <filename> to remote current working directory.");
	  printf("\nhelp\n  displays this message.");
    printf("\nexit\n  terminates this program.\n");

    return;
}

void recieve_list(int socket_fd) {
    char buff[MAX_SEG];

    memset(buff, '\0', MAX_SEG);

    if( read(socket_fd, buff, MAX_SEG) > 0) {
        printf("%s", buff);
    }

    printf("\n");

    return;
}

void download_handler(int socket_fd, char filename[]) {
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
        printf("ERROR: download failed.\n");
    }

    return;
}

void upload_handler(int socket_fd, char filename[]) {
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
        memset(buff, '\0', MAX_SEG);
        sprintf(buff, "[CLIENT] uploading: %s", filename);
        if(write(socket_fd, buff, strlen(buff)) < 0) {
            perror("upload: start message failed.");
            exit(1);
        }

        // get file size, store in file_size
        fseek(filePtr, 0, SEEK_END);
        total_file_size = ftell(filePtr);
        rewind(filePtr);

        // send file size
        memset(buff, '\0', MAX_SEG);
        sprintf(buff, "%d", total_file_size);
        if(write(socket_fd, buff, strlen(buff)) < 0) {
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
        sprintf(buff, "%s", "Upload complete!!\n");
        write(socket_fd, buff, strlen(buff));
        printf("Upload complete!!\n");
    } else {
        printf("ERROR: upload failed");
    }

    return;
}
