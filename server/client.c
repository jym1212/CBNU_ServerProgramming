#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// receive_messages 함수 수정
void *receive_messages(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    char server_reply[BUFFER_SIZE];
    ssize_t recv_size;

    while ((recv_size = recv(client_socket, server_reply, BUFFER_SIZE - 1, 0)) > 0) {
        server_reply[recv_size] = '\0';
        // CHECK_LOGIN 관련 응답은 별도 처리
        if (strcmp(server_reply, "LOGGED_IN") != 0 && 
            strcmp(server_reply, "NOT_LOGGED_IN") != 0) {
            printf("\nServer: %s\n", server_reply);
        }
    }

    if (recv_size == 0) {
        printf("Server disconnected.\n");
    } else if (recv_size < 0) {
        perror("Receive failed");
    }

    return NULL;
}

// check_login_status 함수 수정
int check_login_status(int client_socket) {
    char message[BUFFER_SIZE];
    char server_reply[BUFFER_SIZE];
    
    snprintf(message, sizeof(message), "CHECK_LOGIN");
    if (send(client_socket, message, strlen(message), 0) < 0) {
        return -1; // 전송 실패
    }

    // 타임아웃 설정
    struct timeval tv;
    tv.tv_sec = 5;  // 5초 타임아웃
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
    if (recv_size > 0) {
        server_reply[recv_size] = '\0';
        if (strcmp(server_reply, "LOGGED_IN") == 0) {
            return 1; // 로그인됨
        }
    }
    
    return 0; // 로그인되지 않음
}

// 클라이언트 메뉴 표시
void display_menu() {
    sleep(1);
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
    printf("0. Exit\n");
    printf("Select an option: ");
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t thread_id;
    int choice;

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
                    break;
                }
                break;
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
                    break;
                }
                break;
            }

            case 3: { // 게시글 생성
                // 로그인 상태 확인 요청    
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

                // 게시글 생성
                char title[MAX_TITLE], content[MAX_CONTENT];
                printf("제목 입력: ");  
                fgets(title, sizeof(title), stdin);
                title[strcspn(title, "\n")] = '\0';

                printf("내용 입력: ");
                fgets(content, sizeof(content), stdin);
                content[strcspn(content, "\n")] = '\0'; 

                sprintf(message, "CREATE_POST \"%s\" \"%s\"", title, content);
                if(send(client_socket, message, strlen(message), 0) < 0) {
                    printf("메시지 전송 실패\n");
                    break;
                }
                break;
            }

            case 4: { // 게시글 목록 조회
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

                sprintf(message, "LIST_POSTS");
                if(send(client_socket, message, strlen(message), 0) < 0){
                    perror("Send failed");
                }

                char server_reply[BUFFER_SIZE * 4];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("\n=== List of Posts ===\n%s", server_reply);
                } else {
                    printf("Failed to receive posts from server.\n");
                }
                break;
            }

            case 5: { // 게시글 조회
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

                int post_id;    
                printf("Enter Post ID to view: ");
                scanf("%d", &post_id);
                getchar(); // 입력 버퍼 정리

                sprintf(message, "READ_POST %d", post_id);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("\n=== Post Details ===\n%s", server_reply);
                    sleep(1);
                }
                break;
            }

            case 6: { // 게시글 수정
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

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

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("%s\n", server_reply);
                    sleep(1);
                }
                break;
            }

            case 7: { // 게시글 삭제
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

                int post_id;
                printf("Enter Post ID to delete: ");
                scanf("%d", &post_id);
                getchar(); // 입력 버퍼 정리

                sprintf(message, "DELETE_POST %d", post_id);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("%s\n", server_reply);
                    sleep(1);
                }
                break;
            }

            case 8: { // 채팅방 생성
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

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
                    sleep(1);
                }
                break;
            }

            case 9: { // 채팅방 목록 조회
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

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
                    sleep(1);
                }
                break;
            }

            case 10: { // 채팅방 참여
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

                int room_id;
                printf("Enter chat room ID to join: ");
                scanf("%d", &room_id);
                getchar(); // 입력 버퍼 클리어

                sprintf(message, "JOIN_CHAT %d", room_id);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    if (strcmp(server_reply, "JOIN_CHAT_SUCCESS") == 0) {
                        printf("Successfully joined chat room %d\n", room_id);
                    } else {
                        printf("Failed to join chat room: %s\n", server_reply);
                    }
                    sleep(1);
                }
                break;
            }

            case 11: { // 메시지 송신
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

                int room_id;
                char chat_message[MAX_MESSAGE];
                printf("Enter chat room ID to send message: ");
                scanf("%d", &room_id);
                getchar(); // 입력 버퍼 클리어

                printf("Enter message: ");
                fgets(chat_message, sizeof(chat_message), stdin);
                chat_message[strcspn(chat_message, "\n")] = '\0';

                sprintf(message, "SEND_MESSAGE %d %s", room_id, chat_message);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("%s\n", server_reply);
                    sleep(1);
                }
                break;
            }

            case 12: { // 채팅방 퇴장
                int login_status = check_login_status(client_socket);
                if (login_status < 0) {
                    printf("서버와 통신 중 오류가 발생했습니다.\n");
                    break;
                }
                if (login_status == 0) {
                    printf("로그인이 필요한 서비스입니다.\n");
                    sleep(1);
                    break;
                }
                getchar(); // 입력 버퍼 정리

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
                    sleep(1);
                }
                    break;
            }

            case 0: { // 종료
                printf("Exiting client...\n");
                close(client_socket);
                return 0;
            }

            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }

    // 클라이언트 종료 처리
    close(client_socket);
    return 0;
}