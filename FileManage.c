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