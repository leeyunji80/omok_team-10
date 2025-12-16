# 오목 게임 Makefile
# Windows / macOS / Linux 크로스 플랫폼 빌드

CC = gcc
CFLAGS = -Wall -O2

# 플랫폼 감지
ifeq ($(OS),Windows_NT)
    # Windows
    EXE_EXT = .exe
    SERVER_LIBS = -lws2_32
    CLIENT_LIBS = -lws2_32
    RM = del /Q
else
    # macOS / Linux
    EXE_EXT =
    SERVER_LIBS =
    CLIENT_LIBS =
    RM = rm -f
endif

# 실행 파일 이름
CLIENT = omok_client$(EXE_EXT)
SERVER = omok_server$(EXE_EXT)

# 소스 파일
CLIENT_SRC = GameControl.c network.c minimax.c cJSON.c
SERVER_SRC = server.c network.c cJSON.c

# 기본 타겟: 클라이언트와 서버 모두 빌드
all: $(CLIENT) $(SERVER)

# 클라이언트 빌드
$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(CLIENT_LIBS)

# 서버 빌드
$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(SERVER_LIBS)

# 클라이언트만 빌드
client: $(CLIENT)

# 서버만 빌드
server: $(SERVER)

# 정리
clean:
	$(RM) $(CLIENT) $(SERVER)

# 도움말
help:
	@echo "사용법:"
	@echo "  make          - 클라이언트와 서버 모두 빌드"
	@echo "  make client   - 클라이언트만 빌드"
	@echo "  make server   - 서버만 빌드"
	@echo "  make clean    - 빌드 파일 삭제"
	@echo ""
	@echo "실행 방법:"
	@echo "  1. 서버 실행: ./omok_server (또는 omok_server.exe)"
	@echo "  2. 클라이언트 실행: ./omok_client (또는 omok_client.exe)"
	@echo ""
	@echo "서버 포트 지정: ./omok_server 9999"

.PHONY: all client server clean help
