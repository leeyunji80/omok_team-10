#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <ctype.h>
#include <string.h>
#include "minimax.h"

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>

    // Unix/macOS용 getch 구현
    int _getch(void) {
        struct termios oldattr, newattr;
        int ch;
        tcgetattr(STDIN_FILENO, &oldattr);
        newattr = oldattr;
        newattr.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
        return ch;
    }

    // Unix/macOS용 Sleep 구현 (밀리초)
    void Sleep(int ms) {
        usleep(ms * 1000);
    }
#endif

#define SIZE 15
#define BLACK 1
#define WHITE 2
#define EMPTY 0
#define SAVE_BOARD_SIZE 15
#define MAX_SAVE_SLOTS 5
#define BOARD_SIZE SIZE
#define MAX_MOVES 60
#define INFINITY_SCORE 10000000

/*==========전역 변수 상태=============*/
int board[SIZE][SIZE];
int cursorX = 0, cursorY = 0;
int currentPlayer = BLACK;
int gameMode = 0; // 1=1인용, 2=2인용
int difficulty = MEDIUM; // AI 난이도 (기본: 중간)
int lastMoveX = -1, lastMoveY = -1;
char player_nickname[50] = "Player";

/*============= AI 관련 =====================*/
static const int DX[] = { 1, 0, 1, 1 };
static const int DY[] = { 0, 1, 1, -1 };
typedef enum {
    SCORE_FIVE = 1000000,   // 5목 (즉시 승리)
    SCORE_OPEN_FOUR = 100000,    // 열린 4 (막을 수 없음)
    SCORE_FOUR = 15000,     // 닫힌 4 (한쪽 막힘)
    SCORE_OPEN_THREE = 5000,      // 열린 3
    SCORE_THREE = 800,       // 닫힌 3
    SCORE_OPEN_TWO = 300,       // 열린 2
    SCORE_TWO = 50,        // 닫힌 2
    SCORE_ONE = 10         // 1개
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

void cleanupAI(void) {
    initialized = 0;
}
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
    int dx, int dy, int color, int* count, int* openEnds) {
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
        }
        else if (count == 4) {
            if (openEnds == 2) {
                score += SCORE_OPEN_FOUR;  // 열린 4 = 승리 확정
            }
            else if (openEnds == 1) {
                score += SCORE_FOUR;
                fours++;
            }
        }
        else if (count == 3) {
            if (openEnds == 2) {
                score += SCORE_OPEN_THREE;
                openThrees++;
            }
            else if (openEnds == 1) {
                score += SCORE_THREE;
            }
        }
        else if (count == 2) {
            if (openEnds == 2) {
                score += SCORE_OPEN_TWO;
            }
            else if (openEnds == 1) {
                score += SCORE_TWO;
            }
        }
        else if (count == 1) {
            if (openEnds == 2) {
                score += SCORE_ONE * 2;
            }
            else if (openEnds == 1) {
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
                }
                else if (count == 4) {
                    if (openEnds == 2) lineScore = SCORE_OPEN_FOUR;
                    else if (openEnds == 1) lineScore = SCORE_FOUR;
                }
                else if (count == 3) {
                    if (openEnds == 2) lineScore = SCORE_OPEN_THREE;
                    else if (openEnds == 1) lineScore = SCORE_THREE;
                }
                else if (count == 2) {
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

// 후보 수 비교 함수
static int compareMoves(const void* a, const void* b) {
    return ((ScoredMove*)b)->score - ((ScoredMove*)a)->score;
}

// 착수 가능한 위치 찾기
int getPossibleMoves(int board[BOARD_SIZE][BOARD_SIZE], Move moves[], int maxCount) {
    int visited[BOARD_SIZE][BOARD_SIZE] = { 0 };
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
    MoveResult result = { 0, -1, -1 };
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
    }
    else {
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
        Move center = { BOARD_SIZE / 2, BOARD_SIZE / 2 };
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
        Move bestMove = { result.row, result.col };
        return bestMove;
    }

    // 예외 처리: 첫 번째 후보 반환
    return moves[0];
}

typedef struct {
    int board[SAVE_BOARD_SIZE][SAVE_BOARD_SIZE];
    int currentTurn;
    int gameMode;
} SaveData;

typedef struct {
    int row;
    int col;
    int priority;
} CandidateMove;
// 후보 수 구조체
typedef struct {
    int row;
    int col;
    int score;
} ScoredMove;


/*==========함수 프로토 타입============*/
void clearScreen(void);
void initBoard(void);
void printBoard(void);
void moveCursor(char key);
int placeStone(int x, int y);
void aiMove(void);
int checkWinGameplay(int x, int y);
void showMenu(void);
void gameLoop(void);
void SaveGame(const SaveData* data);
int LoadGame(SaveData* data);
void manage_fifo(const char* newFilename);
void get_filename(char* buffer);
void HandleExit(const SaveData* currentData);
void ResetGame(SaveData* data);
void update_game_result(const char* nickname, int did_win);
void print_rankings(void);
void gotoxy(int x, int y);
void hideCursor(int hide);


void gotoxy(int x, int y) {
#ifdef _WIN32
    COORD pos = {x, y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
#else
    printf("\033[%d;%dH", y + 1, x + 1);
#endif
}

void hideCursor(int hide) {
#ifdef _WIN32
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(consoleHandle, &info);
    info.bVisible = !hide;
    SetConsoleCursorInfo(consoleHandle, &info);
#else
    if (hide) printf("\033[?25l");
    else printf("\033[?25h");
#endif
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
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

void aiMove() {
    Move bestMove = findBestMove(board, WHITE, difficulty);
    if (bestMove.row >= 0 && bestMove.col >= 0) {
        placeStone(bestMove.col, bestMove.row);
    }
}

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

// 승리 체크
int checkWinGameplay(int x, int y) {
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

/*===============랭킹 관련 함수===============*/

void update_game_result(const char* nickname, int did_win) {
    cJSON* root = NULL;
    FILE* fp = NULL;
    char* buffer = NULL;
    long length = 0;
    time_t tim=time(NULL);
    struct tm tm = *localtime(&tim);
    char date_str[16];
    sprintf_s(date_str, sizeof(date_str), "%02d/%02d", tm.tm_mon + 1, tm.tm_mday);

    fopen_s(&fp, "user_data.json", "r");

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (length > 0) {
        buffer = (char*)malloc(length + 1);
        if (buffer) {
            size_t read_bytes = fread(buffer, 1, length, fp);
            buffer[read_bytes] = '\0';
        }
    }
    fclose(fp);

    if (buffer == NULL) {
        root = cJSON_CreateArray();
    }
    else {
        root = cJSON_Parse(buffer);
        free(buffer);
    }

    int found = 0;
    int size = cJSON_GetArraySize(root);

    for (int i = 0; i < size; i++) {
        cJSON* player = cJSON_GetArrayItem(root, i);
        cJSON* j_nickname = cJSON_GetObjectItemCaseSensitive(player, "nickname");

        if (cJSON_IsString(j_nickname) && (strcmp(j_nickname->valuestring, nickname) == 0)) {
            found = 1;

            int wins = cJSON_GetObjectItemCaseSensitive(player, "wins")->valueint;
            int losses = cJSON_GetObjectItemCaseSensitive(player, "losses")->valueint;

            if (did_win) {
                wins++;
            }
            else {
                losses++;
            }

            int total_games = wins + losses;
            double win_rate;
            if (total_games > 0) win_rate = (double)wins / total_games;
            else win_rate = 0.0;

            cJSON_ReplaceItemInObject(player, "wins", cJSON_CreateNumber(wins));
            cJSON_ReplaceItemInObject(player, "losses", cJSON_CreateNumber(losses));
            cJSON_ReplaceItemInObject(player, "win_rate", cJSON_CreateNumber(win_rate));
            cJSON_ReplaceItemInObject(player, "time", cJSON_CreateString(date_str));
            break;
        }
    }

    if (!found) {
        cJSON* new_player = cJSON_CreateObject();

        int wins;
        int losses;

        if (did_win) {
            wins = 1;
            losses = 0;
        }
        else {
            wins = 0;
            losses = 1;
        }

        double win_rate = (double)wins / (wins + losses);

        cJSON_AddStringToObject(new_player, "nickname", nickname);
        cJSON_AddNumberToObject(new_player, "wins", wins);
        cJSON_AddNumberToObject(new_player, "losses", losses);
        cJSON_AddNumberToObject(new_player, "win_rate", win_rate);
        cJSON_AddStringToObject(new_player, "time", date_str);
        cJSON_AddItemToArray(root, new_player);

    }

    char* json_string = cJSON_Print(root);

    fopen_s(&fp, "user_data.json", "w");

    fprintf_s(fp, "%s", json_string);
    fclose(fp);

    cJSON_free(json_string);
    cJSON_Delete(root);
}

void print_rankings() {
    typedef struct {
        char nickname[50];
        int wins;
        int losses;
        double win_rate;
    } RankPlayer;

    FILE* fp = NULL;
    char* buffer = NULL;
    long length = 0;

    if (fopen_s(&fp, "user_data.json", "r") == 0 && fp != NULL) {
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (length > 0) {
            buffer = (char*)malloc(length + 1);
            if (buffer) {
                fread(buffer, 1, length, fp);
                buffer[length] = '\0';
            }
        }
        fclose(fp);
    }

    if (buffer == NULL) {
        printf("랭킹 정보가 없습니다.\n");
        return;
    }

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);

    if (root == NULL) return;

    int size = cJSON_GetArraySize(root);
    if (size == 0) {
        cJSON_Delete(root);
        return;
    }

    RankPlayer* players = (RankPlayer*)malloc(sizeof(RankPlayer) * size);
    if (players == NULL) {
        cJSON_Delete(root);
        return;
    }

    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        cJSON* name = cJSON_GetObjectItem(item, "nickname");
        cJSON* wins = cJSON_GetObjectItem(item, "wins");
        cJSON* losses = cJSON_GetObjectItem(item, "losses");
        cJSON* rate = cJSON_GetObjectItem(item, "win_rate");

        if (name && wins && losses && rate) {
            strcpy_s(players[i].nickname, sizeof(players[i].nickname), name->valuestring);
            players[i].wins = wins->valueint;
            players[i].losses = losses->valueint;
            players[i].win_rate = rate->valuedouble;
        }
    }

    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - 1 - i; j++) {
            int swap_needed = 0;

            if (players[j].win_rate < players[j + 1].win_rate) {
                swap_needed = 1;
            }
            else if (players[j].win_rate == players[j + 1].win_rate) {
                if (players[j].wins < players[j + 1].wins) {
                    swap_needed = 1;
                }
            }

            if (swap_needed) {
                RankPlayer temp = players[j];
                players[j] = players[j + 1];
                players[j + 1] = temp;
            }
        }
    }

    printf("\n=== 랭킹 시스템 (상위 5명) ===\n");
    printf("%-5s %-15s %-10s %-10s\n", "순위", "닉네임", "승률", "전적");
    printf("-------------------------------------------\n");

    int limit = (size < 5) ? size : 5;
    for (int i = 0; i < limit; i++) {
        printf("%-5d %-15s %-9.1f%% %d승 %d패\n",
            i + 1,
            players[i].nickname,
            players[i].win_rate * 100,
            players[i].wins,
            players[i].losses);
    }
    printf("-------------------------------------------\n");

    free(players);
    cJSON_Delete(root);
}

