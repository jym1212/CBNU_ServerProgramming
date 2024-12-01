#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>  // POSIX 스레드 사용
#include <unistd.h>   // close 함수 사용
#include <arpa/inet.h> // 소켓 함수 사용

#define SERVER_IP "127.0.0.1" // 서버 IP (로컬 테스트용)
#define PORT 8080            // 서버 포트
#define BUFFER_SIZE 1024     // 송수신 버퍼 크기

#define MAX_USERID 20
#define MAX_PASSWORD 20
#define MAX_TITLE 20
#define MAX_CONTENT 50

// receive_messages 함수 수정
void *receive_messages(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    char server_reply[BUFFER_SIZE];
    ssize_t recv_size;

    while ((recv_size = recv(client_socket, server_reply, BUFFER_SIZE, 0)) > 0) {
        server_reply[recv_size] = '\0';
        // NOT_LOGGED_IN 메시지는 메인 스레드에서 처리하도록 무시
        if (strcmp(server_reply, "NOT_LOGGED_IN") != 0) {
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

// 클라이언트 메뉴 표시
void display_menu() {
    printf("\n=== Menu ===\n");
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Send Message\n");
    printf("4. Create Posts\n");
    printf("5. View ALL Posts\n");
    printf("6. Read Post\n");
    printf("7. Update Post\n");
    printf("8. Delete Post\n");
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
        sleep(1);
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

            case 3: { // 메시지 송신
                printf("Enter message: ");
                fgets(message, BUFFER_SIZE, stdin);
                message[strcspn(message, "\n")] = '\0'; // 개행 문자 제거

                if (strlen(message) == 0) {
                    continue; // 빈 메시지는 전송하지 않음
                }

                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                }
                break;
            }

            case 4: { // 게시글 생성
                // 로그인 상태 확인 요청    
                sprintf(message, "CHECK_LOGIN");
                if(send(client_socket, message, strlen(message), 0) < 0){
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    if (strcmp(server_reply, "NOT_LOGGED_IN") == 0) {
                        printf("Please log in first.\n");
                        sleep(1); // 메시지를 볼 수 있도록 잠시 대기
                        continue;
                    }   
                }

                // 로그인 상태가 확인되면 게시글 작성 진행
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

                // 게시글 작성 결과 수신
                memset(server_reply, 0, sizeof(server_reply));
                recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("%s\n", server_reply);
                    sleep(1);
                }
                break;
            }

            case 5: { // 게시글 목록 조회
                sprintf(message, "LIST_POSTS");
                if(send(client_socket, message, strlen(message), 0) < 0){
                    perror("Send failed");
                }

                char server_reply[BUFFER_SIZE * 4];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    if (strcmp(server_reply, "NOT_LOGGED_IN") == 0) {
                        printf("Please log in first.\n");
                        continue;
                    }
                    printf("\n=== List of Posts ===\n%s", server_reply);
                } else {
                    printf("Failed to receive posts from server.\n");
                }
                break;
            }

            case 6: { // 게시글 조회
                // 로그인 상태 확인 요청
                sprintf(message, "CHECK_LOGIN");
                if(send(client_socket, message, strlen(message), 0) < 0){
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    if (strcmp(server_reply, "NOT_LOGGED_IN") == 0) {
                        printf("Please log in first.\n");
                        sleep(1);
                        continue;
                    }
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

                memset(server_reply, 0, sizeof(server_reply));
                recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("\n=== Post Details ===\n%s", server_reply);
                    sleep(1);
                }
                break;
            }

            case 7: { // 게시글 수정
                // 로그인 상태 확인 요청
                sprintf(message, "CHECK_LOGIN");
                if(send(client_socket, message, strlen(message), 0) < 0){
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    if (strcmp(server_reply, "NOT_LOGGED_IN") == 0) {
                        printf("Please log in first.\n");
                        sleep(1);
                        continue;
                    }
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

                memset(server_reply, 0, sizeof(server_reply));
                recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("%s\n", server_reply);
                    sleep(1);
                }
                break;
            }

            case 8: { // 게시글 삭제
                // 로그인 상태 확인 요청
                sprintf(message, "CHECK_LOGIN");
                if(send(client_socket, message, strlen(message), 0) < 0){
                    perror("Send failed");
                    break;
                }

                char server_reply[BUFFER_SIZE];
                ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    if (strcmp(server_reply, "NOT_LOGGED_IN") == 0) {
                        printf("Please log in first.\n");
                        sleep(1);
                        continue;
                    }
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

                memset(server_reply, 0, sizeof(server_reply));
                recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                if (recv_size > 0) {
                    server_reply[recv_size] = '\0';
                    printf("%s\n", server_reply);
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