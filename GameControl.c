#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include "cJSON.h"
#include "minimax.h"
#include "network.h"

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #define CLEAR_SCREEN "cls"
#else
    #include <termios.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <sys/select.h>
    #define CLEAR_SCREEN "clear"

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

    // Unix/macOS용 _kbhit 구현
    int _kbhit(void) {
        struct termios oldt, newt;
        int ch;
        int oldf;

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        struct timeval tv;
        fd_set rdfs;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);
        select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
        int ret = FD_ISSET(STDIN_FILENO, &rdfs);

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ret;
    }

    // Unix/macOS용 Sleep 구현 (밀리초)
    void Sleep(unsigned int ms) {
        usleep(ms * 1000);
    }

    // Unix/macOS용 GetTickCount 구현 (밀리초)
    unsigned long GetTickCount(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    }

    // DWORD 타입 정의
    typedef unsigned long DWORD;
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
int gameMode = 0; /* 1=1인용, 2=2인용, 3=온라인 */
int difficulty = MEDIUM; /* AI 난이도 (기본: 중간) */
int lastMoveX = -1, lastMoveY = -1;
char player_nickname[50] = "Player";
char player1_nickname[50] = "Player1";  /* 2인용 흑돌 플레이어 */
char player2_nickname[50] = "Player2";  /* 2인용 백돌 플레이어 */
DWORD lastTick = 0;
int gameEndedByVictory = 0;

/*==========네트워크 관련 전역 변수=============*/
static SOCKET_TYPE netSocket = INVALID_SOCK;
static int myColor = 0;           /* 내 돌 색상 (1=흑, 2=백) */
static int isMyTurn = 0;          /* 내 턴 여부 */
static char opponentNickname[50] = "";  /* 상대방 닉네임 */
static int networkGameOver = 0;   /* 네트워크 게임 종료 여부 */

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

// 후보 수 구조체
typedef struct {
    int row;
    int col;
    int score;
} ScoredMove;

// AI 초기화

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

// 후보 수 비교 함수
static int compareMoves(const void* a, const void* b) {
    return ((ScoredMove*)b)->score - ((ScoredMove*)a)->score;
}



typedef struct {
    int board[SAVE_BOARD_SIZE][SAVE_BOARD_SIZE];
    int currentTurn;
    int gameMode;
    int aiDifficulty;
} SaveData;

typedef struct {
    int row;
    int col;
    int priority;
} CandidateMove;

/*==========함수 프로토 타입============*/
void clearScreen(void);
void initBoard(void);
void printBoard(int remainTime);
void moveCursor(char key);
int placeStone(int x, int y);
void aiMove(void);
int checkWinGameplay(int x, int y);
void showMenu(void);
void gameLoop(void);
void SaveGame(const SaveData* data);
int LoadGame(SaveData* data);
void HandleExit(const SaveData* currentData);
void update_game_result(const char* nickname, int did_win, int game_mode, int ai_difficulty);
void print_rankings(void);
void gotoxy(int x, int y);
void hideCursor(int hide);
cJSON* loadSaveList(void);
int writeSaveList(cJSON* root);


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

void printTemporaryMessage(const char* msg, int seconds) {
    printf("\n%s", msg);
    fflush(stdout);
    DWORD start = GetTickCount();
    while (GetTickCount() - start < seconds * 1000) {
        Sleep(50); // 메시지 표시 시간 동안 CPU 부담 최소화
    }
    // 메시지 지우기
    printf("\r");
    for (int i = 0; i < 80; i++) printf(" ");
    printf("\r");
    fflush(stdout);
}

void printRemainTime(int remain) {
    gotoxy(0, 0);
    printf("남은 시간 : %2d초  ", remain);
    fflush(stdout);
}

