#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "board.h"

// 게시글 작성 함수
int create_post(const char *user_id, const char *title, const char *content) {
    FILE *fp = fopen("file_log/boards.txt", "r"); 
    int post_id = 1; 

    if (!fp) { 
        fp = fopen("file_log/boards.txt", "w"); 
        if (!fp) {
            perror("Error opening boards.txt");
            perror("Unable to create file.\n");
            return -1;
        }
        fclose(fp);
    } else {  
        Post last_post;
        while (fscanf(fp, "%d %s %s %d \"%[^\"]\" \"%[^\"]\"\n", 
                    &last_post.post_id, last_post.user_id, last_post.date, 
                    &last_post.views, last_post.title, last_post.content) == 6) {
            post_id = last_post.post_id + 1; 
        }
        fclose(fp);
    }

    // 새 게시글 작성
    time_t now = time(NULL);
    Post new_post;
    new_post.post_id = post_id;
    snprintf(new_post.user_id, sizeof(new_post.user_id), "%s", user_id);
    strftime(new_post.date, sizeof(new_post.date), "%Y-%m-%d", localtime(&now));
    new_post.views = 0;
    snprintf(new_post.title, sizeof(new_post.title), "%s", title);
    snprintf(new_post.content, sizeof(new_post.content), "%s", content);

    fp = fopen("file_log/boards.txt", "a");
    if (!fp) {
        perror("Error opening boards.txt");
        perror("Unable to open file for writing.\n");
        return -1;
    }

    // 파일에 새 게시글 추가
    if (fprintf(fp, "%d %s %s %d \"%s\" \"%s\"\n", new_post.post_id, new_post.user_id, new_post.date, 
                new_post.views, new_post.title, new_post.content) < 0) {
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
    FILE *fp = fopen("file_log/boards.txt", "r");  
    if (!fp) {
        perror("Error opening boards.txt");
        perror("Unable to open file.\n");
        return -1; 
    }

    // 게시글 검색
    while (fscanf(fp, "%d %s %s %d \"%[^\"]\" \"%[^\"]\"\n", 
                  &post->post_id, post->user_id, post->date, 
                  &post->views, post->title, post->content) == 6) {
        
        if (post->post_id == post_id) { 
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

// 게시글 수정 함수
int update_post(int post_id, const char *title, const char *content){
    FILE *fp = fopen("file_log/boards.txt", "r");  
    FILE *temp = fopen("file_log/temp.txt", "w");
    if (!fp || !temp) {
        perror("Error opening boards.txt");
        perror("Unable to open file.\n");
        return -1; 
    }

    Post post;
    int found = 0;

    while (fscanf(fp, "%d %s %s %d \"%[^\"]\" \"%[^\"]\"\n", 
                  &post.post_id, post.user_id, post.date, 
                  &post.views, post.title, post.content) == 6) {
        if (post.post_id == post_id) {
            fprintf(temp, "%d %s %s %d \"%s\" \"%s\"\n",
                    post.post_id, post.user_id, post.date,
                    post.views, title, content);
            found = 1;
        } else {
            fprintf(temp, "%d %s %s %d \"%s\" \"%s\"\n",
                    post.post_id, post.user_id, post.date,
                    post.views, post.title, post.content);
        }
    }

    fclose(fp);
    fclose(temp);

    remove("file_log/boards.txt");
    rename("file_log/temp.txt", "file_log/boards.txt");

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
    FILE *fp = fopen("file_log/boards.txt", "r");
    FILE *temp = fopen("file_log/temp.txt", "w");
    if (!fp || !temp) {
        perror("Error opening boards.txt");
        perror("Unable to open file.\n");
        return -1;
    }

    Post post;
    int found = 0;

    // 삭제할 게시글을 제외한 나머지 게시글을 임시 파일에 복사
    while (fscanf(fp, "%d %s %s %d \"%[^\"]\" \"%[^\"]\"\n",
                  &post.post_id, post.user_id, post.date,
                  &post.views, post.title, post.content) == 6) {
        if (post.post_id != post_id) {
            fprintf(temp, "%d %s %s %d \"%s\" \"%s\"\n",
                    post.post_id, post.user_id, post.date,
                    post.views, post.title, post.content);
        } else {
            found = 1;
        }
    }

    fclose(fp);
    fclose(temp);

    remove("file_log/boards.txt");
    rename("file_log/temp.txt", "file_log/boards.txt");

    return found;
}

// 게시글 목록 조회 함수
int list_posts(char *output, size_t output_size) {
    FILE *fp = fopen("file_log/boards.txt", "r");
    if (!fp) {
        perror("Error opening boards.txt");
        snprintf(output, output_size, "No posts available.\n");
        return 0;
    }

    char buffer[512];
    output[0] = '\0';      
    int found = 0;  

    Post post;
    // 파일 형식에 맞춰 fscanf 호출 수정
    while (fscanf(fp, "%d %s %s %d \"%[^\"]\" \"%[^\"]\"\n", 
                  &post.post_id, post.user_id, post.date, 
                  &post.views, post.title, post.content) == 6) {
        found = 1;
        snprintf(buffer, sizeof(buffer), 
                "ID: %d | User: %s | Date: %s | Views: %d | Title: %s\n",
                post.post_id, post.user_id, post.date, 
                post.views, post.title);
        
        if (strlen(output) + strlen(buffer) >= output_size - 1) {
            break;
        }
        strcat(output, buffer);
    }

    fclose(fp);

    if (!found) {
        snprintf(output, output_size, "No posts available.\n");
        return 0;
    }

    return 1; 
}