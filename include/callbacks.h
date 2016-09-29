#ifndef CLIENT_CALLBACKS
#define CLIENT_CALLBACKS

#include <pjlib.h>
#include <pjnath.h>

#if 0
	int MQTTcallbackHandler(MQTTCallbackParams params);

	void disconnectCallbackHandler(void);
#endif 

void timer_callback(pj_timer_heap_t *timer_heap, pj_timer_entry *entry);

void cb_on_ice_complete(pj_ice_strans *ice_st, pj_ice_strans_op op, pj_status_t status);

#endif