/*=============저장 및 불러오기============*/
void get_filename(char* buffer) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(buffer, "%04d%02d%02d_%02d%02d%02d.dat",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void manage_fifo(const char* newFilename) {
    char fileList[MAX_SAVE_SLOTS + 1][256];
    int count = 0;
    FILE* fp;

    fp = fopen("save_list.txt", "r");
    if (fp != NULL) {
        while (count < MAX_SAVE_SLOTS && fscanf(fp, "%s", fileList[count]) != EOF) {
            count++;
        }
        fclose(fp);
    }

    if (count >= MAX_SAVE_SLOTS) {
        remove(fileList[0]);
        for (int i = 0; i < count - 1; i++) {
            strcpy(fileList[i], fileList[i + 1]);
        }
        count--;
    }

    strcpy(fileList[count], newFilename);
    count++;

    fp = fopen("save_list.txt", "w");
    if (fp != NULL) {
        for (int i = 0; i < count; i++) {
            fprintf(fp, "%s\n", fileList[i]);
        }
        fclose(fp);
    }
}
void SaveGame(const SaveData* data) {
    char filename[256];
    get_filename(filename);

    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        return;
    }
    fwrite(data, sizeof(SaveData), 1, fp);
    fclose(fp);

    manage_fifo(filename);
}

