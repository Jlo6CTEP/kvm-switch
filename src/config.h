//
// Created by ysuho on 19-Nov-22.
//

#include <map>

#ifndef SWITCH_CONFIG_H
    #define SWITCH_CONFIG_H

    //#define TEST

    #ifdef TEST
        #define WSS_HOST "localhost"
        #define WSS_PORT 8500
        #define REST_HOST "http://localhost"
    #else
        #define WSS_HOST "dev-oasys.ru"
        #define WSS_PORT 8501
        #define REST_HOST "https://stage.dev-oasys.ru"
    #endif

    #define WSS_URL "/connection/websocket"
    #define AUTHORIZE "kvm/authorize"
    #define ACTIVATE "/method/kvm/activate"
    #define TEST_JWT "kvm"
    #define TIMEZONE_SERVER  "https://ipapi.co/utc_offset/"
    #define CODE "HKU95V6SV"
    #define AP_PASSWORD "kotikiii"
    #define RETRY_COUNT 5
    #define CONFIG_PORTAL_TIMEOUT 240
    #define SERIAL_CONFIG_COMMAND "config"


    enum Commands {
        START,
        STOP,
        MESSAGE,
        ADD_TIME
    };

    static std::map<String, Commands> command_map = {
            {"start", START},
            {"stop", STOP},
            {"show_message", MESSAGE},
            {"add_time", ADD_TIME}
    };

    enum AppState {
        BOOTING,
        IDLE,
        GOT_MESSAGE,
        ACTIVE,
    };
#endif //SWITCH_CONFIG_H