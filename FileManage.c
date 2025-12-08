#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h" 
#include <time.h>
#include <conio.h>
#include <ctype.h>

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
    typedef struct {
        char nickname[50];
        int wins;
        int losses;
        double win_rate;
        char time[6];
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
        cJSON* time = cJSON_GetObjectItem(item, "time");


        if (name && wins && losses && rate && time) {
            strcpy_s(players[i].nickname, sizeof(players[i].nickname), name->valuestring);
            players[i].wins = wins->valueint;
            players[i].losses = losses->valueint;
            players[i].win_rate = rate->valuedouble;
            strcpy_s(players[i].time, sizeof(players[i].time), time->valuestring);
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


/* 게임 저장/불러오기 함수는 GameControl.c에서 JSON 기반으로 구현됨 */
