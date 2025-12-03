// 고급 Minimax 알고리즘 기반 오목 AI (개선 버전)
// 주요 개선사항:
// - Transposition Table (Zobrist Hashing)
// - Iterative Deepening with Time Management
// - 향상된 패턴 인식 (빈 칸 포함 패턴)
// - VCF (Victory by Continuous Four) 탐지
// - 위치 가중치 (중앙 우선)
// - Killer Heuristic & History Heuristic
// - Principal Variation Search (PVS)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "minimax.h"

#define MAX_MOVES 225
#define TT_SIZE (1 << 20)  // Transposition Table 크기 (약 100만 엔트리)
#define MAX_DEPTH 12
#define INFINITY_SCORE 10000000

// 방향 벡터 (가로, 세로, 대각선 2개)
static const int dx[] = { 1, 0, 1, 1 };
static const int dy[] = { 0, 1, 1, -1 };

// 위치 가중치 테이블 (중앙이 높은 점수)
static int positionWeight[BOARD_SIZE][BOARD_SIZE];

// Zobrist 해싱용 랜덤 테이블
static unsigned long long zobristTable[BOARD_SIZE][BOARD_SIZE][3];
static unsigned long long currentHash = 0;

// Transposition Table 엔트리
typedef struct {
    unsigned long long hash;
    int depth;
    int score;
    int flag;  // 0: EXACT, 1: LOWERBOUND, 2: UPPERBOUND
    int bestRow;
    int bestCol;
} TTEntry;

static TTEntry* transpositionTable = NULL;

// Killer Moves (각 깊이별 2개)
static Move killerMoves[MAX_DEPTH][2];

// History Heuristic 테이블
static int historyTable[BOARD_SIZE][BOARD_SIZE];

// 패턴 점수 상수 (더 세분화)
typedef enum {
    PATTERN_FIVE = 1000000,        // 5목
    PATTERN_OPEN_FOUR = 100000,    // 열린 4 (양쪽 열림)
    PATTERN_DOUBLE_FOUR = 100000,  // 쌍사
    PATTERN_FOUR_THREE = 100000,   // 사삼
    PATTERN_CLOSED_FOUR = 15000,   // 닫힌 4 (한쪽만 열림)
    PATTERN_DOUBLE_THREE = 50000,  // 쌍삼
    PATTERN_OPEN_THREE = 8000,     // 열린 3
    PATTERN_JUMP_THREE = 6000,     // 띈 3 (O_OO, OO_O)
    PATTERN_CLOSED_THREE = 1500,   // 닫힌 3
    PATTERN_OPEN_TWO = 500,        // 열린 2
    PATTERN_JUMP_TWO = 300,        // 띈 2
    PATTERN_CLOSED_TWO = 100,      // 닫힌 2
    PATTERN_ONE = 10               // 단일
} PatternScore;

// 라인 분석 결과 구조체
typedef struct {
    int count;          // 연속된 돌 개수
    int openEnds;       // 열린 끝 개수
    int totalLength;    // 빈칸 포함 총 길이
    int gaps;           // 중간 빈칸 개수
    int potentialFive;  // 5목 가능 여부
} LineAnalysis;

// 초기화 함수
void initAI(void) {
    // 난수 시드 설정
    srand((unsigned int)time(NULL));
    
    // Transposition Table 할당
    if (transpositionTable == NULL) {
        transpositionTable = (TTEntry*)calloc(TT_SIZE, sizeof(TTEntry));
    }
    
    // Zobrist 테이블 초기화
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            for (int k = 0; k < 3; k++) {
                zobristTable[i][j][k] = ((unsigned long long)rand() << 32) | rand();
            }
        }
    }
    
    // 위치 가중치 초기화 (중앙이 높음)
    int center = BOARD_SIZE / 2;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int dist = abs(i - center) + abs(j - center);
            positionWeight[i][j] = (BOARD_SIZE - dist) * 2;
        }
    }
    
    // Killer Moves 초기화
    memset(killerMoves, -1, sizeof(killerMoves));
    
    // History Table 초기화
    memset(historyTable, 0, sizeof(historyTable));
}

// AI 정리 함수
void cleanupAI(void) {
    if (transpositionTable != NULL) {
        free(transpositionTable);
        transpositionTable = NULL;
    }
}

// Zobrist 해시 업데이트
static void updateHash(int row, int col, int color) {
    currentHash ^= zobristTable[row][col][color];
}

