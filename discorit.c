#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAXLINE 1024

void register_user(int sock, char *username, char *password) {
    char buffer[MAXLINE];

    sprintf(buffer, "REGISTER %s -p %s", username, password);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s", buffer);
}

void login_user(int sock, char *username, char *password) {
    char buffer[MAXLINE];
    
    sprintf(buffer, "LOGIN %s -p %s", username, password);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s", buffer);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <REGISTER|LOGIN> <username> -p <password>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *command = argv[1];
    char *username = argv[2];
    char *password = argv[4];

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[MAXLINE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    if (strcmp(command, "REGISTER") == 0) {
        register_user(sock, username, password);
    } else if (strcmp(command, "LOGIN") == 0) {
        login_user(sock, username, password);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
    }

    close(sock);
    return 0;
}
