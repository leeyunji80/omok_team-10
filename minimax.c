// 오목 AI - 단순화된 Minimax + Alpha-Beta Pruning
// 핵심: 빠른 연산, 정확한 패턴 인식, 효과적인 방어/공격

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "minimax.h"

#define MAX_MOVES 60
#define INFINITY_SCORE 10000000

// 방향 벡터 (가로, 세로, 대각선 2개)
static const int DX[] = {1, 0, 1, 1};
static const int DY[] = {0, 1, 1, -1};

// 패턴 점수
typedef enum {
    SCORE_FIVE      = 1000000,   // 5목 (즉시 승리)
    SCORE_OPEN_FOUR = 100000,    // 열린 4 (막을 수 없음)
    SCORE_FOUR      = 15000,     // 닫힌 4 (한쪽 막힘)
    SCORE_OPEN_THREE= 5000,      // 열린 3
    SCORE_THREE     = 800,       // 닫힌 3
    SCORE_OPEN_TWO  = 300,       // 열린 2
    SCORE_TWO       = 50,        // 닫힌 2
    SCORE_ONE       = 10         // 1개
} PatternScore;

// 위치 가중치 (중앙 우선)
static int positionWeight[BOARD_SIZE][BOARD_SIZE];
static int initialized = 0;

// AI 초기화
void initAI(void) {
    if (initialized) return;

    srand((unsigned int)time(NULL));

    // 위치 가중치 초기화 (중앙이 높음)
    int center = BOARD_SIZE / 2;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int dist = abs(i - center) + abs(j - center);
            positionWeight[i][j] = (BOARD_SIZE - dist);
        }
    }

    initialized = 1;
}

// AI 정리
void cleanupAI(void) {
    initialized = 0;
}

// 승리 체크
int checkWinBoard(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) return 0;
    if (board[row][col] != color) return 0;

    for (int dir = 0; dir < 4; dir++) {
        int count = 1;

        // 정방향
        int nx = col + DX[dir];
        int ny = row + DY[dir];
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
            count++;
            nx += DX[dir];
            ny += DY[dir];
        }

        // 역방향
        nx = col - DX[dir];
        ny = row - DY[dir];
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
            count++;
            nx -= DX[dir];
            ny -= DY[dir];
        }

        if (count >= 5) return 1;
    }
    return 0;
}

// 한 방향 라인 분석 (연속 돌 수, 열린 끝 수)
static void analyzeLine(int board[BOARD_SIZE][BOARD_SIZE], int row, int col,
                        int dx, int dy, int color, int *count, int *openEnds) {
    *count = 1;
    *openEnds = 0;

    // 정방향
    int nx = col + dx;
    int ny = row + dy;
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
        (*count)++;
        nx += dx;
        ny += dy;
    }
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == EMPTY) {
        (*openEnds)++;
    }

    // 역방향
    nx = col - dx;
    ny = row - dy;
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
        (*count)++;
        nx -= dx;
        ny -= dy;
    }
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == EMPTY) {
        (*openEnds)++;
    }
}

// 특정 위치에 돌을 놓았을 때 점수 계산
static int evaluatePosition(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color) {
    if (board[row][col] != EMPTY) return 0;

    int score = 0;
    int fours = 0;      // 4목 개수
    int openThrees = 0; // 열린 3 개수

    board[row][col] = color;

    for (int dir = 0; dir < 4; dir++) {
        int count, openEnds;
        analyzeLine(board, row, col, DX[dir], DY[dir], color, &count, &openEnds);

        if (count >= 5) {
            score += SCORE_FIVE;
        } else if (count == 4) {
            if (openEnds == 2) {
                score += SCORE_OPEN_FOUR;  // 열린 4 = 승리 확정
            } else if (openEnds == 1) {
                score += SCORE_FOUR;
                fours++;
            }
        } else if (count == 3) {
            if (openEnds == 2) {
                score += SCORE_OPEN_THREE;
                openThrees++;
            } else if (openEnds == 1) {
                score += SCORE_THREE;
            }
        } else if (count == 2) {
            if (openEnds == 2) {
                score += SCORE_OPEN_TWO;
            } else if (openEnds == 1) {
                score += SCORE_TWO;
            }
        } else if (count == 1) {
            if (openEnds == 2) {
                score += SCORE_ONE * 2;
            } else if (openEnds == 1) {
                score += SCORE_ONE;
            }
        }
    }

    board[row][col] = EMPTY;

    // 쌍사 (4목 2개 이상) = 승리 확정
    if (fours >= 2) {
        score += SCORE_OPEN_FOUR;
    }

    // 사삼 (4목 + 열린3) = 승리 확정
    if (fours >= 1 && openThrees >= 1) {
        score += SCORE_OPEN_FOUR / 2;
    }

    // 쌍삼 (열린3 2개 이상) = 매우 유리
    if (openThrees >= 2) {
        score += SCORE_FOUR;
    }

    // 위치 가중치
    score += positionWeight[row][col];

    return score;
}

