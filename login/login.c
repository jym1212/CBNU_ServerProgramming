#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERNAME 20
#define MAX_PASSWORD 20

int check_id(char *id);
void register_user();
int login_user();

// 사용자 정보(id, password) 저장 구조체
typedef struct {
    char id[MAX_USERNAME];
    char password[MAX_PASSWORD];
} User;

// 메인 함수
int main() {
    int menu;
    
    while (1) {
        printf("\n1. register\n2. login\n0. exit\n");
        printf("Select a menu : ");
        scanf("%d", &menu);

        if (menu == 1) { // 회원가입
            register_user();  
        } else if (menu == 2) { // 로그인
            if (login_user()) { 
                printf("Go to the main...\n");
            }
        } else if (menu == 0) { // 프로그램 종료
            printf("Exit the program.\n");
            break;
        } else {
            printf("Invalid choice, please try again.\n");
        }
    }

    return 0;
}

// 중복 아이디 확인 함수
int check_id(char *id){
    FILE *fp = fopen("login/users.txt", "r");
    if (!fp) {
        printf("Unable to open file.\n");
        return 0;
    }

    char file_id[MAX_USERNAME];
    char file_password[MAX_PASSWORD];

    while (fscanf(fp, "%s %s", file_id, file_password) != EOF) {
        if (strcmp(id, file_id) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

// 회원가입 함수
void register_user() {
    FILE *fp = fopen("login/users.txt", "a");  
    if (!fp) {
        printf("Unable to open file.\n");
        return;
    }
    
    User new_user;
    int check = 1;

    printf("\nRegister your membership.\n");
    printf("Enter your id : ");
    scanf("%s", new_user.id);

    // 중복 아이디 확인
    while(check == check_id(new_user.id)){
        printf("\nId already exists. Please choose a different id.\n");
        printf("Enter your id : ");
        scanf("%s", new_user.id);
    }

    printf("Enter your password : ");
    scanf("%s", new_user.password);

    // 파일에 사용자 id, password 저장
    fprintf(fp, "%s %s\n", new_user.id, new_user.password);
    fclose(fp);

    printf("Your membership has been registered.\n");
}

// 로그인 함수
int login_user() {
    FILE *fp = fopen("login/users.txt", "r"); 
    if (!fp) {
        printf("Unable to open file.\n");
        return 0;
    }

    char id[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char file_id[MAX_USERNAME];
    char file_password[MAX_PASSWORD];
    
    printf("Enter your id : ");
    scanf("%s", id);
    printf("Enter your password : ");
    scanf("%s", password);

    // 파일에서 사용자 id, password 확인
    while (fscanf(fp, "%s %s", file_id, file_password) != EOF) {
        if (strcmp(id, file_id) == 0) {
            if(strcmp(password, file_password) == 0) { // 로그인 성공
                printf("Login successful!\n");
                fclose(fp);
                return 1; 
            }
            else{
                printf("\nFailed login! Invalid password.\n"); // 비밀번호 불일치
                fclose(fp);
                login_user();
            }
        }
        else{
            printf("\nFailed login! Invalid username.\n"); // 아이디 불일치
            fclose(fp);
            login_user();  
        }
    }

    fclose(fp);
    return 0;  // 로그인 실패
}