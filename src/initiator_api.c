#include <json-c/json.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include "stateMachine.h"

#include "global.h"
#include "callbacks.h"
#include "icevpn_common.h"
#include "initiator_api.h"

#include "icevpn_fsm.h"

static void send_invite_to_peer(icevpn_ctx_t *ctx, char *peer_id)
{

	struct json_object *msg_obj, *data_obj;
	char body[1024];
	char buf[1024];
	int len = 0;

  if( encode_invite_msg(&body, sizeof(body)) != PJ_SUCCESS )
    goto on_error;

	if( send_to_peer(ctx, peer_id, body) != PJ_SUCCESS ) 
		goto on_error;

	return;

	on_error :
		/* Build Event Response Message */
		msg_obj = json_object_new_object();
		data_obj = json_object_new_object();
		json_object_object_add(msg_obj, "type", json_object_new_int(EV_MQTT));
		json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_ERROR));
		json_object_object_add(msg_obj, "data", data_obj);
		len = pj_ansi_snprintf(buf, sizeof(buf), "%s", json_object_to_json_string(msg_obj));
		event_queue_add_event(&(mqtt_ctx.event_queue), &buf, len);

		json_object_put(data_obj);
		json_object_put(msg_obj);

		return;
}


void action_send_inv( void *stateData, struct event *event )
{

  pj_time_val delay = {5, 0};

  printEnterMsg(stateData, event);

  /* Send Invitation to Peer */
  send_invite_to_peer(&mqtt_ctx, "server");

  Initiator_InvitingState.transitions[2].condition = (void *)init_timer(&delay, &timer_callback);
  Initiator_InvitingState.transitions[3].condition = Initiator_InvitingState.transitions[2].condition; 

}

void action_start_ice( void *stateData, struct event *event ) {
  iceagent_start_nego();
}
