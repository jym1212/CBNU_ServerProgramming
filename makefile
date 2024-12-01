# 컴파일러와 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -pthread

# 디렉토리 설정
LOGIN_DIR = login
SERVER_DIR = server
BOARD_DIR = board

# 소스 파일과 헤더 파일
LOGIN_SRC = $(LOGIN_DIR)/login.c
LOGIN_HDR = $(LOGIN_DIR)/login.h
SERVER_SRC = $(SERVER_DIR)/server.c
CLIENT_SRC = $(SERVER_DIR)/client.c
BOARD_SRC = $(BOARD_DIR)/board.c
BOARD_HDR = $(BOARD_DIR)/board.h

# 출력 파일
SERVER_OUT = server.o
CLIENT_OUT = client.o

# 기본 목표
all: $(SERVER_OUT) $(CLIENT_OUT)

# 서버 컴파일
$(SERVER_OUT): $(SERVER_SRC) $(LOGIN_SRC) $(LOGIN_HDR) $(BOARD_SRC)		
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LOGIN_SRC) $(BOARD_SRC)	

# 클라이언트 컴파일
$(CLIENT_OUT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC)

# 클린업
clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)
