#ifndef CHAT_H
#define CHAT_H

#define MAX_USERID 20
#define MAX_CHAT_USERS 10
#define MAX_CHAT_ROOMS 5
#define MAX_ROOM_NAME 50
#define MAX_MESSAGE 100
#define BUFFER_SIZE 1024

extern int next_room_id;

// 채팅방 구조체
typedef struct {
    int room_id;
    char room_name[MAX_ROOM_NAME];
    char creator_id[MAX_USERID];
    int user_id[MAX_CHAT_USERS];
    int user_sockets[MAX_CHAT_USERS];
    int user_count;
    int is_active;
} ChatRoom;

void init_chat_rooms();
int create_chat_room(const char* room_name, const char* creator_id);
void view_chat_rooms(char* output, size_t size);
int join_chat_room(int room_id, int user_socket);
int leave_chat_room(int room_id, int user_socket);
void broadcast_message(int room_id, int sender_socket, const char* message);
void update_chat_file();
void load_chat_rooms();
int delete_chat_room(int room_id, const char* user_id);

#endif