//
// Created by ysuho on 19-Nov-22.
//
#ifndef SWITCH_UTILITY_H
#define SWITCH_UTILITY_H

#endif //SWITCH_UTILITY_H

#define UUID_LEN 36
#define REST_HOST_LEN 50
typedef struct {
    char uuid[UUID_LEN+1];
    char host[REST_HOST_LEN+1];
} EEPROM_Config;

void fetch_settings(EEPROM_Config * config);

void store_settings(EEPROM_Config * config);

uint8_t is_first_launch();

void generate_default_settings(EEPROM_Config *config);

