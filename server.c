#define _CRT_SECURE_NO_WARNINGS
#include "network.h"

/*
 * 오목 게임 방 기반 매칭 서버
 * - 방 생성/목록/입장 관리
 * - 게임 진행 중계
 *
 * 컴파일:
 *   Windows: gcc -o omok_server server.c network.c cJSON.c -lws2_32
 *   macOS/Linux: gcc -o omok_server server.c network.c cJSON.c
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

/* 전역 변수 */
static ClientInfo clients[MAX_CLIENTS];
static GameRoom rooms[MAX_ROOMS];
static int nextRoomId = 1;

/* 함수 프로토타입 */
void initServer(void);
void handleNewConnection(SOCKET_TYPE serverSocket);
void handleClientMessage(int clientIndex);
void handleDisconnect(int clientIndex);

/* 방 관련 함수 */
int createRoom(int clientIndex, const char* roomName);
void sendRoomList(int clientIndex);
int joinRoom(int clientIndex, int roomId);
void leaveRoom(int clientIndex);
void startGame(int roomIndex);

/* 게임 관련 함수 */
void handleMove(int clientIndex, NetMessage* msg);
void endGame(int roomIndex, int winner);

/* 유틸리티 */
int findEmptyClientSlot(void);
int findEmptyRoomSlot(void);
int findRoomById(int roomId);
int findClientRoom(int clientIndex);
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
    printf("       오목 게임 서버 (방 시스템)\n");
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

/* ========== 서버 초기화 ========== */

void initServer(void) {
    int i, j;

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = INVALID_SOCK;
        clients[i].nickname[0] = '\0';
        clients[i].inRoom = 0;
        clients[i].roomId = -1;
        clients[i].inGame = 0;
        clients[i].color = 0;
        clients[i].opponentIndex = -1;
    }

    for (i = 0; i < MAX_ROOMS; i++) {
        rooms[i].active = 0;
        rooms[i].roomId = -1;
        rooms[i].roomName[0] = '\0';
        rooms[i].hostIndex = -1;
        rooms[i].guestIndex = -1;
        rooms[i].inGame = 0;
        rooms[i].currentTurn = 1;
        rooms[i].moveCount = 0;
        for (j = 0; j < BOARD_NET_SIZE; j++) {
            memset(rooms[i].board[j], 0, sizeof(int) * BOARD_NET_SIZE);
        }
    }
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

int findEmptyRoomSlot(void) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            return i;
        }
    }
    return -1;
}

int findRoomById(int roomId) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && rooms[i].roomId == roomId) {
            return i;
        }
    }
    return -1;
}

int findClientRoom(int clientIndex) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active &&
            (rooms[i].hostIndex == clientIndex || rooms[i].guestIndex == clientIndex)) {
            return i;
        }
    }
    return -1;
}

