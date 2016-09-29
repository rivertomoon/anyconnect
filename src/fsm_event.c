#include <json-c/json.h>
#include <pjnath.h>
#include "stateMachine.h"

#include "global.h"
#include "fsm_event.h"
#include "icevpn_common.h"
#include "mqtt_callbacks.h"

const int EV_MQTT_CONNECTED = 0, EV_MQTT_DISCONNECTED = 1, EV_MQTT_ERROR = 2, EV_MQTT_NONE_ERROR = 3;

int EV_PJ_SUCCESS = 0;
int EV_PJNATH_EICEFAILED = PJNATH_EICEFAILED;
int EV_PJNATH_ESTUNTIMEDOUT = PJNATH_ESTUNTIMEDOUT;
int EV_PJNATH_UNKNOWN = 120111;

const int EV_CMD_INVITE = 0, EV_CMD_ICE = 1, EV_CMD_STARTVPN = 2, EV_CMD_CLOSE = 3, EV_CMD_SHUTDOWN = 4;

struct StrValPair g_mqtt_ev_strval_pair_array[EV_MQTT_NUM] = {
  { {"MQTT CONNECTED", 0 }, 0 },
  { {"MQTT DISCONNECTED", 1 }, 1 },
	{ {"ACTION MQTT_ERROR", 2 }, 2 },
};

struct StrValPair g_ice_ev_strval_pair_array[3] = {
  { {"ACTION PJ_SUCCESS", 0}, 0 },
  { {"PJNATH EICEFAILED", 1}, 1 },
  { {"PJNATH ESTUNTIMEDOUT", 2}, 2 },
};

struct StrValPair g_cmd_ev_strval_pair_array[EV_CMD_NUM] = {
  { {"INVITE", 0 }, 0 },
  { {"ICE", 1 }, 1 },
	{ {"STARTVPN", 2 }, 2 },
	{ {"SHUTDOWN", 3 }, 3 }
};

/**
 * A utility function to convert fourcc type of value to four letters string.
 *
 * @param sig		The fourcc value.
 * @param buf		Buffer to store the string, which MUST be at least
 * 			five bytes long.
 *
 * @return		The string.
 */
PJ_INLINE(const char*) fourcc_name(pj_uint32_t sig, char buf[])
{
    buf[3] = (char)((sig >> 24) & 0xFF);
    buf[2] = (char)((sig >> 16) & 0xFF);
    buf[1] = (char)((sig >>  8) & 0xFF);
    buf[0] = (char)((sig >>  0) & 0xFF);
    buf[4] = '\0';
    return buf;
}

pj_status_t init_ev_queue(event_queue_t * ev_queue, const CONFIG_t *config)
{
	int fd;
	struct hostent *hp;     /* host information */
	char * host = "localhost";

	/* create a socket */
	if ( (ev_queue->fd = socket(AF_INET, SOCK_DGRAM, 0)) ==-1 )
		printf("socket created\n");

	/* bind it to all local addresses and pick any port number */
	memset((char *)&ev_queue->send_addr, 0, sizeof(ev_queue->send_addr));
	ev_queue->send_addr.sin_family = AF_INET;
	ev_queue->send_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ev_queue->send_addr.sin_port = htons(0);

	hp = gethostbyname(host);

	/* put the host's address into the server address structure */
	memset((char *) &ev_queue->recv_addr, 0, sizeof(ev_queue->recv_addr));
	memcpy((void *)&(ev_queue->recv_addr.sin_addr), hp->h_addr_list[0], hp->h_length);
	ev_queue->recv_addr.sin_family = AF_INET;
	ev_queue->recv_addr.sin_port = htons(config->generic.event_port);

}

pj_status_t event_queue_add_event(event_queue_t* ev_queue, char * buf, int len)
{

  int slen=sizeof(ev_queue->recv_addr);

  if (sendto(ev_queue->fd, buf, len, 0, (struct sockaddr *)&(ev_queue->recv_addr), slen)==-1) {
    perror("sendto");
  }

  return PJ_SUCCESS;
}

void mqtt_disconnected_event()
{
	struct json_object *msg_obj, *data_obj;
	char buf[1024];
	int len = 0;

	msg_obj = json_object_new_object();
  data_obj = json_object_new_object();

  json_object_object_add(msg_obj, "type", json_object_new_int(EV_MQTT));
  json_object_object_add(data_obj, "name", json_object_new_int(EV_MQTT_DISCONNECTED));
  json_object_object_add(msg_obj, "data", data_obj);
  len = pj_ansi_snprintf(buf, sizeof(buf), "%s", json_object_to_json_string(msg_obj));

  json_object_put(data_obj);
  json_object_put(msg_obj);

  event_queue_add_event(&(mqtt_ctx.event_queue), buf, len);
}



