#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <ArduinoLog.h>
#include <timeSync.h>
#include "utility/fetch_uuid.h"
#include "utility/api_utils.h"
#include "utility/retry_command.h"
#include "utility/api.h"
#include "config.h"
#include "app/display.h"
#include "neotimer.h"
#include "error_codes.h"
#define LOG_LEVEL LOG_LEVEL_VERBOSE
#define DEBUG LOG_LEVEL >= LOG_LEVEL_VERBOSE

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);
WiFiManager wifiManager;
Neotimer enabled_timer;
Neotimer message_timer = Neotimer(60000);

char request_buffer[512] = {0};
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
uint32_t id;
uint32_t club_id;
EEPROM_Config config = {{0}, {0}};
String token = "";

// display also uses this state for it is passed by-pointer
uint8_t state = AppState::BOOTING;
uint8_t last_state = AppState::BOOTING;

Display display(u8g2, &state);
Neotimer terminated_message_timer = Neotimer(15000);

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


void heap_stats() {
    uint32_t hfree;
    uint16_t hmax;
    uint8_t hfrag;
    EspClass::getHeapStats(&hfree, &hmax, &hfrag);
    Log.verboseln("Free heap %d max heap %d frag %d", hfree, hmax, hfrag);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_t::WStype_DISCONNECTED:
            Log.noticeln(F("[WSc] Disconnected!\n"));
            break;
        case WStype_t::WStype_CONNECTED: {
            Log.noticeln(F("[WSc] Connected to url: %s\n"),  payload);
            DynamicJsonDocument login(256);
            JsonObject params = login.createNestedObject("params");
            params["token"] = token;
            params["name"] = "js";
            login["id"] = 1;
            serializeJson(login, request_buffer);
            Log.noticeln(F("[WSc] Sending payload: %s"),  request_buffer);
            webSocket.sendTXT(request_buffer);

            DynamicJsonDocument subscribe(96);
            subscribe["method"] = 1;
            String channel = "";
            channel.reserve(32);
            channel =  (String) "club" + club_id + "#" + id;
            subscribe["params"]["channel"] = channel;
            subscribe["id"] = 2;
            serializeJson(subscribe, request_buffer);
            Log.noticeln(F("[WSc] Sending payload: %s"),  request_buffer);
            webSocket.sendTXT(request_buffer);
        }
            break;
        case WStype_t::WStype_TEXT: {
            heap_stats();
            Log.noticeln(F("[WSc] get text: %s\n"), payload);
            StaticJsonDocument<64> filter;
            filter["result"]["data"]["data"] = true;
            DynamicJsonDocument doc(128);
            deserializeJson(doc, payload, DeserializationOption::Filter(filter));

            String command = doc["result"]["data"]["data"]["command"];
            switch(command_map.count(command)?command_map[command]: -1) {
                case Commands::START: {
                    unsigned long start = doc["result"]["data"]["data"]["message"]["time"];
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
                    String text = doc["result"]["data"]["data"]["message"]["reason"];
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
                    String text = doc["result"]["data"]["data"]["message"]["text"];
                    Log.noticeln(F("Got a message %s"), text.c_str());
                    last_state=state;
                    state=AppState::GOT_MESSAGE;
                    display.message = text;
                    Log.noticeln(F("Changing state to %d"), state);
                    break;
                }
                case Commands::ADD_TIME: {
                    long time = doc["result"]["data"]["data"]["message"]["time"];
                    Log.noticeln(F("Add time %d"), time);
                    time *= 1000;
                    unsigned long timer_time = enabled_timer.get();
                    // prevent underflow here
                    enabled_timer.set(time < 0 && timer_time < -time ? 0 : (enabled_timer.get() + time) );
                    break;
                }
                case -1:
                    String pretty;
                    serializeJsonPretty(doc, pretty);
                    Log.noticeln(F("[WSc] Malformed input: %s\n"), pretty.c_str());
            }
            Log.noticeln(F("[WSc] get text: %s\n"), payload);
        }
            break;
        default:
            break;
    }
}

void save_config() {

}

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

    WiFiManagerParameter uuid_param("uuid", "uuid", "", UUID_LEN);
    WiFiManagerParameter host("host", "host", REST_);

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

    fetch_parameters(&config);
    // 12 from UUID + 4 from KVM- + 1 terminator
    char ap_name[17] = "KVM";
    memcpy(ap_name+3, config.uuid + 23, 13);

    Log.verboseln(F("Launching captive portal with AP name %s"), ap_name);

    // Force wifi manager to abandon the captive portal on the first try really fast
    // This call will succeed if there are saved creds and will fail if there are none
    // Thus we can show a message. Hacky, but I can not use their connectWifi function to do it properly :(
    wifiManager.setConfigPortalTimeout(1);
    if (!wifiManager.autoConnect(ap_name, AP_PASSWORD)) {
        wifiManager.setConfigPortalTimeout(120);
        display.message = (String)"Connect to " + ap_name + " and configure WiFI";
        display.draw();
        bool res = wifiManager.autoConnect(ap_name, AP_PASSWORD);
        if (!res) {
            reboot((char*)NO_WIFI_CONFIGURED, display);
        }
    }

    display.message = "Connecting to WiFi...";
    display.draw();

    uint8_t count = 0;
    while (WiFi.status() != WL_CONNECTED || count < RETRY_COUNT) {
        delay(1000);
        Log.noticeln(".");
        count++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        reboot((char*)NO_WIFI_CONNECTION, display);
    }

    Log.noticeln(F("Connected to WiFI"));
    // set TZ
    display.message = "Syncing time...";
    display.draw();

    heap_stats();

    Log.noticeln(F("Fetching timezone offset"));
    timeSync.begin();
    timeSync.waitForSyncResult();
    uint16_t time_offset = fetch_timezone_offset(display);
    timeClient.setTimeOffset(time_offset);
    timeClient.update();

    heap_stats();

    display.time_now = timeClient.getEpochTime();

    display.message = "Authorising...";
    display.draw();
    Log.noticeln(F("Activating the device"));
    activate_device(String(con ), &club_id, display);
    heap_stats();

    Log.noticeln(F("Authorizing the device to club %d"), club_id);
    authorize_device(uuid, club_id, &token, &id, display);
    heap_stats();

    Log.verboseln(F("Testing token"));
    test_token(token, display);
    heap_stats();
    Log.verboseln(F("Connecting to WSS"));
    webSocket.beginSSL(WSS_HOST, WSS_PORT, WSS_URL);
    webSocket.onEvent(webSocketEvent);
    display.message = "Done initialisation.";
    display.draw();
    state = AppState::IDLE;
    delay(1000);
    display.message = "Ready to serve";
    display.draw();
}

void loop() {
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

    if (Serial.available()) {
        char buffer[10] = {0};
        Serial.readBytesUntil('\n', buffer, 10);

        if (strcmp(buffer, SERIAL_CONFIG_COMMAND) != 0) {
            return;
        }


    }

}