// 게임 로직 및 AI 평가 함수
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define BOARD_SIZE 15
#define EMPTY 0
#define BLACK 1
#define WHITE 2

// 착수 위치 구조체
typedef struct {
    int row;
    int col;
} Move;

// 승리 조건 체크 함수 (5목 이상 연속)
int checkWin(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color) {
    int dx[] = { 1, 0, 1, 1 };
    int dy[] = { 0, 1, 1, -1 };

    for (int dir = 0; dir < 4; dir++) {
        int count = 1;
        int nx = col + dx[dir], ny = row + dy[dir];
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
            count++;
            nx += dx[dir];
            ny += dy[dir];
        }
        nx = col - dx[dir];
        ny = row - dy[dir];
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
            count++;
            nx -= dx[dir];
            ny -= dy[dir];
        }
        if (count >= 5) return color;
    }
    return 0;
}

// 특정 방향으로 연속된 돌의 개수를 세는 함수
int countLine(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int dx, int dy, int color) {
    int count = 0;
    int r = row, c = col;

    // 한 방향으로 세기
    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && board[r][c] == color) {
        count++;
        r += dx;
        c += dy;
    }

    return count;
}

// 특정 위치에서 특정 방향으로의 패턴 평가
int evaluateLine(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int dx, int dy, int color) {
    int count = 1; // 현재 위치 포함
    int openEnds = 0;

    // 정방향으로 카운트
    int r = row + dx, c = col + dy;
    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && board[r][c] == color) {
        count++;
        r += dx;
        c += dy;
    }
    if (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && board[r][c] == EMPTY) {
        openEnds++;
    }

    // 역방향으로 카운트
    r = row - dx; c = col - dy;
    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && board[r][c] == color) {
        count++;
        r -= dx;
        c -= dy;
    }
    if (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && board[r][c] == EMPTY) {
        openEnds++;
    }

    // 패턴에 따른 점수 부여
    if (count >= 5) return 100000; // 5목
    if (count == 4) {
        if (openEnds == 2) return 10000; // 열린 4목
        if (openEnds == 1) return 1000;  // 닫힌 4목
    }
    if (count == 3) {
        if (openEnds == 2) return 500;   // 열린 3목
        if (openEnds == 1) return 100;   // 닫힌 3목
    }
    if (count == 2) {
        if (openEnds == 2) return 50;    // 열린 2목
        if (openEnds == 1) return 10;    // 닫힌 2목
    }

    return 0;
}

// 보드 전체를 평가하여 점수 반환
int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor) {
    int score = 0;
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;

    // 네 방향: 가로, 세로, 대각선(\), 대각선(/)
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

    // 모든 돌이 놓인 위치를 평가
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == aiColor) {
                // AI 돌의 패턴 평가 (양수 점수)
                for (int dir = 0; dir < 4; dir++) {
                    score += evaluateLine(board, row, col, directions[dir][0], directions[dir][1], aiColor);
                }
            } else if (board[row][col] == opponent) {
                // 상대 돌의 패턴 평가 (음수 점수)
                for (int dir = 0; dir < 4; dir++) {
                    score -= evaluateLine(board, row, col, directions[dir][0], directions[dir][1], opponent);
                }
            }
        }
    }

    return score;
}

// 거리 계산 함수 (맨해튼 거리)
int manhattanDistance(int r1, int c1, int r2, int c2) {
    int dr = (r1 > r2) ? (r1 - r2) : (r2 - r1);
    int dc = (c1 > c2) ? (c1 - c2) : (c2 - c1);
    return dr + dc;
}

// 후보 수 구조체 (우선순위 포함)
typedef struct {
    int row;
    int col;
    int priority;
} CandidateMove;

// 후보 수 정렬 함수 (내림차순)
int compareCandidates(const void* a, const void* b) {
    CandidateMove* moveA = (CandidateMove*)a;
    CandidateMove* moveB = (CandidateMove*)b;
    return moveB->priority - moveA->priority;
}

// 보드에서 착수 가능한 후보 위치들을 생성
int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount) {
    CandidateMove candidates[BOARD_SIZE * BOARD_SIZE];
    int candidateCount = 0;
    int hasStone = 0;

    // 보드에 돌이 있는지 확인
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] != EMPTY) {
                hasStone = 1;
                break;
            }
        }
        if (hasStone) break;
    }

    // 보드가 비어있으면 중앙 위치 반환
    if (!hasStone) {
        moves[0].row = BOARD_SIZE / 2;
        moves[0].col = BOARD_SIZE / 2;
        return 1;
    }

    // 각 빈 칸에 대해 우선순위 계산
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == EMPTY) {
                int priority = 0;
                int hasNearbyStone = 0;

                // 주변 2칸 이내에 돌이 있는지 확인
                for (int dr = -2; dr <= 2; dr++) {
                    for (int dc = -2; dc <= 2; dc++) {
                        int r = row + dr;
                        int c = col + dc;

                        if (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE) {
                            if (board[r][c] != EMPTY) {
                                hasNearbyStone = 1;
                                // 거리가 가까울수록 높은 우선순위
                                int dist = (dr * dr + dc * dc);
                                priority += (10 - dist);
                            }
                        }
                    }
                }

                // 주변에 돌이 있는 경우만 후보로 추가
                if (hasNearbyStone) {
                    candidates[candidateCount].row = row;
                    candidates[candidateCount].col = col;
                    candidates[candidateCount].priority = priority;
                    candidateCount++;
                }
            }
        }
    }

    // 후보가 없으면 모든 빈 칸을 후보로
    if (candidateCount == 0) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board[row][col] == EMPTY) {
                    candidates[candidateCount].row = row;
                    candidates[candidateCount].col = col;
                    candidates[candidateCount].priority = 0;
                    candidateCount++;
                }
            }
        }
    }

    // 우선순위로 정렬
    qsort(candidates, candidateCount, sizeof(CandidateMove), compareCandidates);

    // maxCount만큼만 반환
    int returnCount = (candidateCount < maxCount) ? candidateCount : maxCount;
    for (int i = 0; i < returnCount; i++) {
        moves[i].row = candidates[i].row;
        moves[i].col = candidates[i].col;
    }

    return returnCount;
}
