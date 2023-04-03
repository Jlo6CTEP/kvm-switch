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

void reboot(char * reason, Display display) {
    Log.verboseln(F("Rebooting, reason: %s"), reason);
    display.message=reason;
    display.draw();
    delay(5000);
    EspClass::restart();
}

uint16_t fetch_timezone_offset(Display display) {
    String response = "";
    retry_if_needed(
            ([&]() -> uint8_t { return access_api(TIMEZONE_SERVER, GET, &response); }),
            nullptr,
            [&display]() {reboot((char*)TIME_SYNC_ERROR, display);},
            RETRY_COUNT
    );
    uint16_t time_offset = strtol(response.c_str(), nullptr, 10);
    time_offset = 3600 * (time_offset / 100) + 60 * (time_offset % 100);
    return time_offset;
}

void activate_device(const String &url, const String &uuid, String *rest_url, String *websocket_url, uint32_t *club_id,
                     Display display) {
    char request_buffer[96] = {0};
    String response = "";
    StaticJsonDocument<64> filter;
    filter["result"]["club_id"] = true;
    filter["result"]["server_url"] = true;
    filter["result"]["websocket_url"] = true;
    DynamicJsonDocument doc(96);
    DynamicJsonDocument activation(192);
    doc["uuid"] = uuid;
    doc["code"] = CODE;
    serializeJson(doc, request_buffer);
    retry_if_needed(
            ([&response, &request_buffer, &activation, &filter, &url]() -> uint8_t {
                int8_t resp = access_api((url+ACTIVATE).c_str(), POST, &response, request_buffer,
                                         {{(char *) "Content-Type", (char *) "application/json"}});
                if (resp == -1) return -1;
                resp = chunked_read(response, &response);
                if (resp == -1) return -1;
                DeserializationError err = deserializeJson(activation, response, DeserializationOption::Filter(filter));
                if (err) return -1;
                return 0;
            }),
            nullptr,
            [&display]() {reboot((char*)ACTIVATION_ERROR, display);},
            5
    );
    Log.verboseln("Read response %s", response.c_str());
    *club_id = activation["result"]["club_id"];
    *rest_url = (const char *)activation["result"]["server_url"];
    *websocket_url = (const char *)activation["result"]["websocket_url"];
}

void
authorize_device(const String &url, const String &uuid, uint32_t club_id, String *token, uint32_t *id, Display display) {
    char request_buffer[96] = {0};
    String response = "";
    DynamicJsonDocument auth_request(96);
    auth_request["uuid"] = uuid;
    auth_request["shell_ver"] = "1.0.1";
    auth_request["club_id"] = club_id;
    Log.noticeln("Fetching token");
    serializeJson(auth_request, request_buffer);

    StaticJsonDocument<64> filter;
    filter["result"]["jwt"] = true;
    filter["result"]["id"] = true;

    DynamicJsonDocument doc(384);
    retry_if_needed(
            ([&response, &request_buffer, &doc, &filter, &url]() -> uint8_t {
                int8_t resp = access_api((url+AUTHORIZE).c_str(), POST, &response, request_buffer,
                                         {{(char *) "Content-Type", (char *) "application/json"}});
                if (resp == -1) return -1;
                resp = chunked_read(response, &response);
                if (resp == -1) return -1;
                DeserializationError err = deserializeJson(doc, response, DeserializationOption::Filter(filter));
                if (err) return -1;
                return 0;
            }),
            nullptr,
            [&display]() {reboot((char*)AUTH_ERROR, display);},
            5
    );
    Log.verboseln("Read response %s", response.c_str());
    *token = (const char *)doc["result"]["jwt"];
    *id = doc["result"]["id"];
}

void test_token(const String &url, const String &token, Display display) {
    String response = "";
    retry_if_needed(
            ([&response, &token, &url]() -> uint8_t {
                return access_api((url+TEST_JWT).c_str(), GET, &response, "", {{(char *) "jwt", (char *) token.c_str()}});
            }),
            nullptr,
            [&display]() {reboot((char*)TOKEN_ERROR, display);},
            5
    );
}