int LoadGame(SaveData* data) {
    char fileList[MAX_SAVE_SLOTS][256];
    int count = 0;
    FILE* fp;

    fp = fopen("save_list.txt", "r");
    if (fp == NULL) {
        return 0;
    }

    printf("\n=======저장된 게임 목록=======\n");

    while (count < MAX_SAVE_SLOTS && fscanf(fp, "%s", fileList[count]) != EOF) {
        printf("%d. %s\n", count + 1, fileList[count]);
        count++;
    }
    fclose(fp);

    printf("몇 번 파일의 게임을 불러오시겠습니까? 번호를 입력하세요 :");

    if (count == 0) return 0;

    int choice;
    scanf("%d", &choice);

    if (choice < 1 || choice > count) return 0;

    char* targetFile = fileList[choice - 1];
    fp = fopen(targetFile, "rb");
    if (fp == NULL) {
        return 0;
    }

    fread(data, sizeof(SaveData), 1, fp);
    fclose(fp);

    return 1;
}

void HandleExit(const SaveData* currentData) {
    char key;

    printf("\n  ========================================\n");
    printf("  게임을 저장하시겠습니까? (Y/N) >> ");
    
    while (1) {
        key = _getch();
        key = toupper(key);

        if (key == 'Y') {
            printf(" 예(Y)\n");
            SaveGame(currentData);
            exit(0);
        }
        else if (key == 'N') {
            printf(" 아니오(N)\n");
            exit(0);
        }
    }
}

