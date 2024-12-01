#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "board.h"

// 게시글 생성 함수
int create_post(const char *user_id, const char *title, const char *content){
    FILE *fp = fopen("boards.txt", "a");  
    if (!fp) {
        perror("Unable to open file.\n");
        return -1; 
    }

    int post_id = 1;
    time_t now = time(NULL);

    Post new_post;
    new_post.post_id = post_id++;
    snprintf(new_post.user_id, sizeof(new_post.user_id), "%s", user_id);
    strftime(new_post.date, sizeof(new_post.date), "%Y-%m-%d", localtime(&now));
    new_post.views = 0;
    new_post.likes = 0;
    snprintf(new_post.title, sizeof(new_post.title), "%s", title);
    snprintf(new_post.content, sizeof(new_post.content), "%s", content);
    
    if (fprintf(fp, "%d %s %s %d %d \"%s\" \"%s\"\n", new_post.post_id, new_post.user_id, new_post.date, new_post.views, new_post.likes, new_post.title, new_post.content) < 0) {
        perror("Failed to write to file.");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    printf("Post added successfully.\n");
    return 1;
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
