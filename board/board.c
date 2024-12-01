#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "board.h"

// 게시글 작성 함수
int create_post(const char *user_id, const char *title, const char *content) {
    FILE *fp = fopen("boards.txt", "r");  // 파일 읽기 모드로 열기
    if (!fp) {
        perror("Unable to open file.\n");
        return -1; 
    }

    int post_id = 1;
    Post last_post;

    // 마지막 게시글 ID 가져오기
    while (fscanf(fp, "%d %s %s %d %d \"%[^\"]\" \"%[^\"]\"\n", 
                  &last_post.post_id, last_post.user_id, last_post.date, 
                  &last_post.views, &last_post.likes, last_post.title, last_post.content) == 7) {
        post_id = last_post.post_id + 1; // 마지막 ID의 다음 값
    }
    fclose(fp); // 파일 닫기

    // 새 게시글 작성
    time_t now = time(NULL);
    Post new_post;
    new_post.post_id = post_id;
    snprintf(new_post.user_id, sizeof(new_post.user_id), "%s", user_id);
    strftime(new_post.date, sizeof(new_post.date), "%Y-%m-%d", localtime(&now));
    new_post.views = 0;
    new_post.likes = 0;
    snprintf(new_post.title, sizeof(new_post.title), "%s", title);
    snprintf(new_post.content, sizeof(new_post.content), "%s", content);

    // 파일 쓰기 모드로 열기
    fp = fopen("boards.txt", "a");
    if (!fp) {
        perror("Unable to open file for writing.\n");
        return -1;
    }

    // 파일에 새 게시글 추가
    if (fprintf(fp, "%d %s %s %d %d \"%s\" \"%s\"\n", new_post.post_id, new_post.user_id, new_post.date, 
                new_post.views, new_post.likes, new_post.title, new_post.content) < 0) {
        perror("Failed to write to file.");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    printf("Post added successfully.\n");
    return 1;
}


// 게시글 조회 함수
int read_post(int post_id, Post *post){
    FILE *fp = fopen("boards.txt", "r");  
    if (!fp) {
        perror("Unable to open file.\n");
        return -1; 
    }

    // 게시글 검색
    while (fscanf(fp, "%d %s %s %d %d \"%[^\"]\" \"%[^\"]\"\n", 
                  &post->post_id, post->user_id, post->date, 
                  &post->views, &post->likes, post->title, post->content) == 7) {
        
        if (post->post_id == post_id) { // ID가 일치하는 게시글 찾기
            fclose(fp);
            return 1; // 성공적으로 게시글 찾음
        }
    }

    fclose(fp);
    return 0;
}

// 게시글 수정 함수
int update_post(int post_id, const char *title, const char *content){
    FILE *fp = fopen("boards.txt", "r");  
    FILE *temp = fopen("temp.txt", "w");
    if (!fp || !temp) {
        perror("Unable to open file.\n");
        return -1; 
    }

    Post post;
    int found = 0;

    while (fscanf(fp, "%d %s %s %d %d \"%[^\"]\" \"%[^\"]\"\n", 
                  &post.post_id, post.user_id, post.date, 
                  &post.views, &post.likes, post.title, post.content) == 7) {
        if (post.post_id == post_id) {
            fprintf(temp, "%d %s %s %d %d \"%s\" \"%s\"\n",
                    post.post_id, post.user_id, post.date,
                    post.views, post.likes, title, content);
            found = 1;
        } else {
            fprintf(temp, "%d %s %s %d %d \"%s\" \"%s\"\n",
                    post.post_id, post.user_id, post.date,
                    post.views, post.likes, post.title, post.content);
        }
    }

    fclose(fp);
    fclose(temp);

    remove("boards.txt");
    rename("temp.txt", "boards.txt");

    if (found) {
        printf("Post updated successfully.\n");
        return 1;
    } else {
        printf("Post not found.\n");
        return 0;
    }
}

// 게시글 삭제 함수
int delete_post(int post_id) {
    FILE *fp = fopen("boards.txt", "r");
    FILE *temp = fopen("temp.txt", "w");
    if (!fp || !temp) {
        perror("Unable to open file.\n");
        return -1;
    }

    Post post;
    int found = 0;

    // 삭제할 게시글을 제외한 나머지 게시글을 임시 파일에 복사
    while (fscanf(fp, "%d %s %s %d %d \"%[^\"]\" \"%[^\"]\"\n",
                  &post.post_id, post.user_id, post.date,
                  &post.views, &post.likes, post.title, post.content) == 7) {
        if (post.post_id != post_id) {
            fprintf(temp, "%d %s %s %d %d \"%s\" \"%s\"\n",
                    post.post_id, post.user_id, post.date,
                    post.views, post.likes, post.title, post.content);
        } else {
            found = 1;
        }
    }

    fclose(fp);
    fclose(temp);

    remove("boards.txt");
    rename("temp.txt", "boards.txt");

    return found;
}

// 게시글 목록 조회 함수
int list_posts(char *output, size_t output_size) {
    FILE *fp = fopen("boards.txt", "r");
    if (!fp) {
        perror("Unable to open file.\n");
        snprintf(output, output_size, "No posts available.\n");
        return 0;
    }

    char buffer[512]; // 임시 버퍼
    output[0] = '\0'; // 초기화

    Post post;
    while (fscanf(fp, "%d %s %s %d %d \"%[^\"]\" \"%[^\"]\"\n", &post.post_id, post.user_id, post.date, &post.views, &post.likes, post.title, post.content) == 7) {
        snprintf(buffer, sizeof(buffer), "ID: %d | User: %s | Date: %s | Views: %d | Likes: %d | Title: %s | Content: %s\n",
                 post.post_id, post.user_id, post.date, post.views, post.likes, post.title, post.content);
        strncat(output, buffer, output_size - strlen(output) - 1); // 안전하게 문자열 추가
    }

    fclose(fp);
    return 1;
}

// 게시글 조회수 증가 함수
int view_post(int post_id);

// 게시글 좋아요 증가 함수
int like_post(int post_id);