/* ========== 연결 처리 ========== */

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
    clients[slotIndex].inRoom = 0;
    clients[slotIndex].roomId = -1;
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
    int roomIndex;

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
            strcpy(response.message, "서버 접속 성공! 방을 만들거나 입장하세요.");
            net_send_message(clients[clientIndex].socket, &response);
            break;

        case MSG_ROOM_CREATE:
            /* 방 생성 */
            if (createRoom(clientIndex, msg.nickname) >= 0) {
                printf("[방 생성] %s님이 '%s' 방 생성\n",
                       clients[clientIndex].nickname, msg.nickname);
            }
            break;

        case MSG_ROOM_LIST:
            /* 방 목록 요청 */
            sendRoomList(clientIndex);
            break;

        case MSG_ROOM_JOIN:
            /* 방 입장 (msg.x = roomId) */
            roomIndex = joinRoom(clientIndex, msg.x);
            if (roomIndex >= 0) {
                printf("[방 입장] %s님이 방 #%d 입장\n",
                       clients[clientIndex].nickname, msg.x);
            }
            break;

        case MSG_ROOM_LEAVE:
            /* 방 퇴장 */
            leaveRoom(clientIndex);
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
    int roomIndex;
    int opponentIndex;
    NetMessage msg;

    if (clients[clientIndex].socket == INVALID_SOCK) {
        return;
    }

    printf("[연결 해제] 클라이언트 #%d (%s)\n",
           clientIndex, clients[clientIndex].nickname);

    /* 방에 있었다면 처리 */
    roomIndex = findClientRoom(clientIndex);
    if (roomIndex >= 0) {
        /* 상대방에게 알림 */
        if (rooms[roomIndex].hostIndex == clientIndex) {
            opponentIndex = rooms[roomIndex].guestIndex;
        } else {
            opponentIndex = rooms[roomIndex].hostIndex;
        }

        if (opponentIndex >= 0 && clients[opponentIndex].socket != INVALID_SOCK) {
            memset(&msg, 0, sizeof(msg));
            msg.type = MSG_OPPONENT_LEFT;
            strcpy(msg.message, "상대방이 연결을 종료했습니다.");
            net_send_message(clients[opponentIndex].socket, &msg);

            /* 상대방 상태 초기화 */
            clients[opponentIndex].inGame = 0;
            clients[opponentIndex].inRoom = 0;
            clients[opponentIndex].roomId = -1;
            clients[opponentIndex].opponentIndex = -1;
        }

        /* 방 삭제 */
        rooms[roomIndex].active = 0;
        printf("[방 삭제] 방 #%d 삭제됨\n", rooms[roomIndex].roomId);
    }

    /* 클라이언트 정보 초기화 */
    CLOSE_SOCKET(clients[clientIndex].socket);
    clients[clientIndex].socket = INVALID_SOCK;
    clients[clientIndex].nickname[0] = '\0';
    clients[clientIndex].inRoom = 0;
    clients[clientIndex].roomId = -1;
    clients[clientIndex].inGame = 0;
    clients[clientIndex].color = 0;
    clients[clientIndex].opponentIndex = -1;

    printStatus();
}

/* ========== 방 관련 함수 ========== */

int createRoom(int clientIndex, const char* roomName) {
    int roomIndex;
    NetMessage response;

    roomIndex = findEmptyRoomSlot();
    if (roomIndex < 0) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ERROR;
        strcpy(response.message, "방을 더 이상 만들 수 없습니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return -1;
    }

    /* 방 생성 */
    rooms[roomIndex].active = 1;
    rooms[roomIndex].roomId = nextRoomId++;
    strncpy(rooms[roomIndex].roomName, roomName, ROOM_NAME_LEN - 1);
    rooms[roomIndex].hostIndex = clientIndex;
    rooms[roomIndex].guestIndex = -1;
    rooms[roomIndex].inGame = 0;
    rooms[roomIndex].currentTurn = 1;
    rooms[roomIndex].moveCount = 0;
    memset(rooms[roomIndex].board, 0, sizeof(rooms[roomIndex].board));

    /* 클라이언트 상태 업데이트 */
    clients[clientIndex].inRoom = 1;
    clients[clientIndex].roomId = rooms[roomIndex].roomId;

    /* 응답 전송 */
    memset(&response, 0, sizeof(response));
    response.type = MSG_ROOM_CREATE_ACK;
    response.x = rooms[roomIndex].roomId;
    sprintf(response.message, "방 '%s' 생성 완료! 상대방을 기다리는 중...", roomName);
    net_send_message(clients[clientIndex].socket, &response);

    printStatus();
    return roomIndex;
}

void sendRoomList(int clientIndex) {
    NetMessage response;
    int i;
    int count = 0;

    memset(&response, 0, sizeof(response));
    response.type = MSG_ROOM_LIST_RESP;

    for (i = 0; i < MAX_ROOMS && count < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            response.rooms[count].roomId = rooms[i].roomId;
            strncpy(response.rooms[count].roomName, rooms[i].roomName, ROOM_NAME_LEN - 1);
            strncpy(response.rooms[count].hostName,
                    clients[rooms[i].hostIndex].nickname, 49);

            /* 플레이어 수 계산 */
            response.rooms[count].playerCount = 1;
            if (rooms[i].guestIndex >= 0) {
                response.rooms[count].playerCount = 2;
            }
            response.rooms[count].inGame = rooms[i].inGame;

            count++;
        }
    }

    response.y = count; /* 방 개수 */
    net_send_message(clients[clientIndex].socket, &response);

    printf("[방 목록] %s님에게 %d개 방 정보 전송\n",
           clients[clientIndex].nickname, count);
}