void ResetGame(SaveData* data) {
    for (int i = 0; i < SAVE_BOARD_SIZE; i++) {
        for (int j = 0; j < SAVE_BOARD_SIZE; j++) {
            data->board[i][j] = 0;
        }
    }

    data->currentTurn = 1;
    data->gameMode = 2;
}

/*========데이터 파일 <-> 오목판에 출력 변환 ==========*/
void SaveCurrentGame() {
    SaveData data;
    for (int i = 0; i < SAVE_BOARD_SIZE; i++)
        for (int j = 0; j < SAVE_BOARD_SIZE; j++)
            data.board[i][j] = board[i][j];
    data.currentTurn = currentPlayer;
    data.gameMode = gameMode;
    SaveGame(&data);
}
int LoadSelectedGame() {
    SaveData data;
    if (!LoadGame(&data)) return 0;

    for (int i = 0; i < SAVE_BOARD_SIZE && i < SIZE; i++)
        for (int j = 0; j < SAVE_BOARD_SIZE && j < SIZE; j++)
            board[i][j] = data.board[i][j];

    currentPlayer = data.currentTurn;
    gameMode = data.gameMode;
    //커서 초기화
    cursorX = 0;
    cursorY = 0;
    lastMoveX = -1;
    lastMoveY = -1;
    return 1;
}

/*========================메뉴==========================*/
void showMenu() {
    int choice;
    hideCursor(0);
    clearScreen();
    printf("========== 메뉴 ==========\n");
    printf("1. 게임 저장\n");
    printf("2. 게임 불러오기\n");
    printf("3. 종료\n");
    printf("---------------------------\n");
    printf("원하는 번호를 입력하세요 : ");
    scanf("%d", &choice);

    switch (choice) {
    case 1: SaveCurrentGame(); printf("아무키나 누르면 게임으로 돌아갑니다..."); _getch(); return ;
    case 2:   if (LoadSelectedGame()) {
                return ;
            } else {
                printf("불러오기를 실패하거나 취소했습니다. 아무 키나 누르면 메뉴로 돌아갑니다...");
            }
            _getch();
            break;
    case 3: printf("프로그램을 종료합니다...\n");
            exit(0);
            break;
    default: printf("잘못된 선택입니다.\n"); Sleep(600); break;
    }
  }


// 메인 게임 루프
void gameLoop() {
    clearScreen();
    hideCursor(1);
    printBoard();
    int key;

    while (1) {
        if (gameMode == 1 && currentPlayer == WHITE) { // AI 차례
            aiMove();
            printBoard();
            if (checkWinGameplay(lastMoveX, lastMoveY) == 2) {
                printf("백돌(AI) 승리!\n");
                if(gameMode == 1){
                    fflush(stdin);
                    printf("\n닉네임을 입력하세요:");
					scanf("%s", player_nickname);
                    update_game_result(player_nickname,0);
                }
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
                int winner = checkWinGameplay(lastMoveX, lastMoveY);
                if (winner != 0) {
                    hideCursor(0);
                    printf("%s 승리! 게임이 종료되었습니다.\n", (winner == BLACK) ? "흑" : "백");
                    if(gameMode == 1){
                        if(winner == BLACK){fflush(stdin);
                            printf("\n닉네임을 입력하세요:");
                            scanf("%s", player_nickname);
                            update_game_result(player_nickname, 1);
                        }
                        else update_game_result(player_nickname, 0);
                    }
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
         if (LoadSelectedGame()) {
            printf("게임을 불러왔습니다. 아무 키나 누르면 게임을 시작합니다...");
            _getch();
            gameLoop();
        } else {
            printf("불러오기 실패. 아무 키나 누르면 메뉴로 돌아갑니다...");
            _getch();
        }
    }
    else if(gameMode == 4){
        print_rankings();
    }
    else if(gameMode == 5){
        printf("프로그램을 종료합니다...");
        return 0;
    }
    SaveData currentData;
    for (int i = 0; i < SAVE_BOARD_SIZE; i++)
        for (int j = 0; j < SAVE_BOARD_SIZE; j++)
            currentData.board[i][j] = (i < SIZE && j < SIZE) ? board[i][j] : 0;
    currentData.currentTurn = currentPlayer;
    currentData.gameMode = gameMode;

    if(gameMode == 1 || gameMode == 2 || gameMode == 3){
        printf("\n게임이 종료되었습니다. 저장하시겠습니까?\n");
        SaveData currentData;
        for (int i = 0; i < SAVE_BOARD_SIZE; i++)
        for (int j = 0; j < SAVE_BOARD_SIZE; j++)
            currentData.board[i][j] = (i < SIZE && j < SIZE) ? board[i][j] : 0;
    currentData.currentTurn = currentPlayer;
    currentData.gameMode = gameMode;

    HandleExit(&currentData);
    }
    cleanpAI();
    return 0;
}