//
// Created by ysuho on 17-Nov-22.
//

#ifndef SWITCH_REBOOT_REASONS_H
#define SWITCH_REBOOT_REASONS_H

#define NO_ERROR F("ok")
#define NO_WIFI_CONNECTION F("Wifi connection failure")
#define NO_WIFI_CONFIGURED F("Wifi configuration timeout")
#define TIME_SYNC_ERROR F("Time synchronisation failure")
#define ACTIVATION_ERROR F("Activation failure")
#define AUTH_ERROR F("Authentication failure")
#define TOKEN_ERROR F("Access token fetch failure")
#define CENTRIFUGE_SUBSCRIBE_ERROR F("Centrifuge subscription failure")
#define REST_URL_TOO_LONG F("Provided REST url is too long")
#define APPLY_SETTINGS F("Rebooting to apply new settings")

#endif //SWITCH_REBOOT_REASONS_H
