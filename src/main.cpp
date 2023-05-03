#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <ArduinoLog.h>
#include <timeSync.h>
#include <EEPROM.h>
#include "utility/fetch_settings.h"
#include "utility/api_utils.h"
#include "utility/retry_command.h"
#include "utility/api.h"
#include "config.h"
#include "app/display.h"
#include "neotimer.h"
#include "reboot_reasons.h"
#include "heap_stats.h"

#define LOG_LEVEL LOG_LEVEL_VERBOSE
#define DEBUG LOG_LEVEL >= LOG_LEVEL_VERBOSE


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);
WiFiManager wifiManager;
Neotimer enabled_timer;
Neotimer message_timer = Neotimer(60000);

WiFiManagerParameter *setting_host = nullptr;
WiFiManagerParameter *setting_code = nullptr;

char request_buffer[256] = {0};
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
uint32_t id;
uint32_t club_id;
EEPROM_Config config = {{0}, {0}};
String token = "";

bool settings_save_needed = false;

String host_url = "";
String wss_uri = "";

// display also uses this state for it is passed by-pointer
uint8_t state = AppState::BOOTING;
uint8_t last_state = AppState::BOOTING;

Display display(u8g2, &state);
Neotimer terminated_message_timer = Neotimer(15000);

void enable_hdmi();
void disable_hdmi();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void save_config();
void generate_ap_name(EEPROM_Config * config, char ap_name[17]);
void attempt_config();
void attempt_connect_to_saved_network();

void setup() {
    pinMode(D0, OUTPUT);
    pinMode(D7, INPUT);
    pinMode(D6, OUTPUT);
    analogWriteFreq(100);
    disable_hdmi();
    Serial.begin(115200);
    Log.begin(LOG_LEVEL, &Serial);
    wifiManager.setDebugOutput(DEBUG);
    wifiManager.setBreakAfterConfig(true);
    wifiManager.setSaveConfigCallback(save_config);
    Serial.setDebugOutput(DEBUG);
    display.begin();
    display.message = "Initialising...";
    display.draw();

    for(uint8_t t = 4; t > 0; t--) {
        Log.noticeln(F("[SETUP] BOOT WAIT %d..."), t);
        Serial.flush();
        delay(1000);
    }

    Log.verboseln(F("Fetching UUID"));

    if (is_first_launch()) {
        generate_default_settings(&config);
        store_settings(&config);
    }
    fetch_settings(&config);
    setting_code = new WiFiManagerParameter("code", "code", config.code, CODE_LEN);
    setting_host = new WiFiManagerParameter("host", "host", config.host, REST_HOST_LEN);
    wifiManager.addParameter(setting_host);
    wifiManager.addParameter(setting_code);

    for(uint8_t t = 0; t < 10; t++) {
        attempt_config();
        Log.noticeln(F("[SETUP] Waiting for config"));
        Serial.flush();
        delay(1000);
    }


    // 12 from UUID + 4 from KVM- + 1 terminator
    char ap_name[17] = {0};
    generate_ap_name(&config, ap_name);

    display.message = "Connecting to WiFi...";
    display.draw();
    attempt_connect_to_saved_network();
    if (WiFi.status() != WL_CONNECTED) {
        Log.verboseln(F("Launching captive portal with AP name %s"), ap_name);
        wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
        display.message = (String)"Connect to " + ap_name + " and configure WiFI";
        display.draw();
        bool res = wifiManager.autoConnect(ap_name, AP_PASSWORD);
        if (!res) {
            reboot((char*)NO_WIFI_CONFIGURED, display);
        }

        display.message = "Connecting to WiFi...";
        display.draw();
        attempt_connect_to_saved_network();
    }

    if (WiFi.status() != WL_CONNECTED) {
        reboot((char*)NO_WIFI_CONNECTION, display);
    }

    Log.noticeln(F("Connected to WiFI"));
    // set TZ
    display.message = "Syncing time...";
    display.draw();

    heap_stats();

     Log.noticeln(F("Connected to WiFI"));
    // set TZ

    timeSync.begin();
    timeSync.waitForSyncResult();
    //int32_t time_offset = fetch_timezone_offset(display);
    int32_t time_offset = 0;
    Log.noticeln(F("Timezone offset %d"), time_offset);

    Log.noticeln(F("Activating the device"));
    activate_device(config.host, config.uuid, &host_url,
                    &wss_uri, &club_id, config.code, &time_offset, display);
    timeClient.setTimeOffset(time_offset);
    timeClient.update();
    display.time_now = timeClient.getEpochTime();
    display.draw();
    heap_stats();

    heap_stats();

    display.time_now = timeClient.getEpochTime();

    display.message = "Authorising...";
    display.draw();
    Log.noticeln(F("Activating the device"));

    int32_t kek = 0;
    activate_device(config.host, config.uuid, &host_url,
                    &wss_uri, &club_id, config.code, &kek, display);
    heap_stats();

    Log.noticeln(F("Authorizing the device to club %d"), club_id);
    authorize_device(host_url, config.uuid, club_id, &token, &id, display);
    heap_stats();

    Log.verboseln(F("Testing token"));
    test_token(host_url, token, display);
    heap_stats();
    Log.verboseln(F("Connecting to WSS"));

    int8_t start_index = (int8_t)wss_uri.indexOf(":");
    if (start_index != -1) {
        int8_t res = (int8_t)memcmp(&wss_uri.c_str()[start_index], "://", 3);
        start_index = res == 0 ? (int8_t)start_index + 2 : 0;
    } else {
        start_index = 0;
    }

    auto port_start = (int8_t)wss_uri.indexOf(":", start_index);
    auto url_start =(int8_t)wss_uri.indexOf("/", port_start);
    url_start = url_start == -1 ? (int8_t)wss_uri.length() : url_start;

    String wss_host = wss_uri.substring(start_index+1, port_start);
    String wss_port = wss_uri.substring(port_start+1, url_start);
    String wss_url = wss_uri.substring(url_start);

    Log.verboseln(F("Attempting a connection to %s on port %s with url %s"), wss_host.c_str(), wss_port.c_str(), wss_url.c_str());

    webSocket.beginSSL(wss_host.c_str(), wss_port.toInt(), wss_url.c_str());
    webSocket.onEvent(webSocketEvent);
    display.message = "Done initialisation.";
    display.draw();
    state = AppState::IDLE;
    delay(1000);
    display.message = "Ready to serve";
    display.draw();
}

