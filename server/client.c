//client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
    
    // 새로운 타임아웃 설정 (0.5초)
    tv_new.tv_sec = 1;
    tv_new.tv_usec = 0;  // 0.5초로 변경
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
            printf(" 로그인 후 이용 가능한 서비스입니다. \n");
            usleep(300000);
            return 0;
        }
        return 1;
    } else if (recv_size == 0) {
        printf("Server disconnected.\n");
        return 0;
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            printf("Checking login status...\n");
            usleep(100000);  // 0.1초 대기
            return 0;
        } else {
            perror("Receive failed");
            return 0;
        }
    }
}

// receive_messages 함수 
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

// 파일 출력 함수
void print_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("파일을 열 수 없습니다: %s\n", filename);
        return;
    }
    char ch;
    while ((ch = fgetc(file)) != EOF) {
        putchar(ch);
    }
    fclose(file);
}

int main() {

    system("clear");

    int client_socket;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t thread_id;
    int choice1; //로그인출력
    int choice2; //메인메뉴출력
    //int choice;

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

    system("clear");

    print_file("intro.txt");
    sleep(1);


    
    // 클라이언트 메뉴
    while (1) {
        print_file("mainlogin.txt");
        sleep(1);

        printf("원하는 작업의 번호를 입력해 주세요 (1 또는 2): ");
        scanf("%d", &choice1);
        getchar(); // 입력 버퍼 클리어
        system("clear");
        

        switch (choice1) {
            case 1: { // 회원가입

                print_file("register.txt");

                char user_id[50], password[50];
                printf("이용자 ID : ");
                scanf("%s", user_id);
                printf("비밀번호 : ");
                scanf("%s", password);
                getchar(); // 입력 버퍼 클리어

                // 회원가입 메시지 전송
                sprintf(message, "REGISTER %s %s", user_id, password); 
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }
                sleep(1);
                system("clear");

                print_file("register_success.txt");
                sleep(1);
                system("clear");

                continue;

            }

            case 2: { // 로그인
                system("clear");

                print_file("login.txt");

                char user_id[50], password[50];
                printf("이용자 ID : ");
                scanf("%s", user_id);
                printf("비밀번호 : ");
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
            default:
                printf("잘못된 선택입니다. 다시 입력해 주세요.\n");
            break;
        }
        sleep(1);

    // 로그인 후 메뉴 출력
    system("clear");
    break;
    }

    print_file("login_success.txt");
    sleep(1);
    system("clear");

    // 메인 메뉴

    
    // 클라이언트 종료 처리
    close(client_socket);
    return 0;
    }