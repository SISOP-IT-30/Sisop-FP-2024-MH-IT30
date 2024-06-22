// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <crypt.h>

#define PORT 9000
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define SALT "duwae621h283"
#define USERS_FILE "/home/purofuro/Fico/fpsisop/gg/newone/DiscorIT/users.csv"
#define CHANNELS_FILE "/home/purofuro/Fico/fpsisop/gg/newone/DiscorIT/channels.csv"
#define AUTH_FILE "/home/purofuro/Fico/fpsisop/gg/newone/DiscorIT/auth.csv"
#define CHAT_LOG_FILE "/home/purofuro/Fico/fpsisop/gg/newone/DiscorIT/chat.csv"
#define USERS_LOG_FILE "/home/purofuro/Fico/fpsisop/gg/newone/DiscorIT/users.log"

int user_count = 0;

typedef struct {
    int socket;
    struct sockaddr_in address;
    char logged_in_user[50];
    char logged_in_role[10];
    char logged_in_channel[50];
    char logged_in_room[50];
} client_info;

client_info *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void load_users();
void *handle_client(void *arg);
void register_user(int client_fd, char *username, char *password);
void login(int client_fd, char *username, char *password);
void list_channels(int client_fd, char *username);
void join_channel(int client_fd, char *username, char *channel_name, char *key);
void list_rooms(int client_fd, char *username, char *channel_name);
void list_users(int client_fd, char *username, char *channel_name);
void chat(int client_fd, char *username, char *channel_name, char *room_name, char *message);
void see_chat(int client_fd, char *username, char *channel_name, char *room_name);
void edit_chat(int client_fd, char *username, char *channel_name, char *room_name, int message_id, char *new_message);
void del_chat(int client_fd, char *username, char *channel_name, char *room_name, int message_id);
void create_channel(int client_fd, char *username, char *channel_name, char *key);
void edit_channel(int client_fd, char *username, char *old_channel_name, char *new_channel_name);
void del_channel(int client_fd, char *username, char *channel_name);
void create_room(int client_fd, char *username, char *channel_name, char *room_name);
void edit_room(int client_fd, char *username, char *channel_name, char *old_room_name, char *new_room_name);
void del_room(int client_fd, char *username, char *channel_name, char *room_name);
void del_all_rooms(int client_fd, char *username, char *channel_name);
void ban_user(int client_fd, char *username, char *channel_name, char *user_to_ban);
void unban_user(int client_fd, char *username, char *channel_name, char *user_to_unban);
void remove_user(int client_fd, char *username, char *channel_name, char *user_to_remove);
void edit_profile(int client_fd, char *username, char *new_username, char *new_password);
void exit_discorit(int client_fd, char *username, char *channel_name, char *room_name);
void log_activity(char *username, char *action, char *description);
void create_directory(const char *path);
void create_file(const char *path);

int main() {
    int server_fd, client_socket, client_count = 0;
    struct sockaddr_in server_addr, client_addr;
    pthread_t tid;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept incoming connections and handle them
    while (1) {
        
        user_count = 0;
        load_users();
        socklen_t client_len = sizeof(client_addr);

        // Accept connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Create client info struct
        client_info *new_client = (client_info *)malloc(sizeof(client_info));
        new_client->socket = client_socket;
        new_client->address = client_addr;
        strcpy(new_client->logged_in_user, "");
        strcpy(new_client->logged_in_role, "");
        strcpy(new_client->logged_in_channel, "");
        strcpy(new_client->logged_in_room, "");

        // Add client to array (protected by mutex)
        pthread_mutex_lock(&clients_mutex);
        clients[client_count++] = new_client;
        pthread_mutex_unlock(&clients_mutex);

        // Create thread to handle client
        if (pthread_create(&tid, NULL, handle_client, (void *)new_client) != 0) {
            perror("Thread creation failed");
            continue;
        }

        // Limit number of clients
        if (client_count >= MAX_CLIENTS) {
            printf("Maximum clients reached. No longer accepting connections.\n");
            break;
        }
    }

    // Close server socket
    close(server_fd);

    return 0;
}

