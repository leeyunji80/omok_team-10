#define _CRT_SECURE_NO_WARNINGS
#include "network.h"

/*
 * 오목 게임 매칭 서버
 * - 클라이언트 연결 관리
 * - 1:1 매칭
 * - 게임 진행 중계
 *
 * 컴파일:
 *   Windows: cl server.c network.c cJSON.c /Fe:server.exe
 *   macOS/Linux: gcc -o server server.c network.c cJSON.c
 */

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #include <sys/select.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* 서버 설정 */
#define SERVER_PORT 9999
#define MAX_SESSIONS 5

/* 전역 변수 */
static ClientInfo clients[MAX_CLIENTS];
static GameSession sessions[MAX_SESSIONS];
static int waitingClientIndex = -1;  /* 매칭 대기 중인 클라이언트 */

/* 함수 프로토타입 */
void initServer(void);
void handleNewConnection(SOCKET_TYPE serverSocket);
void handleClientMessage(int clientIndex);
void handleDisconnect(int clientIndex);
void matchPlayers(int client1, int client2);
void handleMove(int clientIndex, NetMessage* msg);
void endGame(int sessionIndex, int winner);
void broadcastToSession(int sessionIndex, NetMessage* msg, int excludeClient);
int findEmptyClientSlot(void);
int findClientSession(int clientIndex);
void printStatus(void);

/* ========== 메인 함수 ========== */

int main(int argc, char* argv[]) {
    SOCKET_TYPE serverSocket;
    fd_set readfds;
    struct timeval tv;
    int port = SERVER_PORT;
    int maxfd;
    int i;
    int activity;

    /* 포트 인자 처리 */
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            port = SERVER_PORT;
        }
    }

    printf("========================================\n");
    printf("       오목 게임 매칭 서버\n");
    printf("========================================\n\n");

    /* 네트워크 초기화 */
    if (net_init() != 0) {
        printf("네트워크 초기화 실패\n");
        return 1;
    }

    /* 서버 초기화 */
    initServer();

    /* 서버 소켓 생성 */
    serverSocket = net_create_server(port);
    if (serverSocket == INVALID_SOCK) {
        printf("서버 소켓 생성 실패\n");
        net_cleanup();
        return 1;
    }

    printf("서버 시작 (포트: %d)\n", port);
    printf("클라이언트 연결 대기 중...\n\n");

    /* 메인 루프 */
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        maxfd = (int)serverSocket;

        /* 모든 클라이언트 소켓 추가 */
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != INVALID_SOCK) {
                FD_SET(clients[i].socket, &readfds);
                if ((int)clients[i].socket > maxfd) {
                    maxfd = (int)clients[i].socket;
                }
            }
        }

        /* 타임아웃 설정 (100ms) */
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        activity = select(maxfd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            /* select 오류 */
#ifdef _WIN32
            if (WSAGetLastError() != WSAEINTR)
#else
            if (errno != EINTR)
#endif
            {
                printf("select 오류\n");
                break;
            }
            continue;
        }

        /* 새 연결 확인 */
        if (FD_ISSET(serverSocket, &readfds)) {
            handleNewConnection(serverSocket);
        }

        /* 클라이언트 메시지 확인 */
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != INVALID_SOCK &&
                FD_ISSET(clients[i].socket, &readfds)) {
                handleClientMessage(i);
            }
        }
    }

    /* 정리 */
    CLOSE_SOCKET(serverSocket);
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCK) {
            CLOSE_SOCKET(clients[i].socket);
        }
    }
    net_cleanup();

    printf("서버 종료\n");
    return 0;
}

/* ========== 서버 함수 구현 ========== */