// 보드 출력
void printBoard(int remainTime) {
    gotoxy(0, 1);
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
    printf("                              \n");  // 이전 메시지 지우기용 빈 줄
    fflush(stdout);
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
        COORD pos = { 0, 23 };
SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
printf("이미 돌이 존재합니다!");
Sleep(800);
SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
printf("                       ");
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

// 게임 결과 저장 (모드, 난이도 포함)
// game_mode: 1=1인용, 2=2인용
// ai_difficulty: 1=EASY, 2=MEDIUM, 3=HARD (2인용일 경우 0)
void update_game_result(const char* nickname, int did_win, int game_mode, int ai_difficulty) {
    cJSON* root = NULL;
    FILE* fp = NULL;
    char* buffer = NULL;
    long length = 0;
    time_t tim = time(NULL);
    struct tm* tm_ptr = localtime(&tim);
    char date_str[16];
    snprintf(date_str, sizeof(date_str), "%02d/%02d", tm_ptr->tm_mon + 1, tm_ptr->tm_mday);

    fp = fopen("user_data.json", "r");

    if (fp != NULL) {
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
    }

    if (buffer == NULL) {
        root = cJSON_CreateArray();
    }
    else {
        root = cJSON_Parse(buffer);
        free(buffer);
        if (root == NULL) {
            root = cJSON_CreateArray();
        }
    }

    // 닉네임 + 게임모드 + 난이도로 고유 식별
    int found = 0;
    int size = cJSON_GetArraySize(root);

    for (int i = 0; i < size; i++) {
        cJSON* player = cJSON_GetArrayItem(root, i);
        cJSON* j_nickname = cJSON_GetObjectItemCaseSensitive(player, "nickname");
        cJSON* j_mode = cJSON_GetObjectItemCaseSensitive(player, "game_mode");
        cJSON* j_diff = cJSON_GetObjectItemCaseSensitive(player, "ai_difficulty");

        int stored_mode = j_mode ? j_mode->valueint : 1;
        int stored_diff = j_diff ? j_diff->valueint : 0;

        if (cJSON_IsString(j_nickname) &&
            (strcmp(j_nickname->valuestring, nickname) == 0) &&
            stored_mode == game_mode &&
            stored_diff == ai_difficulty) {

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

        int wins = did_win ? 1 : 0;
        int losses = did_win ? 0 : 1;
        double win_rate = (double)wins / (wins + losses);

        cJSON_AddStringToObject(new_player, "nickname", nickname);
        cJSON_AddNumberToObject(new_player, "game_mode", game_mode);
        cJSON_AddNumberToObject(new_player, "ai_difficulty", ai_difficulty);
        cJSON_AddNumberToObject(new_player, "wins", wins);
        cJSON_AddNumberToObject(new_player, "losses", losses);
        cJSON_AddNumberToObject(new_player, "win_rate", win_rate);
        cJSON_AddStringToObject(new_player, "time", date_str);
        cJSON_AddItemToArray(root, new_player);
    }

    char* json_string = cJSON_Print(root);

    fp = fopen("user_data.json", "w");
    if (fp != NULL) {
        fprintf(fp, "%s", json_string);
        fclose(fp);
    }

    cJSON_free(json_string);
    cJSON_Delete(root);
}

// 특정 모드/난이도의 랭킹을 출력하는 내부 함수
void print_rankings_filtered(int filter_mode, int filter_difficulty) {
    typedef struct {
        char nickname[50];
        int wins;
        int losses;
        double win_rate;
        char time[16];
    } RankPlayer;

    FILE* fp = NULL;
    char* buffer = NULL;
    long length = 0;

    fp = fopen("user_data.json", "r");
    if (fp != NULL) {
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

    // 제목 설정 (EASY=0, MEDIUM=1, HARD=2)
    const char* modeStr = (filter_mode == 1) ? "1인용 (AI 대전)" : "2인용 (플레이어 대전)";
    const char* diffStr = "";
    if (filter_mode == 1) {
        switch (filter_difficulty) {
            case 0: diffStr = " - 쉬움"; break;
            case 1: diffStr = " - 보통"; break;
            case 2: diffStr = " - 어려움"; break;
        }
    }

    if (buffer == NULL || length == 0) {
        printf("\n============ 랭킹: %s%s ============\n", modeStr, diffStr);
        printf("--------------------------------------------------\n");
        printf("  아직 등록된 랭킹 정보가 없습니다.\n");
        printf("--------------------------------------------------\n");
        printf("\n아무 키나 누르면 돌아갑니다...");
        fflush(stdout);
        _getch();
        if (buffer) free(buffer);
        return;
    }

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);

    if (root == NULL) {
        printf("\n랭킹 데이터를 읽는 중 오류가 발생했습니다.\n");
        printf("아무 키나 누르면 돌아갑니다...");
        fflush(stdout);
        _getch();
        return;
    }

    int size = cJSON_GetArraySize(root);

    RankPlayer* players = (RankPlayer*)malloc(sizeof(RankPlayer) * size);
    if (players == NULL) {
        cJSON_Delete(root);
        printf("\n메모리 할당 오류가 발생했습니다.\n");
        printf("아무 키나 누르면 돌아갑니다...");
        fflush(stdout);
        _getch();
        return;
    }

    // 필터링하여 유효한 플레이어 수 카운트
    int validCount = 0;
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        cJSON* name = cJSON_GetObjectItem(item, "nickname");
        cJSON* wins = cJSON_GetObjectItem(item, "wins");
        cJSON* losses = cJSON_GetObjectItem(item, "losses");
        cJSON* rate = cJSON_GetObjectItem(item, "win_rate");
        cJSON* ptime = cJSON_GetObjectItem(item, "time");
        cJSON* mode = cJSON_GetObjectItem(item, "game_mode");
        cJSON* diff = cJSON_GetObjectItem(item, "ai_difficulty");

        int item_mode = mode ? mode->valueint : 1;
        int item_diff = diff ? diff->valueint : 0;

        // 필터 조건 확인
        if (item_mode != filter_mode) continue;
        if (filter_mode == 1 && item_diff != filter_difficulty) continue;

        if (name && wins && losses && rate && ptime) {
            strncpy(players[validCount].nickname, name->valuestring, sizeof(players[validCount].nickname) - 1);
            players[validCount].nickname[sizeof(players[validCount].nickname) - 1] = '\0';
            players[validCount].wins = wins->valueint;
            players[validCount].losses = losses->valueint;
            players[validCount].win_rate = rate->valuedouble;
            strncpy(players[validCount].time, ptime->valuestring, sizeof(players[validCount].time) - 1);
            players[validCount].time[sizeof(players[validCount].time) - 1] = '\0';
            validCount++;
        }
    }

    if (validCount == 0) {
        printf("\n============ 랭킹: %s%s ============\n", modeStr, diffStr);
        printf("--------------------------------------------------\n");
        printf("  아직 등록된 랭킹 정보가 없습니다.\n");
        printf("--------------------------------------------------\n");
        printf("\n아무 키나 누르면 돌아갑니다...");
        fflush(stdout);
        _getch();
        free(players);
        cJSON_Delete(root);
        return;
    }

    // 버블 정렬 (승률 내림차순, 동률 시 승수 내림차순)
    for (int i = 0; i < validCount - 1; i++) {
        for (int j = 0; j < validCount - 1 - i; j++) {
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

    printf("\n============ 랭킹: %s%s ============\n", modeStr, diffStr);
    printf("%-5s %-15s %-10s %-15s %-12s\n", "순위", "닉네임", "승률", "전적", "최근 플레이");
    printf("----------------------------------------------------------\n");

    int limit = (validCount < 10) ? validCount : 10;
    for (int i = 0; i < limit; i++) {
        printf("%-5d %-15s %6.1f%%   %3d승 %3d패    %-12s\n",
            i + 1,
            players[i].nickname,
            players[i].win_rate * 100,
            players[i].wins,
            players[i].losses,
            players[i].time);
    }
    printf("----------------------------------------------------------\n");
    printf("총 %d명의 플레이어가 등록되어 있습니다.\n", validCount);
    printf("\n아무 키나 누르면 돌아갑니다...");
    fflush(stdout);
    _getch();

    free(players);
    cJSON_Delete(root);
}

void print_rankings() {
    int running = 1;
    int modeChoice;
    int c;
    int diffRunning;
    int diffChoice;

    while (running) {
        /* 화면 지우기 */
        clearScreen();

        printf("\n=========== 랭킹 확인 ===========\n");
        printf("  1. 1인용 랭킹 (AI 대전)\n");
        printf("  2. 2인용 랭킹 (플레이어 대전)\n");
        printf("  0. 메뉴로 돌아가기\n");
        printf("==================================\n");
        printf("선택하세요 (0~2): ");
        fflush(stdout);

        scanf("%d", &modeChoice);
        while ((c = getchar()) != '\n' && c != EOF);

        if (modeChoice == 0) {
            return;
        }
        else if (modeChoice == 1) {
            /* 1인용: 난이도 선택 */
            diffRunning = 1;
            while (diffRunning) {
                clearScreen();

                printf("\n======= 1인용 랭킹 - 난이도 선택 =======\n");
                printf("  1. 쉬움 (Easy)\n");
                printf("  2. 보통 (Medium)\n");
                printf("  3. 어려움 (Hard)\n");
                printf("  0. 뒤로 가기\n");
                printf("========================================\n");
                printf("선택하세요 (0~3): ");
                fflush(stdout);

                scanf("%d", &diffChoice);
                while ((c = getchar()) != '\n' && c != EOF);

                if (diffChoice == 0) {
                    diffRunning = 0;
                }
                else if (diffChoice >= 1 && diffChoice <= 3) {
                    clearScreen();
                    /* 메뉴 선택 1,2,3 -> 실제 난이도 값 0,1,2 (EASY, MEDIUM, HARD) */
                    print_rankings_filtered(1, diffChoice - 1);
                }
            }
        }
        else if (modeChoice == 2) {
            /* 2인용 랭킹 */
            clearScreen();
            print_rankings_filtered(2, 0);
        }
    }
}

/*=============저장 및 불러오기 (JSON 기반)============*/
#define SAVE_FILE "game_saves.json"

// JSON 파일에서 저장 목록 읽기
cJSON* loadSaveList() {
    FILE* fp = fopen(SAVE_FILE, "r");
    if (fp == NULL) {
        return cJSON_CreateArray();
    }

    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (length <= 0) {
        fclose(fp);
        return cJSON_CreateArray();
    }

    char* buffer = (char*)malloc(length + 1);
    if (buffer == NULL) {
        fclose(fp);
        return cJSON_CreateArray();
    }

    size_t read_bytes = fread(buffer, 1, length, fp);
    buffer[read_bytes] = '\0';
    fclose(fp);

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);

    if (root == NULL) {
        return cJSON_CreateArray();
    }

    return root;
}

// JSON 파일에 저장 목록 쓰기
int writeSaveList(cJSON* root) {
    char* json_string = cJSON_Print(root);
    if (json_string == NULL) {
        return 0;
    }

    FILE* fp = fopen(SAVE_FILE, "w");
    if (fp == NULL) {
        cJSON_free(json_string);
        return 0;
    }

    fprintf(fp, "%s", json_string);
    fclose(fp);
    cJSON_free(json_string);
    return 1;
}

// 저장 데이터 엔트리 생성 헬퍼 함수
cJSON* createSaveEntry(const SaveData* data) {
    cJSON* saveEntry = cJSON_CreateObject();

    // 저장 시간 생성
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char timestamp[64];
    sprintf(timestamp, "%04d-%02d-%02d %02d:%02d:%02d",
        tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    cJSON_AddStringToObject(saveEntry, "timestamp", timestamp);
    cJSON_AddNumberToObject(saveEntry, "gameMode", data->gameMode);
    cJSON_AddNumberToObject(saveEntry, "currentTurn", data->currentTurn);
    cJSON_AddNumberToObject(saveEntry, "aiDifficulty", data->aiDifficulty);

    // 보드 상태를 2차원 배열로 저장
    cJSON* boardArray = cJSON_CreateArray();
    for (int i = 0; i < SAVE_BOARD_SIZE; i++) {
        cJSON* rowArray = cJSON_CreateArray();
        for (int j = 0; j < SAVE_BOARD_SIZE; j++) {
            cJSON_AddItemToArray(rowArray, cJSON_CreateNumber(data->board[i][j]));
        }
        cJSON_AddItemToArray(boardArray, rowArray);
    }
    cJSON_AddItemToObject(saveEntry, "board", boardArray);

    return saveEntry;
}

/* 게임 저장 (JSON 기반) - 슬롯 선택 및 덮어쓰기 지원 */
void SaveGame(const SaveData* data) {
    cJSON* saveList;
    cJSON* entry;
    cJSON* timestamp;
    cJSON* mode;
    cJSON* turn;
    cJSON* existingEntry;
    cJSON* existingTs;
    cJSON* saveEntry;
    cJSON* emptyEntry;
    cJSON* ts;
    int count;
    int i;
    int choice;
    int slotIndex;
    int c;
    char confirm;
    const char* modeStr;
    const char* turnStr;

    clearScreen();
    saveList = loadSaveList();
    count = cJSON_GetArraySize(saveList);

    printf("\n=======저장 슬롯 선택=======\n");
    printf("%-4s %-20s %-10s %-10s\n", "번호", "저장 시간", "모드", "차례");
    printf("--------------------------------------------------\n");

    /* 기존 저장 슬롯 표시 */
    for (i = 0; i < MAX_SAVE_SLOTS; i++) {
        if (i < count) {
            entry = cJSON_GetArrayItem(saveList, i);
            timestamp = cJSON_GetObjectItem(entry, "timestamp");
            mode = cJSON_GetObjectItem(entry, "gameMode");
            turn = cJSON_GetObjectItem(entry, "currentTurn");

            modeStr = "알수없음";
            if (mode && mode->valueint == 1) modeStr = "1인용";
            else if (mode && mode->valueint == 2) modeStr = "2인용";

            turnStr = (turn && turn->valueint == BLACK) ? "흑" : "백";

            printf("%-4d %-20s %-10s %-10s\n",
                i + 1,
                timestamp ? timestamp->valuestring : "알수없음",
                modeStr,
                turnStr);
        } else {
            printf("%-4d %-20s %-10s %-10s\n", i + 1, "(빈 슬롯)", "-", "-");
        }
    }
    printf("--------------------------------------------------\n");
    printf("저장할 슬롯 번호를 입력하세요 (1~%d, 0: 취소): ", MAX_SAVE_SLOTS);

    scanf("%d", &choice);

    if (choice < 1 || choice > MAX_SAVE_SLOTS) {
        printf("저장이 취소되었습니다.\n");
        cJSON_Delete(saveList);
        return;
    }

    slotIndex = choice - 1;

    /* 덮어쓰기 확인 */
    if (slotIndex < count) {
        existingEntry = cJSON_GetArrayItem(saveList, slotIndex);
        existingTs = cJSON_GetObjectItem(existingEntry, "timestamp");

        /* 빈 슬롯이 아닌 경우에만 덮어쓰기 확인 */
        if (existingTs && strlen(existingTs->valuestring) > 0) {
            printf("슬롯 %d에 이미 저장된 게임이 있습니다. 덮어쓰시겠습니까? (Y/N): ", choice);
            /* 입력 버퍼 비우기 */
            while ((c = getchar()) != '\n' && c != EOF);

            confirm = _getch();
            printf("%c\n", confirm);
            if (confirm != 'Y' && confirm != 'y') {
                printf("저장이 취소되었습니다.\n");
                cJSON_Delete(saveList);
                return;
            }
        }
    }

    /* 새 저장 데이터 생성 */
    saveEntry = createSaveEntry(data);

    /* 슬롯에 저장 (덮어쓰기 또는 새로 추가) */
    if (slotIndex < count) {
        /* 기존 슬롯 덮어쓰기 */
        cJSON_ReplaceItemInArray(saveList, slotIndex, saveEntry);
    } else {
        /* 빈 슬롯이 중간에 있을 경우 빈 엔트리로 채우기 */
        while (cJSON_GetArraySize(saveList) < slotIndex) {
            emptyEntry = cJSON_CreateObject();
            cJSON_AddStringToObject(emptyEntry, "timestamp", "");
            cJSON_AddNumberToObject(emptyEntry, "gameMode", 0);
            cJSON_AddNumberToObject(emptyEntry, "currentTurn", 0);
            cJSON_AddNumberToObject(emptyEntry, "aiDifficulty", 0);
            cJSON_AddItemToObject(emptyEntry, "board", cJSON_CreateArray());
            cJSON_AddItemToArray(saveList, emptyEntry);
        }
        cJSON_AddItemToArray(saveList, saveEntry);
    }

    /* 파일에 저장 */
    if (writeSaveList(saveList)) {
        ts = cJSON_GetObjectItem(saveEntry, "timestamp");
        printf("\n슬롯 %d에 게임이 저장되었습니다! (%s)\n", choice, ts ? ts->valuestring : "");
    } else {
        printf("\n저장 실패!\n");
    }

    cJSON_Delete(saveList);
}

/* 게임 불러오기 (JSON 기반) */
int LoadGame(SaveData* data) {
    cJSON* saveList;
    cJSON* entry;
    cJSON* timestamp;
    cJSON* mode;
    cJSON* turn;
    cJSON* diff;
    cJSON* boardArray;
    cJSON* rowArray;
    cJSON* cell;
    int count;
    int validCount;
    int i, j;
    int choice;
    const char* modeStr;
    const char* turnStr;

    clearScreen();
    saveList = loadSaveList();
    count = cJSON_GetArraySize(saveList);

    if (count == 0) {
        printf("\n저장된 게임이 없습니다.\n");
        cJSON_Delete(saveList);
        return 0;
    }

    /* 유효한 저장 슬롯 수 확인 */
    validCount = 0;
    for (i = 0; i < count; i++) {
        entry = cJSON_GetArrayItem(saveList, i);
        timestamp = cJSON_GetObjectItem(entry, "timestamp");
        if (timestamp && strlen(timestamp->valuestring) > 0) {
            validCount++;
        }
    }

    if (validCount == 0) {
        printf("\n저장된 게임이 없습니다.\n");
        cJSON_Delete(saveList);
        return 0;
    }

    printf("\n=======저장된 게임 목록=======\n");
    printf("%-4s %-20s %-10s %-10s\n", "번호", "저장 시간", "모드", "차례");
    printf("--------------------------------------------------\n");

    for (i = 0; i < count; i++) {
        entry = cJSON_GetArrayItem(saveList, i);
        timestamp = cJSON_GetObjectItem(entry, "timestamp");
        mode = cJSON_GetObjectItem(entry, "gameMode");
        turn = cJSON_GetObjectItem(entry, "currentTurn");

        /* 빈 슬롯 건너뛰기 */
        if (!timestamp || strlen(timestamp->valuestring) == 0) {
            printf("%-4d %-20s %-10s %-10s\n", i + 1, "(빈 슬롯)", "-", "-");
            continue;
        }

        modeStr = "알수없음";
        if (mode && mode->valueint == 1) modeStr = "1인용";
        else if (mode && mode->valueint == 2) modeStr = "2인용";

        turnStr = (turn && turn->valueint == BLACK) ? "흑" : "백";

        printf("%-4d %-20s %-10s %-10s\n",
            i + 1,
            timestamp->valuestring,
            modeStr,
            turnStr);
    }
    printf("--------------------------------------------------\n");
    printf("불러올 게임 번호를 입력하세요 (0: 취소): ");

    scanf("%d", &choice);

    if (choice < 1 || choice > count) {
        cJSON_Delete(saveList);
        return 0;
    }

    /* 선택한 저장 데이터 로드 */
    entry = cJSON_GetArrayItem(saveList, choice - 1);
    timestamp = cJSON_GetObjectItem(entry, "timestamp");

    /* 빈 슬롯 선택 시 */
    if (!timestamp || strlen(timestamp->valuestring) == 0) {
        printf("빈 슬롯입니다. 다시 선택해주세요.\n");
        cJSON_Delete(saveList);
        return 0;
    }

    mode = cJSON_GetObjectItem(entry, "gameMode");
    turn = cJSON_GetObjectItem(entry, "currentTurn");
    diff = cJSON_GetObjectItem(entry, "aiDifficulty");
    boardArray = cJSON_GetObjectItem(entry, "board");

    if (mode) data->gameMode = mode->valueint;
    if (turn) data->currentTurn = turn->valueint;
    if (diff) data->aiDifficulty = diff->valueint;

    /* 보드 상태 복원 */
    if (boardArray && cJSON_IsArray(boardArray)) {
        for (i = 0; i < SAVE_BOARD_SIZE; i++) {
            rowArray = cJSON_GetArrayItem(boardArray, i);
            if (rowArray && cJSON_IsArray(rowArray)) {
                for (j = 0; j < SAVE_BOARD_SIZE; j++) {
                    cell = cJSON_GetArrayItem(rowArray, j);
                    if (cell) {
                        data->board[i][j] = cell->valueint;
                    }
                }
            }
        }
    }

    cJSON_Delete(saveList);
    printf("\n게임을 불러왔습니다!\n");
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
    data.aiDifficulty = difficulty;
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
    difficulty = data.aiDifficulty;
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
    case 1:
        SaveCurrentGame();
        printf("아무키나 누르면 게임으로 돌아갑니다...");
        _getch();
        clearScreen();
        hideCursor(1);
        return;
    case 2:
        if (LoadSelectedGame()) {
            clearScreen();
            hideCursor(1);
            return;
        } else {
            printf("불러오기를 실패하거나 취소했습니다. 아무 키나 누르면 메뉴로 돌아갑니다...");
        }
        _getch();
        break;
    case 3:
        printf("프로그램을 종료합니다...\n");
        exit(0);
        break;
    default:
        printf("잘못된 선택입니다.\n");
        Sleep(600);
        break;
    }
    printf("아무 키나 누르면 메뉴를 닫습니다...\n");
    _getch();
    clearScreen();
    hideCursor(1);
}


// 메인 게임 루프
void gameLoop() {
    clearScreen();
    hideCursor(1);
    DWORD playerTurnStart = GetTickCount();
    int turnActive = 0;

    while (1) {
        if (gameMode == 1 && currentPlayer == WHITE) { // AI 차례
            aiMove();
            printBoard(-1);
            if (checkWinGameplay(lastMoveX, lastMoveY) == 2) {
                gameEndedByVictory = 1;
                printf("백돌(AI) 승리!\n");
                if(gameMode == 1){
                    fflush(stdin);
                    while(1){
                        printf("\n닉네임을 입력하세요(15자 이내):");
					    scanf("%s", player_nickname);
                        if(strlen(player_nickname)>15){
                            printf("닉네임이 너무 깁니다.\n");
                            _getch();
                            printf("\x1b[1F");
                            printf("\x1b[2K");
                            printf("\x1b[1F");
                            printf("\x1b[2K");
                            printf("\x1b[1F");
                            printf("\x1b[2K");
                        }
                        else break;
                    }
                    update_game_result(player_nickname, 0, 1, difficulty);
                }
                break;
            }
            continue;
        }

         int key = -1;
if (gameMode == 2) {

    if (!turnActive) {
	playerTurnStart = GetTickCount();
    turnActive = 1;
}
     DWORD start = GetTickCount();
     int timed_out = 0;
     printBoard(-1);

         while (1) {

        // 경과 시간 계산
         DWORD now = GetTickCount();
        int remain = 10 - (now - playerTurnStart) / 1000;
        if (remain < 0) remain = 0;

        printRemainTime(remain);

        if (_kbhit()) {
            key = _getch();
            break;
        }
        if (remain == 0) {
            timed_out = 1;
            break;
        }

        Sleep(50);
    }

    if (timed_out) {
        printBoard(0);
       printTemporaryMessage("시간 초과! 턴이 넘어갑니다.", 1); // 1초 표시 후 지움
        currentPlayer = (currentPlayer == BLACK) ? WHITE : BLACK;
        turnActive = 0;
        Sleep(500);
        continue;
    }
}

 else {
     // 1인용은 기존 방식 유지
     printBoard(-1);
     key = _getch();
 }

        if (key == 'w' || key == 'a' || key == 's' || key == 'd' || key == 'W' || key == 'A' || key == 'S' || key == 'D') {
            moveCursor(key);
        }
        else if (key == 'b' || key == 'B') {
            if (placeStone(cursorX, cursorY)) {
                printBoard(-1);
                turnActive = 0;
                printBoard(-1);
                int winner = checkWinGameplay(lastMoveX, lastMoveY);
                if (winner != 0) {
                    gameEndedByVictory = 1;
                    hideCursor(0);
                    printf("%s 승리! 게임이 종료되었습니다.\n", (winner == BLACK) ? "흑" : "백");
                    if(gameMode == 1){
                        // 1인용: 플레이어가 흑, AI가 백
                        if(winner == BLACK){
                            fflush(stdin);
                            while(1){
                                printf("\n닉네임을 입력하세요(15자 이내):");
    	    				    scanf("%s", player_nickname);
                                if(strlen(player_nickname)>15){
                                printf("닉네임이 너무 깁니다.\n");
                                _getch();
                                printf("\x1b[1F");
                                printf("\x1b[2K");
                                printf("\x1b[1F");
                                printf("\x1b[2K");
                                printf("\x1b[1F");
                                printf("\x1b[2K");
                                }
                                else break;
                            }
                            update_game_result(player_nickname, 1, 1, difficulty);
                        }
                        else {
                            fflush(stdin);
                            while(1){
                                printf("\n닉네임을 입력하세요(15자 이내):");
	    	    			    scanf("%s", player_nickname);
                                if(strlen(player_nickname)>15){
                                    printf("닉네임이 너무 깁니다.\n");
                                    _getch();
                                    printf("\x1b[1F");
                                    printf("\x1b[2K");
                                    printf("\x1b[1F");
                                    printf("\x1b[2K");
                                    printf("\x1b[1F");
                                    printf("\x1b[2K");
                                }
                                else break;
                            }
                            update_game_result(player_nickname, 0, 1, difficulty);
                        }
                    }
                    else if(gameMode == 2){
                        /* 2인용: 게임 시작 시 입력받은 닉네임 사용 */
                        if (winner == BLACK) {
                            /* 흑돌(player1) 승리 */
                            printf("\n%s님 승리! %s님 패배!\n", player1_nickname, player2_nickname);
                            update_game_result(player1_nickname, 1, 2, 0);
                            update_game_result(player2_nickname, 0, 2, 0);
                        } else {
                            /* 백돌(player2) 승리 */
                            printf("\n%s님 승리! %s님 패배!\n", player2_nickname, player1_nickname);
                            update_game_result(player2_nickname, 1, 2, 0);
                            update_game_result(player1_nickname, 0, 2, 0);
                        }
                        printf("\n아무 키나 누르면 메뉴로 돌아갑니다...");
                        fflush(stdout);
                        _getch();
                    }
                    break;
                }
            }
        }
        else if (key == 'm' || key == 'M') {
            showMenu();
        }

        printBoard(-1);
        if (gameMode == 2) {
        printRemainTime(10 - (GetTickCount() - playerTurnStart) / 1000);
}
    }
    hideCursor(0);
}

/* 메뉴 화면 출력 함수 */
void showMainMenu() {
    clearScreen();
    printf("\n=========== 오목 게임 ===========\n");
    printf("  1. 1인용 게임 (vs AI)\n");
    printf("  2. 2인용 게임 (로컬)\n");
    printf("  3. 멀티플레이 (온라인)\n");
    printf("  4. 게임 불러오기\n");
    printf("  5. 랭킹 확인하기\n");
    printf("  6. 종료\n");
    printf("==================================\n");
    printf("메뉴 번호를 입력하세요 (1~6): ");
}

// 난이도 선택 함수
int selectDifficulty() {
    clearScreen();
    int diffChoice;
    printf("\n======= 난이도 선택 =======\n");
    printf("  1. 쉬움 (Easy)\n");
    printf("  2. 보통 (Medium)\n");
    printf("  3. 어려움 (Hard)\n");
    printf("===========================\n");
    printf("난이도를 선택하세요 (1~3): ");
    scanf("%d", &diffChoice);
    switch(diffChoice) {
        case 1: return EASY;
        case 2: return MEDIUM;
        case 3: return HARD;
        default: return MEDIUM;
    }
}

/*==========네트워크 게임 관련 함수=============*/

/* 서버 연결 */
int connectToServer(const char* serverIP, int port) {
    NetMessage msg;
    NetMessage response;

    printf("\n서버에 연결 중... (%s:%d)\n", serverIP, port);

    /* 네트워크 초기화 */
    if (net_init() != 0) {
        printf("네트워크 초기화 실패\n");
        return 0;
    }

    /* 서버 연결 */
    netSocket = net_connect_to_server(serverIP, port);
    if (netSocket == INVALID_SOCK) {
        printf("서버 연결 실패!\n");
        printf("서버가 실행 중인지 확인하세요.\n");
        net_cleanup();
        return 0;
    }

    printf("서버에 연결되었습니다!\n");

    /* 접속 메시지 전송 */
    net_create_connect_msg(&msg, player_nickname);
    if (net_send_message(netSocket, &msg) != 0) {
        printf("접속 메시지 전송 실패\n");
        CLOSE_SOCKET(netSocket);
        net_cleanup();
        return 0;
    }

    /* 접속 확인 응답 대기 */
    if (net_recv_message(netSocket, &response) != 0) {
        printf("서버 응답 수신 실패\n");
        CLOSE_SOCKET(netSocket);
        net_cleanup();
        return 0;
    }

    if (response.type != MSG_CONNECT_ACK) {
        printf("서버 접속 실패: %s\n", response.message);
        CLOSE_SOCKET(netSocket);
        net_cleanup();
        return 0;
    }

    printf("%s\n\n", response.message);
    return 1;
}

/* 방 생성 */
int createRoom(const char* roomName) {
    NetMessage msg;
    NetMessage response;
    int waiting = 1;

    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_ROOM_CREATE;
    strncpy(msg.nickname, roomName, sizeof(msg.nickname) - 1);

    if (net_send_message(netSocket, &msg) != 0) {
        printf("방 생성 요청 실패\n");
        return 0;
    }

    /* 응답 대기 */
    if (net_recv_message(netSocket, &response) != 0) {
        printf("서버 응답 실패\n");
        return 0;
    }

    if (response.type == MSG_ROOM_CREATE_ACK) {
        printf("%s\n", response.message);
        printf("(상대방이 입장하면 게임이 시작됩니다)\n\n");

        /* 상대방 입장 대기 */
        while (waiting) {
            if (_kbhit()) {
                int key = _getch();
                if (key == 27) { /* ESC */
                    memset(&msg, 0, sizeof(msg));
                    msg.type = MSG_ROOM_LEAVE;
                    net_send_message(netSocket, &msg);
                    printf("방을 나갔습니다.\n");
                    return 0;
                }
            }

            if (net_recv_message_nonblock(netSocket, &response) == 0) {
                if (response.type == MSG_GAME_START) {
                    myColor = response.player;
                    strncpy(opponentNickname, response.nickname, sizeof(opponentNickname) - 1);
                    isMyTurn = (myColor == 1) ? 1 : 0;

                    printf("\n========================================\n");
                    printf("  상대방 입장!\n");
                    printf("  상대방: %s\n", opponentNickname);
                    printf("  내 돌: %s (%s)\n",
                           (myColor == 1) ? "흑돌" : "백돌",
                           (myColor == 1) ? "선공" : "후공");
                    printf("========================================\n");
                    printf("\n아무 키나 누르면 게임을 시작합니다...");
                    fflush(stdout);
                    _getch();
                    return 1;
                }
            }
            Sleep(100);
        }
    } else if (response.type == MSG_ERROR) {
        printf("오류: %s\n", response.message);
    }

    return 0;
}

/* 방 목록 표시 및 선택 */
int showRoomList(void) {
    NetMessage msg;
    NetMessage response;
    int i;
    int choice;
    int c;

    while (1) {
        clearScreen();
        printf("\n========== 방 목록 ==========\n\n");

        /* 방 목록 요청 */
        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_ROOM_LIST;
        if (net_send_message(netSocket, &msg) != 0) {
            printf("방 목록 요청 실패\n");
            return -1;
        }

        /* 응답 대기 */
        if (net_recv_message(netSocket, &response) != 0) {
            printf("서버 응답 실패\n");
            return -1;
        }

        if (response.type != MSG_ROOM_LIST_RESP) {
            printf("잘못된 응답\n");
            return -1;
        }

        if (response.y == 0) {
            printf("  (대기 중인 방이 없습니다)\n\n");
        } else {
            printf("  번호 | 방 이름            | 방장          | 인원\n");
            printf("  -----+--------------------+---------------+------\n");
            for (i = 0; i < response.y; i++) {
                printf("  %3d  | %-18s | %-13s | %d/2 %s\n",
                       response.rooms[i].roomId,
                       response.rooms[i].roomName,
                       response.rooms[i].hostName,
                       response.rooms[i].playerCount,
                       response.rooms[i].inGame ? "(게임중)" : "");
            }
            printf("\n");
        }

        printf("================================\n");
        printf("  입장할 방 번호를 입력하세요\n");
        printf("  (0: 새로고침, -1: 돌아가기): ");
        fflush(stdout);

        scanf("%d", &choice);
        while ((c = getchar()) != '\n' && c != EOF);

        if (choice == -1) {
            return -1;
        } else if (choice == 0) {
            continue; /* 새로고침 */
        } else if (choice > 0) {
            return choice; /* 방 ID 반환 */
        }
    }
}

/* 방 입장 */
int joinRoom(int roomId) {
    NetMessage msg;
    NetMessage response;

    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_ROOM_JOIN;
    msg.x = roomId;

    if (net_send_message(netSocket, &msg) != 0) {
        printf("방 입장 요청 실패\n");
        return 0;
    }

    /* 응답 대기 */
    if (net_recv_message(netSocket, &response) != 0) {
        printf("서버 응답 실패\n");
        return 0;
    }

    switch (response.type) {
        case MSG_ROOM_JOIN_ACK:
            printf("%s\n", response.message);
            /* 게임 시작 메시지 대기 */
            if (net_recv_message(netSocket, &response) == 0 &&
                response.type == MSG_GAME_START) {
                myColor = response.player;
                strncpy(opponentNickname, response.nickname, sizeof(opponentNickname) - 1);
                isMyTurn = (myColor == 1) ? 1 : 0;

                printf("\n========================================\n");
                printf("  게임 시작!\n");
                printf("  상대방: %s\n", opponentNickname);
                printf("  내 돌: %s (%s)\n",
                       (myColor == 1) ? "흑돌" : "백돌",
                       (myColor == 1) ? "선공" : "후공");
                printf("========================================\n");
                printf("\n아무 키나 누르면 게임을 시작합니다...");
                fflush(stdout);
                _getch();
                return 1;
            }
            break;

        case MSG_ROOM_NOT_FOUND:
            printf("방을 찾을 수 없습니다.\n");
            Sleep(1000);
            break;

        case MSG_ROOM_FULL:
            printf("방이 가득 찼습니다.\n");
            Sleep(1000);
            break;

        case MSG_ERROR:
            printf("오류: %s\n", response.message);
            Sleep(1000);
            break;

        default:
            printf("알 수 없는 응답\n");
            Sleep(1000);
            break;
    }

    return 0;
}

/* 네트워크 게임 보드 출력 */
void printNetworkBoard(void) {
    int x, y;

    clearScreen();
    printf("\n======= 온라인 대전 =======\n");
    printf("나: %s (%s)  vs  상대: %s (%s)\n\n",
           player_nickname, (myColor == 1) ? "흑" : "백",
           opponentNickname, (myColor == 1) ? "백" : "흑");

    printf("   ");
    for (x = 1; x <= SIZE; x++)
        printf("%2d ", x);
    printf("\n  +");
    for (x = 0; x < SIZE * 3; x++) printf("-");
    printf("+\n");

    for (y = 0; y < SIZE; y++) {
        printf("%c |", 'A' + y);
        for (x = 0; x < SIZE; x++) {
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
    for (x = 0; x < SIZE * 3; x++) printf("-");
    printf("+\n");

    printf("흑돌: X  백돌: O\n");

    if (isMyTurn) {
        printf("\n>> 내 턴입니다! (WASD: 이동, B: 착수)\n");
    } else {
        printf("\n>> 상대방 턴... 기다리는 중...\n");
    }
    fflush(stdout);
}

/* 네트워크 게임 루프 */
void networkGameLoop(void) {
    NetMessage msg;
    NetMessage response;
    int key;
    int result;

    networkGameOver = 0;
    initBoard();
    cursorX = SIZE / 2;
    cursorY = SIZE / 2;

    hideCursor(1);

    while (!networkGameOver) {
        printNetworkBoard();

        if (isMyTurn) {
            /* 내 턴: 입력 처리 */
            if (_kbhit()) {
                key = _getch();

                switch (key) {
                    case 'w': case 'W':
                        if (cursorY > 0) cursorY--;
                        break;
                    case 's': case 'S':
                        if (cursorY < SIZE - 1) cursorY++;
                        break;
                    case 'a': case 'A':
                        if (cursorX > 0) cursorX--;
                        break;
                    case 'd': case 'D':
                        if (cursorX < SIZE - 1) cursorX++;
                        break;
                    case 'b': case 'B':
                        /* 착수 시도 */
                        if (board[cursorY][cursorX] == EMPTY) {
                            /* 서버에 착수 전송 */
                            net_create_move_msg(&msg, cursorX, cursorY, myColor);
                            if (net_send_message(netSocket, &msg) != 0) {
                                printf("\n착수 전송 실패! 연결이 끊어졌을 수 있습니다.\n");
                                networkGameOver = 1;
                                break;
                            }

                            /* 착수 확인 대기 */
                            if (net_recv_message(netSocket, &response) != 0) {
                                printf("\n서버 응답 없음! 연결이 끊어졌을 수 있습니다.\n");
                                networkGameOver = 1;
                                break;
                            }

                            if (response.type == MSG_MOVE_ACK) {
                                /* 착수 성공 */
                                board[cursorY][cursorX] = myColor;
                                lastMoveX = cursorX;
                                lastMoveY = cursorY;
                                isMyTurn = 0;
                            } else if (response.type == MSG_ERROR) {
                                printf("\n착수 실패: %s\n", response.message);
                                Sleep(1000);
                            } else if (response.type == MSG_GAME_END) {
                                /* 내가 승리 */
                                board[cursorY][cursorX] = myColor;
                                printNetworkBoard();
                                if (response.result == RESULT_BLACK_WIN) {
                                    printf("\n%s 승리!\n", (myColor == 1) ? player_nickname : opponentNickname);
                                } else if (response.result == RESULT_WHITE_WIN) {
                                    printf("\n%s 승리!\n", (myColor == 2) ? player_nickname : opponentNickname);
                                }
                                networkGameOver = 1;
                            }
                        } else {
                            printf("\n이미 돌이 있는 위치입니다!\n");
                            Sleep(500);
                        }
                        break;
                    case 27: /* ESC */
                        printf("\n게임을 종료하시겠습니까? (Y/N): ");
                        fflush(stdout);
                        key = _getch();
                        if (key == 'y' || key == 'Y') {
                            memset(&msg, 0, sizeof(msg));
                            msg.type = MSG_DISCONNECT;
                            net_send_message(netSocket, &msg);
                            networkGameOver = 1;
                        }
                        break;
                }
            }
        }

        /* 상대방 메시지 확인 (논블로킹) */
        result = net_recv_message_nonblock(netSocket, &response);
        if (result == 0) {
            switch (response.type) {
                case MSG_MOVE:
                    /* 상대방 착수 */
                    board[response.y][response.x] = response.player;
                    lastMoveX = response.x;
                    lastMoveY = response.y;
                    isMyTurn = 1;
                    break;

                case MSG_GAME_END:
                    printNetworkBoard();
                    if (response.result == RESULT_BLACK_WIN) {
                        printf("\n%s 승리!\n", (myColor == 1) ? player_nickname : opponentNickname);
                    } else if (response.result == RESULT_WHITE_WIN) {
                        printf("\n%s 승리!\n", (myColor == 2) ? player_nickname : opponentNickname);
                    } else if (response.result == RESULT_DRAW) {
                        printf("\n무승부입니다!\n");
                    }
                    networkGameOver = 1;
                    break;

                case MSG_OPPONENT_LEFT:
                    printf("\n상대방이 게임을 나갔습니다.\n");
                    networkGameOver = 1;
                    break;

                case MSG_ERROR:
                    printf("\n오류: %s\n", response.message);
                    break;

                default:
                    break;
            }
        } else if (result == -1) {
            /* 연결 끊김 */
            printf("\n서버와의 연결이 끊어졌습니다.\n");
            networkGameOver = 1;
        }

        Sleep(50);
    }

    hideCursor(0);
    printf("\n아무 키나 누르면 메뉴로 돌아갑니다...");
    fflush(stdout);
    _getch();
}

/* 온라인 대전 메뉴 */
void showOnlineMenu(void) {
    char serverIP[64];
    char roomName[ROOM_NAME_LEN];
    int port = DEFAULT_PORT;
    int c;
    int menuChoice;
    int roomId;
    int connected = 0;
    int gameStarted = 0;

    clearScreen();
    printf("\n======= 멀티플레이 (온라인) =======\n\n");

    /* 닉네임 입력 */
    printf("닉네임을 입력하세요: ");
    fflush(stdout);
    scanf("%49s", player_nickname);
    while ((c = getchar()) != '\n' && c != EOF);

    /* 서버 IP 입력 */
    printf("서버 IP를 입력하세요 (기본값: 127.0.0.1): ");
    fflush(stdout);
    if (fgets(serverIP, sizeof(serverIP), stdin) != NULL) {
        serverIP[strcspn(serverIP, "\n")] = '\0';
        if (strlen(serverIP) == 0) {
            strcpy(serverIP, "127.0.0.1");
        }
    } else {
        strcpy(serverIP, "127.0.0.1");
    }

    /* 포트 입력 */
    printf("포트를 입력하세요 (기본값: %d): ", DEFAULT_PORT);
    fflush(stdout);
    {
        char portStr[16];
        if (fgets(portStr, sizeof(portStr), stdin) != NULL) {
            portStr[strcspn(portStr, "\n")] = '\0';
            if (strlen(portStr) > 0) {
                int inputPort = atoi(portStr);
                if (inputPort > 0 && inputPort <= 65535) {
                    port = inputPort;
                }
            }
        }
    }

    /* 서버 연결 */
    if (!connectToServer(serverIP, port)) {
        printf("\n아무 키나 누르면 메뉴로 돌아갑니다...");
        fflush(stdout);
        _getch();
        return;
    }
    connected = 1;

    /* 방 메뉴 루프 */
    while (connected && !gameStarted) {
        clearScreen();
        printf("\n======= 멀티플레이 메뉴 =======\n");
        printf("  접속: %s@%s:%d\n", player_nickname, serverIP, port);
        printf("===============================\n\n");
        printf("  1. 방 만들기\n");
        printf("  2. 방 목록 (입장하기)\n");
        printf("  0. 나가기\n");
        printf("\n선택하세요: ");
        fflush(stdout);

        scanf("%d", &menuChoice);
        while ((c = getchar()) != '\n' && c != EOF);

        switch (menuChoice) {
            case 1: /* 방 만들기 */
                printf("\n방 이름을 입력하세요: ");
                fflush(stdout);
                scanf("%31s", roomName);
                while ((c = getchar()) != '\n' && c != EOF);

                if (createRoom(roomName)) {
                    gameStarted = 1;
                }
                break;

            case 2: /* 방 목록 */
                roomId = showRoomList();
                if (roomId > 0) {
                    if (joinRoom(roomId)) {
                        gameStarted = 1;
                    }
                } else if (roomId == -1) {
                    /* 돌아가기 */
                }
                break;

            case 0: /* 나가기 */
                connected = 0;
                break;

            default:
                printf("잘못된 선택입니다.\n");
                Sleep(500);
                break;
        }
    }

    /* 게임 시작 */
    if (gameStarted) {
        gameMode = 3;
        networkGameLoop();
    }

    /* 연결 종료 */
    if (netSocket != INVALID_SOCK) {
        CLOSE_SOCKET(netSocket);
        netSocket = INVALID_SOCK;
    }
    net_cleanup();
}

int main() {
    int running = 1;
    int c;
    int i, j;
    SaveData currentData;

    srand((unsigned int)time(NULL));
    initBoard();
    initAI();

    printf("\n=========시작화면=======\n");
    printf("1. 1인용 게임 \n");
    printf("2. 2인용 게임 \n");
    printf("3. 게임 불러오기 \n");
    printf("4. 랭킹 확인하기\n");
    printf("5. 종료\n");
    printf("============================\n");
    printf("메뉴 번호를 입력하세요. (1~5): ");
    scanf("%d", &gameMode);
    
    while (running) {
        showMainMenu();
        scanf("%d", &gameMode);
        /* 입력 버퍼 비우기 */
        while ((c = getchar()) != '\n' && c != EOF);

        switch (gameMode) {
            case 1:  /* 1인용 게임 */
                difficulty = selectDifficulty();
                initBoard();
                currentPlayer = BLACK;
                gameEndedByVictory = 0;
                gameLoop();

                /* 게임 종료 후 저장 여부 확인 */
                if (gameEndedByVictory == 0) {
                    for (i = 0; i < SAVE_BOARD_SIZE; i++)
                        for (j = 0; j < SAVE_BOARD_SIZE; j++)
                            currentData.board[i][j] = (i < SIZE && j < SIZE) ? board[i][j] : 0;
                    currentData.currentTurn = currentPlayer;
                    currentData.gameMode = gameMode;
                    currentData.aiDifficulty = difficulty;
                    HandleExit(&currentData);
                }
                break;

            case 2:  /* 2인용 게임 */
                clearScreen();
                printf("\n======= 2인용 게임 - 플레이어 등록 =======\n");
                while(1){
                    printf("흑돌(선공) 플레이어 닉네임(15자 이내): ");
                    fflush(stdout);
				    scanf("%s", player1_nickname);
                    if(strlen(player1_nickname)>15){
                        printf("닉네임이 너무 깁니다.\n");
                        _getch();
                        printf("\x1b[1F");
                        printf("\x1b[2K");
                        printf("\x1b[1F");
                        printf("\x1b[2K");
                    }
                    else break;
                }
                while(1){
                    printf("백돌(후공) 플레이어 닉네임(15자 이내): ");
                    fflush(stdout);
				    scanf("%s", player2_nickname);
                    if(strlen(player2_nickname)>15){
                        printf("닉네임이 너무 깁니다.\n");
                        _getch();
                        printf("\x1b[1F");
                        printf("\x1b[2K");
                        printf("\x1b[1F");
                        printf("\x1b[2K");
                    }
                    else break;
                }
                printf("\n게임을 시작합니다! (%s vs %s)\n", player1_nickname, player2_nickname);
                printf("아무 키나 누르면 게임이 시작됩니다...");
                fflush(stdout);
                /* 입력 버퍼 비우기 */
                while ((c = getchar()) != '\n' && c != EOF);
                _getch();

                initBoard();
                currentPlayer = BLACK;
                gameEndedByVictory = 0;
                gameLoop();

                /* 게임 종료 후 저장 여부 확인 */
                if (gameEndedByVictory == 0) {
                    for (i = 0; i < SAVE_BOARD_SIZE; i++)
                        for (j = 0; j < SAVE_BOARD_SIZE; j++)
                            currentData.board[i][j] = (i < SIZE && j < SIZE) ? board[i][j] : 0;
                    currentData.currentTurn = currentPlayer;
                    currentData.gameMode = gameMode;
                    currentData.aiDifficulty = difficulty;
                    HandleExit(&currentData);
                }
                break;

            case 3:  /* 온라인 대전 */
                showOnlineMenu();
                break;

            case 4:  /* 게임 불러오기 */
                if (LoadSelectedGame()) {
                    printf("아무 키나 누르면 게임을 시작합니다...");
                    _getch();
                    gameEndedByVictory = 0;
                    gameLoop();

                    /* 게임 종료 후 저장 여부 확인 */
                    if (gameEndedByVictory == 0) {
                        for (i = 0; i < SAVE_BOARD_SIZE; i++)
                            for (j = 0; j < SAVE_BOARD_SIZE; j++)
                                currentData.board[i][j] = (i < SIZE && j < SIZE) ? board[i][j] : 0;
                        currentData.currentTurn = currentPlayer;
                        currentData.gameMode = gameMode;
                        currentData.aiDifficulty = difficulty;
                        HandleExit(&currentData);
                    }
                }
                /* 불러오기 실패/취소 시 자동으로 메뉴로 돌아감 */
                break;

            case 5:  /* 랭킹 확인 */
                print_rankings();
                /* 랭킹 확인 후 자동으로 메뉴로 돌아감 */
                break;

            case 6:  /* 종료 */
                running = 0;
                break;

            default:
                printf("잘못된 입력입니다. 다시 선택해주세요.\n");
                Sleep(1000);
                break;
        }
    }

    printf("\n프로그램을 종료합니다...\n");
    cleanupAI();
    return 0;
}