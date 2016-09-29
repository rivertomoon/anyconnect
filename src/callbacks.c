#include <json-c/json.h>

#include "global.h"
#include "callbacks.h"
#include "fsm_event.h"
#include "stateMachine.h"
#include "icevpn_common.h"

#define THIS_FILE "callback.c"

void timer_callback(pj_timer_heap_t *timer_heap,
			   pj_timer_entry *entry)
{
  DEBUG("Timer is Called");

  stateM_handleEvent( &mqtt_ctx.m, &(struct event){ EV_TIMEOUT,
		(void *)(entry) } );
}

/*
 * This is the callback that is registered to the ICE stream transport to
 * receive notification about ICE state progression.
 */
void cb_on_ice_complete(pj_ice_strans *ice_st,
			       pj_ice_strans_op op,
			       pj_status_t status)
{
  const char *opname =
      (op==PJ_ICE_STRANS_OP_INIT? "initialization" :
      (op==PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation" : "unknown_op"));

	if( op == PJ_ICE_STRANS_OP_INIT) {
		stateM_handleEvent( &mqtt_ctx.m, &(struct event){ EV_ICE_STRANS_OP_INIT, (void *)&status } );
	}
  else if( op == PJ_ICE_STRANS_OP_NEGOTIATION ) {
    stateM_handleEvent( &mqtt_ctx.m, &(struct event){ EV_ICE_STRANS_OP_NEGOTIATION, (void *)&status } );
  }
  else {
		INFO("Unhandled ICE Operation %d with status %d", op, status);
  }

#if 0
  if (status == PJ_SUCCESS) {
    PJ_LOG(3,(THIS_FILE, "ICE %s successful", opname));
  } else {
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(THIS_FILE, "ICE %s failed: %s", opname, errmsg));
    pj_ice_strans_destroy(ice_st);
    iceagent.icest = NULL;
  }
#endif
}

void on_ev_socket_read_complete(pj_ioqueue_key_t *key,
  pj_ioqueue_op_key_t *op_key,
  pj_ssize_t bytes_received)
{
  pj_status_t rc;

  // DEBUG("Read Bytes On ev_socket %d", bytes_received);

  struct op_key *recv_rec = (struct op_key *)op_key;

  struct json_object *type_obj, *data_obj;
	struct json_object *name_obj;
  struct json_object *mqtt_msg_obj = json_tokener_parse((const char *)(recv_rec->buffer));
  pj_ssize_t length;
  struct event ev;

  if( json_object_object_get_ex(mqtt_msg_obj, "type", &type_obj) ) {
    ev.type = json_object_get_int(type_obj);
  } else {
    goto next;
  }

  if( json_object_object_get_ex(mqtt_msg_obj, "data", &data_obj) ) {
    ev.data = data_obj;
  } else {
    goto next;
  }

  stateM_handleEvent( &mqtt_ctx.m, &ev);

	json_object_put(type_obj);
	json_object_put(data_obj);
  json_object_put(mqtt_msg_obj);

	length = 1500;

  next:
    rc = pj_ioqueue_recvfrom(key, &recv_rec->op_key_, recv_rec->buffer, &length, 0,
                            &recv_rec->addr, &recv_rec->addrlen);

    if (rc == PJ_SUCCESS) {
      recv_rec->is_pending = 1;
      on_ev_socket_read_complete(g.ev_key, &recv_rec->op_key_, length);
    }
}

void on_ev_socket_write_complete(pj_ioqueue_key_t *key,
  pj_ioqueue_op_key_t *op_key,
  pj_ssize_t bytes_received)
{

  printf("Write msg on ioqueue udp\n");

}
