#include <json-c/json.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include "global.h"
#include "msg.h"
#include "mqtt_topic.h"
#include "mqtt_client.h"
#include "iceagent.h"

#define THIS_FILE "msg_decode.c"

json_object * decode_peer_msg(const char *payload);

json_object * decode_sys_msg(const char *payload);

static pj_uint8_t convert_topic_name_to_ind(const char * topic, const pj_size_t len)
{
    int i;
		pj_str_t s;
		const pj_str_t * p_topic_str;
		pj_str_t str;

		char * line;
		char * token;
		char * lvl_str;

		p_topic_str = (const pj_str_t *)pj_strset(&s, (char *)topic, len);

		// char * line = (char *)malloc(len);
		// memcpy(line, topic, len);

		pj_strdup(mqtt_ctx.pool, &str, p_topic_str);

		token = strtok(str.ptr, g_slash_str);

		while(token) {
    	for(i=0; i<TOPIC_NUM; i++) {
        if( strcmp(g_topic_str_val_pair_array[i].str.ptr, token) == 0 )
            return i;
    	}
			printf("Token %s\n", token);

			token = strtok(NULL, g_slash_str);
		}

    return -1;
}

static pj_uint8_t convert_msg_type_to_ind(const char * topic, const char * type, const pj_size_t len)
{
    int i;
		pj_str_t s;
		const pj_str_t * p_type_str;

		p_type_str = (const pj_str_t *)pj_strset(&s, (char *)type, len);

		if( strcmp(g_topic_str_val_pair_array[SYS_TOPIC_VAL].str.ptr, topic) == 0 ) {
    	for(i=0; i<SYS_MSG_TYPE_NUM; i++) {
        if( pj_strncmp(&g_sys_type_str_val_pair_array[i].str, p_type_str, len) == 0 )
					return i;
    	}
		}
		else if( strcmp(g_topic_str_val_pair_array[ME_TOPIC_VAL].str.ptr, topic) == 0 ) {
			for(i=0; i<ME_MSG_TYPE_NUM; i++) {
				if( pj_strncmp(&g_me_type_str_val_pair_array[i].str, p_type_str, len) == 0 )
					return i;
			}
		}

    return -1;
}


json_object * decode_msg(const char *topic, const pj_size_t topic_len, const char *payload, const pj_size_t payload_len) {

	switch( convert_topic_name_to_ind(topic, topic_len) ) {
		case ME_TOPIC_VAL:
			return decode_peer_msg(payload);
			break;
		case SYS_TOPIC_VAL:
			return decode_sys_msg(payload);
			break;
		default:
			PJ_LOG(3, (THIS_FILE, ("Unknown Message Type  %.*s", topic_len, topic)));
			break;
  }

	return NULL;
}

json_object * decode_peer_msg(const char *payload) {

	json_object *msg_jobj, *type_jobj, *rsp_jobj;
	const char * type_str;
	pj_size_t len;

	msg_jobj = json_tokener_parse(payload);

	if( !json_object_object_get_ex(msg_jobj, "type", &type_jobj) )
		goto on_error;

	if( !json_object_is_type(type_jobj, json_type_string) )
		goto on_error;

	type_str = json_object_get_string(type_jobj);
	len = json_object_get_string_len(type_jobj);

	switch(convert_msg_type_to_ind(g_topic_str_val_pair_array[ME_TOPIC_VAL].str.ptr, type_str, len)) {
		case INVITE_MSG_VAL :
			process_invite_msg(msg_jobj, &rsp_jobj);
			break;
    case ACCEPT_MSG_VAL :
      process_accept_msg(msg_jobj, &rsp_jobj);
      break;
		case BYE_MSG_VAL :
			process_bye_msg(msg_jobj, &rsp_jobj);
			break;
		default:
			PJ_LOG(3, (THIS_FILE, ("Unknown Peer Message Type %s", type_str)));
			break;
	}

	return rsp_jobj;

	on_error :
		return NULL;
}

