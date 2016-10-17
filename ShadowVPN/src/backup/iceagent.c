/* $Id: iceagent.c 4624 2013-10-21 06:37:30Z ming $ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <fcntl.h>

#include "shadowvpn.h"
#include "iceagent.h"

#define THIS_FILE   "iceagent.c"

/* For this demo app, configure longer STUN keep-alive time
 * so that it does't clutter the screen output.
 */
#define KA_INTERVAL 300

typedef struct {
    unsigned media_cnt;
    unsigned comp0_port;
    char     comp0_addr[80];
}comp_info_t;

struct op_key
{
    pj_ioqueue_op_key_t  op_key_;
    struct op_key       *peer;
    char                *buffer;
    pj_size_t            size;
    int                  is_pending;
    pj_status_t          last_err;
    pj_sockaddr_in       addr;
    int                  addrlen;
};

/* This is our global variables */
struct app_t iceagent;

static pj_ioqueue_key_t *key;

static void on_read_complete(pj_ioqueue_key_t *key,
                             pj_ioqueue_op_key_t *op_key,
                             pj_ssize_t bytes_received)
{

	PJ_LOG(3, (THIS_FILE, "Received Pkts"));

}

static void on_write_complete(pj_ioqueue_key_t *key,
                              pj_ioqueue_op_key_t *op_key,
                              pj_ssize_t bytes_sent)
{

	PJ_LOG(3, (THIS_FILE, "Send kts"));

}

/* Utility to display error messages */
static void iceagent_perror(const char *title, pj_status_t status)
{
	char errmsg[PJ_ERR_MSG_SIZE];

	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(1,(THIS_FILE, "%s: %s", title, errmsg));
}

/* Utility: display error message and exit application (usually
 * because of fatal error.
 */
void err_exit(const char *title, pj_status_t status)
{
	if (status != PJ_SUCCESS) {
		iceagent_perror(title, status);
	}
	PJ_LOG(3,(THIS_FILE, "Shutting down.."));

    if (iceagent.icest)
			pj_ice_strans_destroy(iceagent.icest);

    pj_thread_sleep(500);

    iceagent.thread_quit_flag = PJ_TRUE;
    if (iceagent.thread) {
      pj_thread_join(iceagent.thread);
      pj_thread_destroy(iceagent.thread);
    }

    if (iceagent.ice_cfg.stun_cfg.ioqueue)
      pj_ioqueue_destroy(iceagent.ice_cfg.stun_cfg.ioqueue);

    if (iceagent.ice_cfg.stun_cfg.timer_heap)
      pj_timer_heap_destroy(iceagent.ice_cfg.stun_cfg.timer_heap);

    pj_caching_pool_destroy(&iceagent.cp);

    pj_shutdown();

    if (iceagent.log_fhnd) {
      fclose(iceagent.log_fhnd);
      iceagent.log_fhnd = NULL;
    }

    exit(status != PJ_SUCCESS);
}

#define CHECK(expr)	status=expr; \
			if (status!=PJ_SUCCESS) { \
			    err_exit(#expr, status); \
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
    c = pj_timer_heap_poll( iceagent.ice_cfg.stun_cfg.timer_heap, &timeout );
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
		c = pj_ioqueue_poll( iceagent.ice_cfg.stun_cfg.ioqueue, &timeout);
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

/*
 * This is the worker thread that polls event in the background.
 */
static int iceagent_worker_thread(void *unused)
{
	struct op_key read_op,write_op;
	char recv_buf[512], send_buf[512];
	pj_ssize_t length;
	pj_status_t rc;

	PJ_UNUSED_ARG(unused);

	/* length = sizeof(recv_buf);
	rc = pj_ioqueue_send(key, &write_op.op_key_,
  	send_buf, &length, 0); */

	/* length = sizeof(recv_buf);
	rc = pj_ioqueue_recv(key, &read_op.op_key_, recv_buf, &length, 0);

	if (rc == PJ_SUCCESS) {
		read_op.is_pending = 1;
		on_read_complete(key, &read_op.op_key_, length);
	} */

	while (!iceagent.thread_quit_flag) {
 		handle_events(500, NULL);
	}

	return 0;
}

/*
 * This is the callback that is registered to the ICE stream transport to
 * receive notification about incoming data. By "data" it means application
 * data such as RTP/RTCP, and not packets that belong to ICE signaling (such
 * as STUN connectivity checks or TURN signaling).
 */
static void cb_on_rx_data(pj_ice_strans *ice_st,
			  unsigned comp_id,
			  void *pkt, pj_size_t size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len)
{
	char ipstr[PJ_INET6_ADDRSTRLEN+10];

	// PJ_UNUSED_ARG(ice_st);
	// PJ_UNUSED_ARG(src_addr_len);
	// PJ_UNUSED_ARG(pkt);

  // Don't do this! It will ruin the packet buffer in case TCP is used!
  //((char*)pkt)[size] = '\0';

	PJ_LOG(3,(THIS_FILE, "Component %d: received %d bytes data from %s: \"%.*s\"",
	      comp_id, size,
	      pj_sockaddr_print(src_addr, ipstr, sizeof(ipstr), 3),
	      (unsigned)size,
	      (char*)pkt));

	if (-1 == tun_write(iceagent.ctx->tun, (char *)pkt, (unsigned)size) ) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// do nothing
		} else if (errno == EPERM || errno == EINTR || errno == EINVAL) {
			// just log, do nothing
			err("write to tun");
		} else {
			err("write to tun");
		}
	}
}

