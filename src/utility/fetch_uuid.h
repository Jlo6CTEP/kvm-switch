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

void fetch_parameters(EEPROM_Config * config);
