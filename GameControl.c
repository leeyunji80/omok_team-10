#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#define SIZE 15
#define BLACK 1
#define WHITE 2
#define EMPTY 0

int board[SIZE][SIZE];
int cursorX = 0, cursorY = 0;
int currentPlayer = BLACK;
int lastMoveX = -1, lastMoveY = -1;
void clearScreen() {
    system("cls");
}

void initBoard() {
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            board[y][x] = EMPTY;
    cursorX = 0;
    cursorY = 0;
}

// 보드 출력
void printBoard() {
    clearScreen();
    printf("                                        메뉴 M키\n\n");

    printf("   ");
    for (int i = 1; i <= SIZE; i++)
        printf("%2d ", i);
    printf("\n  +");
    for (int i = 0; i < SIZE * 3; i++) printf("-");
    printf("+\n");

    for (int y = 0; y < SIZE; y++) {
        printf("%c |", 'A' + y);
        for (int x = 0; x < SIZE; x++) {
            if (y == cursorY && x == cursorX) {
                if (board[y][x] == BLACK) printf("[X]");
                else if (board[y][x] == WHITE) printf("[O]");
                else printf("[ ]");
            }
            else {
                if (board[y][x] == BLACK) printf(" X ");
                else if (board[y][x] == WHITE) printf(" O ");
                else printf(" . ");
            }
        }
        printf("|\n");
    }

    printf("  +");
    for (int i = 0; i < SIZE * 3; i++) printf("-");
    printf("+\n");

    printf("흑돌: X  백돌: O\t현재 차례: %s\n", (currentPlayer == BLACK) ? "흑" : "백");
    printf("착수: B키\n");
}

// 커서 이동 (WASD)
void moveCursor(char key) {
    switch (key) {
    case 'w': case 'W': if (cursorY > 0) cursorY--; break;
    case 's': case 'S': if (cursorY < SIZE - 1) cursorY++; break;
    case 'a': case 'A': if (cursorX > 0) cursorX--; break;
    case 'd': case 'D': if (cursorX < SIZE - 1) cursorX++; break;
    }
}

// 착수 처리
int placeStone(int x, int y) {
    if (board[y][x] != EMPTY) {
        printf("이미 돌이 존재합니다!\n");
        Sleep(800);
        return 0;
    }
    board[y][x] = currentPlayer;
    lastMoveX = x;
    lastMoveY = y;
    currentPlayer = (currentPlayer == BLACK) ? WHITE : BLACK;
    return 1;
}
