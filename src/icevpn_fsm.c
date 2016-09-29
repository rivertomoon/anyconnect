#include <pjlib.h>
#include <pjlib-util.h>
#include <json-c/json.h>
#include "global.h"
#include "util.h"
#include "mqtt_client.h"
#include "mqtt_topic.h"
#include "callbacks.h"
#include "mqtt_callbacks.h"

#include "fsm_event.h"
#include "icevpn_common.h"
#include "iceagent.h"

#include "common_api.h"
#include "initiator_api.h"
#include "responder_api.h"

#define THIS_FILE "icevpn_fsm.c"

/* Common Accessible State */
struct state icevpnGroupState, errorState;

struct state Initiator_InvitingState;

/* Forward declaration of states so that they can be defined in an logical
 * order: */
static struct state MqttDisconnectedState, MqttConnectedState,
		DiscoverPeerState, ActiveIceInitState, IceSessionInitState, IceDestroyState, Initiator_AcceptedState, Responder_AcceptedState,
		IceState, PeerConnectedState, VPNUpState;

/* All the following states (apart from the error state) are children of this
 * group state. This way, any unrecognised character will be handled by this
 * state's transition, eliminating the need for adding the same transition to
 * all the children states. */
struct state icevpnGroupState = {
   .parentState = NULL,
   /* The entry state is defined in order to demontrate that the 'reset'
    * transtition, going to this group state, will be 'redirected' to the
    * 'idle' state (the transition could of course go directly to the 'idle'
    * state): */
   .entryState = &MqttDisconnectedState,
   .transitions = (struct transition[]){
			/* { EV_START, (void *)(intptr_t)'!', NULL,
        NULL, &MqttDisconnectedState, }, */
			{ EV_ICE_SESS_OP_INIT,(void *)(intptr_t)&EV_PJ_SUCCESS, &ice_ev_handler, NULL,
				&MqttDisconnectedState, },
   },
   .numTransitions = 1,
   .data = (void *)(&(struct StateData){"icevpnGroupState", &icevpnGroupState}),
   .entryAction = &printEnterMsg,
   .exitAction = &printExitMsg,
};

static struct state MqttDisconnectedState = {
	.parentState = &icevpnGroupState,
	.entryState = NULL,
	.transitions = (struct transition[]) {
		{ EV_MQTT, (void *)(intptr_t)&EV_MQTT_CONNECTED, &mqtt_ev_handler, &action_subscribe_topics,
			&MqttConnectedState, },
		/* { EV_MQTT, (void *)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, &action_schedule_mqtt_connect_v2,
      &MqttDisconnectedState, }, */
	},
	.numTransitions = 1,
	.data = (void *)(&(struct StateData){"MQTT Disconnected", &MqttDisconnectedState}),
	.entryAction = &action_schedule_mqtt_connect,  	/*  */
	.exitAction = &printExitMsg,    /* Stop Timer to do MQTT Connect */
};

static struct state MqttConnectedState = {
  .parentState = &icevpnGroupState,
  .entryState = NULL,
  .transitions = (struct transition[]) {
		{ EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, NULL, &MqttDisconnectedState },
		{ EV_EXTERNAL_CMD, (void *)(intptr_t)&EV_CMD_INVITE, &cmd_ev_handler, action_init_session, &Initiator_InvitingState },
		{ EV_INCOMING_MSG, (void *)(intptr_t)NULL, &msg_ev_handler, &action_answer_peer, &Responder_AcceptedState },
 	},
  .numTransitions = 3,
  .data = (void *)(&(struct StateData){"MQTT Connected State", &MqttConnectedState}),
  .entryAction = &printEnterMsg,
  .exitAction = &printExitMsg,
};

/*
static struct state DiscoverPeerState = {
	.parentState = &icevpnGroupState,
	.entryState = NULL,
	.transitions = (struct transition[]) {
		{ EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, NULL, &MqttDisconnectedState },
		{ EV_INCOMING_MSG, (void *)(intptr_t)'a', &msg_ev_handler, NULL, &MqttConnectedState },
	},
	.numTransitions = 2,
	.data = (void *)(&(struct StateData){"Discover Peer State", &DiscoverPeerState}),
	.entryAction = NULL,
	.exitAction = NULL,
};

static struct state ActiveIceInitState = {
	.parentState = &icevpnGroupState,
	.entryState = NULL,
	.transitions = (struct transition[]){
		{ EV_ICE_STRANS_OP_INIT,(void *)(intptr_t)&EV_PJ_SUCCESS, &ice_ev_handler, NULL,
    	&Initiator_AcceptedState, },
    { EV_ICE_STRANS_OP_INIT,(void *)(intptr_t)&EV_PJNATH_ESTUNTIMEDOUT, &ice_ev_handler, NULL,
      &MqttDisconnectedState, },
	},
	.numTransitions = 2,
	.data = (void *)(&(struct StateData){"Ice Init State", &ActiveIceInitState}),
	.entryAction = NULL,
	.exitAction = NULL,
};

static struct state IceSessionInitState = {
  .parentState = &icevpnGroupState,
  .entryState = NULL,
  .transitions = (struct transition[]){
    { EV_ICE_SESS_OP_INIT,(void *)(intptr_t)&EV_PJ_SUCCESS, &ice_ev_handler, NULL,
      &Initiator_InvitingState },
    { EV_ICE_SESS_OP_INIT,(void *)(intptr_t)&EV_MQTT_ERROR, &ice_ev_handler, NULL,
      &MqttConnectedState },
  },
  .numTransitions = 2,
  .data = (void *)(&(struct StateData){"Ice Session Init", &IceSessionInitState}),
  .entryAction = &printEnterMsg,
  .exitAction = &printExitMsg,
};

static struct state IceDetroyState = {
	.parentState = &icevpnGroupState,
  .entryState = NULL,
  .transitions = (struct transition[]){
    { EV_ICE_STRANS_OP_INIT,(void *)(intptr_t)&EV_PJ_SUCCESS, &ice_ev_handler, NULL,
      &MqttDisconnectedState, },
  },
	.numTransitions = 1,
	.entryAction = &printEnterMsg,
	.exitAction = &printExitMsg,
};*/