void initServer(void) {
    int i, j;

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = INVALID_SOCK;
        clients[i].nickname[0] = '\0';
        clients[i].inGame = 0;
        clients[i].color = 0;
        clients[i].opponentIndex = -1;
    }

    for (i = 0; i < MAX_SESSIONS; i++) {
        sessions[i].active = 0;
        sessions[i].player1Index = -1;
        sessions[i].player2Index = -1;
        sessions[i].currentTurn = 1;
        sessions[i].moveCount = 0;
        for (j = 0; j < BOARD_NET_SIZE; j++) {
            memset(sessions[i].board[j], 0, sizeof(int) * BOARD_NET_SIZE);
        }
    }

    waitingClientIndex = -1;
}

int findEmptyClientSlot(void) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == INVALID_SOCK) {
            return i;
        }
    }
    return -1;
}

void handleNewConnection(SOCKET_TYPE serverSocket) {
    struct sockaddr_in clientAddr;
    SOCKET_TYPE clientSocket;
    int slotIndex;
    char ipStr[INET_ADDRSTRLEN];

    clientSocket = net_accept_client(serverSocket, &clientAddr);
    if (clientSocket == INVALID_SOCK) {
        return;
    }

    slotIndex = findEmptyClientSlot();
    if (slotIndex < 0) {
        printf("클라이언트 슬롯 부족, 연결 거부\n");
        CLOSE_SOCKET(clientSocket);
        return;
    }

    clients[slotIndex].socket = clientSocket;
    clients[slotIndex].inGame = 0;
    clients[slotIndex].color = 0;
    clients[slotIndex].opponentIndex = -1;

    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
    printf("[연결] 클라이언트 #%d 접속 (%s:%d)\n",
           slotIndex, ipStr, ntohs(clientAddr.sin_port));

    printStatus();
}

void handleClientMessage(int clientIndex) {
    NetMessage msg;
    NetMessage response;
    int result;

    result = net_recv_message(clients[clientIndex].socket, &msg);
    if (result != 0) {
        handleDisconnect(clientIndex);
        return;
    }

    switch (msg.type) {
        case MSG_CONNECT:
            /* 닉네임 저장 */
            strncpy(clients[clientIndex].nickname, msg.nickname,
                    sizeof(clients[clientIndex].nickname) - 1);
            printf("[접속] 클라이언트 #%d: %s\n",
                   clientIndex, clients[clientIndex].nickname);

            /* 접속 확인 응답 */
            memset(&response, 0, sizeof(response));
            response.type = MSG_CONNECT_ACK;
            strcpy(response.message, "서버 접속 성공");
            net_send_message(clients[clientIndex].socket, &response);
            break;

        case MSG_MATCH_REQUEST:
            printf("[매칭] %s 매칭 요청\n", clients[clientIndex].nickname);

            if (waitingClientIndex < 0) {
                /* 대기열에 추가 */
                waitingClientIndex = clientIndex;

                memset(&response, 0, sizeof(response));
                response.type = MSG_WAITING;
                strcpy(response.message, "상대방을 기다리는 중...");
                net_send_message(clients[clientIndex].socket, &response);

                printf("[매칭] %s 대기 중...\n", clients[clientIndex].nickname);
            } else if (waitingClientIndex != clientIndex) {
                /* 매칭 성공 */
                matchPlayers(waitingClientIndex, clientIndex);
                waitingClientIndex = -1;
            }
            break;

        case MSG_MATCH_CANCEL:
            if (waitingClientIndex == clientIndex) {
                waitingClientIndex = -1;
                printf("[매칭] %s 매칭 취소\n", clients[clientIndex].nickname);
            }
            break;

        case MSG_MOVE:
            handleMove(clientIndex, &msg);
            break;

        case MSG_DISCONNECT:
            handleDisconnect(clientIndex);
            break;

        case MSG_PING:
            memset(&response, 0, sizeof(response));
            response.type = MSG_PONG;
            net_send_message(clients[clientIndex].socket, &response);
            break;

        default:
            printf("[경고] 알 수 없는 메시지 타입: %d\n", msg.type);
            break;
    }
}

