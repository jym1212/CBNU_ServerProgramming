#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h> 
#include <unistd.h>   
#include <arpa/inet.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include "../chat/chat.h" 

#define SERVER_IP "127.0.0.1" // 서버 IP (로컬 테스트용)
#define PORT 8080             // 서버 포트
#define BUFFER_SIZE 1024      // 송수신 버퍼 크기

#define MAX_USERID 20
#define MAX_PASSWORD 20
#define MAX_TITLE 20
#define MAX_CONTENT 50
#define MAX_CHAT_USERS 10
#define MAX_CHAT_ROOMS 5
#define MAX_ROOM_NAME 50
#define MAX_MESSAGE 100

// 채팅 로그 모니터링을 위한 구조체 
typedef struct {
    long last_pos;
    int room_id;
    int should_stop;
} ChatMonitorArgs;

// 채팅방 관리를 위한 전역 변수
extern ChatRoom chat_rooms[MAX_CHAT_ROOMS];

// 로그인 상태 확인 함수 
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
    
    // 새로운 타임아웃 설정
    tv_new.tv_sec = 1;
    tv_new.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_new, sizeof(tv_new)) < 0) {
        perror("Failed to set socket timeout");
        return 0;
    }

    // 로그인 상태 확인 메시지 전송
    sprintf(message, "CHECK_LOGIN");
    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("Send failed");
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_old, sizeof(tv_old));
        return 0;
    }

    usleep(50000); 

    recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
    
    // 이전 타임아웃 설정 복원
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_old, sizeof(tv_old)) < 0) {
        perror("Failed to restore socket timeout");
    }

    // 서버 응답 확인
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
        // 타임아웃 발생 시 조용히 실패 처리
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0; 
        } else {
            perror("Receive failed");
            return 0;
        }
    }
}

// 서버로부터 메시지 수신하는 함수
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

// 파일 출력 함수
void print_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("파일을 열 수 없습니다 : %s\n", filename);
        return;
    }

    char ch;
    while ((ch = fgetc(file)) != EOF) {
        putchar(ch);
    }

    fclose(file);
}

// 채팅 로그 모니터링 함수
void* monitor_chat_log(void* args) {
    ChatMonitorArgs* monitor_args = (ChatMonitorArgs*)args;
    FILE* fp;
    char buffer[BUFFER_SIZE];
    char current_room_str[20];
    snprintf(current_room_str, sizeof(current_room_str), "Room %d", monitor_args->room_id);

    while (!monitor_args->should_stop) {
        fp = fopen("interface/chatting_log.txt", "r");
        if (fp != NULL) {
            fseek(fp, monitor_args->last_pos, SEEK_SET);
            
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                if (strstr(buffer, current_room_str) != NULL) {
                    printf("\n%s\n", buffer);  
                }
            }
            
            monitor_args->last_pos = ftell(fp);
            fclose(fp);
        }
        usleep(100000); 
    }
    
    free(monitor_args);
    return NULL;
}