// Function to handle each client connection
void *handle_client(void *arg) {
    client_info *client = (client_info *)arg;
    int client_socket = client->socket;
    char buffer[BUFFER_SIZE] = {0};
    int read_size;

    // Receive message from client
    while ((read_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        char *token;
        char *commands[10];
        int i = 0;

        // Tokenize incoming message
        token = strtok(buffer, " ");
        while (token != NULL && i < 10) {
            commands[i++] = token;
            token = strtok(NULL, " ");
        }
        commands[i] = NULL;

        // Process command based on the first token
        if (strcmp(commands[0], "REGISTER") == 0) {
            if (commands[1] != NULL && strcmp(commands[2], "-p") == 0 && commands[3] != NULL) {
                register_user(client_socket, commands[1], commands[3]);
            } else {
                send(client_socket, "Invalid command format. Usage: REGISTER username -p password\n", strlen("Invalid command format. Usage: REGISTER username -p password\n"), 0);
            }
        } else if (strcmp(commands[0], "LOGIN") == 0) {
            if (commands[1] != NULL && strcmp(commands[2], "-p") == 0 && commands[3] != NULL) {
                login(client_socket, commands[1], commands[3]);
            } else {
                send(client_socket, "Invalid command format. Usage: LOGIN username -p password\n", strlen("Invalid command format. Usage: LOGIN username -p password\n"), 0);
            }
        } else if (strcmp(commands[0], "CREATE") == 0 && strcmp(commands[1], "CHANNEL") == 0) {
            if (commands[2] != NULL && strcmp(commands[3], "-k") == 0 && commands[4] != NULL) {
                create_channel(client_socket, client->logged_in_user, commands[2], commands[4]);
            } else {
                send(client_socket, "Invalid command format. Usage: CREATE CHANNEL channel -k key\n", strlen("Invalid command format. Usage: CREATE CHANNEL channel -k key\n"), 0);
            }
        } else if (strcmp(commands[0], "LIST") == 0 && strcmp(commands[1], "CHANNEL") == 0) {
            list_channels(client_socket, client->logged_in_user);
        } else if (strcmp(commands[0], "JOIN") == 0) {
            if (commands[1] != NULL) {
                char key[50];
                send(client_socket, "Key: ", strlen("Key: "), 0);
                int key_size = recv(client_socket, key, sizeof(key), 0);
                key[key_size - 1] = '\0'; // Remove newline character
                join_channel(client_socket, client->logged_in_user, commands[1], key);
            } else {
                send(client_socket, "Invalid command format. Usage: JOIN channel\n", strlen("Invalid command format. Usage: JOIN channel\n"), 0);
            }
        } else {
            send(client_socket, "Unknown command\n", strlen("Unknown command\n"), 0);
        }

        memset(buffer, 0, sizeof(buffer));
    }

    // Client disconnected
    if (read_size == 0) {

    } else if (read_size == -1) {
        perror("Receive failed");
    }

    // Remove client from array (protected by mutex)
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == client) {
            free(clients[i]);
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // Close socket and free thread
    close(client_socket);
    pthread_exit(NULL);
}

// Function to register a new user
void register_user(int client_fd, char *username, char *password) {
    FILE *file = fopen(USERS_FILE, "a+");
    if (!file) {
        perror("Failed to open USERS_FILE");
        send(client_fd, "Server error. Please try again later.\n", strlen("Server error. Please try again later.\n"), 0);
        return;
    }

    // Check if username already exists
    char line[256];
    bool username_exists = false;
    while (fgets(line, sizeof(line), file)) {
        
        char *existing_username = strtok(line, ",");
        
        while(existing_username != NULL){

            if(strcmp(existing_username, username) == 0){
                username_exists = true;
                break;
            }
            existing_username = strtok(NULL, ","); 
        }
    }

    if (username_exists) {
        send(client_fd, "username sudah terdaftar\n", strlen("username sudah terdaftar\n"), 0);
    } else {
        // Generate salt and hash the password
        char hashed_password[128];
        strcpy(hashed_password, crypt(password, SALT));

        // Write username, hashed_password, and salt to USERS_FILE
        if(user_count == 0){
            fprintf(file, "%d,%s,%s,%s\n", user_count+1, username, hashed_password, "root");
        } else {
            fprintf(file, "%d,%s,%s,%s\n", user_count+1, username, hashed_password, "user");
        }
        fclose(file);

        send(client_fd, "username berhasil register\n", strlen("username berhasil register\n"), 0);

        // Log registration in users.log
        log_activity(username, "REGISTER", "User registered");

        // Create entry in auth.csv for the new user with role "user"
        FILE *auth_file = fopen(AUTH_FILE, "a+");
        if (!auth_file) {
            perror("Failed to open AUTH_FILE");
            return;
        }
        fprintf(auth_file, "%s,user\n", username);
        fclose(auth_file);
    }
}

// Function to handle user login
void login(int client_fd, char *username, char *password) {
    FILE *file = fopen(USERS_FILE, "r");
    if (!file) {
        perror("Failed to open USERS_FILE");
        send(client_fd, "Server error. Please try again later.\n", strlen("Server error. Please try again later.\n"), 0);
        return;
    }

    // Check if username and password match
    char line[256];
    bool user_found = false;
    char stored_password[128];
    while (fgets(line, sizeof(line), file)) {
        char *stored_id = strtok(line, ",");
        char *stored_username = strtok(NULL, ",");
        char *stored_hashed_password = strtok(NULL, ",");
        char *stored_role = strtok(NULL, ",");

        // Trim newline character from the stored hashed password
        if (stored_hashed_password) {
            stored_hashed_password[strcspn(stored_hashed_password, "\n")] = '\0';
        }

        // Check if the username matches
        if (stored_username && strcmp(stored_username, username) == 0) {
            user_found = true;
            strcpy(stored_password, stored_hashed_password);
            break;
        }
    }

    fclose(file);

    if (!user_found) {
        send(client_fd, "Username tidak terdaftar\n", strlen("Username tidak terdaftar\n"), 0);
    } else {
        // Validate password
        char input_hashed_password[128];
        strcpy(input_hashed_password, crypt(password, stored_password));

        if (strcmp(stored_password, input_hashed_password) != 0) {
            send(client_fd, "Password salah\n", strlen("Password salah\n"), 0);
        } else {
            // Successful login
            send(client_fd, "username berhasil login\n", strlen("username berhasil login\n"), 0);

            // Log login in users.log
            log_activity(username, "LOGIN", "User logged in");

            // Update client info
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (clients[i] != NULL && clients[i]->socket == client_fd) {
                    strcpy(clients[i]->logged_in_user, username);
                    strcpy(clients[i]->logged_in_role, "user");
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }
    }
}


// Function to log activity in users.log
void log_activity(char *username, char *action, char *description) {
    FILE *log_file = fopen(USERS_LOG_FILE, "a");
    if (!log_file) {
        perror("Failed to open USERS_LOG_FILE");
        return;
    }

    time_t now;
    time(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localtime(&now));

    fprintf(log_file, "[%s][%s] %s: %s\n", timestamp, username, action, description);
    fclose(log_file);
}


void list_channels(int client_fd, char *username) {
    FILE *file = fopen(CHANNELS_FILE, "r");
    if (!file) {
        perror("Failed to open CHANNELS_FILE");
        send(client_fd, "Server error. Please try again later.\n", strlen("Server error. Please try again later.\n"), 0);
        return;
    }

    char response[BUFFER_SIZE] = "[";
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *channel_name = strtok(line, ",");
        if (channel_name != NULL) {
            strcat(response, channel_name);
            strcat(response, " ");
        }
    }
    fclose(file);

    strcat(response, "]\n");

    send(client_fd, response, strlen(response), 0);
}

void join_channel(int client_fd, char *username, char *channel_name, char *key) {
    // Open channels.csv to check if the channel exists
    FILE *file = fopen(CHANNELS_FILE, "r+");
    if (!file) {
        perror("Failed to open CHANNELS_FILE");
        send(client_fd, "Server error. Please try again later.\n", strlen("Server error. Please try again later.\n"), 0);
        return;
    }

    char line[256];
    bool channel_found = false;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        if (token != NULL && strcmp(token, channel_name) == 0) {
            channel_found = true;
            break;
        }
    }

    fclose(file);

    if (!channel_found) {
        send(client_fd, "Channel tidak ditemukan\n", strlen("Channel tidak ditemukan\n"), 0);
        return;
    }

    // Now, check if the user is already in the channel
    bool user_already_in_channel = false;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL &&
            strcmp(clients[i]->logged_in_user, username) == 0 &&
            strcmp(clients[i]->logged_in_channel, channel_name) == 0) {
            user_already_in_channel = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (user_already_in_channel) {
        send(client_fd, "Anda sudah bergabung di channel ini\n", strlen("Anda sudah bergabung di channel ini\n"), 0);
        return;
    }

    // If a key is required, check if the provided key matches
    if (key != NULL) {
        // Implement your logic to check if the key matches
        // For now, assume it's correct if key is provided
        // Replace with actual logic to check against stored channel keys
    }

    // Update the client's logged_in_channel
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && clients[i]->socket == client_fd) {
            strcpy(clients[i]->logged_in_channel, channel_name);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // Send success message to client
    char message[BUFFER_SIZE];
    sprintf(message, "Berhasil masuk ke channel %s\n", channel_name);
    send(client_fd, message, strlen(message), 0);

    // Log activity in users.log
    log_activity(username, "JOIN CHANNEL", channel_name);
}

