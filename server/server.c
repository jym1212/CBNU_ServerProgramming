#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> // POSIX 스레드 
#include <unistd.h>  // close 함수 
#include <arpa/inet.h> // 소켓 관련 함수 

#include "../login/login.h" // 로그인 및 회원가입 
#include "../board/board.h" // 게시판 
#include "../chat/chat.h" // 채팅 

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// 채팅방 관리를 위한 전역 변수
extern ChatRoom chat_rooms[MAX_CHAT_ROOMS];

typedef struct {
    int socket;
    char user_id[MAX_USERID];
    int is_logged_in;
} ClientState;

// 클라이언트 소켓 관리
ClientState clients[MAX_CLIENTS];
int client_count = 0;

// 뮤텍스 객체 (클라이언트 리스트 보호용)
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// 클라이언트 상태 초기화 함수
void initialize_client_state(int socket) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == 0) {
            clients[i].socket = socket;
            clients[i].is_logged_in = 0; // 로그인 상태 초기화
            memset(clients[i].user_id, 0, sizeof(clients[i].user_id));
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

// 클라이언트 상태 제거
void remove_client_state(int socket) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == socket) {
            clients[i].socket = 0;
            clients[i].is_logged_in = 0;
            memset(clients[i].user_id, 0, sizeof(clients[i].user_id));
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

// 클라이언트 요청 처리 함수
void *handle_client(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    ssize_t recv_size;

    printf("Client connected. Socket: %d\n", client_socket);

    // 클라이언트와 메시지 송수신
    while ((recv_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[recv_size] = '\0';
        printf("Client %d: %s\n", client_socket, buffer);

        // 로그인 상태 확인 처리
        if (strncmp(buffer, "CHECK_LOGIN", 11) == 0) {
            int is_logged_in = 0;
            
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            // 즉시 응답 전송
            const char* response = is_logged_in ? "LOGGED_IN" : "NOT_LOGGED_IN";
            if (send(client_socket, response, strlen(response), 0) < 0) {
                perror("Send failed");
            }
            continue;
        }
        
        // 회원가입 처리
        if (strncmp(buffer, "REGISTER", 8) == 0) {
            char user_id[MAX_USERID], password[MAX_PASSWORD];
            sscanf(buffer + 9, "%s %s", user_id, password); // username과 password 파싱

            int result = register_user(user_id, password);
            if (result == 1) {
                send(client_socket, "REGISTER_SUCCESS", strlen("REGISTER_SUCCESS"), 0);
            } else if (result == 0) {
                send(client_socket, "REGISTER_FAIL_USER_EXISTS", strlen("REGISTER_FAIL_USER_EXISTS"), 0);
            } else {
                send(client_socket, "REGISTER_FAIL", strlen("REGISTER_FAIL"), 0);
            }
            continue;
        }

        // 로그인 처리
        if (strncmp(buffer, "LOGIN", 5) == 0) {
            char user_id[MAX_USERID], password[MAX_PASSWORD];
            sscanf(buffer + 6, "%s %s", user_id, password); // username과 password 파싱    

            if (login_user(user_id, password) == 1) {
                pthread_mutex_lock(&mutex);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].socket == client_socket) {
                        strcpy(clients[i].user_id, user_id);
                        clients[i].is_logged_in = 1;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);
                send(client_socket, "LOGIN_SUCCESS", strlen("LOGIN_SUCCESS"), 0);
            } else {
                send(client_socket, "LOGIN_FAIL", strlen("LOGIN_FAIL"), 0);
            }
            continue;
        }

        // 게시글 작성 처리
        if (strncmp(buffer, "CREATE_POST", 11) == 0) {
            char user_id[MAX_USERID] = {0};
            int is_logged_in = 0;

            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    strcpy(user_id, clients[i].user_id);
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            
            char title[MAX_TITLE], content[MAX_CONTENT];
            // 큰따옴표 기준으로 데이터 파싱
            sscanf(buffer + 12, "\"%[^\"]\" \"%[^\"]\"", title, content);
            printf("Parsed title: %s\n", title);
            printf("Parsed content: %s\n", content);

            if (create_post(user_id, title, content) == 1) {
                send(client_socket, "CREATE_POST_SUCCESS", strlen("CREATE_POST_SUCCESS"), 0);
            } else {
                send(client_socket, "CREATE_POST_FAIL", strlen("CREATE_POST_FAIL"), 0);
            }
            continue;
        }   

        // 게시글 목록 조회 처리
        if (strncmp(buffer, "LIST_POSTS", 11) == 0) {
            int is_logged_in = 0;
    
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            char post_list[BUFFER_SIZE * 4]; // 클라이언트에 보낼 게시글 목록  
             
            if (list_posts(post_list, sizeof(post_list))) {
                send(client_socket, post_list, strlen(post_list), 0);
            } else {
                send(client_socket, "No posts available.\n", strlen("No posts available.\n"), 0);
            }
            continue;   
        }

        // 단일 게시글 조회 처리
        if (strncmp(buffer, "READ_POST", 9) == 0) {
            int post_id;
            int is_logged_in = 0;

            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            sscanf(buffer + 10, "%d", &post_id); // post_id 파싱
            printf("Requested Post ID: %d\n", post_id); // 디버깅 로그

            Post post;

            if (read_post(post_id, &post) == 1) {
                // 게시글 데이터를 문자열로 변환하여 클라이언트에 전송
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), 
                         "ID: %d | User: %s | Date: %s | Views: %d | Likes: %d\nTitle: %s\nContent: %s\n", 
                         post.post_id, post.user_id, post.date, post.views, post.likes, post.title, post.content);
                send(client_socket, response, strlen(response), 0);
            } else {
                send(client_socket, "Post not found.\n", strlen("Post not found.\n"), 0);
            }
            continue;
        }

        // 게시글 수정 처리
        if (strncmp(buffer, "UPDATE_POST", 11) == 0) {
            int post_id;
            char title[MAX_TITLE], content[MAX_CONTENT];
            char user_id[MAX_USERID] = {0};
            int is_logged_in = 0;
            
            // 로그인 상태 확인
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    strcpy(user_id, clients[i].user_id);
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
            
            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }
            // 입력 파싱
            sscanf(buffer + 12, "%d \"%[^\"]\" \"%[^\"]\"", 
                   &post_id, title, content);
    
            // 게시글 작성자 확인
            Post post;
            if (read_post(post_id, &post) != 1) {
                send(client_socket, "UPDATE_FAIL_POST_NOT_FOUND", 
                     strlen("UPDATE_FAIL_POST_NOT_FOUND"), 0);
                continue;
            }
    
            // 작성자만 수정 가능
            if (strcmp(post.user_id, user_id) != 0) {
                send(client_socket, "UPDATE_FAIL_NOT_AUTHOR", 
                     strlen("UPDATE_FAIL_NOT_AUTHOR"), 0);
                continue;
            }
    
            // 게시글 수정
            if (update_post(post_id, title, content) == 1) {
                send(client_socket, "UPDATE_SUCCESS", strlen("UPDATE_SUCCESS"), 0);
            } else {
                send(client_socket, "UPDATE_FAIL", strlen("UPDATE_FAIL"), 0);
            }
            continue;
        }

        // 게시글 삭제 처리
        if (strncmp(buffer, "DELETE_POST", 11) == 0) {
            int post_id;
            char user_id[MAX_USERID] = {0};
            int is_logged_in = 0;
            
            // 로그인 상태 확인
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    strcpy(user_id, clients[i].user_id);
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
            
            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            sscanf(buffer + 12, "%d", &post_id);

            // 게시글 작성자 확인
            Post post;
            if (read_post(post_id, &post) != 1) {
                send(client_socket, "DELETE_FAIL_POST_NOT_FOUND", 
                     strlen("DELETE_FAIL_POST_NOT_FOUND"), 0);
                continue;
            }

            // 작성자만 삭제 가능
            if (strcmp(post.user_id, user_id) != 0) {
                send(client_socket, "DELETE_FAIL_NOT_AUTHOR", 
                        strlen("DELETE_FAIL_NOT_AUTHOR"), 0);
                continue;
            }

            // 게시글 삭제
            if (delete_post(post_id) == 1) {
                send(client_socket, "DELETE_SUCCESS", strlen("DELETE_SUCCESS"), 0);
            } else {
                send(client_socket, "DELETE_FAIL", strlen("DELETE_FAIL"), 0);
            }
            continue;
        }

         // 채팅방 생성 처리
        if (strncmp(buffer, "CREATE_CHAT", 11) == 0) {
            char room_name[MAX_ROOM_NAME];
            char creator_id[MAX_USERID] = {0};
            char user_id[MAX_USERID] = {0};
            int is_logged_in = 0;

            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    strcpy(user_id, clients[i].user_id);
                    is_logged_in = clients[i].is_logged_in;
                    strncpy(creator_id, clients[i].user_id, MAX_USERID - 1);
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }
            
            sscanf(buffer + 12, "%s", room_name);
            
            int room_id = create_chat_room(room_name, creator_id);
            if (room_id != -1) {
                send(client_socket, "CREATE_CHAT_SUCCESS", strlen("CREATE_CHAT_SUCCESS"), 0);
            } else {
                send(client_socket, "CREATE_CHAT_FAIL", strlen("CREATE_CHAT_FAIL"), 0);
            }
            continue;
        }

        // 채팅방 목록 조회 처리
        if (strncmp(buffer, "VIEW_CHAT_LIST", 14) == 0) {
            int is_logged_in = 0;

            // 로그인 상태 확인
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
            
            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            char chat_list[BUFFER_SIZE * 4] = {0};
            view_chat_rooms(chat_list, sizeof(chat_list));
            send(client_socket, chat_list, strlen(chat_list), 0);
            continue;
        }

        // 채팅방 참여 처리
        if (strncmp(buffer, "JOIN_CHAT", 9) == 0) {
            char user_id[MAX_USERID] = {0};
            int is_logged_in = 0;

            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    strcpy(user_id, clients[i].user_id);
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            int room_id;
            sscanf(buffer + 10, "%d", &room_id);

            int result = join_chat_room(room_id, client_socket);
            if (result == 0) {
                send(client_socket, "JOIN_CHAT_SUCCESS", strlen("JOIN_CHAT_SUCCESS"), 0);
            } else {
                send(client_socket, "JOIN_CHAT_FAIL", strlen("JOIN_CHAT_FAIL"), 0);
            }
            continue;
        }

        // 메시지 송신 처리
        if (strncmp(buffer, "SEND_MESSAGE", 12) == 0) {
            char user_id[MAX_USERID] = {0};
            int is_logged_in = 0;
            
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    strcpy(user_id, clients[i].user_id);
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            int room_id;
            char message[MAX_MESSAGE];
            sscanf(buffer + 13, "%d %[^\n]", &room_id, message);

            broadcast_message(room_id, client_socket, message);
            send(client_socket, "MESSAGE_SENT", strlen("MESSAGE_SENT"), 0);
            continue;
        }

        // 채팅방 퇴장 처리
        if (strncmp(buffer, "LEAVE_CHAT", 10) == 0) {
            char user_id[MAX_USERID] = {0};
            int is_logged_in = 0;

            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == client_socket) {
                    strcpy(user_id, clients[i].user_id);
                    is_logged_in = clients[i].is_logged_in;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (!is_logged_in) {
                send(client_socket, "NOT_LOGGED_IN", strlen("NOT_LOGGED_IN"), 0);
                continue;
            }

            int room_id;
            sscanf(buffer + 11, "%d", &room_id);

            int result = leave_chat_room(room_id, client_socket);
            if (result == 0) {
                send(client_socket, "LEAVE_CHAT_SUCCESS", strlen("LEAVE_CHAT_SUCCESS"), 0);
            } else {
                send(client_socket, "LEAVE_CHAT_FAIL", strlen("LEAVE_CHAT_FAIL"), 0);
            }
            continue;
        }

        // 클라이언트에게 메시지 회신
        send(client_socket, "Message received", strlen("Message received"), 0);
        
    }

    // 클라이언트 연결 종료 처리
    printf("Client disconnected. Socket: %d\n", client_socket);
    remove_client_state(client_socket);
    close(client_socket);
    free(socket_desc);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    // 소켓 생성
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created.\n");

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 소켓 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    printf("Bind successful.\n");

    // 연결 대기
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        return 1;
    }
    printf("Waiting for incoming connections...\n");

    // 채팅방 정보 로드
    init_chat_rooms();
    load_chat_rooms();
    
    // 클라이언트 연결 수락
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size)) >= 0) {
        initialize_client_state(client_socket);

        // 새로운 스레드에서 클라이언트 처리
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
            perror("Could not create thread");
            close(client_socket);
            free(new_sock);
        }

        pthread_detach(thread_id); // 스레드 리소스 자동 정리
    }

    if (client_socket < 0) {
        perror("Accept failed");
        return 1;
    }

    // 소켓 닫기 및 뮤텍스 정리
    close(server_socket);
    pthread_mutex_destroy(&mutex);

    return 0;
}