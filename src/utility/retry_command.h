//
// Created by ysuho on 19-Nov-22.
//
#include <cstdlib>
#ifndef SWITCH_RETRY_COMMAND_H
#define SWITCH_RETRY_COMMAND_H

#endif //SWITCH_RETRY_COMMAND_H
void retry_if_needed(
        const std::function<int8_t ()>& fun,
        const std::function<void ()>& on_success,
        const std::function<void ()>& on_failure,
        uint8_t try_count=5);