void list_rooms(int client_fd, char *username, char *channel_name) {
    // Open channels.csv to check if the channel exists
    FILE *file = fopen(CHANNELS_FILE, "r");
    if (!file) {
        perror("Failed to open CHANNELS_FILE");
        send(client_fd, "Server error. Please try again later.\n", strlen("Server error. Please try again later.\n"), 0);
        return;
    }

    // Check if the channel exists
    char line[256];
    bool channel_found = false;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        if (token != NULL && strcmp(token, channel_name) == 0) {
            channel_found = true;
            break;
        }
    }

    fclose(file);

    if (!channel_found) {
        send(client_fd, "Channel tidak ditemukan\n", strlen("Channel tidak ditemukan\n"), 0);
        return;
    }

    // Assuming rooms are stored in a file named with the channel name
    // e.g., care_rooms.csv for channel "care"
    char rooms_file[256];
    sprintf(rooms_file, "%s_rooms.csv", channel_name);

    file = fopen(rooms_file, "r");
    if (!file) {
        send(client_fd, "Belum ada room di channel ini\n", strlen("Belum ada room di channel ini\n"), 0);
        return;
    }

    char response[BUFFER_SIZE] = "[";
    while (fgets(line, sizeof(line), file)) {
        char *room_name = strtok(line, ",");
        if (room_name != NULL) {
            strcat(response, room_name);
            strcat(response, " ");
        }
    }
    fclose(file);

    strcat(response, "]\n");

    send(client_fd, response, strlen(response), 0);
}

