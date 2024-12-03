#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>  // POSIX 스레드 사용
#include <unistd.h>   // close 함수 사용
#include <arpa/inet.h> // 소켓 함수 사용
#include <sys/socket.h> // send, recv 함수 사용
#include <netinet/in.h> // 소켓 구조체 정의
#include "../chat/chat.h" // 채팅 관련 함수 사용

#define SERVER_IP "127.0.0.1" // 서버 IP (로컬 테스트용)
#define PORT 8080            // 서버 포트
#define BUFFER_SIZE 1024     // 송수신 버퍼 크기

#define MAX_USERID 20
#define MAX_PASSWORD 20
#define MAX_TITLE 20
#define MAX_CONTENT 50
#define MAX_CHAT_USERS 10
#define MAX_CHAT_ROOMS 5
#define MAX_ROOM_NAME 50
#define MAX_MESSAGE 100

extern ChatRoom chat_rooms[MAX_CHAT_ROOMS];

// 로그인 상태 확인 함수 추가
int check_login_status(int client_socket) {
    char message[BUFFER_SIZE];
    char server_reply[BUFFER_SIZE];
    ssize_t recv_size;
    struct timeval tv_old, tv_new;
    socklen_t len = sizeof(tv_old);
    
    // 기존 타임아웃 설정 저장
    if (getsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv_old, &len) < 0) {
        perror("Failed to get socket timeout");
        return 0;
    }
    
    // 새로운 타임아웃 설정 (1초)
    tv_new.tv_sec = 1;
    tv_new.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_new, sizeof(tv_new)) < 0) {
        perror("Failed to set socket timeout");
        return 0;
    }

    // 메시지 전송
    sprintf(message, "CHECK_LOGIN");
    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("Send failed");
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_old, sizeof(tv_old));
        return 0;
    }

    // 서버 응답 대기
    usleep(50000);  // 50ms 대기

    // 서버 응답 수신
    recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
    
    // 이전 타임아웃 설정 복원
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_old, sizeof(tv_old)) < 0) {
        perror("Failed to restore socket timeout");
    }

    if (recv_size > 0) {
        server_reply[recv_size] = '\0';
        if (strcmp(server_reply, "NOT_LOGGED_IN") == 0) {
            printf("Please log in first.\n");
            usleep(300000);
            return 0;
        }
        return 1;
    } else if (recv_size == 0) {
        printf("Server disconnected.\n");
        return 0;
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;  // 타임아웃 발생 시 조용히 실패 처리
        } else {
            perror("Receive failed");
            return 0;
        }
    }
}

// receive_messages 함수 수정
void *receive_messages(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    char server_reply[BUFFER_SIZE];
    ssize_t recv_size;

    while ((recv_size = recv(client_socket, server_reply, BUFFER_SIZE - 1, 0)) > 0) {
        server_reply[recv_size] = '\0';
        
        // "LOGGED_IN" 메시지는 무시
        if (strcmp(server_reply, "LOGGED_IN") == 0) {
            continue;
        }
        
        // 채팅 메시지나 다른 서버 응답만 출력
        printf("\nServer: %s\n", server_reply);
    }

    if (recv_size == 0) {
        printf("Server disconnected.\n");
    } else if (recv_size < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("Receive failed");
        }
    }

    return NULL;
}

// 클라이언트 메뉴 표시
void display_menu() {
    printf("\n=== Menu ===\n");
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Create Posts\n");
    printf("4. View ALL Posts\n");
    printf("5. Read Post\n");
    printf("6. Update Post\n");
    printf("7. Delete Post\n");
    printf("8. Create Chatting\n");
    printf("9. View Chatting List\n");
    printf("10. Join Chatting\n");
    printf("11. Send Message\n");
    printf("12. Leave Chatting\n");
    printf("13. Delete Chatting\n");
    printf("0. Exit\n");
    printf("Select an option: ");
}

// 채팅방 메뉴 표시 함수 추가
void display_chat_menu() {
    printf("\n=== Chat Menu ===\n");
    printf("1. Send Message\n");
    printf("2. Exit Chat Room\n");
    printf("Select an option: ");
}

// 채팅 로그 모니터링을 위한 구조체 추가
typedef struct {
    long last_pos;
    int room_id;
    int should_stop;
} ChatMonitorArgs;

