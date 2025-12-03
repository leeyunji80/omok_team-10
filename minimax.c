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

// 특정 위치의 위협 점수 계산 (착수 전 빈 칸 상태에서 호출)
static int evaluatePosition(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color) {
    int score = 0;
    int open4 = 0, closed4 = 0, open3 = 0, closed3 = 0, open2 = 0;

    // 임시로 돌 배치
    board[row][col] = color;

    for (int dir = 0; dir < 4; dir++) {
        int count, openEnds;
        countLine(board, row, col, color, dir, &count, &openEnds);

        if (count >= 5) {
            board[row][col] = EMPTY;
            return 1000000; // 즉시 승리
        } else if (count == 4) {
            if (openEnds == 2) open4++;
            else if (openEnds == 1) closed4++;
        } else if (count == 3) {
            if (openEnds == 2) open3++;
            else if (openEnds == 1) closed3++;
        } else if (count == 2) {
            if (openEnds == 2) open2++;
        }
    }

    // 돌 제거
    board[row][col] = EMPTY;

    // 패턴 조합 점수
    if (open4 >= 1) score = 100000;           // 열린 4 = 승리 확정
    else if (closed4 >= 2) score = 100000;    // 쌍사 (닫힌4 2개) = 승리 확정
    else if (closed4 >= 1 && open3 >= 1) score = 100000; // 사삼 = 승리 확정
    else if (open3 >= 2) score = 50000;       // 쌍삼 = 매우 강력
    else if (closed4 >= 1) score = 10000;     // 닫힌 4
    else if (open3 >= 1) score = 5000;        // 열린 3
    else if (closed3 >= 2) score = 3000;      // 닫힌 3 두개
    else if (closed3 >= 1) score = 500;       // 닫힌 3
    else if (open2 >= 2) score = 300;         // 열린 2 두개
    else if (open2 >= 1) score = 100;         // 열린 2
    else score = 10;

    return score;
}