void list_users(int client_fd, char *username, char *channel_name) {
    // Open auth.csv to check if the user is an admin of the channel
    FILE *file = fopen(AUTH_FILE, "r");
    if (!file) {
        perror("Failed to open AUTH_FILE");
        send(client_fd, "Server error. Please try again later.\n", strlen("Server error. Please try again later.\n"), 0);
        return;
    }

    // Check if the user is an admin of the channel
    char line[256];
    bool is_admin = false;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        char *role = strtok(NULL, ",");
        if (token != NULL && role != NULL && strcmp(token, username) == 0 && strcmp(role, "admin") == 0) {
            is_admin = true;
            break;
        }
    }
    fclose(file);

    if (!is_admin) {
        send(client_fd, "Anda tidak memiliki akses untuk melihat user di channel ini\n", strlen("Anda tidak memiliki akses untuk melihat user di channel ini\n"), 0);
        return;
    }

    // Assuming users in the channel are stored in a file named with the channel name
    // e.g., care_users.csv for channel "care"
    char users_file[256];
    sprintf(users_file, "%s_users.csv", channel_name);

    file = fopen(users_file, "r");
    if (!file) {
        send(client_fd, "Belum ada user di channel ini\n", strlen("Belum ada user di channel ini\n"), 0);
        return;
    }

    char response[BUFFER_SIZE] = "[";
    while (fgets(line, sizeof(line), file)) {
        char *user_name = strtok(line, ",");
        if (user_name != NULL) {
            strcat(response, user_name);
            strcat(response, " ");
        }
    }
    fclose(file);

    strcat(response, "]\n");

    send(client_fd, response, strlen(response), 0);
}

void chat(int client_fd, char *username, char *channel_name, char *room_name, char *message) {
    // Validate if the user is logged into the specified channel and room
    pthread_mutex_lock(&clients_mutex);
    bool user_found = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL &&
            strcmp(clients[i]->logged_in_user, username) == 0 &&
            strcmp(clients[i]->logged_in_channel, channel_name) == 0 &&
            strcmp(clients[i]->logged_in_room, room_name) == 0) {
            user_found = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!user_found) {
        send(client_fd, "Anda tidak memiliki akses untuk mengirim pesan di room ini\n", strlen("Anda tidak memiliki akses untuk mengirim pesan di room ini\n"), 0);
        return;
    }

    // Append the chat message to chat.csv
    FILE *file = fopen("chat.csv", "a+");
    if (!file) {
        perror("Failed to open chat.csv");
        send(client_fd, "Server error. Please try again later.\n", strlen("Server error. Please try again later.\n"), 0);
        return;
    }

    // Get current time
    time_t rawtime;
    struct tm *info;
    char timestamp[80];
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", info);

    // Generate a unique ID for the message
    int message_id;
    // Logic to generate message_id goes here

    // Write to chat.csv
    fprintf(file, "[%s][%d][%s] \"%s\"\n", timestamp, message_id, username, message);
    fclose(file);

    // Send confirmation message to client
    char response[BUFFER_SIZE];
    sprintf(response, "[%s] \"%s\" berhasil dikirim\n", timestamp, message);
    send(client_fd, response, strlen(response), 0);
}

