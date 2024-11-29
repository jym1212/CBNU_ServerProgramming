#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> // POSIX 스레드 사용
#include <unistd.h>  // close 함수 사용
#include <arpa/inet.h> // 소켓 관련 함수 사용
#include "../login/login.h" // 로그인 및 회원가입 관련 헤더 추가

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// 클라이언트 소켓 관리
int client_sockets[MAX_CLIENTS];
int client_count = 0;

// 뮤텍스 객체 (클라이언트 리스트 보호용)
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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

        // 회원가입 처리
        if (strncmp(buffer, "REGISTER", 8) == 0) {
            char username[50], password[50];
            sscanf(buffer + 9, "%s %s", username, password); // username과 password 파싱

            int result = register_user(username, password);
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
            char username[50], password[50];
            sscanf(buffer + 6, "%s %s", username, password); // username과 password 파싱

            if (login_user(username, password) == 1) {
                send(client_socket, "LOGIN_SUCCESS", strlen("LOGIN_SUCCESS"), 0);
            } else {
                send(client_socket, "LOGIN_FAIL", strlen("LOGIN_FAIL"), 0);
            }
            continue;
        }

        // 클라이언트에게 메시지 회신
        send(client_socket, "Message received", strlen("Message received"), 0);
    }

    // 클라이언트 연결 종료 처리
    printf("Client disconnected. Socket: %d\n", client_socket);
    close(client_socket);

    // 클라이언트 리스트에서 소켓 제거
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == client_socket) {
            for (int j = i; j < client_count - 1; j++) {
                client_sockets[j] = client_sockets[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

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

    // 클라이언트 연결 수락
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size)) >= 0) {
        printf("New connection accepted. Socket: %d\n", client_socket);

        // 클라이언트 소켓 추가
        pthread_mutex_lock(&mutex);
        if (client_count >= MAX_CLIENTS) {
            printf("Max clients reached. Connection rejected: %d\n", client_socket);
            close(client_socket);
            pthread_mutex_unlock(&mutex);
            continue;
        }
        client_sockets[client_count++] = client_socket;
        pthread_mutex_unlock(&mutex);

        // 새로운 스레드에서 클라이언트 처리
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            perror("Malloc failed");
            close(client_socket);
            continue;
        }
        *new_sock = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
            perror("Could not create thread");
            close(client_socket);
            free(new_sock);
            continue;
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
