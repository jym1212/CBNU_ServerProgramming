#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>  // POSIX 스레드 사용
#include <unistd.h>   // close 함수 사용
#include <arpa/inet.h> // 소켓 함수 사용

#define SERVER_IP "127.0.0.1" // 서버 IP (로컬 테스트용)
#define PORT 8080            // 서버 포트
#define BUFFER_SIZE 1024     // 송수신 버퍼 크기

// 서버로부터 메시지 수신을 처리하는 스레드 함수
void *receive_messages(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    char server_reply[BUFFER_SIZE];
    ssize_t recv_size;

    while ((recv_size = recv(client_socket, server_reply, BUFFER_SIZE, 0)) > 0) {
        server_reply[recv_size] = '\0';
        printf("\nServer: %s\n", server_reply);
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
    server_addr.sin_port = htons(PORT);                // 서버 포트

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
                char username[50], password[50];
                printf("Enter new username: ");
                scanf("%s", username);
                printf("Enter new password: ");
                scanf("%s", password);
                getchar(); // 입력 버퍼 클리어

                // 회원가입 메시지 전송
                sprintf(message, "REGISTER %s %s", username, password);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }
                break;
            }

            case 2: { // 로그인
                char username[50], password[50];
                printf("Enter username: ");
                scanf("%s", username);
                printf("Enter password: ");
                scanf("%s", password);
                getchar(); // 입력 버퍼 클리어

                // 로그인 메시지 전송
                sprintf(message, "LOGIN %s %s", username, password);
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