// Zobrist 해시 계산 (전체 보드)
static unsigned long long computeHash(int board[BOARD_SIZE][BOARD_SIZE]) {
    unsigned long long hash = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) {
                hash ^= zobristTable[i][j][board[i][j]];
            }
        }
    }
    return hash;
}

// Transposition Table 조회
static TTEntry* probeTT(unsigned long long hash) {
    int index = hash % TT_SIZE;
    TTEntry* entry = &transpositionTable[index];
    if (entry->hash == hash) {
        return entry;
    }
    return NULL;
}

// Transposition Table 저장
static void storeTT(unsigned long long hash, int depth, int score, int flag, int bestRow, int bestCol) {
    int index = hash % TT_SIZE;
    TTEntry* entry = &transpositionTable[index];
    
    // 더 깊은 탐색 결과만 덮어쓰기
    if (entry->hash != hash || entry->depth <= depth) {
        entry->hash = hash;
        entry->depth = depth;
        entry->score = score;
        entry->flag = flag;
        entry->bestRow = bestRow;
        entry->bestCol = bestCol;
    }
}

// 승리 여부 확인
int checkWinBoard(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color) {
    if (board[row][col] != color) return 0;

    for (int dir = 0; dir < 4; dir++) {
        int count = 1;

        int nx = col + dx[dir];
        int ny = row + dy[dir];
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

        if (count >= 5) return 1;
    }
    return 0;
}

// 향상된 라인 분석 (빈칸 포함 패턴 인식)
static LineAnalysis analyzeLineAdvanced(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color, int dir) {
    LineAnalysis result = {0, 0, 0, 0, 0};
    
    if (board[row][col] != color) return result;
    
    result.count = 1;
    result.totalLength = 1;
    
    // 정방향 탐색 (빈칸 하나까지 허용)
    int nx = col + dx[dir];
    int ny = row + dy[dir];
    int gapUsed = 0;
    int consecutiveEmpty = 0;
    
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
        if (board[ny][nx] == color) {
            result.count++;
            result.totalLength++;
            consecutiveEmpty = 0;
        } else if (board[ny][nx] == EMPTY) {
            consecutiveEmpty++;
            if (consecutiveEmpty > 1) break;  // 연속 빈칸 2개면 중단
            
            // 빈칸 다음에 같은 색 돌이 있는지 확인
            int nnx = nx + dx[dir];
            int nny = ny + dy[dir];
            if (nnx >= 0 && nnx < BOARD_SIZE && nny >= 0 && nny < BOARD_SIZE && board[nny][nnx] == color) {
                if (!gapUsed) {
                    result.gaps++;
                    result.totalLength++;
                    gapUsed = 1;
                } else {
                    break;
                }
            } else {
                result.openEnds++;
                break;
            }
        } else {
            break;  // 상대 돌
        }
        nx += dx[dir];
        ny += dy[dir];
    }
    
    // 정방향 끝이 비어있는지 확인
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == EMPTY && consecutiveEmpty == 0) {
        result.openEnds++;
    }
    
    // 역방향 탐색
    nx = col - dx[dir];
    ny = row - dy[dir];
    gapUsed = 0;
    consecutiveEmpty = 0;
    
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
        if (board[ny][nx] == color) {
            result.count++;
            result.totalLength++;
            consecutiveEmpty = 0;
        } else if (board[ny][nx] == EMPTY) {
            consecutiveEmpty++;
            if (consecutiveEmpty > 1) break;
            
            int nnx = nx - dx[dir];
            int nny = ny - dy[dir];
            if (nnx >= 0 && nnx < BOARD_SIZE && nny >= 0 && nny < BOARD_SIZE && board[nny][nnx] == color) {
                if (!gapUsed && result.gaps == 0) {
                    result.gaps++;
                    result.totalLength++;
                    gapUsed = 1;
                } else {
                    break;
                }
            } else {
                result.openEnds++;
                break;
            }
        } else {
            break;
        }
        nx -= dx[dir];
        ny -= dy[dir];
    }
    
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == EMPTY && consecutiveEmpty == 0) {
        result.openEnds++;
    }
    
    // 5목 가능 여부 (빈칸 포함 5칸 이상 연속 공간)
    result.potentialFive = (result.totalLength + result.openEnds >= 5) ? 1 : 0;
    
    return result;
}