// 보드 상태 평가 함수
int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor) {
    int score = 0;

    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == EMPTY) continue;

            int color = board[row][col];
            int isAI = (color == aiColor);

            for (int dir = 0; dir < 4; dir++) {
                int count, openEnds;
                countLine(board, row, col, color, dir, &count, &openEnds);

                // 중복 계산 방지: 시작점에서만 계산
                int prevX = col - dx[dir];
                int prevY = row - dy[dir];
                if (prevX >= 0 && prevX < BOARD_SIZE && prevY >= 0 && prevY < BOARD_SIZE) {
                    if (board[prevY][prevX] == color) continue;
                }

                // 패턴별 점수 부여 (방어에 더 높은 가중치)
                int patternScore = 0;
                if (count >= 5) {
                    patternScore = 1000000;
                } else if (count == 4) {
                    if (openEnds == 2) patternScore = 100000;     // 열린 4
                    else if (openEnds == 1) patternScore = 15000; // 닫힌 4
                } else if (count == 3) {
                    if (openEnds == 2) patternScore = 10000;      // 열린 3
                    else if (openEnds == 1) patternScore = 1000;  // 닫힌 3
                } else if (count == 2) {
                    if (openEnds == 2) patternScore = 500;        // 열린 2
                    else if (openEnds == 1) patternScore = 50;    // 닫힌 2
                } else if (count == 1) {
                    if (openEnds == 2) patternScore = 20;         // 열린 1
                }

                // 상대 패턴은 1.2배 가중치로 방어 우선
                score += isAI ? patternScore : (int)(-patternScore * 1.2);
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

// 착수 후보들을 점수순으로 정렬하기 위한 구조체
typedef struct {
    int row;
    int col;
    int score;
} ScoredMove;

// 점수 비교 함수 (내림차순)
static int compareMoves(const void* a, const void* b) {
    return ((ScoredMove*)b)->score - ((ScoredMove*)a)->score;
}

// AI의 최적 착수 찾기
Move findBestMove(int board[BOARD_SIZE][BOARD_SIZE], int aiColor, int difficulty) {
    // 난이도별 깊이 설정 (0: easy, 1: medium, 2: hard)
    int depthMap[] = { 2, 4, 6 };
    int depth = depthMap[difficulty];

    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    Move possibleMoves[MAX_MOVES];
    int moveCount = getPossibleMoves(board, possibleMoves, 50);
    Move bestMove = { -1, -1 };

    // 후보가 없으면 중앙 반환
    if (moveCount == 0) {
        bestMove.row = BOARD_SIZE / 2;
        bestMove.col = BOARD_SIZE / 2;
        return bestMove;
    }

    // Easy 난이도: 30% 확률로 랜덤 실수
    if (difficulty == EASY && (rand() % 100) < 30) {
        int randomIndex = rand() % ((moveCount < 5) ? moveCount : 5);
        return possibleMoves[randomIndex];
    }

    // 각 후보 위치의 공격/방어 점수 계산
    ScoredMove scoredMoves[MAX_MOVES];
    int bestAttackScore = 0, bestDefenseScore = 0;
    int bestAttackIdx = -1, bestDefenseIdx = -1;

    for (int i = 0; i < moveCount; i++) {
        int row = possibleMoves[i].row;
        int col = possibleMoves[i].col;

        // 공격 점수 (AI가 여기 두면 얼마나 좋은가)
        int attackScore = evaluatePosition(board, row, col, aiColor);
        // 방어 점수 (상대가 여기 두면 얼마나 위험한가)
        int defenseScore = evaluatePosition(board, row, col, opponent);

        // 즉시 승리 가능하면 바로 반환
        if (attackScore >= 1000000) {
            bestMove.row = row;
            bestMove.col = col;
            return bestMove;
        }

        // 상대 즉시 승리 막기
        if (defenseScore >= 1000000) {
            if (bestDefenseIdx == -1) {
                bestDefenseIdx = i;
                bestDefenseScore = defenseScore;
            }
        }

        // 최고 공격 수 추적
        if (attackScore > bestAttackScore) {
            bestAttackScore = attackScore;
            bestAttackIdx = i;
        }
        // 최고 방어 수 추적
        if (defenseScore > bestDefenseScore) {
            bestDefenseScore = defenseScore;
            bestDefenseIdx = i;
        }

        // 종합 점수 (공격 + 방어 * 1.1)
        scoredMoves[i].row = row;
        scoredMoves[i].col = col;
        scoredMoves[i].score = attackScore + (int)(defenseScore * 1.1);
    }

    // 우선순위 1: 상대가 즉시 이길 수 있는 곳(5목) 막기
    if (bestDefenseScore >= 1000000 && bestDefenseIdx >= 0) {
        bestMove.row = possibleMoves[bestDefenseIdx].row;
        bestMove.col = possibleMoves[bestDefenseIdx].col;
        return bestMove;
    }

    // 우선순위 2: 내가 승리 확정 수가 있으면 (열린4, 쌍사, 사삼)
    if (bestAttackScore >= 100000 && bestAttackIdx >= 0) {
        bestMove.row = possibleMoves[bestAttackIdx].row;
        bestMove.col = possibleMoves[bestAttackIdx].col;
        return bestMove;
    }

    // 우선순위 3: 상대가 승리 확정 수를 만들 수 있으면 막기 (열린4, 쌍사, 사삼)
    if (bestDefenseScore >= 100000 && bestDefenseIdx >= 0) {
        bestMove.row = possibleMoves[bestDefenseIdx].row;
        bestMove.col = possibleMoves[bestDefenseIdx].col;
        return bestMove;
    }

    // 우선순위 4: 상대 닫힌4(10000점) 막기 - 한 수 안에 5목 가능
    if (bestDefenseScore >= 10000 && bestDefenseIdx >= 0) {
        bestMove.row = possibleMoves[bestDefenseIdx].row;
        bestMove.col = possibleMoves[bestDefenseIdx].col;
        return bestMove;
    }

    // 우선순위 5: 내 쌍삼(50000점) 만들기
    if (bestAttackScore >= 50000 && bestAttackIdx >= 0) {
        bestMove.row = possibleMoves[bestAttackIdx].row;
        bestMove.col = possibleMoves[bestAttackIdx].col;
        return bestMove;
    }

    // 우선순위 6: 상대 열린3 등 위협 막기
    if (bestDefenseScore >= 5000 && bestDefenseIdx >= 0) {
        bestMove.row = possibleMoves[bestDefenseIdx].row;
        bestMove.col = possibleMoves[bestDefenseIdx].col;
        return bestMove;
    }

    // 점수순 정렬 후 상위 후보만 Minimax 탐색
    qsort(scoredMoves, moveCount, sizeof(ScoredMove), compareMoves);

    // 상위 N개만 Minimax로 심층 탐색
    int searchCount = (moveCount < 15) ? moveCount : 15;
    int maxScore = INT_MIN;

    for (int i = 0; i < searchCount; i++) {
        int row = scoredMoves[i].row;
        int col = scoredMoves[i].col;

        board[row][col] = aiColor;
        MoveResult result = minimax(board, depth - 1, INT_MIN, INT_MAX, 0, aiColor);
        board[row][col] = EMPTY;

        // 휴리스틱 점수 가산
        int totalScore = result.score + scoredMoves[i].score / 10;

        if (totalScore > maxScore) {
            maxScore = totalScore;
            bestMove.row = row;
            bestMove.col = col;
        }
    }

    return bestMove;
}
