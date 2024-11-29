// login.c
#include "login.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERNAME 20
#define MAX_PASSWORD 20

int check_id(char *id);
int register_user(char *username, char *password);
int login_user(char *username, char *password);

// 중복 아이디 확인 함수
int check_id(char *id){
    FILE *fp = fopen("users.txt", "r");
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
int register_user(char *username, char *password) {
    // users.txt 파일을 추가 모드로 열기
    FILE *fp = fopen("users.txt", "a+");  
    if (!fp) {
        printf("Unable to open file.\n");
        return -1; // 파일 열기 실패
    }

    char file_id[MAX_USERNAME];
    char file_password[MAX_PASSWORD];

    // 중복 아이디 확인
    while (fscanf(fp, "%s %s", file_id, file_password) != EOF) {
        if (strcmp(username, file_id) == 0) {
            fclose(fp);
            return 0; // 중복 아이디
        }
    }

    // 중복되지 않은 경우 파일에 저장
    fprintf(fp, "%s %s\n", username, password);
    fclose(fp);

    return 1; // 회원가입 성공
}

// 로그인 함수
int login_user(char *username, char *password) {
    FILE *fp = fopen("users.txt", "r");
    if (!fp) {
        printf("Unable to open file.\n");
        return 0;
    }

    char file_id[MAX_USERNAME];
    char file_password[MAX_PASSWORD];
    while (fscanf(fp, "%s %s", file_id, file_password) != EOF) {
        if (strcmp(username, file_id) == 0 && strcmp(password, file_password) == 0) {
            fclose(fp);
            return 1; // 로그인 성공
        }
    }

    fclose(fp);
    return 0; // 로그인 실패
}