// 특정 위치의 위협 점수 계산 (개선 버전)
static int evaluatePositionAdvanced(int board[BOARD_SIZE][BOARD_SIZE], int row, int col, int color) {
    int score = 0;
    int patterns[6] = {0};  // [0]:5목, [1]:열린4, [2]:닫힌4, [3]:열린3, [4]:띈3, [5]:닫힌3
    
    board[row][col] = color;
    
    for (int dir = 0; dir < 4; dir++) {
        LineAnalysis analysis = analyzeLineAdvanced(board, row, col, color, dir);
        
        if (analysis.count >= 5) {
            patterns[0]++;
        } else if (analysis.count == 4) {
            if (analysis.openEnds == 2) patterns[1]++;      // 열린 4
            else if (analysis.openEnds == 1) patterns[2]++; // 닫힌 4
        } else if (analysis.count == 3) {
            if (analysis.gaps > 0) {
                patterns[4]++;  // 띈 3 (O_OO)
            } else if (analysis.openEnds == 2) {
                patterns[3]++;  // 열린 3
            } else if (analysis.openEnds == 1) {
                patterns[5]++;  // 닫힌 3
            }
        } else if (analysis.count == 4 && analysis.gaps == 1) {
            // 띈 4 (OO_OO) - 열린 4와 동등
            patterns[1]++;
        }
    }
    
    board[row][col] = EMPTY;
    
    // 패턴 조합 점수
    if (patterns[0] >= 1) return PATTERN_FIVE;
    if (patterns[1] >= 1) return PATTERN_OPEN_FOUR;
    if (patterns[2] >= 2) return PATTERN_DOUBLE_FOUR;  // 쌍사
    if (patterns[2] >= 1 && patterns[3] >= 1) return PATTERN_FOUR_THREE;  // 사삼
    if (patterns[3] >= 2) return PATTERN_DOUBLE_THREE;  // 쌍삼
    
    // 개별 패턴 점수 합산
    score += patterns[2] * PATTERN_CLOSED_FOUR;
    score += patterns[3] * PATTERN_OPEN_THREE;
    score += patterns[4] * PATTERN_JUMP_THREE;
    score += patterns[5] * PATTERN_CLOSED_THREE;
    
    // 위치 가중치 추가
    score += positionWeight[row][col];
    
    return score > 0 ? score : 10;
}

// VCF (Victory by Continuous Four) 탐지
// 연속적인 4 위협으로 승리 가능한지 확인
static int detectVCF(int board[BOARD_SIZE][BOARD_SIZE], int color, int depth, int maxDepth) {
    if (depth > maxDepth) return 0;
    
    int opponent = (color == BLACK) ? WHITE : BLACK;
    
    // 모든 빈 칸에서 4 위협 찾기
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] != EMPTY) continue;
            
            int score = evaluatePositionAdvanced(board, row, col, color);
            
            // 즉시 승리
            if (score >= PATTERN_FIVE) return 1;
            
            // 열린 4나 쌍사면 승리 확정
            if (score >= PATTERN_OPEN_FOUR) return 1;
            
            // 닫힌 4인 경우 상대 방어 후 계속 공격
            if (score >= PATTERN_CLOSED_FOUR && score < PATTERN_OPEN_FOUR) {
                board[row][col] = color;
                
                // 상대가 막아야 하는 위치 찾기
                int blocked = 0;
                for (int dr = 0; dr < BOARD_SIZE && !blocked; dr++) {
                    for (int dc = 0; dc < BOARD_SIZE && !blocked; dc++) {
                        if (board[dr][dc] != EMPTY) continue;
                        
                        // 이 위치에 두면 5목이 막히는지 확인
                        board[dr][dc] = opponent;
                        int stillWin = 0;
                        for (int cr = 0; cr < BOARD_SIZE && !stillWin; cr++) {
                            for (int cc = 0; cc < BOARD_SIZE && !stillWin; cc++) {
                                if (board[cr][cc] == color) {
                                    if (checkWinBoard(board, cr, cc, color)) {
                                        stillWin = 1;
                                    }
                                }
                            }
                        }
                        
                        if (!stillWin) {
                            // 상대가 여기 막음, 우리가 다시 공격
                            if (detectVCF(board, color, depth + 1, maxDepth)) {
                                board[dr][dc] = EMPTY;
                                board[row][col] = EMPTY;
                                return 1;
                            }
                            blocked = 1;
                        }
                        board[dr][dc] = EMPTY;
                    }
                }
                
                board[row][col] = EMPTY;
            }
        }
    }
    
    return 0;
}