void see_chat(int client_fd, char *username, char *channel_name, char *room_name) {
    // Validate if the user is logged into the specified channel and room
    pthread_mutex_lock(&clients_mutex);
    bool user_found = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL &&
            strcmp(clients[i]->logged_in_user, username) == 0 &&
            strcmp(clients[i]->logged_in_channel, channel_name) == 0 &&
            strcmp(clients[i]->logged_in_room, room_name) == 0) {
            user_found = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!user_found) {
        send(client_fd, "Anda tidak memiliki akses untuk melihat chat di room ini\n", strlen("Anda tidak memiliki akses untuk melihat chat di room ini\n"), 0);
        return;
    }

    // Assuming chat history is stored in a file named with the room name
    // e.g., care_room1_chat.csv for room "room1" in channel "care"
    char chat_file[256];
    sprintf(chat_file, "%s_%s_chat.csv", channel_name, room_name);

    FILE *file = fopen(chat_file, "r");
    if (!file) {
        send(client_fd, "Belum ada chat di room ini\n", strlen("Belum ada chat di room ini\n"), 0);
        return;
    }

    char response[BUFFER_SIZE] = "";
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        strcat(response, line);
    }
    fclose(file);

    send(client_fd, response, strlen(response), 0);
}

void edit_chat(int client_fd, char *username, char *channel_name, char *room_name, int message_id, char *new_message) {
    // Validate if the user is logged into the specified channel and room
    pthread_mutex_lock(&clients_mutex);
    bool user_found = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL &&
            strcmp(clients[i]->logged_in_user, username) == 0 &&
            strcmp(clients[i]->logged_in_channel, channel_name) == 0 &&
            strcmp(clients[i]->logged_in_room, room_name) == 0) {
            user_found = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!user_found) {
        send(client_fd, "Anda tidak memiliki akses untuk mengedit chat di room ini\n", strlen("Anda tidak memiliki akses untuk mengedit chat di room ini\n"), 0);
        return;
    }

    // Assuming chat messages are stored in a file named with the room name
    // e.g., care_room1_chat.csv for room "room1" in channel "care"
    char chat_file[256];
    sprintf(chat_file, "%s_%s_chat.csv", channel_name, room_name);

    // Open the chat file for reading and writing
    FILE *file = fopen(chat_file, "r+");
    if (!file) {
        send(client_fd, "Gagal membuka chat untuk diedit\n", strlen("Gagal membuka chat untuk diedit\n"), 0);
        return;
    }

    // Find and edit the specified message_id
    char line[256];
    char edited_message[256];
    bool message_found = false;
    while (fgets(line, sizeof(line), file)) {
        int id;
        sscanf(line, "[%*[^]]][%d]", &id);
        if (id == message_id) {
            // Construct the edited message
            char timestamp[80];
            sscanf(line, "[%[^]]][%*d]", timestamp);
            sprintf(edited_message, "[%s][%d][%s] \"%s\"\n", timestamp, message_id, username, new_message);

            // Move the file pointer back to overwrite the line
            fseek(file, -strlen(line), SEEK_CUR);
            fprintf(file, "%s", edited_message);
            message_found = true;
            break;
        }
    }

    fclose(file);

    if (!message_found) {
        send(client_fd, "Pesan dengan ID tersebut tidak ditemukan\n", strlen("Pesan dengan ID tersebut tidak ditemukan\n"), 0);
        return;
    }

    send(client_fd, "Pesan berhasil diedit\n", strlen("Pesan berhasil diedit\n"), 0);
}

void del_chat(int client_fd, char *username, char *channel_name, char *room_name, int message_id) {
    // Validate if the user is logged into the specified channel and room
    pthread_mutex_lock(&clients_mutex);
    bool user_found = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL &&
            strcmp(clients[i]->logged_in_user, username) == 0 &&
            strcmp(clients[i]->logged_in_channel, channel_name) == 0 &&
            strcmp(clients[i]->logged_in_room, room_name) == 0) {
            user_found = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!user_found) {
        send(client_fd, "Anda tidak memiliki akses untuk menghapus chat di room ini\n", strlen("Anda tidak memiliki akses untuk menghapus chat di room ini\n"), 0);
        return;
    }

    // Assuming chat messages are stored in a file named with the room name
    // e.g., care_room1_chat.csv for room "room1" in channel "care"
    char chat_file[256];
    sprintf(chat_file, "%s_%s_chat.csv", channel_name, room_name);

    // Open the chat file for reading and writing
    FILE *file = fopen(chat_file, "r+");
    if (!file) {
        send(client_fd, "Gagal membuka chat untuk dihapus\n", strlen("Gagal membuka chat untuk dihapus\n"), 0);
        return;
    }

    // Find and delete the specified message_id
    char line[256];
    char edited_message[256];
    bool message_found = false;
    while (fgets(line, sizeof(line), file)) {
        int id;
        sscanf(line, "[%*[^]]][%d]", &id);
        if (id == message_id) {
            message_found = true;
            break;
        }
    }

    if (!message_found) {
        fclose(file);
        send(client_fd, "Pesan dengan ID tersebut tidak ditemukan\n", strlen("Pesan dengan ID tersebut tidak ditemukan\n"), 0);
        return;
    }

    // Create a temporary file to copy all lines except the one to delete
    FILE *temp_file = fopen("temp_chat.csv", "w");
    if (!temp_file) {
        fclose(file);
        send(client_fd, "Gagal membuat file sementara\n", strlen("Gagal membuat file sementara\n"), 0);
        return;
    }

    // Copy all lines except the one to delete
    rewind(file);
    while (fgets(line, sizeof(line), file)) {
        int id;
        sscanf(line, "[%*[^]]][%d]", &id);
        if (id != message_id) {
            fprintf(temp_file, "%s", line);
        }
    }

    fclose(file);
    fclose(temp_file);

    // Replace the original file with the temporary file
    if (rename("temp_chat.csv", chat_file) != 0) {
        send(client_fd, "Gagal menghapus pesan\n", strlen("Gagal menghapus pesan\n"), 0);
        return;
    }

    send(client_fd, "Pesan berhasil dihapus\n", strlen("Pesan berhasil dihapus\n"), 0);
}

