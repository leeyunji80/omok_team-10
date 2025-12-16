#define _CRT_SECURE_NO_WARNINGS
#include "network.h"

/*
 * 오목 게임 네트워크 모듈 구현
 * Windows / macOS / Linux 크로스 플랫폼 지원
 */

/* ========== 네트워크 초기화/정리 ========== */

int net_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup 실패: %d\n", result);
        return -1;
    }
#endif
    return 0;
}

void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ========== 소켓 유틸리티 ========== */

SOCKET_TYPE net_create_socket(void) {
    SOCKET_TYPE sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK) {
        printf("소켓 생성 실패\n");
        return INVALID_SOCK;
    }
    return sock;
}

int net_set_nonblocking(SOCKET_TYPE sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

int net_set_blocking(SOCKET_TYPE sock) {
#ifdef _WIN32
    u_long mode = 0;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

/* ========== 서버 함수 ========== */

SOCKET_TYPE net_create_server(int port) {
    SOCKET_TYPE serverSocket;
    struct sockaddr_in serverAddr;
    int opt = 1;

    serverSocket = net_create_socket();
    if (serverSocket == INVALID_SOCK) {
        return INVALID_SOCK;
    }

    /* SO_REUSEADDR 설정 */
#ifdef _WIN32
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons((unsigned short)port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERR) {
        printf("바인드 실패\n");
        CLOSE_SOCKET(serverSocket);
        return INVALID_SOCK;
    }

    if (listen(serverSocket, MAX_CLIENTS) == SOCKET_ERR) {
        printf("리슨 실패\n");
        CLOSE_SOCKET(serverSocket);
        return INVALID_SOCK;
    }

    return serverSocket;
}

SOCKET_TYPE net_accept_client(SOCKET_TYPE serverSocket, struct sockaddr_in* clientAddr) {
    socklen_t addrLen = sizeof(struct sockaddr_in);
    SOCKET_TYPE clientSocket;

    clientSocket = accept(serverSocket, (struct sockaddr*)clientAddr, &addrLen);
    return clientSocket;
}

/* ========== 클라이언트 함수 ========== */

SOCKET_TYPE net_connect_to_server(const char* ip, int port) {
    SOCKET_TYPE sock;
    struct sockaddr_in serverAddr;

    sock = net_create_socket();
    if (sock == INVALID_SOCK) {
        return INVALID_SOCK;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((unsigned short)port);

    /* IP 주소 변환 */
    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        printf("잘못된 IP 주소: %s\n", ip);
        CLOSE_SOCKET(sock);
        return INVALID_SOCK;
    }

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERR) {
        printf("서버 연결 실패\n");
        CLOSE_SOCKET(sock);
        return INVALID_SOCK;
    }

    return sock;
}

/* ========== 메시지 송수신 ========== */

/*
 * 메시지 전송 형식: [4바이트 길이][NetMessage 구조체]
 * 네트워크 바이트 순서 사용
 */

int net_send_message(SOCKET_TYPE sock, const NetMessage* msg) {
    char buffer[BUFFER_SIZE];
    int msgLen = sizeof(NetMessage);
    int netLen;
    int totalLen;
    int sent;

    /* 길이를 네트워크 바이트 순서로 변환 */
    netLen = htonl(msgLen);

    /* 버퍼에 길이 + 메시지 복사 */
    memcpy(buffer, &netLen, 4);
    memcpy(buffer + 4, msg, msgLen);
    totalLen = 4 + msgLen;

    /* 전송 */
    sent = send(sock, buffer, totalLen, 0);
    if (sent != totalLen) {
        return -1;
    }

    return 0;
}

int net_recv_message(SOCKET_TYPE sock, NetMessage* msg) {
    char buffer[BUFFER_SIZE];
    int netLen;
    int msgLen;
    int received;
    int totalReceived;

    /* 먼저 4바이트 길이 수신 */
    totalReceived = 0;
    while (totalReceived < 4) {
        received = recv(sock, buffer + totalReceived, 4 - totalReceived, 0);
        if (received <= 0) {
            return -1;
        }
        totalReceived += received;
    }

    memcpy(&netLen, buffer, 4);
    msgLen = ntohl(netLen);

    if (msgLen != sizeof(NetMessage) || msgLen > BUFFER_SIZE - 4) {
        return -1;
    }

    /* 메시지 본문 수신 */
    totalReceived = 0;
    while (totalReceived < msgLen) {
        received = recv(sock, buffer + totalReceived, msgLen - totalReceived, 0);
        if (received <= 0) {
            return -1;
        }
        totalReceived += received;
    }

    memcpy(msg, buffer, msgLen);
    return 0;
}

int net_recv_message_nonblock(SOCKET_TYPE sock, NetMessage* msg) {
    char buffer[BUFFER_SIZE];
    int netLen;
    int msgLen;
    int received;

    /* 논블로킹 모드로 설정 */
    net_set_nonblocking(sock);

    /* 4바이트 길이 시도 */
    received = recv(sock, buffer, 4, 0);

#ifdef _WIN32
    if (received == SOCKET_ERROR) {
        int err = WSAGetLastError();
        net_set_blocking(sock);
        if (err == WSAEWOULDBLOCK) {
            return 1; /* 데이터 없음 */
        }
        return -1; /* 오류 */
    }
#else
    if (received < 0) {
        net_set_blocking(sock);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1; /* 데이터 없음 */
        }
        return -1; /* 오류 */
    }
#endif

    if (received == 0) {
        net_set_blocking(sock);
        return -1; /* 연결 종료 */
    }

    if (received < 4) {
        /* 나머지 길이 바이트 수신 */
        int remaining = 4 - received;
        int more;
        net_set_blocking(sock);
        more = recv(sock, buffer + received, remaining, 0);
        if (more <= 0) return -1;
        received += more;
    }

    memcpy(&netLen, buffer, 4);
    msgLen = ntohl(netLen);

    if (msgLen != sizeof(NetMessage)) {
        net_set_blocking(sock);
        return -1;
    }

    /* 블로킹으로 전환하여 메시지 본문 수신 */
    net_set_blocking(sock);

    received = 0;
    while (received < msgLen) {
        int r = recv(sock, buffer + received, msgLen - received, 0);
        if (r <= 0) return -1;
        received += r;
    }

    memcpy(msg, buffer, msgLen);
    return 0;
}

