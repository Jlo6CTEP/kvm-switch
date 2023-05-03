//
// Created by ysuho on 19-Nov-22.
//

#include "api.h"
#include "ArduinoLog.h"
#include "retry_command.h"
#include "api_utils.h"
#include "config.h"
#include "ArduinoJson.h"
#include "reboot_reasons.h"
#include "app/display.h"

#define RESPONSE_BUFFER_LEN 512
#define MFLN_BUFFER_LEN 4096
char response_buffer[RESPONSE_BUFFER_LEN];

void reboot(char * reason, Display display) {
    Log.verboseln(F("Rebooting, reason: %s"), reason);
    display.message=reason;
    display.draw();
    delay(5000);
    EspClass::restart();
}

uint16_t fetch_timezone_offset(Display display) {
    retry_if_needed(
            ([&]() -> uint8_t { return access_api(
                    TIMEZONE_SERVER, GET, response_buffer, RESPONSE_BUFFER_LEN); }),
            nullptr,
            [&display]() {reboot((char*)TIME_SYNC_ERROR, display);},
            RETRY_COUNT
    );
    Log.verboseln(F("Done with retry"));
    uint16_t time_offset = strtol(response_buffer, nullptr, 10);
    time_offset = 3600 * (time_offset / 100) + 60 * (time_offset % 100);
    return time_offset;
}

void activate_device(const String &url, const String &uuid, String *rest_url, String *websocket_url,
                     uint32_t *club_id, const String &code, int32_t *time_offset, Display display) {
    char request_buffer[96] = {0};
    StaticJsonDocument<128> filter;
    filter["result"]["club_id"] = true;
    filter["result"]["server_url"] = true;
    filter["result"]["websocket_url"] = true;
    filter["result"]["utc_offset"] = true;
    StaticJsonDocument<96> doc;
    StaticJsonDocument<256> activation;
    doc["uuid"] = uuid;
    doc["code"] = code;
    serializeJson(doc, request_buffer);
    retry_if_needed(
            ([&request_buffer, &activation, &filter, &url]() -> uint8_t {
                int8_t resp = access_api(
                        (url+ACTIVATE).c_str(), POST, response_buffer, RESPONSE_BUFFER_LEN, request_buffer,
                        {{(char *) "Content-Type", (char *) "application/json"}}, MFLN_BUFFER_LEN);
                if (resp == -1) return -1;
                resp = chunked_read(response_buffer, response_buffer, RESPONSE_BUFFER_LEN);
                if (resp == -1) return -1;
                DeserializationError err = deserializeJson(activation, response_buffer, DeserializationOption::Filter(filter));
                if (err) return -1;
                return 0;
            }),
            nullptr,
            [&display]() {reboot((char*)ACTIVATION_ERROR, display);},
            5
    );
    Log.verboseln(F("Done with retry"));
    Log.verboseln(F("Read response %s"), response_buffer);
    *club_id = activation["result"]["club_id"];
    *rest_url = (const char *)activation["result"]["server_url"];
    *websocket_url = (const char *)activation["result"]["websocket_url"];
    *time_offset=activation["result"]["utc_offset"];
}

void
authorize_device(const String &url, const String &uuid, uint32_t club_id, String *token, uint32_t *id, Display display) {
    char request_buffer[96] = {0};
    StaticJsonDocument<96> auth_request;
    auth_request["uuid"] = uuid;
    auth_request["shell_ver"] = "1.0.1";
    auth_request["club_id"] = club_id;
    Log.noticeln("Fetching token");
    serializeJson(auth_request, request_buffer);

    StaticJsonDocument<64> filter;
    filter["result"]["jwt"] = true;
    filter["result"]["id"] = true;

    StaticJsonDocument<384> doc;
    retry_if_needed(
            ([&request_buffer, &doc, &filter, &url]() -> uint8_t {
                int8_t resp = access_api(
                        (url+AUTHORIZE).c_str(), POST, response_buffer, RESPONSE_BUFFER_LEN, request_buffer,
                        {{(char *) "Content-Type", (char *) "application/json"}}, MFLN_BUFFER_LEN);
                if (resp == -1) return -1;
                resp = chunked_read(response_buffer, response_buffer, RESPONSE_BUFFER_LEN);
                if (resp == -1) return -1;
                DeserializationError err = deserializeJson(doc, response_buffer, DeserializationOption::Filter(filter));
                if (err) return -1;
                return 0;
            }),
            nullptr,
            [&display]() {reboot((char*)AUTH_ERROR, display);},
            5
    );
    Log.verboseln("Read response %s", response_buffer);
    *token = (const char *)doc["result"]["jwt"];
    *id = doc["result"]["id"];
}

void test_token(const String &url, const String &token, Display display) {
    retry_if_needed(
            ([&token, &url]() -> uint8_t {
                return access_api(
                        (url+TEST_JWT).c_str(), GET, response_buffer,
                        RESPONSE_BUFFER_LEN, "",
                        {{(char *) "jwt", (char *) token.c_str()}}, MFLN_BUFFER_LEN);
            }),
            nullptr,
            [&display]() {reboot((char*)TOKEN_ERROR, display);},
            5
    );
}