#ifndef BOARD_H
#define BOARD_H

#define MAX_TITLE 20
#define MAX_CONTENT 50

// 게시글 구조체
typedef struct {
    int post_id;
    char user_id[20];
    char date[20];
    int views; 
    char title[MAX_TITLE];
    char content[MAX_CONTENT];
} Post;

// 게시글 관련 함수
int create_post(const char *user_id, const char *title, const char *content);
int read_post(int post_id, Post *post);
int update_post(int post_id, const char *title, const char *content);
int delete_post(int post_id);
int list_posts(char *output, size_t output_size);

#endif