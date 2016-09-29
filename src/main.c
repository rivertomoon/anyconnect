#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "mqtt_callbacks.h"
#include "global.h"
#include "icevpn_common.h"

int disc_finished = 0;

static const char *help_message =
"usage: icevpn -c config_file \n"
"\n"
"  -h, --help            show this help message and exit\n"
"  -c config_file        path to config file\n"
"\n";

static void print_help() __attribute__ ((noreturn));

static void print_help() {
  printf("%s", help_message);
  exit(1);
}

/*
 * This function checks for events from both timer and ioqueue (for
 * network events). It is invoked by the worker thread.
 */
static pj_status_t handle_events(unsigned max_msec, unsigned *p_count)
{
	enum { MAX_NET_EVENTS = 1 };
	pj_time_val max_timeout = {0, 0};
	pj_time_val timeout = { 0, 0};
	unsigned count = 0, net_event_count = 0;
	int c;

	max_timeout.msec = max_msec;

	/* Poll the timer to run it and also to retrieve the earliest entry. */
	timeout.sec = timeout.msec = 0;
	c = pj_timer_heap_poll( g.timer_heap, &timeout );
	if (c > 0)
		count += c;

	/* timer_heap_poll should never ever returns negative value, or otherwise
	 * ioqueue_poll() will block forever!
	 */
	pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
	if (timeout.msec >= 1000) timeout.msec = 999;

	/* compare the value with the timeout to wait from timer, and use the
	 * minimum value.
	 */
	if (PJ_TIME_VAL_GT(timeout, max_timeout))
		timeout = max_timeout;

	/* Poll ioqueue.
 	 * Repeat polling the ioqueue while we have immediate events, because
 	 * timer heap may process more than one events, so if we only process
 	 * one network events at a time (such as when IOCP backend is used),
 	 * the ioqueue may have trouble keeping up with the request rate.
 	 *
 	 * For example, for each send() request, one network event will be
 	 *   reported by ioqueue for the send() completion. If we don't poll
 	 *   the ioqueue often enough, the send() completion will not be
 	 *   reported in timely manner.
 	 */
	do {
		c = pj_ioqueue_poll( g.ioqueue, &timeout);
		if (c < 0) {
			pj_status_t err = pj_get_netos_error();
			pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
			if (p_count)
				*p_count = count;
			return err;
		} else if (c == 0) {
			break;
		} else {
			net_event_count += c;
			timeout.sec = timeout.msec = 0;
		}
	} while (c > 0 && net_event_count < MAX_NET_EVENTS);

	count += net_event_count;
	if (p_count)
		*p_count = count;

	return PJ_SUCCESS;
}

int parse_args(int argc, char** argv) {
  int opt;

  while (-1 != (opt = getopt(argc, argv, "c:v"))) {
    switch (opt) {
    	case 'c':
				g.conf_file = strdup(optarg);
      	break;
    	default:
      	ERROR_LOG("Error in command line argument parsing");
      	break;
    }
  }

	if (!g.conf_file)
    print_help();

	return 0;

}

static void
quit(EV_P_ ev_signal *w, int revents)
{
	pj_shutdown();
  MQTTAsync_destroy(&client);

}

int main(int argc, char* argv[])
{
	pj_status_t status = PJ_SUCCESS;

	struct op_key read_op;
	char ev_recv_buf[1500];

	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTAsync_token token;
	int rc;
	int ch;

	CHECK( pj_init() );
  CHECK( pjlib_util_init() );
  CHECK( pjnath_init() );

	// parse args
 	if (0 != parse_args(argc, argv)) {
    errf("error when parsing args");
  }

  if( (status = parse_file(g.conf_file, &config)) != PJ_SUCCESS )
    goto on_exit;

	shadowvpn_load_config(&g.args, config.vpn_config.config_file);

  if( PJ_SUCCESS != g_init(&config)) {
		ERROR_LOG("Global Initialize Failed");
		goto on_exit;	
	};

  /* pj_thread_create(g.pool, "tun", &vpn_run_new,
                  &vpn_ctx, 0, 0, &iceagent.f_thread); */

	read_op.is_pending = 0;
  read_op.last_err = 0;
  read_op.buffer = ev_recv_buf;
  read_op.size = sizeof(ev_recv_buf);
  read_op.addrlen = sizeof(read_op.addr);

  status = pj_ioqueue_recvfrom(g.ev_key, &read_op.op_key_, read_op.buffer, &read_op.size, 0,
                            &read_op.addr, &read_op.addrlen);

  if (status == PJ_SUCCESS) {
  	read_op.is_pending = 1;
    on_ev_socket_read_complete(g.ev_key, &read_op.op_key_, read_op.size);
  }

	/* Initialize MQTT Client */
  if( (status = mqtt_init(&mqtt_ctx, &config)) != PJ_SUCCESS )
    goto on_exit;

  /* Initialized ICE Agent */
  if( (status = iceagent_init(&config)) != PJ_SUCCESS )
    goto on_exit;

	iceagent_create_instance();

	//stateM_handleEvent( &mqtt_ctx.m, &(struct event) { EV_START, (void*)(intptr_t)'!'});

	stateM_handleEvent( &mqtt_ctx.m, &(struct event) { EV_ICE_SESS_OP_INIT, (void*)(intptr_t)&EV_PJ_SUCCESS});

	/* Handle Events */
  while (true) {
    handle_events(500, NULL);
  }

	disc_opts.onSuccess = onDisconnect;
	if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start disconnect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

 	while	(!disc_finished)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

on_exit:
	pj_shutdown();
	MQTTAsync_destroy(&client);
 	return rc;
}
