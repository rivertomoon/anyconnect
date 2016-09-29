#ifndef FSM_EVENT_H
#define FSM_EVENT_H

#ifdef TARGET_WIN32
	#include "win32.h"
#else
	#include <netdb.h>
  #include <sys/socket.h>
#endif

#include "strval_pair.h"
#include "stateMachine.h"

#define FSM_EVENT_PORT 8001
#define MAX_EVENTS 16

/* Types of events */
enum fsm_event_type {
  EV_ICE_STRANS_OP_INIT,
  EV_MQTT,
  EV_ICE_STRANS_OP_NEGOTIATION,
  EV_TIMEOUT,
  EV_INCOMING_MSG,
  EV_EXTERNAL_CMD,
	EV_START,
	EV_ICE_SESS_OP_INIT,
};

typedef struct ev_msg
{
  int type;
  int name;
} ev_msg_t;

typedef struct event_queue
{
	int fd;
	char * srv_str;
	struct sockaddr_in send_addr;
	struct sockaddr_in recv_addr;
	/* pj_sockaddr_in send_addr; */
	/* pj_sockaddr_in recv_addr; */
} event_queue_t;

void on_ev_socket_read_complete(pj_ioqueue_key_t *key,
	pj_ioqueue_op_key_t *op_key,
	pj_ssize_t bytes_received);

void on_ev_socket_write_complete(pj_ioqueue_key_t *key,
	pj_ioqueue_op_key_t *op_key,
	pj_ssize_t bytes_received);

#define EV_MQTT_NUM 3
#define EV_ICE_NUM 3
#define EV_CMD_NUM 5

extern const int EV_MQTT_CONNECTED, EV_MQTT_DISCONNECTED, EV_MQTT_ERROR, EV_MQTT_NONE_ERROR;

extern int EV_PJ_SUCCESS;
extern int EV_PJNATH_EICEFAILED, EV_PJNATH_ESTUNTIMEDOUT, EV_PJNATH_UNKNOWN;

extern const int EV_CMD_INVITE, EV_CMD_ICE, EV_CMD_STARTVPN, EV_CMD_CLOSE, EV_CMD_SHUTDOWN;

extern struct StrValPair g_mqtt_ev_strval_pair_array[EV_MQTT_NUM];
extern struct StrValPair g_ice_ev_strval_pair_array[EV_ICE_NUM];
extern struct StrValPair g_cmd_ev_strval_pair_array[EV_CMD_NUM];

void enter_state_common( void *stateData, struct event *event );

void mqtt_disconnected_event();

#endif