// 보드 전체 평가
int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor) {
    int score = 0;
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;

    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == EMPTY) continue;

            int color = board[row][col];
            int sign = (color == aiColor) ? 1 : -1;

            // 각 방향별 분석 (중복 방지: 시작점에서만)
            for (int dir = 0; dir < 4; dir++) {
                // 이전 칸에 같은 색 돌이 있으면 스킵 (중복 계산 방지)
                int px = col - DX[dir];
                int py = row - DY[dir];
                if (px >= 0 && px < BOARD_SIZE && py >= 0 && py < BOARD_SIZE) {
                    if (board[py][px] == color) continue;
                }

                int count, openEnds;
                analyzeLine(board, row, col, DX[dir], DY[dir], color, &count, &openEnds);

                int lineScore = 0;
                if (count >= 5) {
                    lineScore = SCORE_FIVE;
                } else if (count == 4) {
                    if (openEnds == 2) lineScore = SCORE_OPEN_FOUR;
                    else if (openEnds == 1) lineScore = SCORE_FOUR;
                } else if (count == 3) {
                    if (openEnds == 2) lineScore = SCORE_OPEN_THREE;
                    else if (openEnds == 1) lineScore = SCORE_THREE;
                } else if (count == 2) {
                    if (openEnds == 2) lineScore = SCORE_OPEN_TWO;
                    else if (openEnds == 1) lineScore = SCORE_TWO;
                }

                score += sign * lineScore;
            }

            // 위치 가중치
            score += sign * positionWeight[row][col];
        }
    }

    return score;
}

// 후보 수 구조체
typedef struct {
    int row;
    int col;
    int score;
} ScoredMove;

// 후보 수 비교 함수
static int compareMoves(const void *a, const void *b) {
    return ((ScoredMove*)b)->score - ((ScoredMove*)a)->score;
}

// 착수 가능한 위치 찾기
int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount) {
    int visited[BOARD_SIZE][BOARD_SIZE] = {0};
    ScoredMove candidates[225];
    int candidateCount = 0;
    int hasStone = 0;

    // 기존 돌 주변 2칸 이내만 탐색
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] != EMPTY) {
                hasStone = 1;
                for (int dr = -2; dr <= 2; dr++) {
                    for (int dc = -2; dc <= 2; dc++) {
                        int nr = row + dr;
                        int nc = col + dc;
                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                            if (board[nr][nc] == EMPTY && !visited[nr][nc]) {
                                visited[nr][nc] = 1;
                                candidates[candidateCount].row = nr;
                                candidates[candidateCount].col = nc;
                                // 간단한 우선순위 (중앙에 가까울수록)
                                candidates[candidateCount].score = positionWeight[nr][nc];
                                candidateCount++;
                            }
                        }
                    }
                }
            }
        }
    }

    // 보드가 비어있으면 중앙
    if (!hasStone) {
        moves[0].row = BOARD_SIZE / 2;
        moves[0].col = BOARD_SIZE / 2;
        return 1;
    }

    // 정렬
    qsort(candidates, candidateCount, sizeof(ScoredMove), compareMoves);

    // 최대 개수만큼 반환
    int returnCount = (candidateCount < maxCount) ? candidateCount : maxCount;
    for (int i = 0; i < returnCount; i++) {
        moves[i].row = candidates[i].row;
        moves[i].col = candidates[i].col;
    }

    return returnCount;
}

