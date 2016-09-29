#include <string.h>
#include "mqtt_topic.h"

char * g_slash_str = "/";
char * g_colon_str = ":";

struct StrValPair g_topic_str_val_pair_array[TOPIC_NUM] = {
	{ {"sys", 4}, 0 },
	{ {"message", 8}, 1 },
};

struct StrValPair g_sys_type_str_val_pair_array[SYS_MSG_TYPE_NUM] = {
	{ {"reboot", 7}, 0 },
};

struct StrValPair g_me_type_str_val_pair_array[ME_MSG_TYPE_NUM] = {
	{ {"invite", 7}, 0 },
	{ {"accept", 7}, 1 },
	{ {"bye", 4}, 2 },
};

