//
// Created by ysuho on 19-Nov-22.
//
#include <cstdlib>
#include "ArduinoLog.h"
#include "retry_command.h"


void retry_if_needed(
        const std::function<int8_t ()>& fun,
        const std::function<void ()>& on_success,
        const std::function<void ()>& on_failure,
        uint8_t try_count) {

    int8_t return_code = true;
    // CLion might think that the code below is unreachable. Do not worry, it is pretty much reachable
    for (uint8_t i = 0; !((i >= try_count) || (return_code == 0)); i++) {
        return_code = fun();
        Log.verboseln("Is error %d", return_code);
        if (i >= 1) {
            Log.verboseln("Attempting %d times, max attempts: %d", i, try_count);
        }
    }
    if (return_code == 0) {
        if (on_success != nullptr) {on_success();}
    } else {
        if (on_failure != nullptr) {on_failure();}
    }
}