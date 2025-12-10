#ifndef NETWORK_H
#define NETWORK_H

/*
 * 오목 게임 네트워크 모듈
 * Windows / macOS / Linux 크로스 플랫폼 지원
 */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 플랫폼별 소켓 헤더 */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_TYPE SOCKET
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCKET_ERR SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <netdb.h>
    #define CLOSE_SOCKET close
    #define SOCKET_TYPE int
    #define INVALID_SOCK -1
    #define SOCKET_ERR -1
#endif

/* 상수 정의 */
#define DEFAULT_PORT 9999
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 20
#define MAX_ROOMS 10
#define BOARD_NET_SIZE 15
#define ROOM_NAME_LEN 32

/* 메시지 타입 */
typedef enum {
    MSG_CONNECT = 1,        /* 서버 접속 요청 */
    MSG_CONNECT_ACK = 2,    /* 접속 승인 */
    MSG_DISCONNECT = 3,     /* 연결 종료 */

    /* 방 관련 메시지 */
    MSG_ROOM_CREATE = 10,   /* 방 생성 요청 */
    MSG_ROOM_CREATE_ACK = 11, /* 방 생성 완료 */
    MSG_ROOM_LIST = 12,     /* 방 목록 요청 */
    MSG_ROOM_LIST_RESP = 13, /* 방 목록 응답 */
    MSG_ROOM_JOIN = 14,     /* 방 입장 요청 */
    MSG_ROOM_JOIN_ACK = 15, /* 방 입장 승인 */
    MSG_ROOM_LEAVE = 16,    /* 방 퇴장 */
    MSG_ROOM_FULL = 17,     /* 방 가득 참 */
    MSG_ROOM_NOT_FOUND = 18, /* 방 없음 */

    MSG_GAME_START = 20,    /* 게임 시작 */
    MSG_MOVE = 21,          /* 착수 */
    MSG_MOVE_ACK = 22,      /* 착수 확인 */
    MSG_GAME_END = 23,      /* 게임 종료 */
    MSG_OPPONENT_LEFT = 24, /* 상대방 퇴장 */

    MSG_CHAT = 30,          /* 채팅 */
    MSG_PING = 40,          /* 핑 */
    MSG_PONG = 41,          /* 퐁 */
    MSG_ERROR = 99          /* 오류 */
} MessageType;

/* 게임 결과 */
typedef enum {
    RESULT_NONE = 0,
    RESULT_BLACK_WIN = 1,
    RESULT_WHITE_WIN = 2,
    RESULT_DRAW = 3,
    RESULT_DISCONNECT = 4
} GameResult;

/* 방 정보 구조체 */
typedef struct {
    int roomId;
    char roomName[ROOM_NAME_LEN];
    char hostName[50];
    int playerCount;        /* 1 또는 2 */
    int inGame;             /* 게임 진행 중 여부 */
} RoomInfo;

/* 네트워크 메시지 구조체 */
typedef struct {
    int type;               /* MessageType */
    int x;                  /* 착수 x좌표 / roomId */
    int y;                  /* 착수 y좌표 / roomCount */
    int player;             /* 플레이어 색상 (1=흑, 2=백) */
    int result;             /* 게임 결과 */
    char nickname[50];      /* 닉네임 / 방 이름 */
    char message[256];      /* 추가 메시지 */
    RoomInfo rooms[MAX_ROOMS]; /* 방 목록 (MSG_ROOM_LIST_RESP용) */
} NetMessage;

/* 클라이언트 정보 (서버용) */
typedef struct {
    SOCKET_TYPE socket;
    char nickname[50];
    int inRoom;             /* 방에 있는지 */
    int roomId;             /* 속한 방 ID */
    int inGame;
    int color;              /* 1=흑, 2=백 */
    int opponentIndex;      /* 상대방 클라이언트 인덱스 */
} ClientInfo;

/* 게임 방 (서버용) */
typedef struct {
    int active;
    int roomId;
    char roomName[ROOM_NAME_LEN];
    int hostIndex;          /* 방장 클라이언트 인덱스 */
    int guestIndex;         /* 참가자 클라이언트 인덱스 */
    int inGame;
    int board[BOARD_NET_SIZE][BOARD_NET_SIZE];
    int currentTurn;        /* 1=흑, 2=백 */
    int moveCount;
} GameRoom;

/* ========== 함수 선언 ========== */

/* 네트워크 초기화/정리 */
int net_init(void);
void net_cleanup(void);

/* 소켓 유틸리티 */
SOCKET_TYPE net_create_socket(void);
int net_set_nonblocking(SOCKET_TYPE sock);
int net_set_blocking(SOCKET_TYPE sock);

/* 서버 함수 */
SOCKET_TYPE net_create_server(int port);
SOCKET_TYPE net_accept_client(SOCKET_TYPE serverSocket, struct sockaddr_in* clientAddr);

/* 클라이언트 함수 */
SOCKET_TYPE net_connect_to_server(const char* ip, int port);

/* 메시지 송수신 */
int net_send_message(SOCKET_TYPE sock, const NetMessage* msg);
int net_recv_message(SOCKET_TYPE sock, NetMessage* msg);
int net_recv_message_nonblock(SOCKET_TYPE sock, NetMessage* msg);

/* 메시지 생성 헬퍼 */
void net_create_connect_msg(NetMessage* msg, const char* nickname);
void net_create_move_msg(NetMessage* msg, int x, int y, int player);
void net_create_game_start_msg(NetMessage* msg, int yourColor, const char* opponentNick);
void net_create_game_end_msg(NetMessage* msg, int result);

/* 승리 체크 (서버용) */
int net_check_win(int board[BOARD_NET_SIZE][BOARD_NET_SIZE], int x, int y, int player);

#endif /* NETWORK_H */