// 간단한 VCF 체크 (성능을 위해 깊이 제한)
static int quickVCFCheck(int board[BOARD_SIZE][BOARD_SIZE], int color) {
    return detectVCF(board, color, 0, 4);  // 최대 4수 깊이
}

// 보드 상태 평가 함수 (개선 버전)
int evaluateBoard(int board[BOARD_SIZE][BOARD_SIZE], int aiColor) {
    int score = 0;
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    
    // 패턴 카운터
    int aiPatterns[6] = {0};   // 5, 열린4, 닫힌4, 열린3, 띈3, 닫힌3
    int oppPatterns[6] = {0};
    
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == EMPTY) continue;
            
            int color = board[row][col];
            int* patterns = (color == aiColor) ? aiPatterns : oppPatterns;
            
            for (int dir = 0; dir < 4; dir++) {
                // 중복 방지: 시작점에서만 계산
                int prevX = col - dx[dir];
                int prevY = row - dy[dir];
                if (prevX >= 0 && prevX < BOARD_SIZE && prevY >= 0 && prevY < BOARD_SIZE) {
                    if (board[prevY][prevX] == color) continue;
                }
                
                LineAnalysis analysis = analyzeLineAdvanced(board, row, col, color, dir);
                
                if (!analysis.potentialFive) continue;  // 5목 불가능한 라인 무시
                
                if (analysis.count >= 5) {
                    patterns[0]++;
                } else if (analysis.count == 4) {
                    if (analysis.openEnds == 2 || analysis.gaps > 0) patterns[1]++;
                    else if (analysis.openEnds == 1) patterns[2]++;
                } else if (analysis.count == 3) {
                    if (analysis.gaps > 0) patterns[4]++;
                    else if (analysis.openEnds == 2) patterns[3]++;
                    else if (analysis.openEnds == 1) patterns[5]++;
                }
            }
        }
    }
    
    // AI 점수 계산
    if (aiPatterns[0] > 0) return INFINITY_SCORE;
    if (aiPatterns[1] > 0) score += PATTERN_OPEN_FOUR;
    if (aiPatterns[2] >= 2) score += PATTERN_DOUBLE_FOUR;
    else score += aiPatterns[2] * PATTERN_CLOSED_FOUR;
    if (aiPatterns[3] >= 2) score += PATTERN_DOUBLE_THREE;
    else score += aiPatterns[3] * PATTERN_OPEN_THREE;
    score += aiPatterns[4] * PATTERN_JUMP_THREE;
    score += aiPatterns[5] * PATTERN_CLOSED_THREE;
    
    // 상대 점수 계산 (1.3배 가중치 - 방어 우선)
    int oppScore = 0;
    if (oppPatterns[0] > 0) return -INFINITY_SCORE;
    if (oppPatterns[1] > 0) oppScore += PATTERN_OPEN_FOUR;
    if (oppPatterns[2] >= 2) oppScore += PATTERN_DOUBLE_FOUR;
    else oppScore += oppPatterns[2] * PATTERN_CLOSED_FOUR;
    if (oppPatterns[3] >= 2) oppScore += PATTERN_DOUBLE_THREE;
    else oppScore += oppPatterns[3] * PATTERN_OPEN_THREE;
    oppScore += oppPatterns[4] * PATTERN_JUMP_THREE;
    oppScore += oppPatterns[5] * PATTERN_CLOSED_THREE;
    
    score -= (int)(oppScore * 1.3);
    
    // 위치 점수 추가
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == aiColor) {
                score += positionWeight[row][col];
            } else if (board[row][col] == opponent) {
                score -= positionWeight[row][col];
            }
        }
    }
    
    return score;
}

// 착수 후보 정렬용 구조체
typedef struct {
    int row;
    int col;
    int score;
    int isKiller;
    int historyScore;
} ScoredMove;

// 착수 후보 비교 함수
static int compareScoredMoves(const void* a, const void* b) {
    const ScoredMove* ma = (const ScoredMove*)a;
    const ScoredMove* mb = (const ScoredMove*)b;
    
    // Killer Move 우선
    if (ma->isKiller != mb->isKiller) return mb->isKiller - ma->isKiller;
    
    // 점수순 (내림차순)
    if (ma->score != mb->score) return mb->score - ma->score;
    
    // History Heuristic
    return mb->historyScore - ma->historyScore;
}

