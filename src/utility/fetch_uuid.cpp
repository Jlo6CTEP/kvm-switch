//
// Created by ysuho on 19-Nov-22.
//

#include "fetch.h"
#include <EEPROM.h>
#include "ESP8266TrueRandom.h"
#include "ArduinoLog.h"

void fetch_uuid(char * uuid_buffer){
    EEPROM.begin(4096);
    // Check if first 36 bytes of EEPROM are the same (likely from the factory)
    // If so, generate UUID (which is 36-bit)
    bool is_first_launch = true;
    uint8_t eeprom_content = EEPROM.read(0);
    for (uint16_t i = 1; i < 36; i++) {
        is_first_launch &= eeprom_content == EEPROM.read(i);
    }

    if (is_first_launch) {
        Log.verboseln("Looks like this is the first launch. Generating UUID");
        uint8_t uuid[16];
        ESP8266TrueRandom.uuid(uuid);
        strcpy(uuid_buffer, ESP8266TrueRandom.uuidToString(uuid).c_str());
        for (uint8_t i = 0; i < 36; i++) {
            EEPROM.write(i, uuid_buffer[i]);
        }
        EEPROM.commit();
        Log.verboseln("UUID %s saved", uuid_buffer);
    } else {
        char fetched_uuid[37] = {0};
        for (uint8_t i = 0; i < 36; i++) {
            fetched_uuid[i] = (char)EEPROM.read(i);
        }
        strcpy(uuid_buffer, fetched_uuid);
        Log.verboseln("My UUID is %s", uuid_buffer);
    }
    EEPROM.end();
}