#include <json-c/json.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include "msg.h"
#include "ice.h"

pj_status_t encode_hello_msg(char * buf, pj_size_t len)
{

	struct json_object *msg_obj, *lvl1_obj, *lvl2_obj;

	msg_obj = json_object_new_object();

	json_object_object_add(msg_obj, "type", json_object_new_string(TYPE_HELLO_STR));

	pj_ansi_snprintf(buf, sizeof(buf), "%s", json_object_to_json_string(msg_obj));

	return PJ_SUCCESS;

}


pj_status_t encode_invite_msg(char * buf, pj_size_t len)
{
  struct json_object *msg_obj;  /* , *lvl1_obj, *lvl2_obj */

	char sdp_buf[1024];

  msg_obj = json_object_new_object();

  json_object_object_add(msg_obj, "type", json_object_new_string(TYPE_INVITE_STR));

  json_object_object_add(msg_obj, "initiator", json_object_new_string(mqtt_ctx.uuid.ptr));

	encode_session(sdp_buf, 1024);

  json_object_object_add(msg_obj, "sdp", json_object_new_string(sdp_buf));

	pj_ansi_snprintf(buf, len, "%s", json_object_to_json_string(msg_obj));

	return PJ_SUCCESS;

}

pj_status_t encode_answer_msg(char * buf, pj_size_t len)
{
  struct json_object *msg_obj;  /* , *lvl1_obj, *lvl2_obj */
  char sdp_buf[1024];

  msg_obj = json_object_new_object();

  json_object_object_add(msg_obj, "type", json_object_new_string(TYPE_ANSWER_STR));

  encode_session(sdp_buf, 1024);

  json_object_object_add(msg_obj, "sdp", json_object_new_string(sdp_buf));

  pj_ansi_snprintf(buf, len, "%s", json_object_to_json_string(msg_obj));

  return PJ_SUCCESS;

}

pj_status_t encode_bye_msg(char * buf, pj_size_t len)
{
  struct json_object *msg_obj;  /* , *lvl1_obj, *lvl2_obj */
  char sdp_buf[1024];

  msg_obj = json_object_new_object();

  json_object_object_add(msg_obj, "type", json_object_new_string(TYPE_BYE_STR));

  pj_ansi_snprintf(buf, len, "%s", json_object_to_json_string(msg_obj));

  return PJ_SUCCESS;
}

void respond_invite_msg()
{


}

pj_status_t encode_sdp()
{

	return PJ_SUCCESS;
}

pj_status_t encode_offer()
{

	return PJ_SUCCESS;
}
