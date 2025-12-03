#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
<<<<<<< HEAD
#include <stdlib.h>
#include "cJSON.h"
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <ctype.h>
#include <string.h>
#include "minimax.h"

#define SIZE 15
#define BLACK 1
#define WHITE 2
#define EMPTY 0
#define SAVE_BOARD_SIZE 15
#define MAX_SAVE_SLOTS 5
#define BOARD_SIZE SIZE
#define MAX_MOVES 225

typedef struct{
    int board[SAVE_BOARD_SIZE][SAVE_BOARD_SIZE];
    int currentTurn;
    int gameMode;
} SaveData;

typedef struct{
    int score;
    int row;
    int col;
}MoveResult;

typedef struct{
    int row;
    int col;
}Move;

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

/*==========전역 변수 상태=============*/
int board[SIZE][SIZE];
int cursorX = 0, cursorY = 0;
int currentPlayer = BLACK;
int gameMode = 0; // 1=1인용, 2=2인용
int lastMoveX = -1, lastMoveY = -1;
char player_nickname[50] = "Player";

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