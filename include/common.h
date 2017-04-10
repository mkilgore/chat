#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#include <stdint.h>
#include <stdlib.h>

#define PORT 8128

#define SERVER "127.0.0.1"

struct name_header {
    uint32_t name_len;
};

struct msg_header {
    uint32_t flags;
    uint32_t msg_len;
};

#define MSG_FLAG_ME 0x01
#define MSG_FLAG_URG 0x02

#endif
