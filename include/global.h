#ifndef CLIENT_GLOBAL_H
#define CLIENT_GLOBAL_H

#include "config.h"

#include "util.h"
#include "mqtt_client.h"
#include "shadowvpn.h"

#define MQTT_MSG_LEN 1024
#define MQTT_TOPIC_LEN 256

extern vpn_ctx_t vpn_ctx;

extern char * g_colon_str;
extern char * g_slash_str;

extern CommonUtil_t util;
extern icevpn_ctx_t mqtt_ctx;

struct op_key
{
    pj_ioqueue_op_key_t  op_key_;
    struct op_key       *peer;
    char                *buffer;
    pj_ssize_t            size;
    int                  is_pending;
    pj_status_t          last_err;
    pj_sockaddr_in       addr;
    int                  addrlen;
};

struct global
{
  /* Our global variables */
	const char *conf_file;
	FILE *log_fhnd;

	pj_caching_pool cp;
	pj_pool_t *pool;

	/*
	 * Ioqueue
	 */
	pj_ioqueue_t *ioqueue;

	/*
	 * Timer heap instance
	 */
	pj_timer_heap_t *timer_heap;

	pj_sock_t cmd_sock;
	pj_ioqueue_key_t *cmd_key;

	/* MQTT Event/Message to Statemachie Socket */
	pj_sock_t ev_sock;
	pj_ioqueue_key_t *ev_key;

	/* */
	vpn_ctx_t vpn_ctx;
	shadowvpn_args_t args;

	pj_bool_t quit;

};

extern CONFIG_t config;
extern struct global g;

#endif // CLIENT_GLOABAL_H
