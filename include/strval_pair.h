#ifndef INCLUDE_STRING_VALUE_PAIR_H
#define INCLUDE_STRING_VALUE_PAIR_H

#include <pjlib.h>

typedef struct StrValPair{
    pj_str_t str;
    pj_int32_t index;
}StrValPair_t;

#endif