void create_channel(int client_fd, char *username, char *channel_name, char *key) {
    // Validate if the user is logged in and is an admin or root
    pthread_mutex_lock(&clients_mutex);
    client_info *user_info = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            user_info = clients[i];
            break;
        }
    }

    if (user_info == NULL) {
        pthread_mutex_unlock(&clients_mutex);
        send(client_fd, "Anda tidak terdaftar atau sudah logout\n", strlen("Anda tidak terdaftar atau sudah logout\n"), 0);
        return;
    }

    // Check if the user has admin or root role in any channel
    bool is_admin_or_root = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && (strcmp(clients[i]->logged_in_role, "admin") == 0 || strcmp(clients[i]->logged_in_role, "root") == 0)) {
            is_admin_or_root = true;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    
    FILE *file = fopen(CHANNELS_FILE, "a+");
    if (!file) {
        send(client_fd, "Gagal membuka file channels\n", strlen("Gagal membuka file channels\n"), 0);
        return;
    }

    // Check if the channel name already exists
    char line[256];
    bool channel_exists = false;
    int dummy_id;
    char existing_channel[50];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%d,%49[^,],", &dummy_id, existing_channel);
        if (strcmp(existing_channel, channel_name) == 0) {
            channel_exists = true;
            break;
        }
    }
    fclose(file);

    if (channel_exists) {
        send(client_fd, "Channel sudah ada\n", strlen("Channel sudah ada\n"), 0);
        return;
    }

    // Write new channel to file
    file = fopen(CHANNELS_FILE, "a");
    if (!file) {
        send(client_fd, "Gagal membuka file channels\n", strlen("Gagal membuka file channels\n"), 0);
        return;
    }
    static int channel_count = 0;  // Make sure to initialize channel_count properly based on existing channels
    fprintf(file, "%d,%s,%s\n", ++channel_count, channel_name, key);
    fclose(file);

    // Create directory structure for the new channel
    char channel_path[256];
    sprintf(channel_path, "DiscorIT/%s", channel_name);
    create_directory(channel_path);

    char admin_path[256];
    sprintf(admin_path, "DiscorIT/%s/admin", channel_name);
    create_directory(admin_path);

    char auth_file[256];
    sprintf(auth_file, "DiscorIT/%s/admin/auth.csv", channel_name);
    create_file(auth_file);

    // Add the user as an admin in the new channel's auth.csv
    file = fopen(auth_file, "a");
    if (!file) {
        send(client_fd, "Gagal membuka file auth\n", strlen("Gagal membuka file auth\n"), 0);
        return;
    }
    fprintf(file, "%d,%s,admin\n", user_info->socket, username);  // Assuming socket is used as id_user
    fclose(file);

    char user_log_file[256];
    sprintf(user_log_file, "DiscorIT/%s/admin/user.log", channel_name);
    create_file(user_log_file);

    // Log the creation in users.log
    char log_file[256] = USERS_LOG_FILE;
    FILE *log = fopen(log_file, "a");
    if (!log) {
        send(client_fd, "Gagal membuka file log\n", strlen("Gagal membuka file log\n"), 0);
        return;
    }

    time_t now = time(NULL);
    char *time_str = asctime(localtime(&now));
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline character

    fprintf(log, "[%s] Channel %s dibuat oleh %s\n", time_str, channel_name, username);
    fclose(log);

    send(client_fd, "Channel berhasil dibuat\n", strlen("Channel berhasil dibuat\n"), 0);
}

void create_directory(const char *path) {
    if (mkdir(path, 0777) == -1) {
        if (errno != EEXIST) {
            perror("Failed to create directory");
            exit(1);
        }
    }
}

void create_file(const char *path) {
    FILE *file = fopen(path, "a");
    if (!file) {
        perror("Failed to create file");
        exit(1);
    }
    fclose(file);
}

