#ifndef ICE_AGENT_H
#define ICE_AGENT_H

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include "global.h"
#include "util.h"
#include "transport_ice.h"

#define ETH_MTU 1500
#define HDR_SIZE  40

struct accept_op
{
    pj_ioqueue_op_key_t	op_key_;
    pj_sock_t		sock;
    pj_sockaddr		src_addr;
    int			    src_addr_len;
};

/* struct op_key
{
    pj_ioqueue_op_key_t  op_key_;
    struct op_key       *peer;
    char                *buffer;
    pj_size_t            size;
    int                  is_pending;
    pj_status_t          last_err;
    pj_sockaddr_in       addr;
    int                  addrlen;
}; */

/* This is our global variables */
struct app_t
{
	/* Command line options are stored here */
	struct options {
		unsigned    comp_cnt;
		pj_str_t    ns;
		int	        max_host;
		pj_bool_t   regular;
		pj_str_t    stun_srv;
		pj_str_t    turn_srv;
		pj_bool_t   turn_tcp;
		pj_str_t    turn_username;
		pj_str_t    turn_password;
		pj_bool_t   turn_fingerprint;
		int	        port;
		pj_bool_t 	app_tcp;
		pj_bool_t		app_srv;
	} opt;

	/* Our global variables */
	pj_caching_pool		cp;
	pj_pool_t		    	*pool;
	pj_thread_t		  	*thread;
	pj_bool_t       	thread_quit_flag;
	pj_ice_strans_cfg	ice_cfg;
	pj_ice_strans	    *icest;

	struct rem_info rem;

	struct app_info {
		struct op_key read_op;
		struct op_key write_op;
		pj_ioqueue_key_t *key;
		char recv_buf[ETH_MTU - HDR_SIZE];
		char send_buf[ETH_MTU - HDR_SIZE];
	} app;
};

extern struct app_t iceagent;

/*
 * This is the main application initialization function. It is called
 * once (and only once) during application initialization sequence by
 * main().
 */
pj_status_t iceagent_init(CONFIG_t * config);

/*
 * Main console loop.
 */
void iceagent_console(void);

/*
 * Display program usage.
 */
void iceagent_usage();

/*
 * Send application data to remote agent.
 */
pj_status_t iceagent_send_data(unsigned comp_id, const char *data, pj_size_t bytes);

pj_status_t app_interface_proxy_ioqueue(pj_bool_t, int);

void iceagent_init_session(unsigned rolechar);

void iceagent_create_instance(void);

pj_status_t iceagent_parse_remote(const char *sdp, pj_pool_t *pool, pjmedia_sdp_session ** pp_ses);

#endif
