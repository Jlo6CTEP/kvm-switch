//
// Created by ysuho on 19-Nov-22.
//

#ifndef SWITCH_API_ACCESS_H
#define SWITCH_API_ACCESS_H

#endif //SWITCH_API_ACCESS_H

#include <cstdint>
#include <WString.h>
#include <vector>

enum methods {
    GET,
    POST,
    PUT
};

int8_t chunked_read(const char * source, char * dest, uint16_t buffer_len);
int8_t access_api(const char * url, methods method, char * response, uint16_t response_len,
                  const char * payload = "", const std::vector<std::vector<char*>>& headers = {}, int16_t buffer_len = -1);