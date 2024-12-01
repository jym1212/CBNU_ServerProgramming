#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "login.h"

// 중복 아이디 확인 함수
int check_id(char *id){
    FILE *fp = fopen("users.txt", "r");
    if (!fp) {
        perror("Unable to open file.\n");
        return 0;
    }

    char file_id[MAX_USERID];
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
int register_user(char *user_id, char *password) {
    FILE *fp = fopen("users.txt", "a");  
    if (!fp) {
        perror("Unable to open file.\n");
        return -1; 
    }

    char file_id[MAX_USERID];
    char file_password[MAX_PASSWORD];

    // 중복 아이디 확인
    while (fscanf(fp, "%s %s", file_id, file_password) != EOF) {
        if (strcmp(user_id, file_id) == 0) {
            fclose(fp);
            return 0; // 회원가입 실패
        }
    }

    fprintf(fp, "%s %s\n", user_id, password);
    fclose(fp);

    return 1; // 회원가입 성공
}

// 로그인 함수
int login_user(char *user_id, char *password) {
    FILE *fp = fopen("users.txt", "r");
    if (!fp) {
        perror("Unable to open file.\n");
        return 0;
    }

    char file_id[MAX_USERID];
    char file_password[MAX_PASSWORD];
    while (fscanf(fp, "%s %s", file_id, file_password) != EOF) {
        if (strcmp(user_id, file_id) == 0 && strcmp(password, file_password) == 0) {
            fclose(fp);
            return 1; // 로그인 성공
        }
    }

    fclose(fp);
    return 0; // 로그인 실패
}
