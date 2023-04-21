#include "heap_stats.h"

void heap_stats() {
uint32_t hfree;
uint16_t hmax;
uint8_t hfrag;
EspClass::getHeapStats(&hfree, &hmax, &hfrag);
Log.verboseln(F("Free heap %d max heap %d frag %d"), hfree, hmax, hfrag);
}
//
// Created by ysuho on 16-Apr-23.
//
