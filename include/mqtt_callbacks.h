#ifndef MQTT_AYSNC_CALLBACKS_H
#define MQTT_AYSNC_CALLBACKS_H

#include <pjlib.h>
#include <pjlib-util.h>

#include "MQTTAsync.h"

#define DELIVERY_QOS 1
#define TIMEOUT      10000L

extern volatile MQTTAsync_token deliveredtoken;

typedef struct
{
	char *topicName;
	int topicLen;
	MQTTAsync_message *message;

} MQTTAsyncSubscribeParams;

#if 0
typedef struct {
	// int finished;
	// int subscribed;
	pj_bool_t connected;
	pj_bool_t disc_completed;
}mqtt_ctx_t;
#endif

extern int finished, subscribed;

void connlost(void *context, char *cause);

void onDisconnect(void* context, MQTTAsync_successData* response);

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message);

void onSubscribe(void* context, MQTTAsync_successData* response);

void onSubscribeFailure(void* context, MQTTAsync_failureData* response);

void onSend(void* context, MQTTAsync_successData* response);

void onConnectFailure(void* context, MQTTAsync_failureData* response);

void onConnect(void* context, MQTTAsync_successData* response);

#endif // MQTT_AYSNC_CALLBACKS_H


