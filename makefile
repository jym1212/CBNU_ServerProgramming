# 컴파일러와 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -pthread

# 디렉토리 설정
LOGIN_DIR = login
SERVER_DIR = server

# 소스 파일과 헤더 파일
LOGIN_SRC = $(LOGIN_DIR)/login.c
LOGIN_HDR = $(LOGIN_DIR)/login.h
SERVER_SRC = $(SERVER_DIR)/server.c
CLIENT_SRC = $(SERVER_DIR)/client.c

# 출력 파일
SERVER_O = server.o
CLIENT_O = client.o

# 기본 목표
all: $(SERVER_O) $(CLIENT_O)

# 서버 컴파일
$(SERVER_O): $(SERVER_SRC) $(LOGIN_SRC) $(LOGIN_HDR)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LOGIN_SRC)

# 클라이언트 컴파일
$(CLIENT_O): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC)

# 클린업
clean:
	rm -f $(SERVER_O) $(CLIENT_O)