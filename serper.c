#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <crypt.h>

#define PORT 9999
// struct to help store data to csv
struct user {
    int id;
    char name[255];
    char password[255];
    char encrypted_password[255];
    char global_role[255];
} u[100];

struct channel {
    int id;
    char name[255];
    char key[255];
    char role[255];
} c[100];

struct room {
    int id;
    int date;
    char sender[255];
    char chat[255];
    
} r[100];


int nSocket;
int nClientSocket[10] = { 0 };
struct sockaddr_in srv;
fd_set fr;
int nMaxFd = 0;
int user_count = 0;

void save_users() {
    FILE *file = fopen("users.csv", "a");
    if (!file) return;
    for (int i = 0; i < user_count; i++) {
        
        fprintf(file, "%d,%s,%s,%s\n", u[i].id, u[i].name, u[i].password, u[i].global_role);
    }
    fclose(file);
}

void load_users() {
    FILE *file = fopen("users.csv", "r");
    if (!file) return;
    while (fscanf(file, "%d,%100[^,],%100[^,],%s", &u[user_count].id, u[user_count].name, u[user_count].password, u[user_count].global_role) == 4) {
        user_count++;
    }
    fclose(file);
}

void save_channels() {
    FILE *file = fopen("auth.csv", "a");
    if (!file) return;
    for (int i = 0; i < user_count; i++) {
        fprintf(file, "%d,%s,%s\n", c[i].id, c[i].name, c[i].role);
    }
    fclose(file);
}

void load_channels() {
    FILE *file = fopen("auth.csv", "r");
    if (!file) return;
    while (fscanf(file, "%d,%100[^,],%100[^,]", &c[user_count].id, c[user_count].name, c[user_count].role) == 3) {
        user_count++;
    }
    fclose(file);
}

void save_rooms() {
    FILE *file = fopen("chat.csv", "a");
    if (!file) return;
    for (int i = 0; i < user_count; i++) {
        fprintf(file, "%d,%d,%s,%s\n", r[i].id, r[i].date, r[i].sender, r[i].chat);
    }
    fclose(file);
}

void load_rooms() {
    FILE *file = fopen("chat.csv", "r");
    if (!file) return;
    while (fscanf(file, "%d,%d,%100[^,],%100[^,]", &r[user_count].id, &r[user_count].date, r[user_count].sender, r[user_count].chat) == 4) {
        user_count++;
    }
    fclose(file);
}

void HandleNewConnection() {
    int nNewClient = accept(nSocket, NULL, NULL);

    if (nNewClient < 0) {
        perror("Not able to get a new client socket");
    } else {
        int nIndex;
        for (nIndex = 0; nIndex < 10; nIndex++) {
            if (nClientSocket[nIndex] == 0) {
                nClientSocket[nIndex] = nNewClient;
                if (nNewClient >= nMaxFd) {
                    nMaxFd = nNewClient + 1;
                }
                break;
            }
        }

        if (nIndex == 10) {
            printf("Server busy. Cannot accept anymore connections\n");
        }
    }
}

int main() {
    int nRet;
    struct timeval tv;

    nSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (nSocket < 0) {
        perror("The socket cannot be initialized");
        exit(EXIT_FAILURE);
    }

    srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    srv.sin_addr.s_addr = INADDR_ANY;
    memset(&srv.sin_zero, 0, sizeof(srv.sin_zero));

    nRet = bind(nSocket, (struct sockaddr*)&srv, sizeof(struct sockaddr));
    if (nRet < 0) {
        perror("The socket cannot be bind");
        close(nSocket);
        exit(EXIT_FAILURE);
    }

    nRet = listen(nSocket, 5);
    if (nRet < 0) {
        perror("The listen failed");
        close(nSocket);
        exit(EXIT_FAILURE);
    }

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    nMaxFd = nSocket + 1;

    while (1) {
        FD_ZERO(&fr);
        FD_SET(nSocket, &fr);
        for (int nIndex = 0; nIndex < 10; nIndex++) {
            if (nClientSocket[nIndex] > 0) {
                FD_SET(nClientSocket[nIndex], &fr);
            }
        }

        nRet = select(nMaxFd, &fr, NULL, NULL, &tv);
        if (nRet < 0) {
            perror("select api call failed. Will exit");
            close(nSocket);
            return EXIT_FAILURE;
        } else if (nRet == 0) {
            continue;
        } else {
            if (FD_ISSET(nSocket, &fr)) {
                HandleNewConnection();
            } 
            else{
                for (int nIndex = 0; nIndex < 10; nIndex++) {
                    if (nClientSocket[nIndex] > 0) {
                        if (FD_ISSET(nClientSocket[nIndex], &fr)) {
                            char sBuff[255] = { 0 };
                            int nRet = recv(nClientSocket[nIndex], sBuff, 255, 0);

                            if (nRet <= 0) {
                                if (nRet < 0) perror("Error at client socket");
                                close(nClientSocket[nIndex]);
                                nClientSocket[nIndex] = 0;
                            } else {
                                char first[100], second[100], third[100], fourth[100];
                                sscanf(sBuff, "%s %s %s %s", first, second, third, fourth);
                                if (strcmp(first, "register") == 0 && strcmp(third, "-p") == 0) {
                                    strcpy(u[user_count].name, second);
                                    strcpy(u[user_count].password, fourth);
                                    
                                    if(u[user_count].id == 1) strcpy(u[user_count].global_role, "root");
                                    else strcpy(u[user_count].global_role, "user");

                                    u[user_count].id = user_count + 1;
                                    user_count++;
                                    save_users();
                                    
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(nSocket);
    return 0;
}
