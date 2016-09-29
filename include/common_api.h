#ifndef COMMON_API_H
#define COMMON_API_H

bool buffer_ev_handler(void * r, struct event *event );

void enter_state_common( void *stateData, struct event *event );

void action_init_session( void *currentStateData, struct event *event, void *newStateData );

void action_init_session_v2( void *stateData, struct event *event );

void action_start_vpn( void *stateData, struct event *event );

void action_start_vpn_v2( void *currentStateData, struct event *event, void *newStateData );

void action_stop_vpn( void *stateData, struct event *event );

#endif