/*
 * This is the callback that is registered to the ICE stream transport to
 * receive notification about ICE state progression.
 */
static void cb_on_ice_complete(pj_ice_strans *ice_st,
			       pj_ice_strans_op op,
			       pj_status_t status)
{
  const char *opname =
      (op==PJ_ICE_STRANS_OP_INIT? "initialization" :
      (op==PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation" : "unknown_op"));

  if( op == PJ_ICE_STRANS_OP_NEGOTIATION && status == PJ_SUCCESS) {
      if (-1 == vpn_ctx_init(iceagent.ctx, iceagent.pargs))
        errf("vpn ctx init failed");
      CHECK( pj_thread_create(iceagent.pool, "tun", &vpn_run_new,
        iceagent.ctx, 0, 0, &iceagent.f_thread) );
  }

  if (status == PJ_SUCCESS) {
    PJ_LOG(3,(THIS_FILE, "ICE %s successful", opname));
  } else {
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(THIS_FILE, "ICE %s failed: %s", opname, errmsg));
    pj_ice_strans_destroy(ice_st);
    iceagent.icest = NULL;
  }


}

/* log callback to write to file */
static void log_func(int level, const char *data, int len)
{
    pj_log_write(level, data, len);
    if (iceagent.log_fhnd) {
	if (fwrite(data, len, 1, iceagent.log_fhnd) != 1)
	    return;
    }
}

/*
 * This is the main application initialization function. It is called
 * once (and only once) during application initialization sequence by
 * main().
 */
pj_status_t iceagent_init(CONFIG_t * config)
{
    pj_status_t status;
    int fd = 0;

		iceagent.opt.comp_cnt = 1;
		iceagent.opt.max_host = -1;

    pj_ioqueue_callback callback;

    pj_bzero(&callback, sizeof(callback));
    callback.on_read_complete = &on_read_complete;
    callback.on_write_complete = &on_write_complete;

    if (iceagent.opt.log_file) {
        iceagent.log_fhnd = fopen(iceagent.opt.log_file, "a");
        pj_log_set_log_func(&log_func);
    }

    /* Initialize the libraries before anything else */
    CHECK( pj_init() );
    CHECK( pjlib_util_init() );
    CHECK( pjnath_init() );

    /* Must create pool factory, where memory allocations come from */
    pj_caching_pool_init(&iceagent.cp, NULL, 0);

    /* Init our ICE settings with null values */
    pj_ice_strans_cfg_default(&iceagent.ice_cfg);

    iceagent.ice_cfg.stun_cfg.pf = &iceagent.cp.factory;

    /* Create application memory pool */
    iceagent.pool = pj_pool_create(&iceagent.cp.factory, "iceagent",
				  512, 512, NULL);

    /* Create timer heap for timer stuff */
    CHECK( pj_timer_heap_create(iceagent.pool, 100,
				&iceagent.ice_cfg.stun_cfg.timer_heap) );

    /* and create ioqueue for network I/O stuff */
    CHECK( pj_ioqueue_create(iceagent.pool, 16,
			     &iceagent.ice_cfg.stun_cfg.ioqueue) );

    /* something must poll the timer heap and ioqueue,
     * unless we're on Symbian where the timer heap and ioqueue run
     * on themselves.
     */
    CHECK( pj_thread_create(iceagent.pool, "iceagent", &iceagent_worker_thread,
        NULL, 0, 0, &iceagent.thread) );

    iceagent.ice_cfg.af = pj_AF_INET();

    /* Create DNS resolver if nameserver is set */
    if (iceagent.opt.ns.slen) {
        CHECK( pj_dns_resolver_create(&iceagent.cp.factory,
				      "resolver",
				      0,
				      iceagent.ice_cfg.stun_cfg.timer_heap,
				      iceagent.ice_cfg.stun_cfg.ioqueue,
				      &iceagent.ice_cfg.resolver) );

        CHECK( pj_dns_resolver_set_ns(iceagent.ice_cfg.resolver, 1,
				      &iceagent.opt.ns, NULL) );
    }

    /* -= Start initializing ICE stream transport config =- */

    /* Maximum number of host candidates */
		if (iceagent.opt.max_host != -1)
			iceagent.ice_cfg.stun.max_host_cands = iceagent.opt.max_host;

		/* Nomination strategy */
		if (iceagent.opt.regular)
			iceagent.ice_cfg.opt.aggressive = PJ_FALSE;
    else
			iceagent.ice_cfg.opt.aggressive = PJ_TRUE;

		/* Configure STUN/srflx candidate resolution */
		if (iceagent.opt.stun_srv.slen) {
		char *pos;

	/* Command line option may contain port number */
	if ((pos=pj_strchr(&iceagent.opt.stun_srv, ':')) != NULL) {
	    iceagent.ice_cfg.stun.server.ptr = iceagent.opt.stun_srv.ptr;
	    iceagent.ice_cfg.stun.server.slen = (pos - iceagent.opt.stun_srv.ptr);

	    iceagent.ice_cfg.stun.port = (pj_uint16_t)atoi(pos+1);
	} else {
	    iceagent.ice_cfg.stun.server = iceagent.opt.stun_srv;
	    iceagent.ice_cfg.stun.port = PJ_STUN_PORT;
	}

	/* For this demo app, configure longer STUN keep-alive time
	 * so that it does't clutter the screen output.
	 */
	iceagent.ice_cfg.stun.cfg.ka_interval = KA_INTERVAL;
    }

    /* Configure TURN candidate */
    if (iceagent.opt.turn_srv.slen) {
	char *pos;

	/* Command line option may contain port number */
	if ((pos=pj_strchr(&iceagent.opt.turn_srv, ':')) != NULL) {
	    iceagent.ice_cfg.turn.server.ptr = iceagent.opt.turn_srv.ptr;
	    iceagent.ice_cfg.turn.server.slen = (pos - iceagent.opt.turn_srv.ptr);

	    iceagent.ice_cfg.turn.port = (pj_uint16_t)atoi(pos+1);
	} else {
	    iceagent.ice_cfg.turn.server = iceagent.opt.turn_srv;
	    iceagent.ice_cfg.turn.port = PJ_STUN_PORT;
	}

	/* TURN credential */
	iceagent.ice_cfg.turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	iceagent.ice_cfg.turn.auth_cred.data.static_cred.username = iceagent.opt.turn_username;
	iceagent.ice_cfg.turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
	iceagent.ice_cfg.turn.auth_cred.data.static_cred.data = iceagent.opt.turn_password;

	/* Connection type to TURN server */
	if (iceagent.opt.turn_tcp)
	    iceagent.ice_cfg.turn.conn_type = PJ_TURN_TP_TCP;
	else
	    iceagent.ice_cfg.turn.conn_type = PJ_TURN_TP_UDP;

	/* For this demo app, configure longer keep-alive time
	 * so that it does't clutter the screen output.
	 */
		iceagent.ice_cfg.turn.alloc_param.ka_interval = KA_INTERVAL;
	}

	/* fd = (iceagent.ctx)->tun;

	status = pj_ioqueue_register_fd(iceagent.pool, iceagent.ice_cfg.stun_cfg.ioqueue, fd, NULL,
                                  &callback, &key);
	if (status != PJ_SUCCESS) {
			iceagent_perror("...error registering socket", status);
			return -40;
	} */

	/* -= That's it for now, initialization is complete =- */
	return PJ_SUCCESS;
}


/*
 * Create ICE stream transport instance, invoked from the menu.
 */
void iceagent_create_instance(void)
{
    pj_ice_strans_cb icecb;
    pj_status_t status;

    if (iceagent.icest != NULL) {
	puts("ICE instance already created, destroy it first");
	return;
    }

    /* init the callback */
    pj_bzero(&icecb, sizeof(icecb));
    icecb.on_rx_data = cb_on_rx_data;
    icecb.on_ice_complete = cb_on_ice_complete;

    /* create the instance */
    status = pj_ice_strans_create("iceagent",		    /* object name  */
				&iceagent.ice_cfg,	    /* settings	    */
				iceagent.opt.comp_cnt,	    /* comp_cnt	    */
				NULL,			    /* user data    */
				&icecb,			    /* callback	    */
				&iceagent.icest)		    /* instance ptr */
				;
    if (status != PJ_SUCCESS)
			iceagent_perror("error creating ice", status);
    else
			PJ_LOG(3,(THIS_FILE, "ICE instance successfully created"));
}

/* Utility to nullify parsed remote info */
static void reset_rem_info(void)
{
    pj_bzero(&iceagent.rem, sizeof(iceagent.rem));
}


/*
 * Destroy ICE stream transport instance, invoked from the menu.
 */
void iceagent_destroy_instance(void)
{
    if (iceagent.icest == NULL) {
        PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
        return;
    }

    pj_ice_strans_destroy(iceagent.icest);
    iceagent.icest = NULL;

    reset_rem_info();

    PJ_LOG(3,(THIS_FILE, "ICE instance destroyed"));
}


/*
 * Create ICE session, invoked from the menu.
 */
void iceagent_init_session(unsigned rolechar)
{
    pj_ice_sess_role role = (pj_tolower((pj_uint8_t)rolechar)=='o' ?
				PJ_ICE_SESS_ROLE_CONTROLLING :
				PJ_ICE_SESS_ROLE_CONTROLLED);
    pj_status_t status;

    if (iceagent.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    if (pj_ice_strans_has_sess(iceagent.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: Session already created"));
	return;
    }

    status = pj_ice_strans_init_ice(iceagent.icest, role, NULL, NULL);
    if (status != PJ_SUCCESS)
        iceagent_perror("error creating session", status);
    else
        PJ_LOG(3,(THIS_FILE, "ICE session created"));

    reset_rem_info();
}


/*
 * Stop/destroy ICE session, invoked from the menu.
 */
void iceagent_stop_session(void)
{
    pj_status_t status;

    if (iceagent.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    if (!pj_ice_strans_has_sess(iceagent.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE session, initialize first"));
	return;
    }

    status = pj_ice_strans_stop_ice(iceagent.icest);
    if (status != PJ_SUCCESS)
	iceagent_perror("error stopping session", status);
    else
	PJ_LOG(3,(THIS_FILE, "ICE session stopped"));

    reset_rem_info();
}

#define PRINT(...)	    \
	printed = pj_ansi_snprintf(p, maxlen - (p-buffer),  \
				   __VA_ARGS__); \
	if (printed <= 0 || printed >= (int)(maxlen - (p-buffer))) \
	    return -PJ_ETOOSMALL; \
	p += printed


/* Utility to create a=candidate SDP attribute */
static int print_cand(char buffer[], unsigned maxlen,
		      const pj_ice_sess_cand *cand)
{
    char ipaddr[PJ_INET6_ADDRSTRLEN];
    char *p = buffer;
    int printed;

    PRINT("a=candidate:%.*s %u UDP %u %s %u typ ",
	  (int)cand->foundation.slen,
	  cand->foundation.ptr,
	  (unsigned)cand->comp_id,
	  cand->prio,
	  pj_sockaddr_print(&cand->addr, ipaddr,
			    sizeof(ipaddr), 0),
	  (unsigned)pj_sockaddr_get_port(&cand->addr));

    PRINT("%s\n",
	  pj_ice_get_cand_type_name(cand->type));

    if (p == buffer+maxlen)
	return -PJ_ETOOSMALL;

    *p = '\0';

    return (int)(p-buffer);
}

/*
 * Encode ICE information in SDP.
 */
static int encode_session(char buffer[], unsigned maxlen)
{
    char *p = buffer;
    unsigned comp;
    int printed;
    pj_str_t local_ufrag, local_pwd;
    pj_status_t status;

    /* Write "dummy" SDP v=, o=, s=, and t= lines */
    PRINT("v=0\no=- 3414953978 3414953978 IN IP4 localhost\ns=ice\nt=0 0\n");

    /* Get ufrag and pwd from current session */
    pj_ice_strans_get_ufrag_pwd(iceagent.icest, &local_ufrag, &local_pwd,
				NULL, NULL);

    /* Write the a=ice-ufrag and a=ice-pwd attributes */
    PRINT("a=ice-ufrag:%.*s\na=ice-pwd:%.*s\n",
	   (int)local_ufrag.slen,
	   local_ufrag.ptr,
	   (int)local_pwd.slen,
	   local_pwd.ptr);

    /* Write each component */
    for (comp=0; comp<iceagent.opt.comp_cnt; ++comp) {
	unsigned j, cand_cnt;
	pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
	char ipaddr[PJ_INET6_ADDRSTRLEN];

	/* Get default candidate for the component */
	status = pj_ice_strans_get_def_cand(iceagent.icest, comp+1, &cand[0]);
	if (status != PJ_SUCCESS)
	    return -status;

	/* Write the default address */
	if (comp==0) {
	    /* For component 1, default address is in m= and c= lines */
	    PRINT("m=audio %d RTP/AVP 0\n"
		  "c=IN IP4 %s\n",
		  (int)pj_sockaddr_get_port(&cand[0].addr),
		  pj_sockaddr_print(&cand[0].addr, ipaddr,
				    sizeof(ipaddr), 0));
	} else if (comp==1) {
	    /* For component 2, default address is in a=rtcp line */
	    PRINT("a=rtcp:%d IN IP4 %s\n",
		  (int)pj_sockaddr_get_port(&cand[0].addr),
		  pj_sockaddr_print(&cand[0].addr, ipaddr,
				    sizeof(ipaddr), 0));
	} else {
	    /* For other components, we'll just invent this.. */
	    PRINT("a=Xice-defcand:%d IN IP4 %s\n",
		  (int)pj_sockaddr_get_port(&cand[0].addr),
		  pj_sockaddr_print(&cand[0].addr, ipaddr,
				    sizeof(ipaddr), 0));
	}

	/* Enumerate all candidates for this component */
	cand_cnt = PJ_ARRAY_SIZE(cand);
	status = pj_ice_strans_enum_cands(iceagent.icest, comp+1,
					  &cand_cnt, cand);
	if (status != PJ_SUCCESS)
	    return -status;

	/* And encode the candidates as SDP */
	for (j=0; j<cand_cnt; ++j) {
	    printed = print_cand(p, maxlen - (unsigned)(p-buffer), &cand[j]);
	    if (printed < 0)
		return -PJ_ETOOSMALL;
	    p += printed;
	}
    }

    if (p == buffer+maxlen)
	return -PJ_ETOOSMALL;

    *p = '\0';
    return (int)(p - buffer);
}


/*
 * Show information contained in the ICE stream transport. This is
 * invoked from the menu.
 */
void iceagent_show_ice(void)
{
    static char buffer[1000];
    int len;

    if (iceagent.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    puts("General info");
    puts("---------------");
    printf("Component count    : %d\n", iceagent.opt.comp_cnt);
    printf("Status             : ");
    if (pj_ice_strans_sess_is_complete(iceagent.icest))
	puts("negotiation complete");
    else if (pj_ice_strans_sess_is_running(iceagent.icest))
	puts("negotiation is in progress");
    else if (pj_ice_strans_has_sess(iceagent.icest))
	puts("session ready");
    else
	puts("session not created");

    if (!pj_ice_strans_has_sess(iceagent.icest)) {
	puts("Create the session first to see more info");
	return;
    }

    printf("Negotiated comp_cnt: %d\n",
	   pj_ice_strans_get_running_comp_cnt(iceagent.icest));
    printf("Role               : %s\n",
	   pj_ice_strans_get_role(iceagent.icest)==PJ_ICE_SESS_ROLE_CONTROLLED ?
	   "controlled" : "controlling");

    len = encode_session(buffer, sizeof(buffer));
    if (len < 0)
	err_exit("not enough buffer to show ICE status", -len);

    puts("");
    printf("Local SDP (paste this to remote host):\n"
	   "--------------------------------------\n"
	   "%s\n", buffer);


    puts("");
    puts("Remote info:\n"
	 "----------------------");
    if (iceagent.rem.cand_cnt==0) {
	puts("No remote info yet");
    } else {
	unsigned i;

	printf("Remote ufrag       : %s\n", iceagent.rem.ufrag);
	printf("Remote password    : %s\n", iceagent.rem.pwd);
	printf("Remote cand. cnt.  : %d\n", iceagent.rem.cand_cnt);

	for (i=0; i<iceagent.rem.cand_cnt; ++i) {
	    len = print_cand(buffer, sizeof(buffer), &iceagent.rem.cand[i]);
	    if (len < 0)
		err_exit("not enough buffer to show ICE status", -len);

	    printf("  %s", buffer);
	}
    }
}

char * iceagent_strip_line(char * linebuf, pj_size_t * p_len)
{
    char * line = linebuf;
    pj_size_t len = *p_len;

    while (len && (linebuf[*p_len-1] == '\r' || linebuf[*p_len-1] == '\n'))
		linebuf[--(*p_len)] = '\0';

	while (len && pj_isspace(*linebuf))
		++line, --(*p_len);

    return line;
}

void iceagent_parse_line(comp_info_t * comp_info, char * line, pj_size_t len)
{
    //char *line;
	/* while (len && (linebuf[len-1] == '\r' || linebuf[len-1] == '\n'))
		linebuf[--len] = '\0';

	line = linebuf;
	while (len && pj_isspace(*line))
		++line, --len;

	if (len==0)
		return; */

	/* Ignore subsequent media descriptors */
	/* if (media_cnt > 1)
		 return; */

	switch (line[0]) {
		case 'm':
		    {
			int cnt;
			char media[32], portstr[32];

			++(comp_info->media_cnt);
			if (comp_info->media_cnt > 1) {
				puts("Media line ignored");
				break;
			}

			cnt = sscanf(line+2, "%s %s RTP/", media, portstr);
			if (cnt != 2) {
				PJ_LOG(1,(THIS_FILE, "Error parsing media line"));
				goto on_error;
			}

			comp_info->comp0_port = atoi(portstr);
		    }
            break;
        case 'c':
            {
            int cnt;
            char c[32], net[32], ip[80];

            cnt = sscanf(line+2, "%s %s %s", c, net, ip);
            if (cnt != 3) {
                PJ_LOG(1,(THIS_FILE, "Error parsing connection line"));
                goto on_error;
            }

            strcpy(comp_info->comp0_addr, ip);
            }
            break;
        case 'a':
            {
            char *attr = strtok(line+2, ": \t\r\n");
            if (strcmp(attr, "ice-ufrag")==0) {
                strcpy(iceagent.rem.ufrag, attr+strlen(attr)+1);
            } else if (strcmp(attr, "ice-pwd")==0) {
                strcpy(iceagent.rem.pwd, attr+strlen(attr)+1);
            } else if (strcmp(attr, "rtcp")==0) {
                char *val = attr+strlen(attr)+1;
                int af, cnt;
                int port;
                char net[32], ip[64];
                pj_str_t tmp_addr;
                pj_status_t status;

                cnt = sscanf(val, "%d IN %s %s", &port, net, ip);
                if (cnt != 3) {
                    PJ_LOG(1,(THIS_FILE, "Error parsing rtcp attribute"));
                    goto on_error;
                }
                if (strchr(ip, ':'))
                    af = pj_AF_INET6();
                else
                    af = pj_AF_INET();

                pj_sockaddr_init(af, &iceagent.rem.def_addr[1], NULL, 0);
                tmp_addr = pj_str(ip);
                status = pj_sockaddr_set_str_addr(af, &iceagent.rem.def_addr[1], &tmp_addr);
                if (status != PJ_SUCCESS) {
                    PJ_LOG(1,(THIS_FILE, "Invalid IP address"));
                    goto on_error;
                }
                pj_sockaddr_set_port(&iceagent.rem.def_addr[1], (pj_uint16_t)port);
            } else if (strcmp(attr, "candidate")==0) {
                char *sdpcand = attr+strlen(attr)+1;
                int af, cnt;
                char foundation[32], transport[12], ipaddr[80], type[32];
                pj_str_t tmpaddr;
                int comp_id, prio, port;
                pj_ice_sess_cand *cand;
                pj_status_t status;

                cnt = sscanf(sdpcand, "%s %d %s %d %s %d typ %s",
				 foundation, &comp_id, transport,
				 &prio, ipaddr, &port, type);
                if (cnt != 7) {
                    PJ_LOG(1, (THIS_FILE, "error: Invalid ICE candidate line"));
                    goto on_error;
                }

                cand = &iceagent.rem.cand[iceagent.rem.cand_cnt];
                pj_bzero(cand, sizeof(*cand));

                if (strcmp(type, "host")==0)
                    cand->type = PJ_ICE_CAND_TYPE_HOST;
                else if (strcmp(type, "srflx")==0)
                    cand->type = PJ_ICE_CAND_TYPE_SRFLX;
                else if (strcmp(type, "relay")==0)
                    cand->type = PJ_ICE_CAND_TYPE_RELAYED;
                else {
                    PJ_LOG(1, (THIS_FILE, "Error: invalid candidate type '%s'",
                        type));
                    goto on_error;
                }

                cand->comp_id = (pj_uint8_t)comp_id;
                pj_strdup2(iceagent.pool, &cand->foundation, foundation);
                cand->prio = prio;

                if (strchr(ipaddr, ':'))
                    af = pj_AF_INET6();
                else
                    af = pj_AF_INET();

                tmpaddr = pj_str(ipaddr);
                pj_sockaddr_init(af, &cand->addr, NULL, 0);
                status = pj_sockaddr_set_str_addr(af, &cand->addr, &tmpaddr);
                if (status != PJ_SUCCESS) {
                    PJ_LOG(1,(THIS_FILE, "Error: invalid IP address '%s'",
                    ipaddr));
                    goto on_error;
                }

                pj_sockaddr_set_port(&cand->addr, (pj_uint16_t)port);
                ++iceagent.rem.cand_cnt;

                if (cand->comp_id > iceagent.rem.comp_cnt)
                    iceagent.rem.comp_cnt = cand->comp_id;
            }
            }
            break;
    }

    return;

    on_error :
        printf("parse line goto error");
}

/*
 * Input and parse SDP from the remote (containing remote's ICE information)
 * and save it to global variables.
 */
void iceagent_input_remote()
{
    char linebuf[80];
    /* unsigned media_cnt = 0;
    unsigned comp0_port = 0;
    char     comp0_addr[80]; */
    comp_info_t comp_info;
    pj_bool_t done = PJ_FALSE;

    puts("Paste SDP from remote host, end with empty line");

    reset_rem_info();

    comp_info.comp0_addr[0] = '\0';

    while (!done) {
        pj_size_t len;
        char *line;

        printf(">");
        if (stdout) fflush(stdout);

        if (fgets(linebuf, sizeof(linebuf), stdin)==NULL)
            break;

        /* Ignore subsequent media descriptors */
        if (comp_info.media_cnt > 1)
            continue;

        len = strlen(linebuf);

        line = iceagent_strip_line(linebuf, &len);

        if (len==0)
            break;

        iceagent_parse_line(&comp_info, line, len);
    }

    if (iceagent.rem.cand_cnt==0 ||
        iceagent.rem.ufrag[0]==0 ||
        iceagent.rem.pwd[0]==0 ||
        iceagent.rem.comp_cnt == 0)
    {
        PJ_LOG(1, (THIS_FILE, "Error: not enough info"));
        goto on_error;
    }

    if ( comp_info.comp0_port==0 || comp_info.comp0_addr[0]=='\0') {
        PJ_LOG(1, (THIS_FILE, "Error: default address for component 0 not found"));
        goto on_error;
    } else {
        int af;
        pj_str_t tmp_addr;
        pj_status_t status;

        if (strchr(comp_info.comp0_addr, ':'))
            af = pj_AF_INET6();
        else
            af = pj_AF_INET();

        pj_sockaddr_init(af, &iceagent.rem.def_addr[0], NULL, 0);
        tmp_addr = pj_str(comp_info.comp0_addr);
        status = pj_sockaddr_set_str_addr(af, &iceagent.rem.def_addr[0],
					  &tmp_addr);
        if (status != PJ_SUCCESS) {
            PJ_LOG(1,(THIS_FILE, "Invalid IP address in c= line"));
            goto on_error;
        }
        pj_sockaddr_set_port(&iceagent.rem.def_addr[0], (pj_uint16_t)(comp_info.comp0_port));
    }

    PJ_LOG(3, (THIS_FILE, "Done, %d remote candidate(s) added",
	       iceagent.rem.cand_cnt));
    return;

    on_error:
        reset_rem_info();
}


/*
 * Start ICE negotiation! This function is invoked from the menu.
 */
/*
void iceagent_start_nego(void)
{
    pj_str_t rufrag, rpwd;
    pj_status_t status;

    if (iceagent.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    if (!pj_ice_strans_has_sess(iceagent.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE session, initialize first"));
	return;
    }

    if (iceagent.rem.cand_cnt == 0) {
	PJ_LOG(1,(THIS_FILE, "Error: No remote info, input remote info first"));
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Starting ICE negotiation.."));

    status = pj_ice_strans_start_ice(iceagent.icest,
				     pj_cstr(&rufrag, iceagent.rem.ufrag),
				     pj_cstr(&rpwd, iceagent.rem.pwd),
				     iceagent.rem.cand_cnt,
				     iceagent.rem.cand);
    if (status != PJ_SUCCESS)
	iceagent_perror("Error starting ICE", status);
    else
	PJ_LOG(3,(THIS_FILE, "ICE negotiation started"));
}*/


 /*
  * Send application data to remote agent.
  */
ssize_t iceagent_send_data(unsigned comp_id, const void *pkt, ssize_t size)
 {
    pj_status_t status;

    if (iceagent.icest == NULL) {
			PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
			return;
    }

    if (!pj_ice_strans_has_sess(iceagent.icest)) {
			PJ_LOG(1,(THIS_FILE, "Error: No ICE session, initialize first"));
			return;
    }

    /*
    if (!pj_ice_strans_sess_is_complete(iceagent.icest)) {
			PJ_LOG(1,(THIS_FILE, "Error: ICE negotiation has not been started or is in progress"));
			return;
    }
    */

    if (comp_id<1||comp_id>pj_ice_strans_get_running_comp_cnt(iceagent.icest)) {
			PJ_LOG(1,(THIS_FILE, "Error: invalid component ID"));
			return;
    }

		status = pj_ice_strans_sendto(iceagent.icest, comp_id, pkt, size,
			&iceagent.rem.def_addr[comp_id-1],
			pj_sockaddr_get_len(&iceagent.rem.def_addr[comp_id-1]));

		if (status != PJ_SUCCESS)
			iceagent_perror("Error sending data", status);
		else
			PJ_LOG(3,(THIS_FILE, "Data sent"));
			return size;
}


/*
 * Display help for the menu.
 */
/* void iceagent_help_menu(void)
{
    puts("");
    puts("-= Help on using ICE and this iceagent program =-");
    puts("");
    puts("This application demonstrates how to use ICE in pjnath without having\n"
	 "to use the SIP protocol. To use this application, you will need to run\n"
	 "two instances of this application, to simulate two ICE agents.\n");

    puts("Basic ICE flow:\n"
	 " create instance [menu \"c\"]\n"
	 " repeat these steps as wanted:\n"
	 "   - init session as offerer or answerer [menu \"i\"]\n"
	 "   - display our SDP [menu \"s\"]\n"
	 "   - \"send\" our SDP from the \"show\" output above to remote, by\n"
	 "     copy-pasting the SDP to the other iceagent application\n"
	 "   - parse remote SDP, by pasting SDP generated by the other iceagent\n"
	 "     instance [menu \"r\"]\n"
	 "   - begin ICE negotiation in our end [menu \"b\"], and \n"
	 "   - immediately begin ICE negotiation in the other iceagent instance\n"
	 "   - ICE negotiation will run, and result will be printed to screen\n"
	 "   - send application data to remote [menu \"x\"]\n"
	 "   - end/stop ICE session [menu \"e\"]\n"
	 " destroy instance [menu \"d\"]\n"
	 "");

    puts("");
    puts("This concludes the help screen.");
    puts("");
}*/