json_object * decode_sys_msg(const char *payload) {

	json_object *msg_jobj, *type_jobj;
	const char * type_str;
	pj_size_t len;

	msg_jobj = json_tokener_parse(payload);

	if( !json_object_object_get_ex(msg_jobj, "type", &type_jobj) )
		goto on_error;

	if( !json_object_is_type(type_jobj, json_type_string) )
		goto on_error;

	type_str = json_object_get_string(type_jobj);
	len = json_object_get_string_len(type_jobj);

	switch(convert_msg_type_to_ind(g_topic_str_val_pair_array[SYS_TOPIC_VAL].str.ptr, type_str, len)) {
		default:
			PJ_LOG(3, (THIS_FILE, ("Unknown Sys Message Type %s", type_str)));
			break;
	}

	on_error :
		return NULL;
}

pj_status_t process_invite_msg(json_object *msg_jobj, json_object ** rsp_jobj) {

  json_object *initiator_jobj,*sdp_jobj;
	const char * sdp;

	if( !json_object_object_get_ex(msg_jobj, "initiator", &initiator_jobj) ) {
    return PJ_EUNKNOWN;
	} else {
		mqtt_ctx.peer_ctx.exist = PJ_TRUE;

		mqtt_ctx.peer_ctx.uuid.ptr =  (char *)pj_pool_zalloc(mqtt_ctx.pool, json_object_get_string_len(initiator_jobj));
		memcpy(mqtt_ctx.peer_ctx.uuid.ptr, json_object_get_string(initiator_jobj), json_object_get_string_len(initiator_jobj));
		mqtt_ctx.peer_ctx.uuid.slen = json_object_get_string_len(initiator_jobj);
	}

  if( !json_object_object_get_ex(msg_jobj, "sdp", &sdp_jobj) )
    return PJ_EUNKNOWN;

	if ( !pj_ice_strans_has_sess(iceagent.icest)) {
		iceagent_init_session(config.ice_config.mode);
  }

  return process_sdp(sdp_jobj);

}

pj_status_t process_accept_msg(json_object *msg_jobj, json_object ** rsp_jobj) {

  json_object *sdp_jobj;

  if( !json_object_object_get_ex(msg_jobj, "sdp", &sdp_jobj) )
    return PJ_EUNKNOWN;

  return process_sdp(sdp_jobj);

}

pj_status_t process_bye_msg(json_object *msg_jobj, json_object ** rsp_jobj) {

	iceagent_stop_session();

	/* ifdown the tap interface */

	return PJ_SUCCESS;

}

pj_status_t process_sdp(json_object *sdp_jobj) {
	pj_status_t status;
	const char * sdp;
	int af;
	pj_str_t ipv4_str, ipv6_str;

  if( !json_object_is_type(sdp_jobj, json_type_string) )
    goto on_error;

  printf("Start Decode the SDP Message\n");

	sdp = json_object_get_string(sdp_jobj);

  iceagent_parse_remote(sdp, mqtt_ctx.pool, &iceagent.rem.sdp_sess);

  pjmedia_sdp_session *rem_sdp = iceagent.rem.sdp_sess;

	if (pj_strcmp(&(rem_sdp->media[0]->conn->addr_type), pj_cstr(&ipv6_str, "IP6")) == 0 )
      af = pj_AF_INET6();
	else if(pj_strcmp(&(rem_sdp->media[0]->conn->addr_type), pj_cstr(&ipv4_str, "IP4")) == 0)
      af = pj_AF_INET();
	else
      goto on_error;

	pj_sockaddr_init(af, &iceagent.rem.def_addr[0], NULL, 0);
	status = pj_sockaddr_set_str_addr(af, &iceagent.rem.def_addr[0],
																		&(rem_sdp->media[0]->conn->addr));
	if (status != PJ_SUCCESS) {
		PJ_LOG(1,(THIS_FILE, "Invalid IP address in c= line"));
		goto on_error;
	}

	pj_sockaddr_set_port(&iceagent.rem.def_addr[0], rem_sdp->media[0]->desc.port);

  ice_parse(mqtt_ctx.pool, &iceagent.rem, 0);

 	return PJ_SUCCESS;

  on_error :
    return PJ_EUNKNOWN;
}

