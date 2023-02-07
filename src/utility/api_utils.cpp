//
// Created by ysuho on 19-Nov-22.
//

#include "api_utils.h"
#include "fetch.h"
#include "ArduinoLog.h"
#include "vector"

int8_t chunked_read(const String& source, String*dest) {
    int32_t tgt_i = 0;
    String temp_result;

    enum states {
        PARSING_HEADER,
        PARSING_BODY
    };

    uint8_t state = PARSING_HEADER;
    char chunk_size[10] = {0};
    uint8_t chunk_counter = 0;
    int32_t length;
    for (uint32_t src_i = 0; src_i < source.length(); src_i++) {
        switch (state) {
            case PARSING_HEADER:
                chunk_size[chunk_counter] = source[src_i];
                chunk_counter++;
                if (source[src_i + 1] == '\r') {
                    state = PARSING_BODY;
                    length = strtol(chunk_size, nullptr, 16);
                    if (length == 0) {
                        dest->remove(tgt_i, dest->length());
                        return 0;
                    }
                    memset(chunk_size, 0, 10);
                    src_i+=2;
                }
                break;
            case PARSING_BODY:
                dest->setCharAt(tgt_i, source[src_i]);
                tgt_i++;

                if (source[src_i + 1] == '\r') {
                    state = PARSING_HEADER;
                    src_i += 2;
                }

                break;
            default:
                break;
        }
    }
    return  -1;
}

int8_t access_api(const char * url, methods method, String * response, const char * payload, const std::vector<std::vector<char*>>& headers) {

    fetch.begin(url, true);
    for (std::vector<char*> i: headers) {
        fetch.addHeader(i[0], i[1]);
        Log.verboseln(F("Adding header %s: %s"), i[0], i[1]);
    }
    int code = 0;
    switch (method) {
        case POST:
            Log.verboseln(F("Attempting POST to %s with payload %s"), url, payload);
            code = fetch.POST(payload);
            break;
        case GET:
            Log.verboseln(F("Attempting GET to %s"), url);
            code = fetch.GET();
            break;
        case PUT:
            Log.verboseln(F("Attempting PUT to %s"), url);
            code = fetch.PUT(payload);
    }
    Log.verboseln(F("Return code: %d"), code);
    if (code != 200) {
        fetch.clean();
        return -1;
    }
    String res = fetch.readString();
    *response = res;
    Log.verboseln(F("Received payload: %s"), response->c_str());
    fetch.clean();
    return 0;
}
