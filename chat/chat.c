#include "chat.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>  // pthread 라이브러리 사용

ChatRoom chat_rooms[MAX_CHAT_ROOMS];

// 뮤텍스 변수 선언
extern pthread_mutex_t mutex;

void init_chat_rooms() {
    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        chat_rooms[i].room_id = i + 1;  // 1부터 시작하도록 수정
        chat_rooms[i].room_name[0] = '\0';
        chat_rooms[i].creator_id[0] = '\0';
        chat_rooms[i].user_count = 0;
        chat_rooms[i].is_active = 0;
    }
    
    // 파일에서 채팅방 정보 로드
    FILE *fp = fopen("chatting.txt", "r");
    if (fp != NULL) {
        char room_name[MAX_ROOM_NAME];
        char creator_id[MAX_USERID];
        int room_id, user_count;
        
        while (fscanf(fp, "%d %s %s %d", &room_id, creator_id, room_name, &user_count) == 4) {
            if (room_id > 0 && room_id <= MAX_CHAT_ROOMS) {
                int idx = room_id - 1;  // 배열 인덱스로 변환
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

int create_chat_room(const char* room_name, const char* creator_id) {
    if (room_name == NULL || creator_id == NULL || strlen(creator_id) == 0) {
        return -1;  // 유효하지 않은 입력
    }

    // 마지막 room_id 찾기
    int last_room_id = 0;
    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        if (chat_rooms[i].is_active && chat_rooms[i].room_id > last_room_id) {
            last_room_id = chat_rooms[i].room_id;
        }
    }

    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        if (!chat_rooms[i].is_active) {
            chat_rooms[i].room_id = last_room_id + 1;  // 마지막 room_id의 다음 값 사용
            strncpy(chat_rooms[i].room_name, room_name, MAX_ROOM_NAME - 1);
            chat_rooms[i].room_name[MAX_ROOM_NAME - 1] = '\0';
            strncpy(chat_rooms[i].creator_id, creator_id, MAX_USERID - 1);
            chat_rooms[i].creator_id[MAX_USERID - 1] = '\0';
            chat_rooms[i].is_active = 1;
            chat_rooms[i].user_count = 0;
            
            // 채팅방 정보를 파일에 저장
            FILE *fp = fopen("chatting.txt", "a");
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

void view_chat_rooms(char* output, size_t size) {
    int offset = 0;
    
    // 버퍼의 시작에 헤더 추가
    offset += snprintf(output + offset, size - offset, "\n=== 채팅방 목록 ===\n");
    
    // 각 채팅방 정보 추가
    for (int i = 0; i < MAX_CHAT_ROOMS; i++) {
        if (chat_rooms[i].is_active) {
            offset += snprintf(output + offset, size - offset, 
                "방 ID: %d | 생성자: %s | 방 이름: %s | 참여자 수: %d\n",
                i, 
                chat_rooms[i].creator_id,
                chat_rooms[i].room_name, 
                chat_rooms[i].user_count);
        }
    }
    
    snprintf(output + offset, size - offset, "================\n");
}

int join_chat_room(int room_id, int user_socket) {
    if (room_id <= 0 || room_id > MAX_CHAT_ROOMS || !chat_rooms[room_id - 1].is_active) {
        return -1;  // 잘못된 방 번호 또는 존재하지 않는 방
    }
    
    int idx = room_id - 1;  // 배열 인덱스로 변환
    if (chat_rooms[idx].user_count >= MAX_CHAT_USERS) {
        return -2;  // 방이 가득 참
    }
    
    chat_rooms[idx].user_id[chat_rooms[idx].user_count++] = user_socket;
    update_chat_file();  // 파일 업데이트
    return 0;
}

int leave_chat_room(int room_id, int user_socket) {
    if (room_id <= 0 || room_id > MAX_CHAT_ROOMS || !chat_rooms[room_id - 1].is_active) {
        return -1;
    }
    
    int idx = room_id - 1;  // 배열 인덱스로 변환
    for (int i = 0; i < chat_rooms[idx].user_count; i++) {
        if (chat_rooms[idx].user_id[i] == user_socket) {
            // 해당 사용자를 제거하고 나머지 사용자들을 앞으로 당김
            for (int j = i; j < chat_rooms[idx].user_count - 1; j++) {
                chat_rooms[idx].user_id[j] = chat_rooms[idx].user_id[j + 1];
            }
            chat_rooms[idx].user_count--;
            update_chat_file();  // 파일 업데이트
            return 0;
        }
    }
    return -1;
}

void broadcast_message(int room_id, int sender_socket __attribute__((unused)), const char* message) {
    if (room_id < 0 || room_id >= MAX_CHAT_ROOMS || !chat_rooms[room_id].is_active) {
        return;
    }
    
    pthread_mutex_lock(&mutex);  // 뮤텍스 잠금 추가
    for (int i = 0; i < chat_rooms[room_id].user_count; i++) {
        int receiver_socket = chat_rooms[room_id].user_id[i];
        // 발신자를 포함한 모든 참여자에게 메시지 전송
        send(receiver_socket, message, strlen(message), 0);
    }
    pthread_mutex_unlock(&mutex);  // 뮤텍스 해제
}

// 채팅방 정보를 파일에 업데이트하는 함수 수정
void update_chat_file() {
    FILE *fp = fopen("chatting.txt", "w");
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

// 서버 시작 시 채팅방 정보를 로드하는 함수 추가
void load_chat_rooms() {
    FILE *fp = fopen("chatting.txt", "r");
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