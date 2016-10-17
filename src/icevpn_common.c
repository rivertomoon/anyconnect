#include <json-c/json.h>

#include "global.h"
#include "util.h"
#include "callbacks.h"
#include "mqtt_callbacks.h"
#include "icevpn_fsm.h"
#include "icevpn_common.h"
#include "iceagent.h"

#define THIS_FILE "icevpn_common.c"

char full_topic_buf[256];
pj_time_val reconnect_delay = {1, 0};

bool mqtt_ev_handler(void * r, struct event *event) {

	struct json_object *lvl1_obj;

	if( json_object_object_get_ex((struct json_object *)event->data, "name", &lvl1_obj) ) {

		int ev_idx = json_object_get_int(lvl1_obj);

		if( ev_idx < EV_MQTT_NUM ) {
			DEBUG("Condition %s Handle MQTT Event %s", g_mqtt_ev_strval_pair_array[ev_idx].str.ptr, g_mqtt_ev_strval_pair_array[*(int *)r].str.ptr);
			return *(int *)r == ev_idx;
		} else {
			WARN("Handle Unknown MQTT Event");
			return false;
		}
	} else {
		WARN("Handle Invalid MQTT Event ( No Name Field in Json )");
		return false;
	}

}

bool timeout_ev_handler(void * r, struct event *event) {
  return (pj_timer_entry *)r == (pj_timer_entry *)event->data;
}

bool ice_ev_handler(void * r, struct event *event ) {
	int ev_idx = *(int *)event->data;

	if( ev_idx < EV_ICE_NUM ) {
		DEBUG("Handle ICE Event: %s", g_ice_ev_strval_pair_array[ev_idx].str.ptr)
		return *(int *)r == *(int *)(event->data);
	} else {
		WARN("Handle Unknown ICE Event");
		return false;
	}

}

bool cmd_ev_handler(void * r, struct event *event) {

  struct json_object *lvl1_obj;

  if( json_object_object_get_ex((struct json_object *)event->data, "name", &lvl1_obj) ) {
		int ev_idx = json_object_get_int(lvl1_obj);
		INFO("Handle CMD EVENT: %s", g_cmd_ev_strval_pair_array[ev_idx].str.ptr);
		json_object_put(lvl1_obj);
		return *(int *)r == ev_idx;
  } else {

    WARN("Invalid CMD Event ( No Name Field in Json )");
    return false;
  }

}

bool msg_ev_handler(void * r, struct event *event ) {

	MQTTAsyncSubscribeParams * p_params = (MQTTAsyncSubscribeParams *)(event->data);

	decode_msg(p_params->topicName, p_params->topicLen, p_params->message->payload, p_params->message->payloadlen);

	MQTTAsync_freeMessage(&p_params->message);
	MQTTAsync_free(p_params->topicName);

	pj_timer_entry *entry = (pj_timer_entry *)r;

  if( NULL != entry )
    pj_timer_heap_cancel(g.timer_heap, entry);

	return true;
}