// 착수 가능한 위치 찾기 (개선 버전)
int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount) {
    int visited[BOARD_SIZE][BOARD_SIZE] = {0};
    int moveCount = 0;
    int hasStone = 0;

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

    if (!hasStone) {
        moves[0].row = BOARD_SIZE / 2;
        moves[0].col = BOARD_SIZE / 2;
        return 1;
    }

    return moveCount;
}

// 착수 후보 정렬 (개선된 Move Ordering)
static int getSortedMoves(int board[BOARD_SIZE][BOARD_SIZE], ScoredMove moves[], 
                          int aiColor, int depth, int maxCount, Move* ttMove) {
    Move rawMoves[MAX_MOVES];
    int rawCount = getPossibleMoves(board, rawMoves, MAX_MOVES);
    
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    int moveCount = 0;
    
    for (int i = 0; i < rawCount && moveCount < maxCount; i++) {
        int row = rawMoves[i].row;
        int col = rawMoves[i].col;
        
        moves[moveCount].row = row;
        moves[moveCount].col = col;
        
        // 공격 및 방어 점수
        int attackScore = evaluatePositionAdvanced(board, row, col, aiColor);
        int defenseScore = evaluatePositionAdvanced(board, row, col, opponent);
        moves[moveCount].score = attackScore + (int)(defenseScore * 1.1);
        
        // Transposition Table의 최선 수
        if (ttMove && ttMove->row == row && ttMove->col == col) {
            moves[moveCount].score += 1000000;
        }
        
        // Killer Move 체크
        moves[moveCount].isKiller = 0;
        if (depth < MAX_DEPTH) {
            if ((killerMoves[depth][0].row == row && killerMoves[depth][0].col == col) ||
                (killerMoves[depth][1].row == row && killerMoves[depth][1].col == col)) {
                moves[moveCount].isKiller = 1;
            }
        }
        
        // History Heuristic
        moves[moveCount].historyScore = historyTable[row][col];
        
        moveCount++;
    }
    
    // 정렬
    qsort(moves, moveCount, sizeof(ScoredMove), compareScoredMoves);
    
    return moveCount;
}

// Killer Move 업데이트
static void updateKillerMove(int depth, int row, int col) {
    if (depth >= MAX_DEPTH) return;
    
    // 이미 있으면 무시
    if (killerMoves[depth][0].row == row && killerMoves[depth][0].col == col) return;
    
    // 두 번째를 첫 번째로, 새 것을 두 번째로
    killerMoves[depth][1] = killerMoves[depth][0];
    killerMoves[depth][0].row = row;
    killerMoves[depth][0].col = col;
}

// History Heuristic 업데이트
static void updateHistory(int row, int col, int depth) {
    historyTable[row][col] += depth * depth;
}

