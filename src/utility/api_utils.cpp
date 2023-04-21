//
// Created by ysuho on 19-Nov-22.
//

#include "api_utils.h"
#include "fetch.h"
#include "ArduinoLog.h"
#include "vector"

int8_t chunked_read(const char * source, char * dest, uint16_t buffer_len) {
    int32_t tgt_i = 0;

    enum states {
        PARSING_HEADER,
        PARSING_BODY
    };

    uint8_t state = PARSING_HEADER;
    char chunk_size[10] = {0};
    uint8_t chunk_counter = 0;
    uint16_t length;
    uint16_t src_i = 0;
    char current_char = source[0];
    while (current_char != '\n' || src_i < buffer_len) {
        switch (state) {
            case PARSING_HEADER:
                chunk_size[chunk_counter] = source[src_i];
                chunk_counter++;
                if (source[src_i + 1] == '\r') {
                    state = PARSING_BODY;
                    length = strtol(chunk_size, nullptr, 16);
                    if (length == 0) {
                        memset(dest+tgt_i, 0, buffer_len - tgt_i);
                        return 0;
                    }
                    memset(chunk_size, 0, 10);
                    src_i+=2;
                }
                break;
            case PARSING_BODY:
                dest[tgt_i] = source[src_i];
                tgt_i++;

                if (source[src_i + 1] == '\r') {
                    state = PARSING_HEADER;
                    src_i += 2;
                }

                break;
            default:
                break;
        }
        src_i++;
        current_char = source[src_i];
    }
    return  -1;
}

int8_t access_api(const char * url, methods method, char * response, uint16_t response_len,
                  const char * payload, const std::vector<std::vector<char*>>& headers, int16_t buffer_len) {
    fetch.begin(url, true, buffer_len);
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
    // -1 for 0 terminator
    uint16_t read_size = fetch.readBytes(response, response_len-1);
    response[read_size+1] = '\0';
    Log.verboseln(F("Received payload: %s"), response);
    fetch.clean();
    return 0;
}
