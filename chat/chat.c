#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 채팅방 헤더
#include "chat.h"

// 채팅방 구조체
ChatRoom chat_rooms[MAX_CHAT_ROOMS];

// 뮤텍스 변수
extern pthread_mutex_t mutex;

// 채팅방 초기화 함수
void init_chat_rooms() {

    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        chat_rooms[i].room_id = i + 1;
        chat_rooms[i].room_name[0] = '\0';
        chat_rooms[i].creator_id[0] = '\0';
        chat_rooms[i].user_count = 0;
        chat_rooms[i].is_active = 0;
    }
    
    FILE *fp = fopen("file_log/chatting.txt", "r");
    if (fp != NULL) {
        char room_name[MAX_ROOM_NAME];
        char creator_id[MAX_USERID];
        int room_id, user_count;
        
        while (fscanf(fp, "%d %s %s %d", &room_id, creator_id, room_name, &user_count) == 4) {
            if (room_id > 0 && room_id <= MAX_CHAT_ROOMS) {
                int idx = room_id - 1;  
                chat_rooms[idx].room_id = room_id;
                strncpy(chat_rooms[idx].room_name, room_name, MAX_ROOM_NAME - 1);
                strncpy(chat_rooms[idx].creator_id, creator_id, MAX_USERID - 1);
                chat_rooms[idx].user_count = user_count;
                chat_rooms[idx].is_active = 1;
            }
        }
        fclose(fp);
    }
}

// 채팅방 생성 함수
int create_chat_room(const char* room_name, const char* creator_id) {

    // 빈 문자열이나 NULL을 입력했을 때 처리
    if (room_name == NULL || creator_id == NULL || strlen(creator_id) == 0) {
        return -1;  
    }

    // 마지막 room_id 찾기
    int last_room_id = 0;
    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        if (chat_rooms[i].is_active && chat_rooms[i].room_id > last_room_id) {
            last_room_id = chat_rooms[i].room_id;
        }
    }

    // 채팅방 생성
    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        if (!chat_rooms[i].is_active) {
            chat_rooms[i].room_id = last_room_id + 1;  // 마지막 room_id의 다음 값 사용
            strncpy(chat_rooms[i].room_name, room_name, MAX_ROOM_NAME - 1);
            chat_rooms[i].room_name[MAX_ROOM_NAME - 1] = '\0';
            strncpy(chat_rooms[i].creator_id, creator_id, MAX_USERID - 1);
            chat_rooms[i].creator_id[MAX_USERID - 1] = '\0';
            chat_rooms[i].is_active = 1;
            chat_rooms[i].user_count = 0;
            
            FILE *fp = fopen("file_log/chatting.txt", "a");
            if (fp != NULL) {
                fprintf(fp, "%d %s %s %d\n", 
                        chat_rooms[i].room_id, creator_id, room_name, chat_rooms[i].user_count);
                fclose(fp);
            }
            return chat_rooms[i].room_id;
        }
    }
    return -1;
}

// 채팅방 목록 조회 함수
void view_chat_rooms(char* output, size_t size) {

    // 각 채팅방 정보 추가
    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        if (chat_rooms[i].is_active) {
            offset += snprintf(output + offset, size - offset, 
                "방 ID: %d | 생성자: %s | 방 이름: %s | 참여자 수: %d\n",
                chat_rooms[i].room_id, 
                chat_rooms[i].creator_id,
                chat_rooms[i].room_name, 
                chat_rooms[i].user_count);
        }
    }
}

int join_chat_room(int room_id, int user_socket) {

    // 잘못된 번호를 입력했을 때 처리
    if (room_id <= 0 || room_id > MAX_CHAT_ROOMS || !chat_rooms[room_id - 1].is_active) {
        return -1;  
    }
    
    // 방이 가득 찼을 때 처리
    int idx = room_id - 1; 
    if (chat_rooms[idx].user_count >= MAX_CHAT_USERS) {
        return -2;  
    }
    
    chat_rooms[idx].user_id[chat_rooms[idx].user_count++] = user_socket;
    update_chat_file();  
    return 0;
}