void loop() {

    // Reconnect if no Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
        for (uint8_t i = 0; i < RETRY_COUNT; i++) {
            Log.noticeln(F("No wifi connection"));
            WiFi.reconnect();
            delay(1000);
        }

        if (WiFi.status() != WL_CONNECTED) {
            reboot((char*)NO_WIFI_CONNECTION, display);
        }
    }


    //Remove the termination message when timer expires
    if (terminated_message_timer.done()) {
        terminated_message_timer.stop();
        if (!message_timer.waiting()) {
            display.message = "Ready to serve";
        }
    }

    //When the message timer expires, remove the message from screen
    if (message_timer.done()) {
        message_timer.stop();
        state = last_state;
        Log.noticeln(F("Changing state to %d"), state);
        display.message = "Ready to serve";
    }

    //Terminate the user session
    if (enabled_timer.done()) {
        disable_hdmi();
        enabled_timer.stop();
        state = AppState::IDLE;
        display.message = "Ready to serve";
        Log.noticeln(F("Timer done"));
        Log.noticeln(F("Changing state to %d"), state);
    }

    timeClient.update();
    display.time_now = timeClient.getEpochTime();
    display.time_remaining = enabled_timer.started() ? (enabled_timer.get() - enabled_timer.getEllapsed()) / 1000 : 0;

    // display accepts state as a pointer, so no need to update it explicitly for the display as well
    display.draw();
    webSocket.loop();

    // check serial commands
    attempt_config();
    //save parameters
}

