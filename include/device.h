#ifndef DEVICE_H
#define DEVICE_H

#include <pj/types.h>

enum STATE { UNREGISTERED, OFFLINE, ONLINE};

typedef struct DEVICE {
    const char *name;
    pj_bool_t registered;
    const char *uuid;
    const char *token;

    enum STATE state;
}DEVICE_t;

#endif