// 채팅방 퇴장 함수
int leave_chat_room(int room_id, int user_socket) {

    if (room_id <= 0 || room_id > MAX_CHAT_ROOMS || !chat_rooms[room_id - 1].is_active) {
        return -1;
    }
    
    int idx = room_id - 1;  
    for (int i = 0; i < chat_rooms[idx].user_count; i++) {
        if (chat_rooms[idx].user_id[i] == user_socket) {
            for (int j = i; j < chat_rooms[idx].user_count - 1; j++) {
                chat_rooms[idx].user_id[j] = chat_rooms[idx].user_id[j + 1];
            }
            chat_rooms[idx].user_count--;
            update_chat_file();  
            return 0;
        }
    }
    return -1;
}

// 채팅 메시지를 모든 참여자에게 전송하는 함수
void broadcast_message(int room_id, int sender_socket __attribute__((unused)), const char* message) {

    if (room_id < 0 || room_id >= MAX_CHAT_ROOMS || !chat_rooms[room_id].is_active) {
        return;
    }
    
    pthread_mutex_lock(&mutex); 
    for (int i = 0; i < chat_rooms[room_id].user_count; i++) {
        int receiver_socket = chat_rooms[room_id].user_id[i];
        send(receiver_socket, message, strlen(message), 0);
    }
    pthread_mutex_unlock(&mutex);  
}

// 채팅방 정보를 파일에 업데이트하는 함수
void update_chat_file() {

    FILE *fp = fopen("file_log/chatting.txt", "w");
    if (fp != NULL) {
        for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
            if (chat_rooms[i].is_active) {
                fprintf(fp, "%d %s %s %d\n", 
                        chat_rooms[i].room_id, 
                        chat_rooms[i].creator_id,
                        chat_rooms[i].room_name, 
                        chat_rooms[i].user_count);
            }
        }
        fclose(fp);
    }
}

// 파일에서 채팅방 정보를 읽어오는 함수
void load_chat_rooms() {

    FILE *fp = fopen("file_log/chatting.txt", "r");
    if (fp != NULL) {
        char room_name[MAX_ROOM_NAME];
        char creator_id[MAX_USERID];
        int room_id, user_count;
        
        while (fscanf(fp, "%d %s %s %d", &room_id, creator_id, room_name, &user_count) == 4) {
            if (room_id > 0 && room_id <= MAX_CHAT_ROOMS) {
                int idx = room_id - 1;
                chat_rooms[idx].room_id = room_id;
                strncpy(chat_rooms[idx].room_name, room_name, MAX_ROOM_NAME - 1);
                strncpy(chat_rooms[idx].creator_id, creator_id, MAX_USERID - 1);
                chat_rooms[idx].user_count = user_count;
                chat_rooms[idx].is_active = 1;
            }
        }
        fclose(fp);
    }
}

// 채팅방 삭제 함수
int delete_chat_room(int room_id, const char* user_id) {

    // 잘못된 번호를 입력했을 때 처리
    if (room_id <= 0 || room_id > MAX_CHAT_ROOMS) {
        return -1;  
    }
    
    int idx = room_id - 1;
    
    // 방이 존재하지 않거나 이미 비활성화된 경우 처리
    if (!chat_rooms[idx].is_active) {
        return -2;
    }
    
    // 방 생성자만 삭제할 수 있게 처리
    if (strcmp(chat_rooms[idx].creator_id, user_id) != 0) {
        return -3;
    }
    
    chat_rooms[idx].is_active = 0;
    chat_rooms[idx].user_count = 0;
    memset(chat_rooms[idx].user_id, 0, sizeof(int) * MAX_CHAT_USERS);
    
    update_chat_file();
    
    return 0;
}