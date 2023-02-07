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

int8_t chunked_read(const String& source, String * dest);
int8_t access_api(const char * url, methods method, String * response, const char * payload = "", const std::vector<std::vector<char*>>& headers = {});