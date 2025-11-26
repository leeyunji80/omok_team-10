// Minimax 알고리즘 기반 오목 AI (C 버전)

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#define BOARD_SIZE 15
#define EMPTY 0
#define BLACK 1
#define WHITE 2
#define MAX_MOVES 225

// 착수 결과 구조체
typedef struct {
    int score;
    int row;
    int col;
} MoveResult;

// 가능한 수 구조체
typedef struct {
    int row;
    int col;
} Move;

// 외부 함수 선언 (gameLogic.c에서 구현 필요)
extern int checkWin(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color);
extern int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor);
extern int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount);

// Alpha-Beta Pruning을 적용한 Minimax 알고리즘
MoveResult minimax(int board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, int isMaximizing, int aiColor) {
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    MoveResult result = { 0, -1, -1 };

    // 깊이가 0이면 보드 평가 점수 반환
    if (depth == 0) {
        result.score = evaluateBoard(board, aiColor);
        return result;
    }

    // 탐색 후보 수 가져오기
    Move possibleMoves[MAX_MOVES];
    int moveCount = getPossibleMoves(board, possibleMoves, (depth > 2) ? 10 : 15);

    if (moveCount == 0) {
        result.score = evaluateBoard(board, aiColor);
        return result;
    }

    if (isMaximizing) {
        int maxScore = INT_MIN;
        int bestRow = -1, bestCol = -1;

        for (int i = 0; i < moveCount; i++) {
            int row = possibleMoves[i].row;
            int col = possibleMoves[i].col;

            // 착수
            board[row][col] = aiColor;

            // 즉시 승리하는 수인지 확인
            if (checkWin(board, row, col, aiColor)) {
                board[row][col] = EMPTY;
                result.score = 1000000;
                result.row = row;
                result.col = col;
                return result;
            }

            // 재귀 호출
            MoveResult childResult = minimax(board, depth - 1, alpha, beta, 0, aiColor);

            // 착수 취소
            board[row][col] = EMPTY;

            if (childResult.score > maxScore) {
                maxScore = childResult.score;
                bestRow = row;
                bestCol = col;
            }

            if (maxScore > alpha) {
                alpha = maxScore;
            }
            if (beta <= alpha) {
                break; // Beta cutoff
            }
        }

        result.score = maxScore;
        result.row = bestRow;
        result.col = bestCol;
        return result;

    } else {
        int minScore = INT_MAX;
        int bestRow = -1, bestCol = -1;

        for (int i = 0; i < moveCount; i++) {
            int row = possibleMoves[i].row;
            int col = possibleMoves[i].col;

            // 착수
            board[row][col] = opponent;

            // 상대가 즉시 승리하는 수인지 확인
            if (checkWin(board, row, col, opponent)) {
                board[row][col] = EMPTY;
                result.score = -1000000;
                result.row = row;
                result.col = col;
                return result;
            }

            // 재귀 호출
            MoveResult childResult = minimax(board, depth - 1, alpha, beta, 1, aiColor);

            // 착수 취소
            board[row][col] = EMPTY;

            if (childResult.score < minScore) {
                minScore = childResult.score;
                bestRow = row;
                bestCol = col;
            }

            if (minScore < beta) {
                beta = minScore;
            }
            if (beta <= alpha) {
                break; // Alpha cutoff
            }
        }

        result.score = minScore;
        result.row = bestRow;
        result.col = bestCol;
        return result;
    }
}

// AI의 최적 착수 찾기
Move findBestMove(int board[BOARD_SIZE][BOARD_SIZE], int aiColor, int difficulty) {
    // 난이도별 깊이 설정 (0: easy, 1: medium, 2: hard)
    int depthMap[] = { 2, 3, 4 };
    int depth = depthMap[difficulty];

    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    Move possibleMoves[MAX_MOVES];
    int moveCount = getPossibleMoves(board, possibleMoves, 20);
    Move bestMove = { -1, -1 };

    // 상대의 4목을 막아야 하는 경우 찾기
    for (int i = 0; i < moveCount; i++) {
        int row = possibleMoves[i].row;
        int col = possibleMoves[i].col;

        board[row][col] = opponent;
        if (checkWin(board, row, col, opponent)) {
            board[row][col] = EMPTY;
            bestMove.row = row;
            bestMove.col = col;
            return bestMove; // 즉시 막기
        }
        board[row][col] = EMPTY;
    }

    // AI가 즉시 이길 수 있는 경우 찾기
    for (int i = 0; i < moveCount; i++) {
        int row = possibleMoves[i].row;
        int col = possibleMoves[i].col;

        board[row][col] = aiColor;
        if (checkWin(board, row, col, aiColor)) {
            board[row][col] = EMPTY;
            bestMove.row = row;
            bestMove.col = col;
            return bestMove; // 즉시 승리
        }
        board[row][col] = EMPTY;
    }

    // Minimax 알고리즘으로 최적 수 찾기
    MoveResult result = minimax(board, depth, INT_MIN, INT_MAX, 1, aiColor);

    // Easy 난이도: 30% 확률로 랜덤 실수
    if (difficulty == 0 && (rand() % 100) < 30) {
        int randomIndex = rand() % ((moveCount < 5) ? moveCount : 5);
        return possibleMoves[randomIndex];
    }

    bestMove.row = result.row;
    bestMove.col = result.col;
    return bestMove;
}