// Alpha-Beta Pruning Minimax
MoveResult minimax(int board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta,
                   int isMaximizing, int aiColor) {
    MoveResult result = {0, -1, -1};
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    int currentColor = isMaximizing ? aiColor : opponent;

    // 기저 조건: 깊이 0
    if (depth == 0) {
        result.score = evaluateBoard(board, aiColor);
        return result;
    }

    // 후보 수 가져오기
    Move moves[MAX_MOVES];
    int moveCount = getPossibleMoves(board, moves, MAX_MOVES);

    if (moveCount == 0) {
        result.score = evaluateBoard(board, aiColor);
        return result;
    }

    // 후보 수 점수 매기기 및 정렬 (move ordering)
    ScoredMove scoredMoves[MAX_MOVES];
    for (int i = 0; i < moveCount; i++) {
        scoredMoves[i].row = moves[i].row;
        scoredMoves[i].col = moves[i].col;
        // 공격/방어 점수 합산
        int attackScore = evaluatePosition(board, moves[i].row, moves[i].col, currentColor);
        int defenseScore = evaluatePosition(board, moves[i].row, moves[i].col,
                                           (currentColor == BLACK) ? WHITE : BLACK);
        scoredMoves[i].score = attackScore + defenseScore;
    }
    qsort(scoredMoves, moveCount, sizeof(ScoredMove), compareMoves);

    // 깊이에 따라 후보 수 제한 (성능 최적화)
    int maxMoves = moveCount;
    if (depth <= 2) maxMoves = (moveCount < 20) ? moveCount : 20;
    else if (depth <= 4) maxMoves = (moveCount < 15) ? moveCount : 15;
    else maxMoves = (moveCount < 10) ? moveCount : 10;

    result.row = scoredMoves[0].row;
    result.col = scoredMoves[0].col;

    if (isMaximizing) {
        result.score = -INFINITY_SCORE;

        for (int i = 0; i < maxMoves; i++) {
            int row = scoredMoves[i].row;
            int col = scoredMoves[i].col;

            board[row][col] = aiColor;

            // 승리 체크
            if (checkWinBoard(board, row, col, aiColor)) {
                board[row][col] = EMPTY;
                result.score = INFINITY_SCORE - (10 - depth);  // 빠른 승리 우선
                result.row = row;
                result.col = col;
                return result;
            }

            MoveResult child = minimax(board, depth - 1, alpha, beta, 0, aiColor);
            board[row][col] = EMPTY;

            if (child.score > result.score) {
                result.score = child.score;
                result.row = row;
                result.col = col;
            }

            if (result.score > alpha) {
                alpha = result.score;
            }

            if (beta <= alpha) {
                break;  // Pruning
            }
        }
    } else {
        result.score = INFINITY_SCORE;

        for (int i = 0; i < maxMoves; i++) {
            int row = scoredMoves[i].row;
            int col = scoredMoves[i].col;

            board[row][col] = opponent;

            // 상대 승리 체크
            if (checkWinBoard(board, row, col, opponent)) {
                board[row][col] = EMPTY;
                result.score = -INFINITY_SCORE + (10 - depth);
                result.row = row;
                result.col = col;
                return result;
            }

            MoveResult child = minimax(board, depth - 1, alpha, beta, 1, aiColor);
            board[row][col] = EMPTY;

            if (child.score < result.score) {
                result.score = child.score;
                result.row = row;
                result.col = col;
            }

            if (result.score < beta) {
                beta = result.score;
            }

            if (beta <= alpha) {
                break;  // Pruning
            }
        }
    }

    return result;
}

