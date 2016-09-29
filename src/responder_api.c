#include <json-c/json.h>

#include "global.h"
#include "util.h"
#include "callbacks.h"
#include "mqtt_callbacks.h"
#include "iceagent.h"

#if 0
void action_prepare_responder( void *stateData, struct event *event ) {

		action_start_vpn(stateData, event);
		action_prepare_ice(stateData, event);
}
#endif

static void answer_peer(icevpn_ctx_t *ctx, char *peer_id)
{
	struct json_object *msg_obj, *data_obj;
	char body[1024];
	char buf[1024];
	int len = 0;

	/* Build Event Response Message */
  msg_obj = json_object_new_object();
	data_obj = json_object_new_object();
  json_object_object_add(msg_obj, "type", json_object_new_int(EV_MQTT));

	if( encode_answer_msg(&body, sizeof(body)) != PJ_SUCCESS ) {
		json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_ERROR));
    goto on_exit;
	}

	if( send_to_peer(ctx, peer_id, body) == PJ_SUCCESS )
		json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_NONE_ERROR));
	else
		json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_ERROR));

	on_exit :
		// json_object_object_add(msg_obj, "data", data_obj);
		// len = pj_ansi_snprintf(buf, sizeof(buf), "%s", json_object_to_json_string(msg_obj));
		// event_queue_add_event(&(mqtt_ctx.event_queue), &buf, len);

		json_object_put(data_obj);
		json_object_put(msg_obj);

		return;
}

void action_answer_peer(void *currentStateData, struct event *event, void *newStateData)
{
	pj_time_val delay = {3, 0};

	INFO("Action Anwser Peer");

	/* Answer Invitation from Peer */
	answer_peer(&mqtt_ctx, "client");

	/* AcceptedState.transitions[1].condition = (void *)init_timer(&delay, &timer_callback); */
}
