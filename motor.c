#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>

#define PORT 9999

int nSocket;
int nClientSocket[5] = { 0 };
struct sockaddr_in srv;
fd_set fr;
int nMaxFd = 0;

void HandleDataFromClient() {
    for (int nIndex = 0; nIndex < 5; nIndex++) {
        if (nClientSocket[nIndex] > 0) {
            if (FD_ISSET(nClientSocket[nIndex], &fr)) {
                char sBuff[255] = { 0 };
                int nRet = recv(nClientSocket[nIndex], sBuff, 255, 0);

                if (nRet <= 0) {
                    // This happens when client closes connection abruptly
                    if (nRet < 0) perror("Error at client socket");
                    close(nClientSocket[nIndex]);
                    nClientSocket[nIndex] = 0;
                } else {
                    printf("Received data from: %d [Message: %s]\n", nClientSocket[nIndex], sBuff);
                }
            }
        }
    }
}

int main(){
    int nRet;
    struct sockaddr_in srv;
    struct timeval tv;

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

    while (1) {
        FD_ZERO(&fr);
        FD_SET(nSocket, &fr);
        for (int nIndex = 0; nIndex < 5; nIndex++) {
            if (nClientSocket[nIndex] > 0) {
                FD_SET(nClientSocket[nIndex], &fr);
            }
        }

        nRet = connect(nSocket, (struct sockaddr*)&srv, sizeof(srv));
        if (nRet < 0) {
            perror("select api call failed. Will exit");
            close(nSocket);
            return EXIT_FAILURE;
        } else if (nRet == 0) {
            continue;
        } else {
            if (FD_ISSET(nSocket, &fr)) {
                HandleNewConnection();
            } else {
                HandleDataFromClient();
            }
        }
    }

}