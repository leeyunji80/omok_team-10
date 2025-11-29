// 게임 로직 헤더 파일
#ifndef GAMELOGIC_H
#define GAMELOGIC_H

#define BOARD_SIZE 15
#define EMPTY 0
#define BLACK 1
#define WHITE 2

// 착수 위치 구조체
typedef struct {
    int row;
    int col;
} Move;

// 함수 선언
int checkWin(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color);
int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor);
int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount);

#endif
