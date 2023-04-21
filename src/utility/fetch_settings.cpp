//
// Created by ysuho on 19-Nov-22.
//

#include "fetch.h"
#include <EEPROM.h>
#include "ESP8266TrueRandom.h"
#include "ArduinoLog.h"
#include "fetch_settings.h"
#include "config.h"

void fetch_settings(EEPROM_Config *config) {
    EEPROM.begin(4096);
    // Check if first 36 bytes of EEPROM are the same (likely from the factory) or there are non-ASCII characters
    // If so, generate UUID (which is 36-bit)
    for (uint16_t i = 0; i < sizeof(EEPROM_Config); i++) {
        ((char *) config)[i] = (char) EEPROM.read(i);
    }
    Log.verboseln(F("My UUID is %s"), config->uuid);
    Log.verboseln(F("REST host is %s"), config->host);
    Log.verboseln(F("REST code is %s"), config->code);
    EEPROM.end();
}

void store_settings(EEPROM_Config *config) {
    EEPROM.begin(4096);
    for (uint16_t i = 0; i < sizeof(EEPROM_Config); i++) {
        EEPROM.write(i, ((char *) config)[i]);
    }
    EEPROM.commit();
    EEPROM.end();
}

uint8_t is_first_launch() {
    EEPROM.begin(4096);
    // Check if first 36 bytes of EEPROM are the same (likely from the factory) or there are non-ASCII characters
    // If so, generate UUID (which is 36-bit)
    bool is_first_launch = true;
    bool non_ascii = false;
    uint8_t eeprom_content = EEPROM.read(0);
    for (uint16_t i = 1; i < sizeof(EEPROM_Config); i++) {
        char c = EEPROM.read(i);
        is_first_launch &= (eeprom_content == c);
        non_ascii |= (c > 127);
    }
    EEPROM.end();
    return is_first_launch || non_ascii;
}

void generate_default_settings(EEPROM_Config *config) {
    Log.verboseln(F("Looks like this is the first launch. Generating UUID"));
    uint8_t uuid[16];
    ESP8266TrueRandom.uuid(uuid);
    strcpy(config->uuid, ESP8266TrueRandom.uuidToString(uuid).c_str());
    strcpy(config->host, REST_HOST);
    strcpy(config->code, CODE);
}