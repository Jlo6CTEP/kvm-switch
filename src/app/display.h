//
// Created by ysuho on 29-Jan-23.
//

#ifndef SWITCH_DISPLAY_H
#define SWITCH_DISPLAY_H


#include "U8g2lib.h"

class Display {
public:
    U8G2 display;
    unsigned long time_now{};
    String message{};
    unsigned long time_remaining{};
    const uint8_t * state;

    explicit Display(U8G2 display, const uint8_t * state);
    int8_t draw();

    void begin();
private:
    uint8_t height;
    uint8_t width;
};



#endif //SWITCH_DISPLAY_H