int main() {
    system("clear");

    int client_socket;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t thread_id;

    int choice1;    //로그인 출력
    int choice2;    //메인 메뉴 출력 (게시판, 채팅)

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
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); 
    server_addr.sin_port = htons(PORT);                 

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

    system("clear");
    print_file("interface/intro.txt");
    sleep(1);

    // 클라이언트 메뉴
    while (1) {
        print_file("interface/mainlogin.txt");
        sleep(1);

        printf("원하는 작업의 번호를 입력해 주세요 (1 또는 2): ");
        scanf("%d", &choice1);
        getchar();
        system("clear");
        
        // 로그인 또는 회원가입 메뉴
        switch (choice1) {
            case 1: { // 회원가입
                print_file("interface/login/register.txt");

                char user_id[MAX_USERID], password[MAX_PASSWORD];
                printf("이용자 ID : ");
                scanf("%s", user_id);
                printf("비밀번호 : ");
                scanf("%s", password);
                getchar(); 

                // 회원가입 메시지 전송
                sprintf(message, "REGISTER %s %s", user_id, password); 
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }
                sleep(1);
                system("clear");

                print_file("interface/login/register_success.txt");
                sleep(1);
                system("clear");

                continue;

            }

            case 2: { // 로그인
                system("clear");
                print_file("interface/login/login.txt");

                char user_id[MAX_USERID], password[MAX_PASSWORD];
                printf("이용자 ID : ");
                scanf("%s", user_id);
                printf("비밀번호 : ");
                scanf("%s", password);
                getchar();

                // 로그인 메시지 전송
                sprintf(message, "LOGIN %s %s", user_id, password);
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    perror("Send failed");
                    break;
                }
                break;
            }
            default: { // 잘못된 선택 처리

                printf("잘못된 선택입니다. 다시 입력해 주세요.\n");
                break;
            }
        }
        sleep(1);

        // 로그인 후 메뉴 출력
        system("clear");
        break;
    }

    // 로그인 성공 
    print_file("interface/login/login_success.txt");
    sleep(1);
    system("clear");

    // 메인 메뉴
    while (1) {

        system("clear");
        print_file("interface/main2.txt");
        sleep(1);

        printf(" 원하는 작업의 번호를 입력해 주세요 : ");
        scanf("%d", &choice2);
        getchar();
        system("clear");

            switch (choice2) {
                case 11: { // 게시글 목록 조회
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/board/board_all.txt");

                    // 게시글 목록 메세지 전송
                    sprintf(message, "LIST_POSTS");
                    if(send(client_socket, message, strlen(message), 0) < 0){
                        perror("Send failed");
                        continue; 
                    }

                    // 게시글 목록 수신
                    char post_list[BUFFER_SIZE * 4];
                    ssize_t recv_size = recv(client_socket, post_list, sizeof(post_list) - 1, 0);
                    if (recv_size > 0) {
                        post_list[recv_size] = '\0';
                        printf("\n%s\n", post_list);
                    } else {
                        printf("Failed to receive posts from server.\n");
                    }
                    usleep(300000); 

                    printf("\n\n메뉴로 돌아가려면 0을 입력하세요: ");
                    int back_choice;
                    scanf("%d", &back_choice);
                    if (back_choice == 0) {
                        continue; 
                    }

                    system("clear");
                    break;
                }


                case 12: { // 게시글 검색
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/board/board_read.txt");

                    int post_id;    
                    printf("조회하고 싶은 게시글 ID를 입력하세요: ");
                    scanf("%d", &post_id);
                    getchar(); 

                    // 게시글 조회 메시지 전송
                    sprintf(message, "READ_POST %d", post_id);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        break;
                    }

                    system("clear");

                    // 게시글 조회 결과 수신
                    char post_details[BUFFER_SIZE * 4]; 
                    ssize_t recv_size = recv(client_socket, post_details, sizeof(post_details) - 1, 0);
                    if (recv_size > 0) {
                        print_file("interface/board/board_result.txt");
                        post_details[recv_size] = '\0';
                        printf("\n\n%s", post_details);
                    } else {
                        printf("Failed to receive post details.\n");
                    }
                    usleep(300000);

                    printf("\n\n메뉴로 돌아가려면 0을 입력하세요 : ");
                    int back_choice;
                    scanf("%d", &back_choice);
                    if (back_choice == 0) {
                        continue; 
                    }

                    system("clear");

                    break;
                }

                case 13: { // 게시글 생성
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/board/board_create.txt");

                    char title[MAX_TITLE], content[MAX_CONTENT];
                    printf(" 제목: ");
                    fgets(title, sizeof(title), stdin);
                    title[strcspn(title, "\n")] = '\0';

                    printf(" 내용: ");
                    fgets(content, sizeof(content), stdin);
                    content[strcspn(content, "\n")] = '\0';

                    // 게시글 생성 메시지 전송
                    sprintf(message, "CREATE_POST \"%s\" \"%s\"", title, content);
                    if(send(client_socket, message, strlen(message), 0) < 0){   
                        perror("Send failed");
                        break;
                    }

                    // 게시글 생성 결과 수신
                    char create_result[BUFFER_SIZE];
                    ssize_t recv_size = recv(client_socket, create_result, sizeof(create_result) - 1, 0);
                    if (recv_size > 0) {
                        create_result[recv_size] = '\0';
                        printf("%s\n", create_result);
                    } else {
                        printf("Failed to receive server response.\n");
                    }

                    usleep(300000);
                    system("clear");
                    break;
                }

                case 14: { // 게시글 수정
                    // 로그인 상태 확인 
                    if (!check_login_status(client_socket)) {
                        continue;
                    }   

                    print_file("interface/board/board_update.txt");

                    int post_id;
                    char title[MAX_TITLE], content[MAX_CONTENT];

                    printf(" 수정하고 싶은 게시글 ID를 입력하세요: ");
                    scanf("%d", &post_id);
                    getchar(); 
                    
                    printf(" 수정 제목: ");
                    fgets(title, sizeof(title), stdin);
                    title[strcspn(title, "\n")] = '\0';
                    
                    printf(" 수정 내용: ");
                    fgets(content, sizeof(content), stdin);
                    content[strcspn(content, "\n")] = '\0';
                    
                    // 게시글 수정 메시지 전송
                    sprintf(message, "UPDATE_POST %d \"%s\" \"%s\"", post_id, title, content);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        break;
                    }

                    // 게시글 수정 결과 수신
                    char update_result[BUFFER_SIZE];
                    ssize_t recv_size = recv(client_socket, update_result, sizeof(update_result) - 1, 0);
                    if (recv_size > 0) {
                        update_result[recv_size] = '\0';
                        printf("%s\n", update_result);
                    }

                    usleep(300000);
                    system("clear");
                    break;
                }

                case 15: { // 게시글 삭제
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/board/board_delete.txt");

                    int post_id;
                    printf(" 삭제하고 싶은 게시글 ID를 입력하세요 : ");
                    scanf("%d", &post_id);
                    getchar();

                    // 게시글 삭제 메시지 전송
                    sprintf(message, "DELETE_POST %d", post_id);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        break;
                    }

                    // 게시글 삭제 결과 수신
                    char delete_result[BUFFER_SIZE];
                    ssize_t recv_size = recv(client_socket, delete_result, sizeof(delete_result) - 1, 0);
                    if (recv_size > 0) {
                        delete_result[recv_size] = '\0';
                        printf("%s\n", delete_result);
                    }

                    usleep(300000);
                    system("clear");
                    break;
                }

                case 22: { // 채팅방 생성
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/chat/chat_create.txt");

                    char room_name[MAX_ROOM_NAME];
                    printf("채팅방 이름을 입력하세요: ");
                    fgets(room_name, sizeof(room_name), stdin);
                    room_name[strcspn(room_name, "\n")] = '\0';

                    // 채팅방 생성 메시지 전송
                    sprintf(message, "CREATE_CHAT %s", room_name);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        break;
                    }

                    // 채팅방 생성 결과 수신
                    char server_reply[BUFFER_SIZE];
                    ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                    if (recv_size > 0) {
                        server_reply[recv_size] = '\0';
                        printf("%s\n", server_reply);
                        usleep(300000);
                    }
                    continue;
                }

                case 21: { // 채팅방 목록 조회
                    // 로그인 상태 확인 
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/chat/chat_all.txt");

                    // 채팅방 목록 조회 메시지 전송
                    sprintf(message, "VIEW_CHAT_LIST");
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        break;
                    }

                    // 채팅방 목록 수신
                    char chat_list[BUFFER_SIZE * 4];
                    ssize_t recv_size = recv(client_socket, chat_list, sizeof(chat_list) - 1, 0);
                    if (recv_size > 0) {
                        chat_list[recv_size] = '\0';
                        printf("%s", chat_list);
                        usleep(300000);
                    }

                    printf("\n\n메뉴로 돌아가려면 0을 입력하세요: ");
                    int back_choice;
                    scanf("%d", &back_choice);
                    if (back_choice == 0) {
                        continue; 
                    }

                    system("clear");
                    continue;;
                }

                case 23: { // 채팅방 참여
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/chat/chat_start.txt");

                    int room_id;
                    printf("참여할 채팅방 ID를 입력하세요: ");
                    scanf("%d", &room_id);
                    getchar(); 

                    // 채팅방 참여 메시지 전송
                    sprintf(message, "JOIN_CHAT %d", room_id);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        continue;
                    }

                    // 채팅방 참여 결과 수신
                    char join_result[BUFFER_SIZE];
                    ssize_t recv_size = recv(client_socket, join_result, sizeof(join_result) - 1, 0);
                    if (recv_size > 0) {
                        join_result[recv_size] = '\0';

                        // 채팅방 참여 성공 시, 채팅방 메뉴로 이동
                        if (strcmp(join_result, "JOIN_CHAT_SUCCESS") == 0) {
                            printf("채팅방에 성공적으로 들어왔습니다. %d\n", room_id);
                            
                            // 채팅 로그 모니터링 스레드
                            pthread_t monitor_thread;
                            ChatMonitorArgs* monitor_args = malloc(sizeof(ChatMonitorArgs));
                            monitor_args->last_pos = 0;
                            monitor_args->room_id = room_id;
                            monitor_args->should_stop = 0;

                            // 모니터링 스레드 생성
                            if (pthread_create(&monitor_thread, NULL, monitor_chat_log, monitor_args) != 0) {
                                perror("Failed to create monitor thread");
                                free(monitor_args);
                                continue;
                            }

                            // 채팅방 메뉴 
                            while (1) {
                                usleep(300000);
                                print_file("interface/chat/chat_menu.txt");

                                int chat_choice;
                                printf("원하는 작업의 번호를 입력해 주세요 (1 또는 2): ");
                                scanf("%d", &chat_choice);
                                getchar();

                                switch (chat_choice) {
                                    case 1: { // 메시지 전송
                                        system("clear");
                                        printf("\n\n보낼 메시지:");
                                        char chat_message[MAX_MESSAGE];
                                        fgets(chat_message, sizeof(chat_message), stdin);
                                        chat_message[strcspn(chat_message, "\n")] = '\0';

                                        // 채팅방 메시지 전송
                                        snprintf(message, sizeof(message), "SEND_MESSAGE %d %s %s", 
                                                room_id, current_user, chat_message);
                                        if (send(client_socket, message, strlen(message), 0) < 0) {
                                            perror("Send failed");
                                        }
                                        break;
                                    }
                                    case 2: { // 채팅방 퇴장
                                        // 채팅방 퇴장 메시지 전송
                                        sprintf(message, "LEAVE_CHAT %d", room_id);
                                        if (send(client_socket, message, strlen(message), 0) < 0) {
                                            perror("Send failed");
                                        }
                                        printf("채팅방을 나갑니다 ...\n");
                                        
                                        // 모니터링 스레드 종료
                                        monitor_args->should_stop = 1;
                                        pthread_join(monitor_thread, NULL);
                                        
                                        // 채팅방 퇴장 
                                        goto exit_chat;
                                    }
                                    default: {
                                        printf("잘못된 접근입니다. 다시 입력 입력해주세요.\n");
                                        break;
                                    }
                                }
                            }

                            // 채팅방 퇴장 
                            exit_chat:  
                            continue;

                        } else {
                            printf("채팅방 참여에 실패했습니다. %s\n", join_result);
                        }
                    }
                    usleep(300000);
                    continue;
                }

                case 24: { // 채팅 메시지 전송
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    static char current_user[MAX_USERID];  // 현재 로그인한 사용자 ID
                    int room_id;
                    char chat_message[MAX_MESSAGE];
                    
                    printf("채팅방 ID를 입력하세요: ");
                    scanf("%d", &room_id);
                    getchar(); 

                    printf("메시지를 입력하세요: ");
                    fgets(chat_message, sizeof(chat_message), stdin);
                    chat_message[strcspn(chat_message, "\n")] = '\0';

                    // current_user가 비어있는지 확인
                    if (current_user[0] == '\0') {
                        printf("Error: User ID not found. Please login again.\n");
                        usleep(300000);
                        continue;
                    }

                    // 채팅방 메시지 전송
                    sprintf(message, "SEND_MESSAGE %d %s %s", room_id, current_user, chat_message);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        continue;
                    }

                    usleep(300000);
                    continue;
                }

                case 25: { // 채팅방 퇴장
                    // 로그인 상태 확인     
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    print_file("interface/chat/chat_end.txt");

                    int room_id;
                    printf("채팅방 ID를 입력하세요: ");
                    scanf("%d", &room_id);
                    getchar(); 

                    // 채팅방 퇴장 메시지 전송
                    sprintf(message, "LEAVE_CHAT %d", room_id);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        break;
                    }

                    // 채팅방 퇴장 결과 수신
                    char server_reply[BUFFER_SIZE];
                    ssize_t recv_size = recv(client_socket, server_reply, sizeof(server_reply) - 1, 0);
                    if (recv_size > 0) {
                        server_reply[recv_size] = '\0';
                        if (strcmp(server_reply, "LEAVE_CHAT_SUCCESS") == 0) {
                            printf("채팅방을 성공적로 마무리했습니다.%d\n", room_id);
                        } else {
                            printf("Failed to leave chat room: %s\n", server_reply);
                        }
                        usleep(300000);
                    }
                    continue;
                }

                case 26: { // 채팅방 삭제
                    // 로그인 상태 확인
                    if (!check_login_status(client_socket)) {
                        continue;
                    }

                    int room_id;
                    printf("삭제하기 위한 채팅방의 번호를 입력하세요: ");
                    scanf("%d", &room_id);
                    getchar(); 

                    print_file("interface/chat/chat_delete.txt");

                    // 채팅방 삭제 메시지 전송
                    sprintf(message, "DELETE_CHAT %d", room_id);
                    if (send(client_socket, message, strlen(message), 0) < 0) {
                        perror("Send failed");
                        continue;
                    }

                    // 채팅방 삭제 결과 수신
                    char delete_result[BUFFER_SIZE];
                    ssize_t recv_size = recv(client_socket, delete_result, sizeof(delete_result) - 1, 0);
                    if (recv_size > 0) {
                        delete_result[recv_size] = '\0';
                        if (strcmp(delete_result, "DELETE_CHAT_SUCCESS") == 0) {
                            printf("채팅방이 성공적으로 삭제되었습니다.\n");
                        } else if (strcmp(delete_result, "DELETE_CHAT_FAIL_NOT_CREATOR") == 0) {
                            printf("채팅방 생성자만 삭제할 수 있습니다.\n");
                        } else if (strcmp(delete_result, "DELETE_CHAT_FAIL_NOT_EXIST") == 0) {
                            printf("존재하지 않는 채팅방입니다.\n");
                        } else if (strcmp(delete_result, "DELETE_CHAT_FAIL_INVALID_ID") == 0) {
                            printf("잘못된 채팅방 ID입니다.\n");
                        } else {
                            printf("채팅방 삭제에 실패했습니다.\n");
                        }
                    }
                    usleep(300000);
                    printf("\n\n메뉴로 돌아가려면 0을 입력하세요: ");

                    int back_choice;
                    scanf("%d", &back_choice);
                    if (back_choice == 0) {
                        continue;  
                    }

                    continue;
                }

                case 0: { // 종료
                    printf("Exiting client...\n");
                    
                    // chatting.txt 파일 비우기
                    FILE *fp1 = fopen("interface/chatting.txt", "w");
                    if (fp1 != NULL) {
                        fclose(fp1);
                    } else {
                        perror("Failed to clear chatting.txt");
                    }

                    FILE *fp2 = fopen("interface/chatting_log.txt", "w");
                    if (fp2 != NULL) {
                        fclose(fp2);
                    } else {
                        perror("Failed to clear chatting.txt");
                    }
                    
                    // 클라이언트 종료 처리
                    close(client_socket);
                    return 0;
                }
                default:
                    printf("잘못된 선택입니다. 다시 입력해 주세요.\n");
                    system("clear");
                    break;
                }

            sleep(1);
        }
    
    // 클라이언트 종료 처리
    close(client_socket);
    return 0;
}