void edit_channel(int client_fd, char *username, char *old_channel_name, char *new_channel_name) {
    // Validate if the user is logged in and is an admin or root
    pthread_mutex_lock(&clients_mutex);
    client_info *user_info = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL &&
            strcmp(clients[i]->logged_in_user, username) == 0) {
            user_info = clients[i];
            break;
        }
    }

    if (user_info == NULL) {
        pthread_mutex_unlock(&clients_mutex);
        send(client_fd, "Anda tidak terdaftar atau sudah logout\n", strlen("Anda tidak terdaftar atau sudah logout\n"), 0);
        return;
    }

    // Check if the user has admin or root role in any channel
    bool is_admin_or_root = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL &&
            (strcmp(clients[i]->logged_in_role, "admin") == 0 ||
             strcmp(clients[i]->logged_in_role, "root") == 0)) {
            is_admin_or_root = true;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    if (!is_admin_or_root) {
        send(client_fd, "Hanya admin atau root yang dapat mengedit channel\n", strlen("Hanya admin atau root yang dapat mengedit channel\n"), 0);
        return;
    }

    // Assuming channels are stored in channels.csv
    char channels_file[256] = CHANNELS_FILE;
    create_directory("DiscorIT");
    FILE *file = fopen(channels_file, "r+");
    if (!file) {
        send(client_fd, "Gagal membuka file channels\n", strlen("Gagal membuka file channels\n"), 0);
        return;
    }

    // Check if the old channel name exists and find its position in the file
    char line[256];
    bool channel_found = false;
    long int pos = -1;
    while (fgets(line, sizeof(line), file)) {
        char existing_channel[50];
        sscanf(line, "%[^,],", existing_channel);
        if (strcmp(existing_channel, old_channel_name) == 0) {
            channel_found = true;
            pos = ftell(file) - strlen(line); // Get position before current line
            break;
        }
    }

    if (!channel_found) {
        fclose(file);
        send(client_fd, "Channel tidak ditemukan\n", strlen("Channel tidak ditemukan\n"), 0);
        return;
    }

    // Open a temporary file to copy all lines except the one to edit
    FILE *temp_file = fopen("temp_channels.csv", "w");
    if (!temp_file) {
        fclose(file);
        send(client_fd, "Gagal membuat file sementara\n", strlen("Gagal membuat file sementara\n"), 0);
        return;
    }

    // Copy all lines except the one to edit
    rewind(file);
    while (fgets(line, sizeof(line), file)) {
        char existing_channel[50];
        sscanf(line, "%[^,],", existing_channel);
        if (strcmp(existing_channel, old_channel_name) != 0) {
            fprintf(temp_file, "%s", line);
        }
    }

    fclose(file);
    fclose(temp_file);

    // Replace the original file with the temporary file
    if (rename("temp_channels.csv", channels_file) != 0) {
        send(client_fd, "Gagal mengedit channel\n", strlen("Gagal mengedit channel\n"), 0);
        return;
    }

    // Append the new channel name to channels.csv
    file = fopen(channels_file, "a");
    if (!file) {
        send(client_fd, "Gagal menambahkan channel baru\n", strlen("Gagal menambahkan channel baru\n"), 0);
        return;
    }

    fprintf(file, "%s,%s,%s\n", new_channel_name, username, "nokey");
    fclose(file);

    // Log the edit in users.log
    char log_file[256] = "users.log";
    FILE *log = fopen(log_file, "a");
    if (!log) {
        send(client_fd, "Gagal membuka file log\n", strlen("Gagal membuka file log\n"), 0);
        return;
    }

    time_t now = time(NULL);
    char *time_str = asctime(localtime(&now));
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline character

    fprintf(log, "[%s] Channel %s diubah oleh %s menjadi %s\n", time_str, old_channel_name, username, new_channel_name);
    fclose(log);

    send(client_fd, "Channel berhasil diubah\n", strlen("Channel berhasil diubah\n"), 0);
}

