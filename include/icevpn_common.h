#ifndef ICEVPN_COMMON
#define ICEVPN_COMMON

// #define MAX_TIMER_COUNT 5

struct StateData {
	char *name;
	void *state_ptr;

	struct event * cached_event;
};

bool timeout_ev_handler(void * r, struct event *event);

bool mqtt_ev_handler(void * r, struct event *event);

bool ice_ev_handler(void * r, struct event *event);

bool cmd_ev_handler(void * r, struct event *event);

bool msg_ev_handler(void * r, struct event *event);

void action_init_instance( void *stateData, struct event *event );

void action_stop_session( void *currentStateData, struct event *event, void *newStateData );

void action_stop_session_v2( void *stateData, struct event *event );

void action_schedule_mqtt_connect( void *stateData, struct event *event );

void action_schedule_mqtt_connect_v2( void *currentStateData, struct event *event,
                                    void *newStateData );

void action_subscribe_topics( void *currentStateData, struct event *event,
                                    void *newStateData );

void action_cancel_timer( void *currentStateData, struct event *event,
                                void *newStateData );

void action_prepare_ice( void *stateData, struct event *event );

pj_timer_entry * init_timer(pj_time_val *delay, void (*callback)(pj_timer_heap_t *,
														pj_timer_entry *));

void printReset( void *oldStateData, struct event *event,
      void *newStateData );

void printErrMsg( void *stateData, struct event *event );
void printEnterMsg( void *stateData, struct event *event );
void printExitMsg( void *stateData, struct event *event );

#endif