// AI 최적 착수 찾기
Move findBestMove(int board[BOARD_SIZE][BOARD_SIZE], int aiColor, int difficulty) {
    if (!initialized) {
        initAI();
    }

    int opponent = (aiColor == BLACK) ? WHITE : BLACK;

    // 후보 수 가져오기
    Move moves[MAX_MOVES];
    int moveCount = getPossibleMoves(board, moves, MAX_MOVES);

    if (moveCount == 0) {
        Move center = {BOARD_SIZE / 2, BOARD_SIZE / 2};
        return center;
    }

    // === 1단계: 즉시 승리 확인 ===
    for (int i = 0; i < moveCount; i++) {
        int score = evaluatePosition(board, moves[i].row, moves[i].col, aiColor);
        if (score >= SCORE_FIVE) {
            return moves[i];
        }
    }

    // === 2단계: 상대 즉시 승리 방어 ===
    for (int i = 0; i < moveCount; i++) {
        int score = evaluatePosition(board, moves[i].row, moves[i].col, opponent);
        if (score >= SCORE_FIVE) {
            return moves[i];
        }
    }

    // === 3단계: 승리 확정 수 (열린4, 쌍사) ===
    for (int i = 0; i < moveCount; i++) {
        int score = evaluatePosition(board, moves[i].row, moves[i].col, aiColor);
        if (score >= SCORE_OPEN_FOUR) {
            return moves[i];
        }
    }

    // === 4단계: 상대 승리 확정 방어 ===
    int bestDefenseIdx = -1;
    int bestDefenseScore = 0;
    for (int i = 0; i < moveCount; i++) {
        int score = evaluatePosition(board, moves[i].row, moves[i].col, opponent);
        if (score >= SCORE_OPEN_FOUR) {
            // 열린4 방어 필수
            return moves[i];
        }
        if (score > bestDefenseScore) {
            bestDefenseScore = score;
            bestDefenseIdx = i;
        }
    }

    // === 5단계: 닫힌4 방어 ===
    if (bestDefenseScore >= SCORE_FOUR) {
        // 방어하면서 공격도 가능한지 확인
        int defRow = moves[bestDefenseIdx].row;
        int defCol = moves[bestDefenseIdx].col;
        int defAttackScore = evaluatePosition(board, defRow, defCol, aiColor);

        // 더 좋은 공격 수가 있는지 확인
        for (int i = 0; i < moveCount; i++) {
            int attackScore = evaluatePosition(board, moves[i].row, moves[i].col, aiColor);
            if (attackScore >= SCORE_OPEN_FOUR) {
                // 공격이 더 좋으면 공격 우선 (상대가 막아야 함)
                return moves[i];
            }
        }

        return moves[bestDefenseIdx];
    }

    // === 6단계: 공격 우선 (열린3 이상) ===
    int bestAttackIdx = -1;
    int bestAttackScore = 0;
    for (int i = 0; i < moveCount; i++) {
        int score = evaluatePosition(board, moves[i].row, moves[i].col, aiColor);
        if (score > bestAttackScore) {
            bestAttackScore = score;
            bestAttackIdx = i;
        }
    }

    // 열린3 이상이면 공격
    if (bestAttackScore >= SCORE_OPEN_THREE && bestAttackIdx >= 0) {
        // 단, 상대 열린3 방어가 더 급하면 방어
        if (bestDefenseScore >= SCORE_OPEN_THREE && bestDefenseScore > bestAttackScore) {
            return moves[bestDefenseIdx];
        }
        return moves[bestAttackIdx];
    }

    // 상대 열린3 방어
    if (bestDefenseScore >= SCORE_OPEN_THREE && bestDefenseIdx >= 0) {
        return moves[bestDefenseIdx];
    }

    // === 7단계: Minimax 탐색 ===
    // 난이도별 깊이 설정
    int depth;
    switch (difficulty) {
        case EASY:
            depth = 2;
            // 30% 확률로 랜덤 선택
            if (rand() % 100 < 30) {
                int randIdx = rand() % ((moveCount < 5) ? moveCount : 5);
                return moves[randIdx];
            }
            break;
        case MEDIUM:
            depth = 4;
            break;
        case HARD:
        default:
            depth = 6;
            break;
    }

    MoveResult result = minimax(board, depth, -INFINITY_SCORE, INFINITY_SCORE, 1, aiColor);

    if (result.row >= 0 && result.col >= 0) {
        Move bestMove = {result.row, result.col};
        return bestMove;
    }

    // 예외 처리: 첫 번째 후보 반환
    return moves[0];
}