// Principal Variation Search (PVS) - Alpha-Beta의 개선 버전
static MoveResult pvs(int board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, 
                      int isMaximizing, int aiColor, int nullMove) {
    MoveResult result = {0, -1, -1};
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    unsigned long long hash = computeHash(board);
    
    // Transposition Table 조회
    TTEntry* ttEntry = probeTT(hash);
    Move ttMove = {-1, -1};
    
    if (ttEntry != NULL && ttEntry->depth >= depth) {
        if (ttEntry->flag == 0) {  // EXACT
            result.score = ttEntry->score;
            result.row = ttEntry->bestRow;
            result.col = ttEntry->bestCol;
            return result;
        } else if (ttEntry->flag == 1 && ttEntry->score > alpha) {  // LOWERBOUND
            alpha = ttEntry->score;
        } else if (ttEntry->flag == 2 && ttEntry->score < beta) {   // UPPERBOUND
            beta = ttEntry->score;
        }
        
        if (alpha >= beta) {
            result.score = ttEntry->score;
            result.row = ttEntry->bestRow;
            result.col = ttEntry->bestCol;
            return result;
        }
        
        ttMove.row = ttEntry->bestRow;
        ttMove.col = ttEntry->bestCol;
    }
    
    // 깊이 0이면 평가
    if (depth == 0) {
        result.score = evaluateBoard(board, aiColor);
        return result;
    }
    
    // 착수 후보 가져오기
    ScoredMove scoredMoves[MAX_MOVES];
    int maxMoves = (depth > 3) ? 12 : 20;  // 깊이에 따라 후보 수 조절
    int moveCount = getSortedMoves(board, scoredMoves, 
                                   isMaximizing ? aiColor : opponent, 
                                   depth, maxMoves, &ttMove);
    
    if (moveCount == 0) {
        result.score = evaluateBoard(board, aiColor);
        return result;
    }
    
    int currentColor = isMaximizing ? aiColor : opponent;
    int bestRow = scoredMoves[0].row;
    int bestCol = scoredMoves[0].col;
    int ttFlag = 2;  // UPPERBOUND
    
    for (int i = 0; i < moveCount; i++) {
        int row = scoredMoves[i].row;
        int col = scoredMoves[i].col;
        
        board[row][col] = currentColor;
        
        // 승리 체크
        if (checkWinBoard(board, row, col, currentColor)) {
            board[row][col] = EMPTY;
            result.score = isMaximizing ? INFINITY_SCORE : -INFINITY_SCORE;
            result.row = row;
            result.col = col;
            
            storeTT(hash, depth, result.score, 0, row, col);
            return result;
        }
        
        MoveResult childResult;
        
        // PVS: 첫 번째는 full window, 나머지는 null window
        if (i == 0) {
            childResult = pvs(board, depth - 1, alpha, beta, !isMaximizing, aiColor, 0);
        } else {
            // Null Window Search
            if (isMaximizing) {
                childResult = pvs(board, depth - 1, alpha, alpha + 1, 0, aiColor, 0);
                if (childResult.score > alpha && childResult.score < beta) {
                    // Re-search with full window
                    childResult = pvs(board, depth - 1, alpha, beta, 0, aiColor, 0);
                }
            } else {
                childResult = pvs(board, depth - 1, beta - 1, beta, 1, aiColor, 0);
                if (childResult.score < beta && childResult.score > alpha) {
                    childResult = pvs(board, depth - 1, alpha, beta, 1, aiColor, 0);
                }
            }
        }
        
        board[row][col] = EMPTY;
        
        if (isMaximizing) {
            if (childResult.score > alpha) {
                alpha = childResult.score;
                bestRow = row;
                bestCol = col;
                ttFlag = 0;  // EXACT
            }
        } else {
            if (childResult.score < beta) {
                beta = childResult.score;
                bestRow = row;
                bestCol = col;
                ttFlag = 0;  // EXACT
            }
        }
        
        // Pruning
        if (alpha >= beta) {
            updateKillerMove(depth, row, col);
            updateHistory(row, col, depth);
            ttFlag = isMaximizing ? 1 : 2;  // LOWERBOUND or UPPERBOUND
            break;
        }
    }
    
    result.score = isMaximizing ? alpha : beta;
    result.row = bestRow;
    result.col = bestCol;
    
    // Transposition Table 저장
    storeTT(hash, depth, result.score, ttFlag, bestRow, bestCol);
    
    return result;
}

// 기존 minimax 함수 (호환성 유지)
MoveResult minimax(int board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, 
                   int isMaximizing, int aiColor) {
    return pvs(board, depth, alpha, beta, isMaximizing, aiColor, 0);
}

// Iterative Deepening
static Move iterativeDeepening(int board[BOARD_SIZE][BOARD_SIZE], int aiColor, int maxDepth, int timeLimit) {
    Move bestMove = {BOARD_SIZE / 2, BOARD_SIZE / 2};
    clock_t startTime = clock();
    
    for (int depth = 2; depth <= maxDepth; depth += 2) {
        MoveResult result = pvs(board, depth, -INFINITY_SCORE, INFINITY_SCORE, 1, aiColor, 0);
        
        if (result.row >= 0 && result.col >= 0) {
            bestMove.row = result.row;
            bestMove.col = result.col;
        }
        
        // 승리 확정이면 조기 종료
        if (result.score >= PATTERN_FIVE) break;
        
        // 시간 체크
        clock_t currentTime = clock();
        double elapsed = (double)(currentTime - startTime) / CLOCKS_PER_SEC * 1000;
        if (elapsed > timeLimit * 0.7) break;  // 70% 시간 사용하면 종료
    }
    
    return bestMove;
}

