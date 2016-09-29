/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

#include <pj/errno.h>

#include "MQTTAsync.h"
#include "mqtt_callbacks.h"
#include "util.h"

#include "global.h"
#include "fsm_event.h"

#if !defined(WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif

volatile MQTTAsync_token deliveredtoken;

int finished = 0;
int subscribed = 0;

static void register_pjlib()
{
	/* Test that pj_thread_register() works. */
  pj_thread_t *this_thread;
  pj_thread_desc desc;

  pj_status_t rc;

  pj_bzero(desc, sizeof(desc));
  rc = pj_thread_register("thread", desc, &this_thread);
  if (rc != PJ_SUCCESS) {
    app_perror("...error in pj_thread_register", rc);
  }
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	register_pjlib();

	INFO("Connection Lost Cause %s", cause);

}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	register_pjlib();

	INFO("Successful Disconnected");
	// finished = 1;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
	int i;
	MQTTAsyncSubscribeParams params;
	char* payloadptr;

	INFO("Message arrived @ Topic %s", topicName);

	params.topicName = topicName;
	params.topicLen = topicLen;
	params.message = message;

	payloadptr = message->payload;
	for(i=0; i<message->payloadlen; i++)
	{
		putchar(*payloadptr++);
	}
	putchar('\n');

	stateM_handleEvent( &mqtt_ctx.m, &(struct event){ EV_INCOMING_MSG, (void *)(&params) } );

	/* Don't Free, Leave to the message consumer to do that */
	/* MQTTAsync_freeMessage(&message);
	   MQTTAsync_free(topicName); */
	return 1;
}

void onSubscribe(void* context, MQTTAsync_successData* response)
{
	INFO("Subscribe Succeeded");
	// subscribed = 1;
}

void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
	INFO("Subscribe failed, rc %d\n", response ? response->code : 0);
	// finished = 1;
}


void onSend(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
	int rc;

	INFO("Message with token value %d delivery confirmed\n", response->token);

	/* opts.onSuccess = onDisconnect;
	opts.context = client;

	if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS)
	{
		INFO("Failed to start sendMessage, return code %d\n", rc);
		exit(EXIT_FAILURE);
	} */
}


void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	register_pjlib();

	INFO("Connect failed, rc %d\n", response ? response->code : 0);

	mqtt_disconnected_event();
}


void onConnect(void* context, MQTTAsync_successData* response)
{
	struct json_object *msg_obj, *data_obj;
	char buf[1024];
	int len = 0;

	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	register_pjlib();

	INFO("Successful Connected");

	msg_obj = json_object_new_object();
	data_obj = json_object_new_object();

  json_object_object_add(msg_obj, "type", json_object_new_int(EV_MQTT));
	json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_CONNECTED));
	json_object_object_add(msg_obj, "data", data_obj);
	len = pj_ansi_snprintf(buf, sizeof(buf), "%s", json_object_to_json_string(msg_obj));

	json_object_put(data_obj);
  json_object_put(msg_obj);

	event_queue_add_event(&(mqtt_ctx.event_queue), buf, len);
}

