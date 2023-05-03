//
// Created by ysuho on 19-Nov-22.
//

#include <cstdint>
#include <WString.h>
#include "app/display.h"

#ifndef SWITCH_API_FETCH_H
#define SWITCH_API_FETCH_H

#endif //SWITCH_API_FETCH_H

void reboot(char * reason, Display display);
uint16_t fetch_timezone_offset(Display display);
void activate_device(const String &url, const String &uuid, String *rest_url,
                     String *websocket_url, uint32_t *club_id, const String &code, int32_t *time_offset, Display display);
void authorize_device(const String &url, const String &uuid, uint32_t club_id,
                      String *token, uint32_t *id, Display display) ;
void test_token(const String &url, const String &token, Display display);
