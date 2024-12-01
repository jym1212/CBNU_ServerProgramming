// login.h
#ifndef LOGIN_H
#define LOGIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERID 20
#define MAX_PASSWORD 20

int check_id(char *id);
int register_user(char *username, char *password);
int login_user(char *username, char *password);

#endif