/* ========== 메시지 생성 헬퍼 ========== */

void net_create_connect_msg(NetMessage* msg, const char* nickname) {
    memset(msg, 0, sizeof(NetMessage));
    msg->type = MSG_CONNECT;
    strncpy(msg->nickname, nickname, sizeof(msg->nickname) - 1);
}

void net_create_move_msg(NetMessage* msg, int x, int y, int player) {
    memset(msg, 0, sizeof(NetMessage));
    msg->type = MSG_MOVE;
    msg->x = x;
    msg->y = y;
    msg->player = player;
}

void net_create_game_start_msg(NetMessage* msg, int yourColor, const char* opponentNick) {
    memset(msg, 0, sizeof(NetMessage));
    msg->type = MSG_GAME_START;
    msg->player = yourColor;
    strncpy(msg->nickname, opponentNick, sizeof(msg->nickname) - 1);
}

void net_create_game_end_msg(NetMessage* msg, int result) {
    memset(msg, 0, sizeof(NetMessage));
    msg->type = MSG_GAME_END;
    msg->result = result;
}

/* ========== 승리 체크 (서버용) ========== */

int net_check_win(int board[BOARD_NET_SIZE][BOARD_NET_SIZE], int x, int y, int player) {
    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};
    int dir;
    int count;
    int nx, ny;

    for (dir = 0; dir < 4; dir++) {
        count = 1;

        /* 정방향 */
        nx = x + dx[dir];
        ny = y + dy[dir];
        while (nx >= 0 && nx < BOARD_NET_SIZE && ny >= 0 && ny < BOARD_NET_SIZE
               && board[ny][nx] == player) {
            count++;
            nx += dx[dir];
            ny += dy[dir];
        }

        /* 역방향 */
        nx = x - dx[dir];
        ny = y - dy[dir];
        while (nx >= 0 && nx < BOARD_NET_SIZE && ny >= 0 && ny < BOARD_NET_SIZE
               && board[ny][nx] == player) {
            count++;
            nx -= dx[dir];
            ny -= dy[dir];
        }

        if (count >= 5) {
            return player;
        }
    }

    return 0;
}