struct state Initiator_InvitingState = {
	.parentState = &icevpnGroupState,
	.entryState = NULL,
	.transitions = (struct transition[]){
		{ EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, NULL,
			&MqttDisconnectedState, },
		{ EV_MQTT, (void*)(intptr_t)&EV_MQTT_ERROR, &mqtt_ev_handler, NULL,
			&IceDestroyState, },
		{ EV_TIMEOUT, (void *)(intptr_t)NULL, &timeout_ev_handler, NULL,
			&MqttConnectedState, },
		{ EV_INCOMING_MSG, (void *)(intptr_t)NULL, &msg_ev_handler, NULL,
			&Initiator_AcceptedState },
	},
	.numTransitions = 4,
	.data = (void *)(&(struct StateData){"Invite Peer", &Initiator_InvitingState}),
	.entryAction = &action_send_inv,
	.exitAction = &printExitMsg,
};

static struct state Initiator_AcceptedState = {
	.parentState = &icevpnGroupState,
	.entryState = NULL,
	.transitions = (struct transition[]){
		{ EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, NULL,
		&MqttDisconnectedState, },
		{ EV_EXTERNAL_CMD, (void *)(intptr_t)&EV_CMD_ICE, &cmd_ev_handler, NULL,
		&IceState, },
	},
	.numTransitions = 2,
	.data = (void *)(&(struct StateData){"Peer Accepted", &Initiator_AcceptedState}),
	.entryAction = &action_prepare_ice,
	.exitAction = &printExitMsg,
};

static struct state Responder_AcceptedState = {
  .parentState = &icevpnGroupState,
  .entryState = NULL,
  .transitions = (struct transition[]){
    { EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, NULL,
    &MqttDisconnectedState, },
		{ EV_ICE_STRANS_OP_NEGOTIATION, (void *)(intptr_t)&EV_PJ_SUCCESS, &ice_ev_handler, NULL,
		&VPNUpState },
  },
  .numTransitions = 2,
  .data = (void *)(&(struct StateData){"Accepting Peer", &Responder_AcceptedState}),
  .entryAction = &action_prepare_ice,
  .exitAction = &printExitMsg,
};

static struct state IceState = {
	.parentState = &icevpnGroupState,
	.entryState = NULL,
	.transitions = (struct transition[]){
		{ EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, NULL,
		&MqttDisconnectedState, },
		{ EV_ICE_STRANS_OP_NEGOTIATION, (void *)(intptr_t)&EV_PJNATH_EICEFAILED, &ice_ev_handler, NULL,
		&Initiator_AcceptedState },
		{ EV_ICE_STRANS_OP_NEGOTIATION, (void *)(intptr_t)&EV_PJ_SUCCESS, &ice_ev_handler, NULL,
		&VPNUpState },
	},
	.numTransitions = 3,
	.data = (void *)(&(struct StateData){"ICE", &IceState}),
	.entryAction = &action_start_ice,
	.exitAction = &printExitMsg,
};

#if 0
static struct state PeerConnectedState = {
	.parentState = &icevpnGroupState,
	.entryState = NULL,
	.transitions = (struct transition[]){
		{ EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, action_stop_session,
		&MqttDisconnectedState, },
		{ EV_EXTERNAL_CMD, (void*)(intptr_t)&EV_CMD_STARTVPN, &cmd_ev_handler, action_start_vpn_v2,
		&VPNUpState, },
		{ EV_EXTERNAL_CMD, (void*)(intptr_t)&EV_CMD_BYE, &cmd_ev_handler, action_stop_session,
		&MqttConnectedState, },
		{ EV_INCOMING_MSG, (void *)(intptr_t)NULL, &msg_ev_handler, NULL,
		&MqttConnectedState, },
	},
	.numTransitions = 4,
	.data = (void *)(&(struct StateData){"Peer Connected", &PeerConnectedState}),
	.entryAction = &printEnterMsg,
	.exitAction = &printExitMsg,
};
#endif

static struct state VPNUpState = {
  .parentState = &icevpnGroupState,
  .entryState = NULL,
  .transitions = (struct transition[]){
    { EV_MQTT, (void*)(intptr_t)&EV_MQTT_DISCONNECTED, &mqtt_ev_handler, action_stop_session,
    &MqttDisconnectedState, },
    { EV_EXTERNAL_CMD, (void*)(intptr_t)&EV_CMD_CLOSE, &cmd_ev_handler, action_stop_session,
    &MqttConnectedState, },
    { EV_INCOMING_MSG, (void *)(intptr_t)NULL, &msg_ev_handler, NULL,
    &MqttConnectedState, },
	},
  .numTransitions = 3,
  .data = (void *)(&(struct StateData){"VPN Up", &VPNUpState}),
  .entryAction = &action_start_vpn,
  .exitAction = &action_stop_vpn,
};

struct state errorState = {
   .entryAction = &printErrMsg
};
