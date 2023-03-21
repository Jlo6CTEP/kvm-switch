//
// Created by ysuho on 29-Jan-23.
//

#include "display.h"

#include <utility>
#include "ArduinoLog.h"
#include "TimeLib.h"
#include "config.h"
Display::Display(U8G2 display, const uint8_t *state) {
    this->display = display;
    this->height = display.getDisplayHeight();
    this->width = display.getDisplayWidth();
    this->state = state;
}

int8_t Display::draw() {
    display.firstPage();
    char timebuff[9] = {0};
    do {
        uint8_t footer = height - display.getMaxCharHeight() - 2;
        display.drawHLine(0, footer, width);
        sprintf(timebuff, "%02d:%02d:%02d", hour(time_remaining), minute(time_remaining), second(time_remaining));
        if (*state == AppState::ACTIVE) {
            display.setFont(u8g2_font_ncenB18_te);
            display.drawStr(
                    width / 2 - (display.getStrWidth(timebuff))/2,
                    (height - footer) / 2  + display.getMaxCharHeight() / 2,
                    timebuff);
        } else if (*state != AppState::ACTIVE) {
            display.setFont(u8g2_font_ncenB08_tr);
            String buffer = "";
            buffer.reserve(128);
            uint8_t multiplier = 1;
            uint8_t max_i = footer / display.getMaxCharHeight();
            for (char & i : message) {
                if (display.getStrWidth(buffer.c_str()) < width && i != '\n') {
                    buffer += i;
                } else {
                    char tmp = buffer[buffer.length()-1];

                    if (i != '\n') {
                        buffer[buffer.length() - 1] = '\0';
                    }
                    display.drawStr(0, multiplier * display.getMaxCharHeight(), buffer.c_str());

                    buffer = "";
                    if (i != '\n') {
                        buffer = tmp;
                        buffer += i;
                    }

                    multiplier++;
                    if (multiplier > max_i) break;
                }
            }
            if (buffer.length() && multiplier <= max_i) {
                display.drawStr(0, multiplier * display.getMaxCharHeight(), buffer.c_str());
            }
        }
        if (time_now != 0) {
            sprintf(timebuff, "%02d:%02d:%02d", hour(time_now), minute(time_now), second(time_now));
            display.setFont(u8g2_font_ncenB10_tr);
            display.drawStr(
                    width - display.getStrWidth(timebuff),
                    height,
                    timebuff);
        }

    } while (display.nextPage());
    return 0;
}

void Display::begin() {
    display.begin();
}