int joinRoom(int clientIndex, int roomId) {
    int roomIndex;
    NetMessage response;
    NetMessage startMsg;
    int j;

    roomIndex = findRoomById(roomId);
    if (roomIndex < 0) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ROOM_NOT_FOUND;
        strcpy(response.message, "방을 찾을 수 없습니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return -1;
    }

    /* 이미 2명인지 확인 */
    if (rooms[roomIndex].guestIndex >= 0) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ROOM_FULL;
        strcpy(response.message, "방이 가득 찼습니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return -1;
    }

    /* 게임 중인지 확인 */
    if (rooms[roomIndex].inGame) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ERROR;
        strcpy(response.message, "이미 게임이 진행 중인 방입니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return -1;
    }

    /* 입장 처리 */
    rooms[roomIndex].guestIndex = clientIndex;
    clients[clientIndex].inRoom = 1;
    clients[clientIndex].roomId = roomId;

    /* 입장 확인 응답 */
    memset(&response, 0, sizeof(response));
    response.type = MSG_ROOM_JOIN_ACK;
    response.x = roomId;
    sprintf(response.message, "방 '%s' 입장 완료!", rooms[roomIndex].roomName);
    net_send_message(clients[clientIndex].socket, &response);

    /* 게임 시작! */
    startGame(roomIndex);

    printStatus();
    return roomIndex;
}

void leaveRoom(int clientIndex) {
    int roomIndex;
    int opponentIndex;
    NetMessage msg;

    roomIndex = findClientRoom(clientIndex);
    if (roomIndex < 0) {
        return;
    }

    printf("[방 퇴장] %s님이 방 #%d 퇴장\n",
           clients[clientIndex].nickname, rooms[roomIndex].roomId);

    /* 상대방에게 알림 */
    if (rooms[roomIndex].hostIndex == clientIndex) {
        opponentIndex = rooms[roomIndex].guestIndex;
    } else {
        opponentIndex = rooms[roomIndex].hostIndex;
    }

    if (opponentIndex >= 0 && clients[opponentIndex].socket != INVALID_SOCK) {
        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_OPPONENT_LEFT;
        strcpy(msg.message, "상대방이 방을 나갔습니다.");
        net_send_message(clients[opponentIndex].socket, &msg);

        /* 상대방 상태 초기화 */
        clients[opponentIndex].inGame = 0;
        clients[opponentIndex].inRoom = 0;
        clients[opponentIndex].roomId = -1;
        clients[opponentIndex].opponentIndex = -1;
    }

    /* 클라이언트 상태 초기화 */
    clients[clientIndex].inRoom = 0;
    clients[clientIndex].roomId = -1;
    clients[clientIndex].inGame = 0;
    clients[clientIndex].opponentIndex = -1;

    /* 방 삭제 */
    rooms[roomIndex].active = 0;

    printStatus();
}

void startGame(int roomIndex) {
    NetMessage msg1, msg2;
    int hostIndex = rooms[roomIndex].hostIndex;
    int guestIndex = rooms[roomIndex].guestIndex;
    int i, j;

    /* 보드 초기화 */
    for (i = 0; i < BOARD_NET_SIZE; i++) {
        for (j = 0; j < BOARD_NET_SIZE; j++) {
            rooms[roomIndex].board[i][j] = 0;
        }
    }
    rooms[roomIndex].currentTurn = 1;
    rooms[roomIndex].moveCount = 0;
    rooms[roomIndex].inGame = 1;

    /* 플레이어 상태 설정 */
    clients[hostIndex].inGame = 1;
    clients[hostIndex].color = 1;  /* 방장이 흑 (선공) */
    clients[hostIndex].opponentIndex = guestIndex;

    clients[guestIndex].inGame = 1;
    clients[guestIndex].color = 2;  /* 참가자가 백 (후공) */
    clients[guestIndex].opponentIndex = hostIndex;

    /* 게임 시작 메시지 전송 */
    net_create_game_start_msg(&msg1, 1, clients[guestIndex].nickname);
    net_create_game_start_msg(&msg2, 2, clients[hostIndex].nickname);

    net_send_message(clients[hostIndex].socket, &msg1);
    net_send_message(clients[guestIndex].socket, &msg2);

    printf("[게임 시작] 방 #%d: %s(흑) vs %s(백)\n",
           rooms[roomIndex].roomId,
           clients[hostIndex].nickname,
           clients[guestIndex].nickname);
}

