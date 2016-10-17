#ifndef DEVICE_H
#define DEVICE_H

#include <pj/types.h>

enum STATE { UNREGISTERED, OFFLINE, ONLINE};

typedef struct DEVICE {
    char name[256];
    pj_bool_t registered;
    char uuid[37];
    char token[40];

    enum STATE state;
}DEVICE_t;

#endif

