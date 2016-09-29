#ifndef RESPONDER_API_H
#define RESPONDER_API_H

/* set permission of TURN, start up vpn interface */
void action_prepare_responder( void *stateData, struct event *event );

void action_answer_peer(void *currentStateData, struct event *event, void *newStateData);

#endif