/* ========== 게임 관련 함수 ========== */

void handleMove(int clientIndex, NetMessage* msg) {
    int roomIndex;
    int x, y;
    int playerColor;
    int opponentIndex;
    NetMessage response;
    NetMessage moveMsg;

    roomIndex = findClientRoom(clientIndex);
    if (roomIndex < 0 || !rooms[roomIndex].inGame) {
        return;
    }

    playerColor = clients[clientIndex].color;
    opponentIndex = clients[clientIndex].opponentIndex;
    x = msg->x;
    y = msg->y;

    /* 턴 확인 */
    if (rooms[roomIndex].currentTurn != playerColor) {
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

    if (rooms[roomIndex].board[y][x] != 0) {
        memset(&response, 0, sizeof(response));
        response.type = MSG_ERROR;
        strcpy(response.message, "이미 돌이 있는 위치입니다.");
        net_send_message(clients[clientIndex].socket, &response);
        return;
    }

    /* 착수 */
    rooms[roomIndex].board[y][x] = playerColor;
    rooms[roomIndex].moveCount++;

    printf("[착수] 방 #%d: %s (%d, %d)\n",
           rooms[roomIndex].roomId, clients[clientIndex].nickname, x, y);

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
    if (net_check_win(rooms[roomIndex].board, x, y, playerColor)) {
        printf("[게임 종료] 방 #%d: %s 승리!\n",
               rooms[roomIndex].roomId, clients[clientIndex].nickname);
        endGame(roomIndex, playerColor);
        return;
    }

    /* 무승부 체크 */
    if (rooms[roomIndex].moveCount >= BOARD_NET_SIZE * BOARD_NET_SIZE) {
        printf("[게임 종료] 방 #%d: 무승부\n", rooms[roomIndex].roomId);
        endGame(roomIndex, 0);
        return;
    }

    /* 턴 변경 */
    rooms[roomIndex].currentTurn = (playerColor == 1) ? 2 : 1;
}

void endGame(int roomIndex, int winner) {
    NetMessage msg;
    int hostIndex = rooms[roomIndex].hostIndex;
    int guestIndex = rooms[roomIndex].guestIndex;
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

    if (hostIndex >= 0 && clients[hostIndex].socket != INVALID_SOCK) {
        net_send_message(clients[hostIndex].socket, &msg);
        clients[hostIndex].inGame = 0;
        clients[hostIndex].inRoom = 0;
        clients[hostIndex].roomId = -1;
        clients[hostIndex].opponentIndex = -1;
    }

    if (guestIndex >= 0 && clients[guestIndex].socket != INVALID_SOCK) {
        net_send_message(clients[guestIndex].socket, &msg);
        clients[guestIndex].inGame = 0;
        clients[guestIndex].inRoom = 0;
        clients[guestIndex].roomId = -1;
        clients[guestIndex].opponentIndex = -1;
    }

    /* 방 삭제 */
    rooms[roomIndex].active = 0;
    printf("[방 삭제] 방 #%d 게임 종료로 삭제\n", rooms[roomIndex].roomId);

    printStatus();
}

void printStatus(void) {
    int connectedCount = 0;
    int inRoomCount = 0;
    int activeRoomCount = 0;
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCK) {
            connectedCount++;
            if (clients[i].inRoom) {
                inRoomCount++;
            }
        }
    }

    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            activeRoomCount++;
        }
    }

    printf("[상태] 접속: %d명, 방 안: %d명, 활성 방: %d개\n",
           connectedCount, inRoomCount, activeRoomCount);
}
