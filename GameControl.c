#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include "minimax.h"

#define SIZE 15

int board[SIZE][SIZE];
int cursorX = 0, cursorY = 0;
int currentPlayer = BLACK;
int gameMode = 0; // 1=1인용, 2=2인용
int difficulty = MEDIUM; // AI 난이도 (기본: 중간)
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

// AI 착수 (Minimax 알고리즘)
void aiMove() {
    Move bestMove = findBestMove(board, WHITE, difficulty);
    if (bestMove.row >= 0 && bestMove.col >= 0) {
        placeStone(bestMove.col, bestMove.row);
    }
}

// 승리 체크
int checkWin(int x, int y) {
    int dx[] = { 1,0,1,1 };
    int dy[] = { 0,1,1,-1 };
    int player = board[y][x];

    for (int dir = 0; dir < 4; dir++) {
        int count = 1;
        int nx = x + dx[dir], ny = y + dy[dir];
        while (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && board[ny][nx] == player) {
            count++; nx += dx[dir]; ny += dy[dir];
        }
        nx = x - dx[dir]; ny = y - dy[dir];
        while (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && board[ny][nx] == player) {
            count++; nx -= dx[dir]; ny -= dy[dir];
        }
        if (count >= 5) return player;
    }
    return 0;
}

// 메뉴 화면
void showMenu() {
    int choice;
    clearScreen();
    printf("========== 메뉴 ==========\n");
    printf("1. 게임 저장\n");
    printf("2. 게임 불러오기\n");
    printf("3. 종료\n");
    printf("---------------------------\n");
    printf("원하는 번호를 입력하세요 : ");
    scanf("%d", &choice);

    switch (choice) {
    case 1: printf("게임 저장 기능 선택\n"); break;
    case 2: printf("게임 불러오기 기능 선택\n"); break;
    case 3: printf("게임 종료 선택\n"); exit(0); break;
    default: printf("잘못된 선택입니다.\n"); break;
    }
    printf("아무 키나 누르면 메뉴를 닫습니다...\n");
    _getch();
}

// 메인 게임 루프
void gameLoop() {
    printBoard();
    int key;

    while (1) {
        if (gameMode == 1 && currentPlayer == WHITE) { // AI 차례
            aiMove();
            printBoard();
            if (checkWin(lastMoveX, lastMoveY) == 2) {
                printf("백돌 승리!\n");
                break;
            }
            continue;
        }

        key = _getch();
        if (key == 'w' || key == 'a' || key == 's' || key == 'd' || key == 'W' || key == 'A' || key == 'S' || key == 'D') {
            moveCursor(key);
        }
        else if (key == 'b' || key == 'B') {
            if (placeStone(cursorX, cursorY)) {
                printBoard();
                int winner = checkWin(lastMoveX, lastMoveY);
                if (winner != 0) {
                    printf("%s 승리!\n", (winner == BLACK) ? "흑" : "백");
                    break;
                }
            }
        }
        else if (key == 'm' || key == 'M') {
            showMenu();
        }

        printBoard();
    }
}

int main() {
    srand((unsigned int)time(NULL));
    initBoard();

    printf("\n=========시작화면=======\n");
    printf("1. 1인용 게임 \n");
    printf("2. 2인용 게임 \n");
    printf("3. 게임 불러오기 \n");
    printf("4. 랭킹 확인하기(1인용) \n");
    printf("5. 종료\n");
    printf("============================\n");
    printf("메뉴 번호를 입력하세요. (1~5): ");
    scanf("%d", &gameMode);

    if(gameMode == 1){
        // 난이도 선택
        int diffChoice;
        printf("\n======= 난이도 선택 =======\n");
        printf("1. 쉬움 (Easy)\n");
        printf("2. 보통 (Medium)\n");
        printf("3. 어려움 (Hard)\n");
        printf("===========================\n");
        printf("난이도를 선택하세요 (1~3): ");
        scanf("%d", &diffChoice);

        switch(diffChoice) {
            case 1: difficulty = EASY; break;
            case 2: difficulty = MEDIUM; break;
            case 3: difficulty = HARD; break;
            default: difficulty = MEDIUM; break;
        }

        gameLoop();
    }
    else if(gameMode == 2){
        gameLoop();
    }
    else if(gameMode == 3){
        printf("게임 불러오기 화면으로 이동합니다..");
    }
    else if(gameMode == 4){
        printf("랭킹 확인 화면으로 이동합니다...");
    }
    else if(gameMode == 5){
        printf("프로그램을 종료합니다...");
        return 0;
    }


    printf("\n게임이 종료되었습니다. 아무 키나 누르면 콘솔이 닫힙니다...\n");
    _getch(); 

    return 0;
}