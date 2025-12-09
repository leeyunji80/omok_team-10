#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h" 
#include <time.h>
#include <conio.h>
#include <ctype.h>
#define DATA_FILE "user_data.json"

typedef struct {
    char nickname[50];
    int wins;
    int losses;
    double win_rate;
    char time[20];
} RankPlayer;

int compare_players(const void* a, const void* b) {
    RankPlayer* p1 = (RankPlayer*)a;
    RankPlayer* p2 = (RankPlayer*)b;

    if (p1->win_rate < p2->win_rate) return 1;
    if (p1->win_rate > p2->win_rate) return -1;
    if (p1->wins < p2->wins) return 1;
    if (p1->wins > p2->wins) return -1;
    return 0;
}

void update_game_result(const char* nickname, int did_win) {
    cJSON* root = NULL;
    FILE* fp = NULL;
    char* buffer = NULL;
    long length = 0;
    time_t tim=time(NULL);
    struct tm tm = *localtime(&tim);
    char date_str[10];
    sprintf_s(date_str, sizeof(date_str), "%02d/%02d", tm.tm_mon + 1, tm.tm_mday);

    if (fopen_s(&fp, "user_data.json", "r") != 0 || fp == NULL) {
        root = cJSON_CreateArray();
    }
    else {
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
            if (root == NULL) {
                root = cJSON_CreateArray();
            }
        }
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
        cJSON* time = cJSON_GetObjectItem(item, "time");


        if (name && wins && losses && rate && time) {
            strcpy_s(players[i].nickname, sizeof(players[i].nickname), name->valuestring);
            players[i].wins = wins->valueint;
            players[i].losses = losses->valueint;
            players[i].win_rate = rate->valuedouble;
            strcpy_s(players[i].time, sizeof(players[i].time), time->valuestring);
        }
    }

    qsort(players, size, sizeof(RankPlayer), compare_players);
    
    system("cls");
    printf("\n====== 랭킹 (상위 5명) ======\n");
    printf("%-5s %-15s %-10s %-10s %-10s\n", "순위", "닉네임", "승률", "전적", "마지막 플레이");
    printf("------------------------------------------------------------\n");

    int limit = (size < 5) ? size : 5;
    for (int i = 0; i < limit; i++) {
        printf("%-5d %-15s %.1f%% %5d승 %3d패 %11s\n",
            i + 1,
            players[i].nickname,
            players[i].win_rate * 100,
            players[i].wins,
            players[i].losses,
            players[i].time);
    }
    printf("------------------------------------------------------------\n");

    free(players);
    cJSON_Delete(root);
    _getch();
}


#define SAVE_BOARD_SIZE 15
#define MAX_SAVE_SLOTS 5

typedef struct {
    int board[SAVE_BOARD_SIZE][SAVE_BOARD_SIZE];
    int currentTurn;
    int gameMode;
} SaveData;

void get_filename(char* buffer) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(buffer, "%04d년%02d월%02d일_%02d시%02d분%02d초.dat",
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
   
    if (scanf_s("%d", &choice) != 1) { 
        while (getchar() != '\n');
        return 0;
    }
    while (getchar() != '\n');

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
    memset(data->board, 0, sizeof(data->board));
    
    data->currentTurn = 1;
    data->gameMode = 2;
}
