#include "global.h"
#include "util.h"
#include "fsm_event.h"
#include "cmd.h"

#define CHECK(expr)	status=expr; \
			if (status!=PJ_SUCCESS) { \
			    app_perror(#expr, status); \
			    return status; \
			}

CONFIG_t config;

CommonUtil_t util;

icevpn_ctx_t mqtt_ctx;

shadowvpn_args_t args;

vpn_ctx_t vpn_ctx;

struct global g;

/* log callback to write to file */
static void log_func(int level, const char *data, int len)
{
  pj_log_write(level, data, len);

  if (util.log_fhnd) {
    if(fwrite(data, len, 1, util.log_fhnd) != 1)
      return;
  }
}

pj_status_t g_init(CONFIG_t * config)
{
	pj_status_t status;
	pj_sock_t cmd_sock, event_sock;
	pj_ioqueue_callback ev_socket_callback;

	/* CHECK( pj_init() );
 	   CHECK( pjlib_util_init() );
	   CHECK( pjnath_init() ); */

	pj_bzero(&ev_socket_callback, sizeof(ev_socket_callback));
	ev_socket_callback.on_read_complete = &on_ev_socket_read_complete;
	ev_socket_callback.on_write_complete = &on_ev_socket_write_complete;

	if (config->generic.log_file) {
		g.log_fhnd = fopen(config->generic.log_file, "a");
		pj_log_set_log_func(&log_func);
	}

	pj_log_set_level(config->generic.log_lvl);

	/* Must create pool factory, where memory allocations come from */
	pj_caching_pool_init(&g.cp, &pj_pool_factory_default_policy, 0);

	g.pool = pj_pool_create(&g.cp.factory, "main", 1000, 1000, NULL);

	/* Create global timer heap */
	CHECK( pj_timer_heap_create(g.pool, 1000, &g.timer_heap) );

	/* Create global ioqueue */
	CHECK( pj_ioqueue_create(g.pool, 16, &g.ioqueue) );

	status = app_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0,
			config->generic.event_port, &g.ev_sock); 

	/* status = unix_socket(pj_AF_UNIX(), pj_SOCK_DGRAM(), 0,
			&g.ev_sock); */
	
	if (status != PJ_SUCCESS) {
		app_perror("...app_socket error", status);
		return -30;
	}

	/* Socket for Event Generated in Action */
	status = pj_ioqueue_register_sock(g.pool, g.ioqueue, g.ev_sock, NULL,
		&ev_socket_callback, &g.ev_key);

	if (status != PJ_SUCCESS) {
		app_perror("...error registering socket", status);
		return -40;
	}

	return PJ_SUCCESS;
}

