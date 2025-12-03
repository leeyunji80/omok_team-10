// Minimax 알고리즘 기반 오목 AI (C 버전)

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include "minimax.h"

#define MAX_MOVES 225

// 방향 벡터 (가로, 세로, 대각선 2개)
static const int dx[] = { 1, 0, 1, 1 };
static const int dy[] = { 0, 1, 1, -1 };

// 보드에서 특정 위치에 착수 후 승리 여부 확인
int checkWinBoard(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color) {
    if (board[row][col] != color) return 0;

    for (int dir = 0; dir < 4; dir++) {
        int count = 1;

        // 정방향 탐색
        int nx = col + dx[dir];
        int ny = row + dy[dir];
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
            count++;
            nx += dx[dir];
            ny += dy[dir];
        }

        // 역방향 탐색
        nx = col - dx[dir];
        ny = row - dy[dir];
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
            count++;
            nx -= dx[dir];
            ny -= dy[dir];
        }

        if (count >= 5) return 1;
    }
    return 0;
}

// 특정 방향으로 연속된 돌 개수와 열린 끝 개수 계산
static void countLine(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color, int dir, int* count, int* openEnds) {
    *count = 1;
    *openEnds = 0;

    // 정방향 탐색
    int nx = col + dx[dir];
    int ny = row + dy[dir];
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
        (*count)++;
        nx += dx[dir];
        ny += dy[dir];
    }
    // 정방향 끝이 비어있는지 확인
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == EMPTY) {
        (*openEnds)++;
    }

    // 역방향 탐색
    nx = col - dx[dir];
    ny = row - dy[dir];
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
        (*count)++;
        nx -= dx[dir];
        ny -= dy[dir];
    }
    // 역방향 끝이 비어있는지 확인
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == EMPTY) {
        (*openEnds)++;
    }
}

// 보드 상태 평가 함수
int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor) {
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    int score = 0;

    // 패턴 점수 정의
    // 5목: 100000, 열린4: 10000, 닫힌4: 1000, 열린3: 1000, 닫힌3: 100, 열린2: 100, 닫힌2: 10

    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == EMPTY) continue;

            int color = board[row][col];
            int multiplier = (color == aiColor) ? 1 : -1;

            for (int dir = 0; dir < 4; dir++) {
                int count, openEnds;
                countLine(board, row, col, color, dir, &count, &openEnds);

                // 중복 계산 방지: 시작점에서만 계산
                int prevX = col - dx[dir];
                int prevY = row - dy[dir];
                if (prevX >= 0 && prevX < BOARD_SIZE && prevY >= 0 && prevY < BOARD_SIZE) {
                    if (board[prevY][prevX] == color) continue;
                }

                // 패턴별 점수 부여
                if (count >= 5) {
                    score += 100000 * multiplier;
                } else if (count == 4) {
                    if (openEnds == 2) score += 10000 * multiplier;      // 열린 4
                    else if (openEnds == 1) score += 1000 * multiplier;  // 닫힌 4
                } else if (count == 3) {
                    if (openEnds == 2) score += 1000 * multiplier;       // 열린 3
                    else if (openEnds == 1) score += 100 * multiplier;   // 닫힌 3
                } else if (count == 2) {
                    if (openEnds == 2) score += 100 * multiplier;        // 열린 2
                    else if (openEnds == 1) score += 10 * multiplier;    // 닫힌 2
                }
            }
        }
    }

    return score;
}

// 착수 가능한 위치 찾기 (기존 돌 주변만 탐색)
int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount) {
    int visited[BOARD_SIZE][BOARD_SIZE] = {0};
    int moveCount = 0;
    int hasStone = 0;

    // 기존 돌 주변 2칸 이내의 빈 칸을 후보로 추가
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] != EMPTY) {
                hasStone = 1;
                // 주변 2칸 탐색
                for (int dr = -2; dr <= 2; dr++) {
                    for (int dc = -2; dc <= 2; dc++) {
                        int nr = row + dr;
                        int nc = col + dc;
                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                            if (board[nr][nc] == EMPTY && !visited[nr][nc]) {
                                visited[nr][nc] = 1;
                                if (moveCount < maxCount) {
                                    moves[moveCount].row = nr;
                                    moves[moveCount].col = nc;
                                    moveCount++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 보드가 비어있으면 중앙에 착수
    if (!hasStone) {
        moves[0].row = BOARD_SIZE / 2;
        moves[0].col = BOARD_SIZE / 2;
        return 1;
    }

    return moveCount;
}

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
            if (checkWinBoard(board, row, col, aiColor)) {
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
            if (checkWinBoard(board, row, col, opponent)) {
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
        if (checkWinBoard(board, row, col, opponent)) {
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
        if (checkWinBoard(board, row, col, aiColor)) {
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