void handleDisconnect(int clientIndex) {
    int sessionIndex;
    int opponentIndex;
    NetMessage msg;

    if (clients[clientIndex].socket == INVALID_SOCK) {
        return;
    }

    printf("[연결 해제] 클라이언트 #%d (%s)\n",
           clientIndex, clients[clientIndex].nickname);

    /* 매칭 대기 중이었다면 제거 */
    if (waitingClientIndex == clientIndex) {
        waitingClientIndex = -1;
    }

    /* 게임 중이었다면 상대방에게 알림 */
    if (clients[clientIndex].inGame) {
        sessionIndex = findClientSession(clientIndex);
        opponentIndex = clients[clientIndex].opponentIndex;

        if (opponentIndex >= 0 && clients[opponentIndex].socket != INVALID_SOCK) {
            memset(&msg, 0, sizeof(msg));
            msg.type = MSG_OPPONENT_LEFT;
            strcpy(msg.message, "상대방이 연결을 종료했습니다.");
            net_send_message(clients[opponentIndex].socket, &msg);

            clients[opponentIndex].inGame = 0;
            clients[opponentIndex].opponentIndex = -1;
        }

        if (sessionIndex >= 0) {
            sessions[sessionIndex].active = 0;
        }
    }

    /* 클라이언트 정보 초기화 */
    CLOSE_SOCKET(clients[clientIndex].socket);
    clients[clientIndex].socket = INVALID_SOCK;
    clients[clientIndex].nickname[0] = '\0';
    clients[clientIndex].inGame = 0;
    clients[clientIndex].color = 0;
    clients[clientIndex].opponentIndex = -1;

    printStatus();
}

void matchPlayers(int client1, int client2) {
    int sessionIndex = -1;
    int i, j;
    NetMessage msg1, msg2;

    /* 빈 세션 찾기 */
    for (i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].active) {
            sessionIndex = i;
            break;
        }
    }

    if (sessionIndex < 0) {
        printf("[오류] 게임 세션 부족\n");
        return;
    }

    /* 세션 초기화 */
    sessions[sessionIndex].active = 1;
    sessions[sessionIndex].player1Index = client1;  /* 흑 */
    sessions[sessionIndex].player2Index = client2;  /* 백 */
    sessions[sessionIndex].currentTurn = 1;         /* 흑 선공 */
    sessions[sessionIndex].moveCount = 0;

    for (i = 0; i < BOARD_NET_SIZE; i++) {
        for (j = 0; j < BOARD_NET_SIZE; j++) {
            sessions[sessionIndex].board[i][j] = 0;
        }
    }

    /* 클라이언트 정보 업데이트 */
    clients[client1].inGame = 1;
    clients[client1].color = 1;  /* 흑 */
    clients[client1].opponentIndex = client2;

    clients[client2].inGame = 1;
    clients[client2].color = 2;  /* 백 */
    clients[client2].opponentIndex = client1;

    /* 게임 시작 메시지 전송 */
    net_create_game_start_msg(&msg1, 1, clients[client2].nickname);
    net_create_game_start_msg(&msg2, 2, clients[client1].nickname);

    net_send_message(clients[client1].socket, &msg1);
    net_send_message(clients[client2].socket, &msg2);

    printf("[게임 시작] 세션 #%d: %s(흑) vs %s(백)\n",
           sessionIndex,
           clients[client1].nickname,
           clients[client2].nickname);

    printStatus();
}

int findClientSession(int clientIndex) {
    int i;
    for (i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            (sessions[i].player1Index == clientIndex ||
             sessions[i].player2Index == clientIndex)) {
            return i;
        }
    }
    return -1;
}

