#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#ifndef HAVE_ANDROID_LOG
	#include "MQTTAsync.h"
#endif 

#include "stateMachine.h"
#include "fsm_event.h"

typedef struct peer_ctx {
	pj_bool_t exist;
	pj_str_t uuid;
}peer_ctx_t;

typedef struct icevpn_ctx {
	// MQTTConnectParams connectParams;
#ifndef HAVE_ANDROID_LOG
	MQTTAsync mqtt_ctx;
#endif

	struct stateMachine m;
	event_queue_t event_queue;

	pj_bool_t pj_registered;
	pj_pool_t *pool;
	pj_str_t uuid;

	peer_ctx_t peer_ctx;

	/* List of pools currently allocated by applications. */
	pj_list	buffered_event;
}icevpn_ctx_t;

/* API */

#endif
