#ifndef CHAT_H
#define CHAT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_USERID 20
#define MAX_CHAT_USERS 10
#define MAX_CHAT_ROOMS 5
#define MAX_ROOM_NAME 50
#define MAX_MESSAGE 100
#define BUFFER_SIZE 1024

extern int next_room_id;

typedef struct {
    int room_id;
    char room_name[MAX_ROOM_NAME];
    char creator_id[MAX_USERID];
    int user_id[MAX_CHAT_USERS];
    int user_sockets[MAX_CHAT_USERS];
    int user_count;
    int is_active;
    int current_room;
} ChatRoom;

void init_chat_rooms();
int create_chat_room(const char* room_name, const char* creator_id);
void view_chat_rooms(char* output, size_t size);
int join_chat_room(int room_id, int user_socket);
int leave_chat_room(int room_id, int user_socket);
void broadcast_message(int room_id, int sender_socket, const char* message);
int delete_chat_room(int room_id, int client_socket);
void update_chat_file();
void load_chat_rooms();

#endif