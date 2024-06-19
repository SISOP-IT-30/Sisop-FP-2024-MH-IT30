#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 8080
#define MAXLINE 1024
#define MAX_USERS 100
#define MAX_CHANNELS 100
#define MAX_ROOMS_PER_CHANNEL 50
#define MAX_USERS_PER_ROOM 50

struct User {
    char username[50];
    char password[50];
    char global_role[10];
};

struct Channel {
    char name[50];
    char key[50];
    char rooms[MAX_ROOMS_PER_CHANNEL][50];
    int num_rooms;
    char users[MAX_USERS_PER_ROOM][50];
    int num_users;
};

struct User users[MAX_USERS];
struct Channel channels[MAX_CHANNELS];
int num_users = 0;
int num_channels = 0;

// Mutex untuk mengamankan akses ke data users (modul 3)
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER; 

int find_user_index(char *username) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

int find_channel_index(char *name) {
    for (int i = 0; i < num_channels; i++) {
        if (strcmp(channels[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void load_users_from_file() {
    FILE *fp = fopen("DiscorIT/users.csv", "r");
    if (fp == NULL) {
        printf("Error opening users.csv\n");
        return;
    }

    num_users = 0; // Reset jumlah pengguna

    char line[MAXLINE];
    while (fgets(line, MAXLINE, fp) != NULL) {
        sscanf(line, "%[^,],%[^,],%s", users[num_users].username, users[num_users].password, users[num_users].global_role);
        num_users++;
    }

    fclose(fp);
}

void load_channels_from_file() {
    FILE *fp = fopen("DiscorIT/channels.csv", "r");
    if (fp == NULL) {
        printf("Error opening channels.csv\n");
        return;
    }

    num_channels = 0; // Reset jumlah channel

    char line[MAXLINE];
    while (fgets(line, MAXLINE, fp) != NULL) {
        char rooms[MAX_ROOMS_PER_CHANNEL][50];
        int num_rooms;
        sscanf(line, "%[^,],%s,%n", channels[num_channels].name, channels[num_channels].key, &num_rooms);

        char *token = strtok(line + num_rooms, ",");
        int i = 0;
        while (token != NULL) {
            strcpy(rooms[i++], token);
            token = strtok(NULL, ",");
        }

        for (int j = 0; j < i; j++) {
            strcpy(channels[num_channels].rooms[j], rooms[j]);
        }
        channels[num_channels].num_rooms = i;
        channels[num_channels].num_users = 0;
        num_channels++;
    }

    fclose(fp);
}

void save_users_to_file() {
    FILE *fp = fopen("DiscorIT/users.csv", "w");
    if (fp == NULL) {
        printf("Error opening users.csv for writing\n");
        return;
    }

    for (int i = 0; i < num_users; i++) {
        fprintf(fp, "%s,%s,%s\n", users[i].username, users[i].password, users[i].global_role);
    }

    fclose(fp);
}

void save_channels_to_file() {
    FILE *fp = fopen("DiscorIT/channels.csv", "w");
    if (fp == NULL) {
        printf("Error opening channels.csv for writing\n");
        return;
    }

    for (int i = 0; i < num_channels; i++) {
        fprintf(fp, "%s,%s", channels[i].name, channels[i].key);
        for (int j = 0; j < channels[i].num_rooms; j++) {
            fprintf(fp, ",%s", channels[i].rooms[j]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);
    char buffer[MAXLINE];
    char command[20], username[50], password[50], channel[50], room[50];
    char dummy;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(client_socket, buffer, sizeof(buffer));

        if (valread <= 0) {
            // perror("Error reading from socket");
            close(client_socket);
            return NULL;
        }

        printf("Received: %s\n\n", buffer);

        sscanf(buffer, "%s %s -%c %s %s -%c %s", command, username, &dummy, password, channel, &dummy, room);

        if (strcmp(command, "REGISTER") == 0) {
            // Kunci mutex sebelum mengakses data users
            pthread_mutex_lock(&users_mutex);

            int user_idx = find_user_index(username);
            if (user_idx == -1) {
                strcpy(users[num_users].username, username);
                strcpy(users[num_users].password, password);

                if (num_users == 0) {
                    strcpy(users[num_users].global_role, "ROOT");
                } else {
                    strcpy(users[num_users].global_role, "USER");
                }
                num_users++;

                save_users_to_file(); // Simpan data pengguna ke file

                sprintf(buffer, "%s berhasil register\n", username);
            } else {
                sprintf(buffer, "%s sudah terdaftar\n", username);
            }
            send(client_socket, buffer, strlen(buffer), 0);
            pthread_mutex_unlock(&users_mutex); // Lepaskan mutex setelah selesai mengakses data users
        } 
        else if (strcmp(command, "LOGIN") == 0) {
            pthread_mutex_lock(&users_mutex);

            int user_idx = find_user_index(username);
            if (user_idx != -1 && strcmp(password, users[user_idx].password) == 0) {
                sprintf(buffer, "%s berhasil login\n[%s] ", username, username);
                send(client_socket, buffer, strlen(buffer), 0);
            } else {
                sprintf(buffer, "Login gagal\n");
                send(client_socket, buffer, strlen(buffer), 0);
            }
            pthread_mutex_unlock(&users_mutex);
        } 
        else if (strcmp(command, "LIST CHANNEL") == 0) {
            pthread_mutex_lock(&users_mutex);
            char channel_list[MAXLINE] = {0};
            for (int i = 0; i < num_channels; i++) {
                strcat(channel_list, channels[i].name);
                strcat(channel_list, " ");
            }
            send(client_socket, channel_list, strlen(channel_list), 0);
            pthread_mutex_unlock(&users_mutex);
        } 
        else if (strcmp(command, "LIST ROOM") == 0) {
            pthread_mutex_lock(&users_mutex);
            int channel_index = find_channel_index(channel);
            if (channel_index != -1) {
                char room_list[MAXLINE] = {0};
                for (int i = 0; i < channels[channel_index].num_rooms; i++) {
                    strcat(room_list, channels[channel_index].rooms[i]);
                    strcat(room_list, " ");
                }
                send(client_socket, room_list, strlen(room_list), 0);
            } else {
                sprintf(buffer, "Channel %s tidak ditemukan\n", channel);
                send(client_socket, buffer, strlen(buffer), 0);
            }
            pthread_mutex_unlock(&users_mutex); 
        } 
        else if (strcmp(command, "LIST USER") == 0) {
            pthread_mutex_lock(&users_mutex);
            int channel_index = find_channel_index(channel);
            if (channel_index != -1) {
                char user_list[MAXLINE] = {0};
                for (int i = 0; i < channels[channel_index].num_users; i++) {
                    strcat(user_list, channels[channel_index].users[i]);
                    strcat(user_list, " ");
                }
                send(client_socket, user_list, strlen(user_list), 0);
            } else {
                sprintf(buffer, "Channel %s tidak ditemukan\n", channel);
                send(client_socket, buffer, strlen(buffer), 0);
            }
            pthread_mutex_unlock(&users_mutex);
        } 
        else {
            sprintf(buffer, "Command tidak dikenali\n");
            send(client_socket, buffer, strlen(buffer), 0);
        }
    }

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    load_users_from_file();
    load_channels_from_file();

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Listening on port %d\n", PORT);

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        int *new_sock = malloc(1);
        *new_sock = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
