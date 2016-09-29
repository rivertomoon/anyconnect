#ifndef MQTT_TOPIC_H
#define MQTT_TOPIC_H

#include <pj/types.h>
#include "strval_pair.h"

#define QOS         1
#define OVERALL_TOPIC_LENGTH 1024

#define SYS_TOPIC_VAL 0
#define ME_TOPIC_VAL 	1
#define TOPIC_NUM     2

#define SYS_RESTART_VAL  0
#define SYS_MSG_TYPE_NUM 1

#define INVITE_MSG_VAL 0
#define ACCEPT_MSG_VAL 1
#define BYE_MSG_VAL 2
#define ME_MSG_TYPE_NUM 3

extern char * g_slash_str;
extern char * g_colon_str;

extern struct StrValPair g_topic_str_val_pair_array[TOPIC_NUM];
extern struct StrValPair g_sys_type_str_val_pair_array[SYS_MSG_TYPE_NUM];
extern struct StrValPair g_me_type_str_val_pair_array[ME_MSG_TYPE_NUM];

#endif // MQTT_TOPIC_H