void attempt_config() {
    if (Serial.available()) {
        webSocket.disconnect();
        char buffer[10] = {0};
        heap_stats();
        Serial.readBytesUntil('\n', buffer, 10);
        Log.verboseln(F("Got serial command: %s"), buffer);
        if (strcmp(buffer, SERIAL_CONFIG_COMMAND) != 0) {
            return;
        }

        char ap_name[17] = {0};
        generate_ap_name(&config, ap_name);
        display.message = (String)"Connect to " + ap_name + " and configure WiFI";
        display.draw();


        wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);\
        heap_stats();
        wifiManager.startConfigPortal(ap_name, AP_PASSWORD);
    }

    if (!settings_save_needed) return;

    Log.verboseln(F("Initiating settings save"));
    strcpy(config.host, setting_host->getValue());
    strcpy(config.code, setting_code->getValue());
    store_settings(&config);
    settings_save_needed = false;
    Log.verboseln(F("Settings save completed"));
    reboot((char*)APPLY_SETTINGS, display);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    StaticJsonDocument<384> payload_doc;
    switch(type) {
        case WStype_t::WStype_DISCONNECTED:
            Log.noticeln(F("[WSc] Disconnected!\n"));
            break;
        case WStype_t::WStype_CONNECTED: {
            Log.noticeln(F("[WSc] Connected to url: %s\n"),  payload);
            JsonObject params = payload_doc.createNestedObject("params");
            params[F("token")] = token;
            params[F("name")] = "js";
            payload_doc[F("id")] = 1;
            serializeJson(payload_doc, request_buffer);
            Log.noticeln(F("[WSc] Sending payload: %s"),  request_buffer);
            webSocket.sendTXT(request_buffer);

            StaticJsonDocument<128> subscribe;
            subscribe[F("method")] = 1;
            String channel = "";
            channel =  (String) "club" + club_id + "#" + id;
            subscribe[F("params")][F("channel")] = channel;
            subscribe[F("id")] = 2;
            serializeJson(subscribe, request_buffer);
            Log.noticeln(F("[WSc] Sending payload: %s"),  request_buffer);
            webSocket.sendTXT(request_buffer);
        }
            break;
        case WStype_t::WStype_TEXT: {
            heap_stats();
            Log.noticeln(F("[WSc] get text: %s\n"), payload);
            StaticJsonDocument<64> filter;
            filter[F("result")][F("data")][F("data")] = true;
            deserializeJson(payload_doc, payload, DeserializationOption::Filter(filter));

            String command = payload_doc[F("result")][F("data")][F("data")][F("command")];
            switch(command_map.count(command)?command_map[command]: -1) {
                case Commands::START: {
                    unsigned long start = payload_doc[F("result")][F("data")][F("data")][F("message")][F("time")];
                    Log.noticeln(F("Got a start for %d sec"), start);
                    enabled_timer.set(start * 1000);
                    enabled_timer.start();
                    state=AppState::ACTIVE;
                    Log.noticeln(F("Changing state to %d"), state);
                    enable_hdmi();
                    break;
                }
                case Commands::STOP: {
                    if (state != AppState::ACTIVE){
                        break;
                    }
                    terminated_message_timer.start();
                    String text = payload_doc[F("result")][F("data")][F("data")][F("message")][F("reason")];
                    Log.noticeln(F("Stopped with reason %s"), text.c_str());
                    enabled_timer.stop();
                    display.message = text;
                    state=AppState::IDLE;
                    Log.noticeln(F("Changing state to %d"), state);
                    disable_hdmi();
                    break;
                }
                case Commands::MESSAGE: {
                    message_timer.start();
                    String text = payload_doc[F("result")][F("data")][F("data")][F("message")][F("text")];
                    Log.noticeln(F("Got a message %s"), text.c_str());
                    last_state=state;
                    state=AppState::GOT_MESSAGE;
                    display.message = text;
                    Log.noticeln(F("Changing state to %d"), state);
                    break;
                }
                case Commands::ADD_TIME: {
                    long time = payload_doc[F("result")][F("data")][F("data")][F("message")][F("time")];
                    Log.noticeln(F("Add time %d"), time);
                    time *= 1000;
                    unsigned long timer_time = enabled_timer.get();
                    // prevent underflow here
                    enabled_timer.set(time < 0 && timer_time < -time ? 0 : (enabled_timer.get() + time) );
                    break;
                }
                case -1:
                    String pretty;
                    serializeJsonPretty(payload_doc, pretty);
                    Log.noticeln(F("[WSc] Malformed input: %s\n"), pretty.c_str());
            }
            Log.noticeln(F("[WSc] get text: %s\n"), payload);
        }
            break;
        default:
            break;
    }
}

void enable_hdmi(){
    analogWrite(D0, 0);
    //digitalWrite(D7, HIGH);
    analogWrite(D6, 0);
}

void disable_hdmi(){
    analogWrite(D0, 128);
    //digitalWrite(D7, HIGH);
    analogWrite(D6, 128);
}

void save_config(){
    settings_save_needed = true;
}

void generate_ap_name(EEPROM_Config * conf, char ap_name[17]) {
    memcpy(ap_name, "KVM", 3);
    memcpy(ap_name+3, conf->uuid + 23, 13);
}

void attempt_connect_to_saved_network(){
    uint8_t count = 0;
    Log.verboseln(F("Attempting connection to %s"), WiFi.SSID().c_str());
    while (WiFi.waitForConnectResult() != WL_CONNECTED && count < RETRY_COUNT) {
        Log.noticeln(F("Connection attempt %d"), count+1);
        count++;
        WiFi.begin();
    }
}