void del_channel(int client_fd, char *username, char *channel_name) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0) {
            // Implement logic to delete the channel
            printf("Channel %s deleted\n", channel_name);
        } else {
            printf("You are not authorized to delete this channel\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void create_room(int client_fd, char *username, char *channel_name, char *room_name) {
    // Validate if the user is logged in and is part of the channel
    pthread_mutex_lock(&clients_mutex);
    client_info *user_info = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            user_info = clients[i];
            break;
        }
    }

    if (user_info == NULL) {
        pthread_mutex_unlock(&clients_mutex);
        send(client_fd, "Anda tidak terdaftar atau sudah logout\n", strlen("Anda tidak terdaftar atau sudah logout\n"), 0);
        return;
    }

    // Check if the user is part of the channel
    char auth_file[256];
    sprintf(auth_file, "DiscorIT/%s/admin/auth.csv", channel_name);

    FILE *file = fopen(auth_file, "r");
    if (!file) {
        send(client_fd, "Gagal membuka file auth\n", strlen("Gagal membuka file auth\n"), 0);
        return;
    }

    char line[256];
    bool is_authorized = false;
    while (fgets(line, sizeof(line), file)) {
        char stored_username[50];
        sscanf(line, "%*d,%49[^,],", stored_username);
        if (strcmp(stored_username, username) == 0) {
            is_authorized = true;
            break;
        }
    }
    fclose(file);

    if (!is_authorized) {
        send(client_fd, "Anda tidak diizinkan untuk membuat room di channel ini\n", strlen("Anda tidak diizinkan untuk membuat room di channel ini\n"), 0);
        return;
    }

    // Create room directory and chat.csv file
    char room_path[256];
    sprintf(room_path, "DiscorIT/%s/%s", channel_name, room_name);
    create_directory(room_path);

    char chat_file[256];
    sprintf(chat_file, "DiscorIT/%s/%s/chat.csv", channel_name, room_name);
    create_file(chat_file);

    // Log the creation in users.log
    char log_file[256] = USERS_LOG_FILE;
    FILE *log = fopen(log_file, "a");
    if (!log) {
        send(client_fd, "Gagal membuka file log\n", strlen("Gagal membuka file log\n"), 0);
        return;
    }

    time_t now = time(NULL);
    char *time_str = asctime(localtime(&now));
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline character

    fprintf(log, "[%s] Room %s di channel %s dibuat oleh %s\n", time_str, room_name, channel_name, username);
    fclose(log);

    send(client_fd, "Room berhasil dibuat\n", strlen("Room berhasil dibuat\n"), 0);
}

void edit_room(int client_fd, char *username, char *channel_name, char *old_room_name, char *new_room_name) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0 && strcmp(current_client->logged_in_room, old_room_name) == 0) {
            printf("Room %s edited to %s in %s channel\n", old_room_name, new_room_name, channel_name);
        } else {
            printf("You are not authorized to edit this room in this channel\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void del_room(int client_fd, char *username, char *channel_name, char *room_name) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0 && strcmp(current_client->logged_in_room, room_name) == 0) {
            // Implement logic to delete the room
            printf("Room %s deleted from %s channel\n", room_name, channel_name);
        } else {
            printf("You are not authorized to delete this room in this channel\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void del_all_rooms(int client_fd, char *username, char *channel_name) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0) {
            // Implement logic to delete all rooms
            printf("All rooms deleted from %s channel\n", channel_name);
        } else {
            printf("You are not authorized to delete all rooms in this channel\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void ban_user(int client_fd, char *username, char *channel_name, char *user_to_ban) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0) {
            // Implement logic to ban the user
            printf("%s banned from %s channel\n", user_to_ban, channel_name);
        } else {
            printf("You are not authorized to ban users from this channel\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void unban_user(int client_fd, char *username, char *channel_name, char *user_to_unban) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0) {
            // Implement logic to unban the user
            printf("%s unbanned from %s channel\n", user_to_unban, channel_name);
        } else {
            printf("You are not authorized to unban users from this channel\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_user(int client_fd, char *username, char *channel_name, char *user_to_remove) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0) {
            // Implement logic to remove the user
            printf("%s removed from %s channel\n", user_to_remove, channel_name);
        } else {
            printf("You are not authorized to remove users from this channel\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void edit_profile(int client_fd, char *username, char *new_username, char *new_password) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        // Update username and/or password
        strncpy(current_client->logged_in_user, new_username, sizeof(current_client->logged_in_user) - 1);
        // Assuming password is stored somewhere else, update it similarly

        printf("Profile edited successfully\n");
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void exit_discorit(int client_fd, char *username, char *channel_name, char *room_name) {
    pthread_mutex_lock(&clients_mutex);

    client_info *current_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL && strcmp(clients[i]->logged_in_user, username) == 0) {
            current_client = clients[i];
            break;
        }
    }

    if (current_client != NULL) {
        if (strcmp(current_client->logged_in_channel, channel_name) == 0 && strcmp(current_client->logged_in_room, room_name) == 0) {
            // Implement logic to handle user exit from channel and room
            printf("User %s exited from channel %s and room %s\n", username, channel_name, room_name);
        } else {
            printf("You are not in the specified channel and room\n");
        }
    } else {
        printf("User not found\n");
    }

    pthread_mutex_unlock(&clients_mutex);
}

void load_users(){
    FILE *file = fopen(USERS_FILE, "r");
    if (!file) {
        perror("Failed to open AUTH_FILE");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        int id = atoi(strtok(line, ","));
        char *username = strtok(line, ",");
        char *password = strtok(NULL, ",");
        char *role = strtok(NULL, ",");
        user_count++;
    }
    fclose(file);
}