void handleMove(int clientIndex, NetMessage* msg) {
    int sessionIndex;
    int x, y;
    int playerColor;
    int opponentIndex;
    NetMessage response;
    NetMessage moveMsg;

    sessionIndex = findClientSession(clientIndex);
    if (sessionIndex < 0) {
        printf("[오류] 세션을 찾을 수 없음: 클라이언트 #%d\n", clientIndex);
        return;
    }

    playerColor = clients[clientIndex].color;
    opponentIndex = clients[clientIndex].opponentIndex;
    x = msg->x;
    y = msg->y;

    /* 턴 확인 */
    if (sessions[sessionIndex].currentTurn != playerColor) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ERROR;
        strcpy(response.message, "상대방 턴입니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return;
    }

    /* 유효성 검사 */
    if (x < 0 || x >= BOARD_NET_SIZE || y < 0 || y >= BOARD_NET_SIZE) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ERROR;
        strcpy(response.message, "잘못된 좌표입니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return;
    }

    if (sessions[sessionIndex].board[y][x] != 0) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ERROR;
        strcpy(response.message, "이미 돌이 있는 위치입니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return;
    }

    /* 착수 */
    sessions[sessionIndex].board[y][x] = playerColor;
    sessions[sessionIndex].moveCount++;

    printf("[착수] 세션 #%d: %s (%d, %d)\n",
           sessionIndex, clients[clientIndex].nickname, x, y);

    /* 착수 확인 응답 */
    memset(&response, 0, sizeof(response));
    response.type = MSG_MOVE_ACK;
    response.x = x;
    response.y = y;
    response.player = playerColor;
    net_send_message(clients[clientIndex].socket, &response);

    /* 상대방에게 착수 전달 */
    net_create_move_msg(&moveMsg, x, y, playerColor);
    net_send_message(clients[opponentIndex].socket, &moveMsg);

    /* 승리 체크 */
    if (net_check_win(sessions[sessionIndex].board, x, y, playerColor)) {
        printf("[게임 종료] 세션 #%d: %s 승리!\n",
               sessionIndex, clients[clientIndex].nickname);
        endGame(sessionIndex, playerColor);
        return;
    }

    /* 무승부 체크 (보드가 가득 찬 경우) */
    if (sessions[sessionIndex].moveCount >= BOARD_NET_SIZE * BOARD_NET_SIZE) {
        printf("[게임 종료] 세션 #%d: 무승부\n", sessionIndex);
        endGame(sessionIndex, 0);
        return;
    }

    /* 턴 변경 */
    sessions[sessionIndex].currentTurn = (playerColor == 1) ? 2 : 1;
}

void endGame(int sessionIndex, int winner) {
    NetMessage msg;
    int player1 = sessions[sessionIndex].player1Index;
    int player2 = sessions[sessionIndex].player2Index;
    int result;

    if (winner == 1) {
        result = RESULT_BLACK_WIN;
    } else if (winner == 2) {
        result = RESULT_WHITE_WIN;
    } else {
        result = RESULT_DRAW;
    }

    /* 게임 종료 메시지 전송 */
    net_create_game_end_msg(&msg, result);

    if (clients[player1].socket != INVALID_SOCK) {
        net_send_message(clients[player1].socket, &msg);
        clients[player1].inGame = 0;
        clients[player1].opponentIndex = -1;
    }

    if (clients[player2].socket != INVALID_SOCK) {
        net_send_message(clients[player2].socket, &msg);
        clients[player2].inGame = 0;
        clients[player2].opponentIndex = -1;
    }

    /* 세션 초기화 */
    sessions[sessionIndex].active = 0;

    printStatus();
}

void printStatus(void) {
    int connectedCount = 0;
    int inGameCount = 0;
    int activeSessionCount = 0;
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCK) {
            connectedCount++;
            if (clients[i].inGame) {
                inGameCount++;
            }
        }
    }

    for (i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active) {
            activeSessionCount++;
        }
    }

    printf("[상태] 접속: %d명, 게임 중: %d명, 활성 세션: %d개, 대기: %s\n",
           connectedCount, inGameCount, activeSessionCount,
           (waitingClientIndex >= 0) ? clients[waitingClientIndex].nickname : "없음");
}
