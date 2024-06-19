#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9999

int main() {
    int nRet;
    struct sockaddr_in srv;

    // Open a socket - listener
    int nSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (nSocket < 0) {
        perror("Cannot Initialize listener socket");
        return -1;
    }

    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = inet_addr("127.0.0.1");
    srv.sin_port = htons(PORT);
    memset(&(srv.sin_zero), 0, 8);

    nRet = connect(nSocket, (struct sockaddr*)&srv, sizeof(srv));
    if (nRet < 0) {
        perror("Cannot connect to server");
        close(nSocket);
        return -1;
    }

    // Keep sending the messages
    char sBuff[1024] = { 0 };
    while (1) {
        sleep(2);
        printf("What message you want to send?\n");
        fgets(sBuff, 1023, stdin);
        sBuff[strcspn(sBuff, "\n")] = 0;
        send(nSocket, sBuff, strlen(sBuff), 0);
    }

    close(nSocket);
    return 0;
}
