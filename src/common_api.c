#include <json-c/json.h>

#include "daemon.h"
#include "iceagent.h"

#include "global.h"
#include "icevpn_common.h"
#include "mqtt_callbacks.h"

static void process_startvpn_ev( struct json_object * data_obj );

bool buffer_ev_handler(void * r, struct event *event ) {

	/* Put in buffer */
	pj_list_push_back( &mqtt_ctx.buffered_event, event );

	return true;

}

static void process_buffered_event( void *stateData, struct event *event )
{
	/* MQTTAsyncSubscribeParams * p_params = mqtt_ctx.incoming_sub.next;

	if (p_params != NULL ) {
		DEBUG("Process Buffered Msg in State %s", (char *)((struct StateData *)stateData)->name);

	} else {

	} */
}

void enter_state_common( void *stateData, struct event *event )
{
	DEBUG("Entering %s state", (char *)((struct StateData *)stateData)->name);

	process_buffered_event(stateData, event);

}


void action_start_vpn( void *stateData, struct event *event ) {

	if (-1 == vpn_ctx_init(&vpn_ctx, &g.args))  {
		ERROR_LOG(" Initiate VPN Context Failed");
		return;
	}

	pj_thread_create(g.pool, "tun", &vpn_run_new,
									&vpn_ctx, 0, 0, &iceagent.f_thread);

	attach_ice_ctx(&vpn_ctx, iceagent_send_data);

}

#if 0
void action_start_vpn_v2( void *currentStateData, struct event *event, void *newStateData )
{
	struct json_object *data_obj = (struct json_object *)event->data;

	process_startvpn_ev(data_obj);
}
#endif


void action_stop_vpn( void *stateData, struct event *event ) {

	vpn_stop(&vpn_ctx);

}

static void send_bye_to_peer(icevpn_ctx_t *ctx, char *peer_id)
{
	struct json_object *msg_obj, *data_obj;
	char body[MQTT_MSG_LEN];

	/* Build Event Response Message */
  msg_obj = json_object_new_object();
	data_obj = json_object_new_object();
  json_object_object_add(msg_obj, "type", json_object_new_int(EV_MQTT));

	if( encode_bye_msg(&body, sizeof(body)) != PJ_SUCCESS ) {
		json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_ERROR));
    goto on_exit;
	}

	if( send_to_peer(ctx, peer_id, body) == PJ_SUCCESS )
		json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_NONE_ERROR));
	else
		json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_ERROR));

	on_exit :
		json_object_put(data_obj);
		json_object_put(msg_obj);

		return;
}

void action_stop_session( void *currentStateData, struct event *event,
															void *newStateData )
{
	/* Send a Message to Peer to Say Bye ( Stop the ICE Session, Ifdown the TUN interface */
  send_bye_to_peer(&mqtt_ctx, "wildc_bebop2");

  g.args.cmd = SHADOWVPN_CMD_STOP;

	if (0 != daemon_stop(&g.args)) {
			ERROR_LOG("can not stop daemon");
	}

  /* Stop the ICE Session */
	iceagent_stop_session();

	/* ifdown the TUN interface */

}

void action_stop_session_v2( void *stateData, struct event *event )
{
		/* Send a Message to Peer to Say Bye ( Stop the ICE Session, Ifdown the TUN interface  */
  send_bye_to_peer(&mqtt_ctx, "wildc_bebop2");

  g.args.cmd = SHADOWVPN_CMD_STOP;

	if (0 != daemon_stop(&g.args)) {
			ERROR_LOG("can not stop daemon");
	}


	iceagent_stop_session();
}

#if 0
static void process_startvpn_ev( struct json_object * data_obj )
{
	int fd, port, mtu, concurrency;
	char *server_ip, *password, *user_token;

	struct json_object *fd_obj = NULL, *server_ip_obj = NULL, *port_obj = NULL, *password_obj = NULL, 
		*user_token_obj = NULL, *mtu_obj = NULL, *concurrency_obj = NULL;

	if( json_object_object_get_ex(data_obj, "file_descriptor", &fd_obj) ) {
		fd = json_object_get_int(fd_obj);
	} else {
		goto on_exit;
	}

	if( json_object_object_get_ex(data_obj, "extra_vpn_server_ip", &server_ip_obj) ) {
		server_ip = json_object_get_string(server_ip_obj);
	} else {
		goto on_exit;
	}

	if( json_object_object_get_ex(data_obj, "extra_vpn_port", &port_obj) ) {
		port = json_object_get_int(port_obj);
	} else {
		goto on_exit;
	}

	if( json_object_object_get_ex(data_obj, "extra_vpn_password", &password_obj) ) {
		password = json_object_get_string(password_obj);
	} else {
		goto on_exit;
	}

  if( json_object_object_get_ex(data_obj, "extra_vpn_user_token", &user_token_obj) ) {
		user_token = json_object_get_string(user_token_obj);
  } else {
    goto on_exit;
	}

	if( json_object_object_get_ex(data_obj, "extra_vpn_maximum_transmission_units", &mtu_obj) ) {
		mtu = json_object_get_int(mtu_obj);
	} else {
		goto on_exit;
	}

	if( json_object_object_get_ex(data_obj, "extra_vpn_concurrency", &concurrency_obj) ) {
		concurrency = json_object_get_int(concurrency_obj);
	} else {
		goto on_exit;
	}

	DEBUG("%d, %s, %s, %s, %d, %d", fd, password, server_ip, user_token, port, mtu, concurrency);

	if( init_vpn(fd, password, server_ip, user_token, port, mtu, concurrency) != 0 ) {
		ERROR_LOG(" Initiate VPN Context Failed");
		goto on_exit;

	} else {
		INFO(" Initiate VPN Context Succeeded");
	}

	attach_ice_ctx(&vpn_ctx, iceagent_send_data);

  if( PJ_SUCCESS != pj_thread_create(g.pool, "tun", &vpn_run_new,
                  &vpn_ctx, 0, 0, &iceagent.f_thread) ) {
		ERROR_LOG("TUN Thread Created Failed");
	} else {
		INFO("TUN Thread Created Success");
	}

	on_exit :
		if(fd_obj) json_object_put(fd_obj);
		if(server_ip_obj) json_object_put(server_ip_obj);
		if(password_obj) json_object_put(port_obj);
		if(user_token_obj) json_object_put(user_token_obj);
		if(mtu_obj) json_object_put(mtu_obj);
		if(concurrency_obj) json_object_put(concurrency_obj);
		return;
}
#endif