// AI의 최적 착수 찾기 (개선 버전)
Move findBestMove(int board[BOARD_SIZE][BOARD_SIZE], int aiColor, int difficulty) {
    // 초기화 확인
    if (transpositionTable == NULL) {
        initAI();
    }
    
    // 난이도별 설정
    // Easy: 깊이 2, 실수 30%
    // Medium: 깊이 6, VCF 탐지
    // Hard: 깊이 10, 전체 기능, 시간 관리
    
    int depthMap[] = {2, 6, 10};
    int timeLimitMap[] = {500, 2000, 5000};  // ms
    int depth = depthMap[difficulty];
    int timeLimit = timeLimitMap[difficulty];
    
    int opponent = (aiColor == BLACK) ? WHITE : BLACK;
    
    // 후보 수 가져오기
    Move possibleMoves[MAX_MOVES];
    int moveCount = getPossibleMoves(board, possibleMoves, 50);
    Move bestMove = {-1, -1};
    
    if (moveCount == 0) {
        bestMove.row = BOARD_SIZE / 2;
        bestMove.col = BOARD_SIZE / 2;
        return bestMove;
    }
    
    // Easy: 30% 확률로 랜덤
    if (difficulty == EASY && (rand() % 100) < 30) {
        int randomIndex = rand() % ((moveCount < 5) ? moveCount : 5);
        return possibleMoves[randomIndex];
    }
    
    // 즉시 승리/방어 체크
    int bestAttackScore = 0, bestDefenseScore = 0;
    int bestAttackIdx = -1, bestDefenseIdx = -1;
    int urgentDefenseIdx = -1;
    
    for (int i = 0; i < moveCount; i++) {
        int row = possibleMoves[i].row;
        int col = possibleMoves[i].col;
        
        int attackScore = evaluatePositionAdvanced(board, row, col, aiColor);
        int defenseScore = evaluatePositionAdvanced(board, row, col, opponent);
        
        // 즉시 승리
        if (attackScore >= PATTERN_FIVE) {
            return possibleMoves[i];
        }
        
        // 상대 5목 방어 필수
        if (defenseScore >= PATTERN_FIVE && urgentDefenseIdx == -1) {
            urgentDefenseIdx = i;
        }
        
        if (attackScore > bestAttackScore) {
            bestAttackScore = attackScore;
            bestAttackIdx = i;
        }
        if (defenseScore > bestDefenseScore && defenseScore < PATTERN_FIVE) {
            bestDefenseScore = defenseScore;
            bestDefenseIdx = i;
        }
    }
    
    // 긴급 방어
    if (urgentDefenseIdx >= 0) {
        return possibleMoves[urgentDefenseIdx];
    }
    
    // 승리 확정 수 (열린4, 쌍사, 사삼)
    if (bestAttackScore >= PATTERN_OPEN_FOUR && bestAttackIdx >= 0) {
        return possibleMoves[bestAttackIdx];
    }
    
    // 상대 승리 확정 막기
    if (bestDefenseScore >= PATTERN_OPEN_FOUR && bestDefenseIdx >= 0) {
        return possibleMoves[bestDefenseIdx];
    }
    
    // Hard 난이도: VCF 탐지
    if (difficulty >= HARD) {
        // 내 VCF 확인
        for (int i = 0; i < moveCount; i++) {
            int row = possibleMoves[i].row;
            int col = possibleMoves[i].col;
            
            board[row][col] = aiColor;
            if (quickVCFCheck(board, aiColor)) {
                board[row][col] = EMPTY;
                return possibleMoves[i];
            }
            board[row][col] = EMPTY;
        }
    }
    
    // 닫힌4 방어
    if (bestDefenseScore >= PATTERN_CLOSED_FOUR && bestDefenseIdx >= 0) {
        return possibleMoves[bestDefenseIdx];
    }
    
    // 쌍삼 공격
    if (bestAttackScore >= PATTERN_DOUBLE_THREE && bestAttackIdx >= 0) {
        return possibleMoves[bestAttackIdx];
    }
    
    // 열린3 방어
    if (bestDefenseScore >= PATTERN_OPEN_THREE && bestDefenseIdx >= 0) {
        return possibleMoves[bestDefenseIdx];
    }
    
    // Iterative Deepening으로 최선의 수 탐색
    bestMove = iterativeDeepening(board, aiColor, depth, timeLimit);
    
    // 유효한 수가 없으면 첫 번째 후보 반환
    if (bestMove.row < 0 || bestMove.col < 0) {
        return possibleMoves[0];
    }
    
    return bestMove;
}