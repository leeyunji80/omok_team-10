// Minimax 알고리즘 헤더 파일

#ifndef MINIMAX_H
#define MINIMAX_H

#define BOARD_SIZE 15
#define EMPTY 0
#define BLACK 1
#define WHITE 2

// 난이도 상수
#define EASY 0
#define MEDIUM 1
#define HARD 2

// 착수 위치 구조체
typedef struct {
    int row;
    int col;
} Move;

// 착수 결과 구조체
typedef struct {
    int score;
    int row;
    int col;
} MoveResult;

// 함수 선언
void initAI(void);      // AI 초기화 (Transposition Table, Zobrist 등)
void cleanupAI(void);   // AI 정리 (메모리 해제)

int checkWinBoard(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color);
int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor);
int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount);
MoveResult minimax(int board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, int isMaximizing, int aiColor);
Move findBestMove(int board[BOARD_SIZE][BOARD_SIZE], int aiColor, int difficulty);

#endif