pj_status_t mqtt_init(icevpn_ctx_t *ctx, CONFIG_t *config)
{
	char mqtt_srv_url[512];
	pj_str_t * str;

	/* Create application memory pool */
	ctx->pool = pj_pool_create(&g.cp.factory, "mqtt",
		512, 512, NULL);

	/* CHECK( pj_timer_heap_create(icedemo.pool, 100,
\		&icedemo.ice_cfg.stun_cfg.timer_heap) ); */

	ctx->pj_registered = PJ_FALSE;

	stateM_init( &ctx->m, &icevpnGroupState, &errorState );

	init_ev_queue(&ctx->event_queue, config);

	// ctx->uuid = (char *)pj_pool_zalloc(ctx->pool, strlen(config->device.uuid));
	// ctx->token = (char *)pj_pool_zalloc(ctx->pool, strlen(device->token));
	// strcpy(ctx->uuid, config->device.uuid);
	// strcpy(ctx->token, device->token);
	ctx->uuid = pj_str((char *)config->device.uuid);

	pj_bzero(mqtt_srv_url, sizeof(mqtt_srv_url));
	pj_ansi_snprintf(mqtt_srv_url, sizeof(mqtt_srv_url), "tcp://%s:%d", config->mqtt_config.host, config->mqtt_config.port);

	MQTTAsync_create(&ctx->mqtt_ctx, mqtt_srv_url, ctx->uuid.ptr, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(ctx->mqtt_ctx, NULL, connlost, msgarrvd, NULL);

	pj_list_init(&ctx->buffered_event);

	return PJ_SUCCESS;
}

void action_init_instance( void *stateData, struct event *event )
{
	iceagent_create_instance();
}

void action_reinject_event( void *currentStateData, struct event *event,
														void *newStateData )
{

}

void action_init_session( void *currentStateData, struct event *event,
															void *newStateData )
{
	iceagent_init_session(config.ice_config.mode);
}

void action_init_session_v2( void *stateData, struct event *event )
{
	iceagent_init_session(config.ice_config.mode);
}

static void connect_mqtt(pj_timer_heap_t *timer_heap, pj_timer_entry *entry )
{
	int rc;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

	INFO("Start MQTT Connect to %s:%d", config.mqtt_config.host, config.mqtt_config.port);

	conn_opts.username = config.device.uuid;
	conn_opts.password = config.device.token;

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = mqtt_ctx.mqtt_ctx;

	if ((rc = MQTTAsync_connect(mqtt_ctx.mqtt_ctx, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		app_perror("Failed to start connect, return code %d\n", rc);
	}
}

void action_schedule_mqtt_connect( void *stateData, struct event *event )
{
	pj_status_t rc;
	pj_timestamp t1;
	pj_timer_entry *entry;
	pj_timer_heap_t *timer;

	entry = (pj_timer_entry*)pj_pool_calloc(mqtt_ctx.pool, 1, sizeof(*entry));
	if (!entry)
		return;

	entry->cb = &connect_mqtt;

	// Schedule timer
	pj_get_timestamp(&t1);
	rc = pj_timer_heap_schedule(g.timer_heap, entry, &reconnect_delay);

	if (rc != PJ_SUCCESS) {
    app_perror("...error: unable to create timer heap", rc);
		return ;
	}
}

void action_schedule_mqtt_connect_v2( void *currentStateData, struct event *event,
                                    void *newStateData )
{
	action_schedule_mqtt_connect(currentStateData, event);
}

void action_subscribe_topics( void *currentStateData, struct event *event,
                                    void *newStateData )
{
	int rc;
	pj_str_t full_topic;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  // full_topic.ptr = full_topic_buf;
  // full_topic.slen = 0;

// pj_strcat2(&full_topic, "message/");
  //pj_strcat(&full_topic, &(mqtt_ctx.uuid));
  pj_bzero(full_topic_buf, sizeof(full_topic_buf));
  pj_ansi_snprintf(full_topic_buf, 1024, "message/%s", (char *)pj_strbuf(&mqtt_ctx.uuid));

  opts.onSuccess = onSubscribe;
  opts.onFailure = onSubscribeFailure;
  opts.context = mqtt_ctx.mqtt_ctx;

  deliveredtoken = 0;

  // (char *)pj_strbuf(&full_topic)

  if ((rc = MQTTAsync_subscribe(mqtt_ctx.mqtt_ctx, full_topic_buf, DELIVERY_QOS, &opts)) != MQTTASYNC_SUCCESS)
  {
    app_perror("Failed to start subscribe, return code %d\n", rc);
  }
}

PJ_DEF(pj_status_t) send_to_peer(icevpn_ctx_t *ctx, char *peer_id, char * body_msg)
{
	char topic[256];
	int rc;

	MQTTAsync client = (MQTTAsync)(ctx->mqtt_ctx);
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;

	opts.onSuccess = onSend;
  opts.context = client;

	/* Build Topic */
	bzero(&topic, sizeof(topic));
	// strcpy(topic, "message/");
	// strcat(topic, peer_id);
	pj_ansi_snprintf(topic, sizeof(topic), "message/%s", peer_id);

	/* Associate Body Message */
	pubmsg.payload = body_msg;
  pubmsg.payloadlen = strlen(body_msg) + 1;
  pubmsg.qos = DELIVERY_QOS;
  pubmsg.retained = 0;
  deliveredtoken = 0;

  if ((rc = MQTTAsync_sendMessage(ctx->mqtt_ctx, topic, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
  	return PJ_EUNKNOWN;
	else
		return PJ_SUCCESS;
}

pj_timer_entry * init_timer(pj_time_val *delay, void (*callback)(pj_timer_heap_t *,
			   pj_timer_entry *) )
{
	pj_status_t rc;
	pj_timestamp t1;
	pj_timer_entry *entry;
	pj_timer_heap_t *timer;

	entry = (pj_timer_entry*)pj_pool_calloc(mqtt_ctx.pool, 1, sizeof(*entry));
	if (!entry)
		return NULL;

	entry->cb = callback;

	// Schedule timer
	pj_get_timestamp(&t1);
	rc = pj_timer_heap_schedule(g.timer_heap, entry, delay);

	if (rc != PJ_SUCCESS) {
		app_perror("...error: unable to create timer heap", rc);
		return NULL;
	}

	return entry;
}

void action_cancel_timer( void *currentStateData, struct event *event, void *newStateData )
{
  // pj_timer_entry *entry = InvitePeerState.transitions[2].condition;
  struct transition *t;

  t = getTransition(&mqtt_ctx.m, ((struct StateData *)currentStateData)->state_ptr, event);

  pj_timer_entry *entry = t->condition;

  // pj_timer_entry *entry = Initiator_InvitingState.transitions[2].condition;

  if( NULL != entry )
    pj_timer_heap_cancel(g.timer_heap, entry);
}

void action_prepare_ice( void *stateData, struct event *event ) {

	pj_status_t status;

	if (iceagent.icest == NULL) {
		ERROR_LOG("Error: No ICE instance, create it first");
    return;
  }
  if (!pj_ice_strans_has_sess(iceagent.icest)) {
    ERROR_LOG("Error: No ICE session, initialize first");
    return;
  }

  if (iceagent.rem.cand_cnt == 0) {
		ERROR_LOG("Error: No remote info, input remote info first");
		return;
  }

	status = pj_ice_strans_prepare_ice(iceagent.icest, iceagent.rem.ufrag_str, \
														iceagent.rem.pwd_str, iceagent.rem.cand_cnt, \
														iceagent.rem.cand);

}

void printReset( void *oldStateData, struct event *event,
      void *newStateData )
{
	puts( "Resetting" );
}

void printErrMsg( void *stateData, struct event *event )
{
	puts( "ENTERED ERROR_LOG STATE!" );
}

void printEnterMsg( void *stateData, struct event *event )
{
	DEBUG("StateMachine Entering %s state", (char *)((struct StateData *)stateData)->name);
}

void printExitMsg( void *stateData, struct event *event )
{
	DEBUG("StateMachine Exiting %s state", (char *)((struct StateData *)stateData)->name);
}