// 채팅 로그 모니터링 함수 수정
void* monitor_chat_log(void* args) {
    ChatMonitorArgs* monitor_args = (ChatMonitorArgs*)args;
    FILE* fp;
    char buffer[BUFFER_SIZE];
    char current_room_str[20];
    snprintf(current_room_str, sizeof(current_room_str), "Room %d", monitor_args->room_id);

    while (!monitor_args->should_stop) {
        fp = fopen("chatting_log.txt", "r");
        if (fp != NULL) {
            fseek(fp, monitor_args->last_pos, SEEK_SET);
            
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                if (strstr(buffer, current_room_str) != NULL) {
                    printf("\n%s\n", buffer);  // 로그만 출력
                }
            }
            
            monitor_args->last_pos = ftell(fp);
            fclose(fp);
        }
        usleep(100000); // 0.1초 대기
    }
    
    free(monitor_args);
    return NULL;
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t thread_id;
    int choice;
    static char current_user[MAX_USERID];

    // 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created.\n");

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); // 서버 IP 주소
    server_addr.sin_port = htons(PORT);                 // 서버 포트

    // 서버에 연결
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }
    printf("Connected to server.\n");

    // 수신 스레드 생성
    if (pthread_create(&thread_id, NULL, receive_messages, (void *)&client_socket) != 0) {
        perror("Could not create thread");
        return 1;
    }

    usleep(500000);

    // 클라이언트 메뉴
    while (1) {
        display_menu();
        scanf("%d", &choice);
        getchar(); // 입력 버퍼 클리어

        switch (choice) {
            case 1: { // 회원가입
                char user_id[MAX_USERID], password[MAX_PASSWORD];
                printf("Enter new username: ");
                scanf("%s", user_id);
                printf("Enter new password: ");
                scanf("%s", password);
                getchar(); // 입력 버퍼 클리어

                // 회원가입 메시지 전송
                sprintf(message, "REGISTER %s %s", user_id, password);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    continue;  // 메뉴로 돌아가기
                }

                // 서버 응답 수신
                char register_result[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, register_result, sizeof(register_result) - 1, 0);
                if (recv_size > 0) {
                    register_result[recv_size] = '\0';
                    printf("Server: %s\n", register_result);
                } else {
                    printf("Failed to receive server response.\n");
                }
                
                usleep(300000);  // 메시지를 읽을 시간 제공
                continue;  // 메뉴로 돌아가기
            }

            case 2: { // 로그인
                char user_id[MAX_USERID], password[MAX_PASSWORD];
                
                printf("Enter username: ");
                scanf("%s", user_id);
                printf("Enter password: ");
                scanf("%s", password);
                getchar(); // 입력 버퍼 클리어

                // 로그인 메시지 전송
                sprintf(message, "LOGIN %s %s", user_id, password);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    continue;
                }

                // 서버 응답 수신
                char login_result[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, login_result, sizeof(login_result) - 1, 0);
                if (recv_size > 0) {
                    login_result[recv_size] = '\0';
                    printf("Server: %s\n", login_result);
                    
                    if (strcmp(login_result, "LOGIN_SUCCESS") == 0) {
                        strncpy(current_user, user_id, MAX_USERID - 1);
                        current_user[MAX_USERID - 1] = '\0';
                    }
                } else {
                    printf("Failed to receive server response.\n");
                }
                usleep(300000);
                continue;  // 메뉴로 돌아가기
            }


            case 3: { // 게시글 생성
                if (!check_login_status(client_socket)) {
                    continue;
                }

                char title[MAX_TITLE], content[MAX_CONTENT];
                printf("Enter title: ");
                fgets(title, sizeof(title), stdin);
                title[strcspn(title, "\n")] = '\0';

                printf("Enter content: ");
                fgets(content, sizeof(content), stdin);
                content[strcspn(content, "\n")] = '\0';

                sprintf(message, "CREATE_POST \"%s\" \"%s\"", title, content);
                if(send(client_socket, message, strlen(message), 0) < 0){   
                    perror("Send failed");
                    break;
                }

                // 게시글 작성 결과 수신을 위한 지역 변수 사용
                char create_result[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, create_result, sizeof(create_result) - 1, 0);
                if (recv_size > 0) {
                    create_result[recv_size] = '\0';
                    printf("%s\n", create_result);
                } else {
                    printf("Failed to receive server response.\n");
                }
                usleep(300000);
                continue;;
            }

            case 4: { // 게시글 목록 조회
                if (!check_login_status(client_socket)) {
                    continue;
                }

                sprintf(message, "LIST_POSTS");
                if(send(client_socket, message, strlen(message), 0) < 0){
                    perror("Send failed");
                    continue;  // 메뉴로 돌아가기
                }

                char post_list[BUFFER_SIZE * 4];
                ssize_t recv_size = recv(client_socket, post_list, sizeof(post_list) - 1, 0);
                if (recv_size > 0) {
                    post_list[recv_size] = '\0';
                    printf("\n=== List of Posts ===\n%s", post_list);
                } else {
                    printf("Failed to receive posts from server.\n");
                }
                usleep(300000);  // 메시지를 읽을 시간 제공
                continue;  // 메뉴로 돌아가기 
            }

            case 5: { // 게시글 조회
                // 로그인 상태 확인 요청
                if (!check_login_status(client_socket)) {
                    continue;
                }

                // 로그인 상태가 확인되면 게시글 조회 진행
                int post_id;    
                printf("Enter Post ID to view: ");
                scanf("%d", &post_id);
                getchar(); // 입력 버퍼 정리

                sprintf(message, "READ_POST %d", post_id);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char post_details[BUFFER_SIZE * 4];  // 더 큰 버퍼 사용
                ssize_t recv_size = recv(client_socket, post_details, sizeof(post_details) - 1, 0);
                if (recv_size > 0) {
                    post_details[recv_size] = '\0';
                    printf("\n=== Post Details ===\n%s", post_details);
                } else {
                    printf("Failed to receive post details.\n");
                }
                usleep(300000);
                continue;;
            }

            case 6: { // 게시글 수정
                // 로그인 상태 확인 요청
                if (!check_login_status(client_socket)) {
                    continue;
                }   

                // 로그인 상태가 확인되면 게시글 수정 진행
                int post_id;
                char title[MAX_TITLE], content[MAX_CONTENT];

                printf("Enter Post ID to update: ");
                scanf("%d", &post_id);
                getchar(); // 버퍼 비우기

                printf("Enter new title: ");
                fgets(title, sizeof(title), stdin);
                title[strcspn(title, "\n")] = '\0';
                
                printf("Enter new content: ");
                fgets(content, sizeof(content), stdin);
                content[strcspn(content, "\n")] = '\0';
                
                sprintf(message, "UPDATE_POST %d \"%s\" \"%s\"", post_id, title, content);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char update_result[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, update_result, sizeof(update_result) - 1, 0);
                if (recv_size > 0) {
                    update_result[recv_size] = '\0';
                    printf("%s\n", update_result);
                }
                usleep(300000);
                continue;;
            }

            case 7: { // 게시글 삭제
                // 로그인 상태 확인 요청
                if (!check_login_status(client_socket)) {
                    continue;
                }

                // 로그인 상태가 확인되면 게시글 삭제 진행
                int post_id;
                printf("Enter Post ID to delete: ");
                scanf("%d", &post_id);
                getchar(); // 입력 버퍼 정리

                sprintf(message, "DELETE_POST %d", post_id);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char delete_result[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, delete_result, sizeof(delete_result) - 1, 0);
                if (recv_size > 0) {
                    delete_result[recv_size] = '\0';
                    printf("%s\n", delete_result);
                }
                usleep(300000);
                continue;;
            }

            case 8: { // 채팅방 생성
                // 로그인 상태 확인 요청
                if (!check_login_status(client_socket)) {
                    continue;
                }

                // 로그인 상태가 확인되면 채팅방 생성 진행
                char room_name[MAX_ROOM_NAME];
                printf("Enter chat room name: ");
                fgets(room_name, sizeof(room_name), stdin);
                room_name[strcspn(room_name, "\n")] = '\0';

                sprintf(message, "CREATE_CHAT %s", room_name);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("%s\n", server_reply);
                    usleep(300000);
                }
                continue;;
            }

            case 9: { // 채팅방 목록 조회
                // 로그인 상태 확인 요청
                if (!check_login_status(client_socket)) {
                    continue;
                }

                // 로그인 상태가 확인되면 채팅방 목록 조회 진행
                sprintf(message, "VIEW_CHAT_LIST");
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char chat_list[BUFFER_SIZE * 4];
                ssize_t recv_size = recv(client_socket, chat_list, sizeof(chat_list) - 1, 0);
                if (recv_size > 0) {
                    chat_list[recv_size] = '\0';
                    printf("%s", chat_list);
                    usleep(300000);
                }
                continue;;
            }

            // ... existing code ...

            case 10: { // 채팅방 참여
                if (!check_login_status(client_socket)) {
                    continue;
                }

                int room_id;
                printf("Enter chat room ID to join: ");
                scanf("%d", &room_id);
                getchar(); // 입력 버퍼 클리어

                sprintf(message, "JOIN_CHAT %d", room_id);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    continue;
                }

                char join_result[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, join_result, sizeof(join_result) - 1, 0);
                if (recv_size > 0) {
                    join_result[recv_size] = '\0';
                    if (strcmp(join_result, "JOIN_CHAT_SUCCESS") == 0) {
                        printf("Successfully joined chat room %d\n", room_id);
                        
                        // 채팅 로그 모니터링 스레드 시작
                        pthread_t monitor_thread;
                        ChatMonitorArgs* monitor_args = malloc(sizeof(ChatMonitorArgs));
                        monitor_args->last_pos = 0;
                        monitor_args->room_id = room_id;
                        monitor_args->should_stop = 0;
                        
                        if (pthread_create(&monitor_thread, NULL, monitor_chat_log, monitor_args) != 0) {
                            perror("Failed to create monitor thread");
                            free(monitor_args);
                            continue;
                        }
                        
                        // 채팅방 메뉴 루프
                        while (1) {
                            usleep(300000);
                            display_chat_menu();
                            int chat_choice;
                            scanf("%d", &chat_choice);
                            getchar(); // 입력 버퍼 클리어

                            switch (chat_choice) {
                                case 1: { // 메시지 전송
                                    printf("Enter message: ");
                                    char chat_message[MAX_MESSAGE];
                                    fgets(chat_message, sizeof(chat_message), stdin);
                                    chat_message[strcspn(chat_message, "\n")] = '\0';

                                    snprintf(message, sizeof(message), "SEND_MESSAGE %d %s %s", 
                                            room_id, current_user, chat_message);
                                    if (send(client_socket, message, strlen(message), 0) < 0) {
                                        perror("Send failed");
                                    }
                                    break;
                                }
                                case 2: { // 채팅방 나가기
                                    sprintf(message, "LEAVE_CHAT %d", room_id);
                                    if (send(client_socket, message, strlen(message), 0) < 0) {
                                        perror("Send failed");
                                    }
                                    printf("Leaving chat room...\n");
                                    
                                    // 모니터링 스레드 종료
                                    monitor_args->should_stop = 1;
                                    pthread_join(monitor_thread, NULL);
                                    
                                    goto exit_chat;
                                }
                                default:
                                    printf("Invalid choice. Please try again.\n");
                                    break;
                            }
                        }
                        exit_chat:  // 채팅방 나가기 레이블
                        continue;
                    } else {
                        printf("Failed to join chat room: %s\n", join_result);
                    }
                }
                usleep(300000);
                continue;
            }

            case 11: { // 채팅 메시지 전송
                if (!check_login_status(client_socket)) {
                    continue;
                }

                static char current_user[MAX_USERID];  // 현재 로그인한 사용자 ID
                int room_id;
                char chat_message[MAX_MESSAGE];
                
                printf("Enter chat room ID: ");
                scanf("%d", &room_id);
                getchar(); // 버퍼 클리어

                printf("Enter message: ");
                fgets(chat_message, sizeof(chat_message), stdin);
                chat_message[strcspn(chat_message, "\n")] = '\0';

                // current_user가 비어있는지 확인
                if (current_user[0] == '\0') {
                    printf("Error: User ID not found. Please login again.\n");
                    usleep(300000);
                    continue;
                }

                // 메시지 형식: "SEND_MESSAGE room_id user_id message"
                sprintf(message, "SEND_MESSAGE %d %s %s", room_id, current_user, chat_message);
                
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    continue;
                }

                usleep(300000);
                continue;
            }

            case 12: { // 채팅방 퇴장
                // 로그인 상태 확인 요청    
                if (!check_login_status(client_socket)) {
                    continue;
                }

                // 로그인 상태가 확인되면 채팅방 퇴장 진행
                int room_id;
                printf("Enter chat room ID to leave: ");
                scanf("%d", &room_id);
                getchar(); // 입력 버퍼 클리어

                sprintf(message, "LEAVE_CHAT %d", room_id);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    if (strcmp(server_reply, "LEAVE_CHAT_SUCCESS") == 0) {
                        printf("Successfully left chat room %d\n", room_id);
                    } else {
                        printf("Failed to leave chat room: %s\n", server_reply);
                    }
                    usleep(300000);
                }
                continue;
            }
            
            case 0: { // 종료
                printf("Exiting client...\n");
                
                // chatting.txt 파일 비우기
                FILE *fp1 = fopen("chatting.txt", "w");
                if (fp1 != NULL) {
                    fclose(fp1);
                } else {
                    perror("Failed to clear chatting.txt");
                }

                FILE *fp2 = fopen("chatting_log.txt", "w");
                if (fp2 != NULL) {
                    fclose(fp2);
                } else {
                    perror("Failed to clear chatting.txt");
                }
                
                close(client_socket);
                return 0;
            }

            default:
                printf("Invalid choice. Please try again.\n");
                usleep(300000);
                break;
        }
    }

    // 클라이언트 종료 처리
    close(client_socket);
    return 0;
}