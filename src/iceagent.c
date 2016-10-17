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

#include <json-c/json.h>

#include "config.h"
#include "global.h"
#include "iceagent.h"
#include "callbacks.h"

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

	PJ_UNUSED_ARG(ice_st);
	PJ_UNUSED_ARG(src_addr_len);
	// PJ_UNUSED_ARG(pkt);

  // Don't do this! It will ruin the packet buffer in case TCP is used!
  //((char*)pkt)[size] = '\0';

	DEBUG("Component %d: received %d bytes data from %s",
	      comp_id, size,
	      pj_sockaddr_print(src_addr, ipstr, sizeof(ipstr), 3));

	vpn_write_wrapper(&vpn_ctx, pkt, size);

}

/*
 * This is the main application initialization function. It is called
 * once (and only once) during application initialization sequence by
 * main().
 */
pj_status_t iceagent_init(CONFIG_t * config)
{
  pj_status_t status;
  /* int fd = 0; */

  iceagent.opt.comp_cnt = 1;
  iceagent.opt.max_host = -1;

  pj_ioqueue_callback callback;
  pj_bzero(&callback, sizeof(callback));
  callback.on_read_complete = &on_read_complete;
  callback.on_write_complete = &on_write_complete;

  /* if (iceagent.opt.log_file) {
    iceagent.log_fhnd = fopen(iceagent.opt.log_file, "a");
    pj_log_set_log_func(&log_func);
  } */

  /* Must create pool factory, where memory allocations come from */
  /* pj_caching_pool_init(&iceagent.cp, NULL, 0); */

  /* Create application memory pool */
  /* iceagent.pool = pj_pool_create(&iceagent.cp.factory, "iceagent", 512, 512, NULL); */

  /* Init our ICE settings with null values */
  pj_ice_strans_cfg_default(&iceagent.ice_cfg);
  iceagent.ice_cfg.stun_cfg.pf = &g.cp.factory;

  /* Create timer heap for timer stuff */
  /* CHECK( pj_timer_heap_create(g.pool, 100,
                              &iceagent.ice_cfg.stun_cfg.timer_heap) ); */
  iceagent.ice_cfg.stun_cfg.timer_heap = g.timer_heap;

  /* and create ioqueue for network I/O stuff */
  /* CHECK( pj_ioqueue_create(g.pool, 16,
                           &iceagent.ice_cfg.stun_cfg.ioqueue) ); */
  iceagent.ice_cfg.stun_cfg.ioqueue = g.ioqueue;

  /* something must poll the timer heap and ioqueue,
  * unless we're on Symbian where the timer heap and ioqueue run
  * on themselves.
  */
  /* CHECK( pj_thread_create(g.pool, "iceagent", &iceagent_worker_thread,
                          NULL, 0, 0, &iceagent.thread) ); */

  iceagent.ice_cfg.af = pj_AF_INET();

  /* Create DNS resolver if nameserver is set */
  if (iceagent.opt.ns.slen) {
    CHECK( pj_dns_resolver_create(&g.cp.factory,
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
	// iceagent.ice_cfg.stun.max_host_cands = 3;

	/* Nomination strategy */
	/* if (iceagent.opt.regular)
	iceagent.ice_cfg.opt.aggressive = PJ_FALSE;
	else */
	// iceagent.ice_cfg.opt.aggressive = PJ_FALSE;
	// iceagent.ice_cfg.opt.aggressive = PJ_TRUE;
	iceagent.ice_cfg.opt.aggressive = PJ_FALSE;

	/* Configure STUN/srflx candidate resolution */
	iceagent.ice_cfg.stun.server = pj_str(config->stun_config.host);
	iceagent.ice_cfg.stun.port = config->stun_config.port;

	iceagent.ice_cfg.stun.cfg.ka_interval = KA_INTERVAL;

	/* Configure TURN candidate */
	iceagent.ice_cfg.turn.server = pj_str(config->turn_config.host);
	iceagent.ice_cfg.turn.port = config->turn_config.port;

	/* TURN credential */
	iceagent.ice_cfg.turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	iceagent.ice_cfg.turn.auth_cred.data.static_cred.username = pj_str(config->turn_config.username);
	iceagent.ice_cfg.turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
	iceagent.ice_cfg.turn.auth_cred.data.static_cred.data = pj_str(config->turn_config.password);

	if (config->turn_config.use_tcp)
	    iceagent.ice_cfg.turn.conn_type = PJ_TURN_TP_TCP;
	else
	    iceagent.ice_cfg.turn.conn_type = PJ_TURN_TP_UDP;

	/* fd = (iceagent.ctx)->tun;

	status = pj_ioqueue_register_fd(iceagent.pool, iceagent.ice_cfg.stun_cfg.ioqueue, fd, NULL,
                                  &callback, &key);
	if (status != PJ_SUCCESS) {
			iceagent_perror("...error registering socket", status);
			return -40;
	} */
	iceagent.ctx = &vpn_ctx;

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
    INFO("ICE instance already created, destroy it first");
    return;
  }

  /* init the callback */
  pj_bzero(&icecb, sizeof(icecb));
  icecb.on_rx_data = cb_on_rx_data;
  icecb.on_ice_complete = cb_on_ice_complete;

  /* create the instance */
  status = pj_ice_strans_create("iceagent",		    /* object name  */
                                &iceagent.ice_cfg,/* settings	    */
                                iceagent.opt.comp_cnt,/* comp_cnt	    */
                                NULL,			       /* user data    */
                                &icecb,			     /* callback	    */
                                &iceagent.icest);/* instance ptr */
  if (status != PJ_SUCCESS)
    iceagent_perror("error creating ice", status);
  else
    INFO("ICE instance successfully created");
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
    ERROR_LOG("Error: No ICE instance, create it first");
    return;
  }

  pj_ice_strans_destroy(iceagent.icest);
  iceagent.icest = NULL;

  reset_rem_info();

  INFO("Info: ICE instance destroyed");
}


/*
 * Create ICE session, invoked from the menu.
 */
void iceagent_init_session(pj_uint8_t mode)
{
  pj_ice_sess_role role = mode;
  pj_status_t status;

  if (iceagent.icest == NULL) {
    ERROR_LOG("Error: No ICE instance, create it first");
    return;
  }

  if (pj_ice_strans_has_sess(iceagent.icest)) {
    ERROR_LOG("Error: Session already created");
    return;
  }

  status = pj_ice_strans_init_ice(iceagent.icest, role, NULL, NULL);

  stateM_handleEvent( &mqtt_ctx.m, &(struct event){ EV_ICE_SESS_OP_INIT, (void *)&status } );

  reset_rem_info();
}


/*
 * Stop/destroy ICE session, invoked from the menu.
 */
void iceagent_stop_session(void)
{
  pj_status_t status;

  if (iceagent.icest == NULL) {
    ERROR_LOG("Error: No ICE instance, create it first");
    return;
  }

  if (!pj_ice_strans_has_sess(iceagent.icest)) {
    ERROR_LOG("Error: No ICE session, initialize first");
    return;
  }

  status = pj_ice_strans_stop_ice(iceagent.icest);
  if (status != PJ_SUCCESS)
    iceagent_perror("error stopping session", status);
  else
    INFO("ICE session stopped");

  reset_rem_info();
}

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
        pj_sockaddr_print(&cand->addr, ipaddr, sizeof(ipaddr), 0),
        (unsigned)pj_sockaddr_get_port(&cand->addr));

  PRINT("%s\n", pj_ice_get_cand_type_name(cand->type));

  if (p == buffer+maxlen)
    return -PJ_ETOOSMALL;

  *p = '\0';

  return (int)(p-buffer);
}

/*
 * Encode ICE information in SDP.
 */
int encode_session(char buffer[], unsigned maxlen)
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
            pj_sockaddr_print(&cand[0].addr, ipaddr, sizeof(ipaddr), 0));
		} else if (comp==1) {
			/* For component 2, default address is in a=rtcp line */
			PRINT("a=rtcp:%d IN IP4 %s\n",
              (int)pj_sockaddr_get_port(&cand[0].addr),
              pj_sockaddr_print(&cand[0].addr, ipaddr, sizeof(ipaddr), 0));
      } else {
        /* For other components, we'll just invent this.. */
        PRINT("a=Xice-defcand:%d IN IP4 %s\n",
              (int)pj_sockaddr_get_port(&cand[0].addr),
              pj_sockaddr_print(&cand[0].addr, ipaddr, sizeof(ipaddr), 0));
      }

    /* Enumerate all candidates for this component */
    cand_cnt = PJ_ARRAY_SIZE(cand);
    status = pj_ice_strans_enum_cands(iceagent.icest, comp+1,
                                      &cand_cnt, cand);
    if (status != PJ_SUCCESS)
	    return -status;

    /* And encode the candidates as SDP */
    for (j=0; j<cand_cnt; ++j) {
			if( cand[j].status == PJ_SUCCESS ) {
				printed = print_cand(p, maxlen - (unsigned)(p-buffer), &cand[j]);
				if (printed < 0)
					return -PJ_ETOOSMALL;
				p += printed;
			}
    }
  }

  if (p == buffer+maxlen)
    return -PJ_ETOOSMALL;

  *p = '\0';
  return (int)(p - buffer);
}

/*
 * Input and parse SDP from the remote (containing remote's ICE information)
 * and save it to global variables.
 */
pj_status_t iceagent_parse_remote(const char *sdp, pj_pool_t *pool, pjmedia_sdp_session ** pp_ses)
{
  int len;
  char buf[1500];

  len = strlen(sdp);

  if( pjmedia_sdp_parse(pool, (char *)sdp, len, pp_ses) != PJ_SUCCESS )
      goto on_error;

  len = pjmedia_sdp_print(*pp_ses, buf, sizeof(buf));
  buf[len] = '\0';

  printf("Parsed:\n%s\n", buf);

  return PJ_SUCCESS;

  on_error :
      return PJ_EUNKNOWN;
}


/*
 * Start ICE negotiation! This function is invoked from the menu.
 */
void iceagent_start_nego(void)
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

  if (iceagent.rem.cand_cnt == 0) {
    PJ_LOG(1,(THIS_FILE, "Error: No remote info, input remote info first"));
    return;
  }

  PJ_LOG(3,(THIS_FILE, "Starting ICE negotiation.."));

  /* status = pj_ice_strans_start_ice(iceagent.icest,
                                   iceagent.rem.ufrag_str,
                                   iceagent.rem.pwd_str,
                                   iceagent.rem.cand_cnt,
                                   iceagent.rem.cand); */
	status = pj_ice_strans_start_ice_2(iceagent.icest);

  if (status != PJ_SUCCESS)
    iceagent_perror("Error starting ICE", status);
  else
    PJ_LOG(3,(THIS_FILE, "ICE negotiation started"));
}

 /*
  * Send application data to remote agent.
  */
ssize_t iceagent_send_data(unsigned comp_id, const void *pkt, ssize_t size)
 {
    pj_status_t status;

    if (iceagent.icest == NULL) {
		ERROR_LOG("Error: No ICE instance, create it first");
		return;
    }

    if (!pj_ice_strans_has_sess(iceagent.icest)) {
		ERROR_LOG("Error: No ICE session, initialize first");
		return;
    }

    if (!pj_ice_strans_sess_is_complete(iceagent.icest)) {
		ERROR_LOG("Error: ICE negotiation has not been started or is in progress");
		return;
    }

    if (comp_id<1||comp_id>pj_ice_strans_get_running_comp_cnt(iceagent.icest)) {
		ERROR_LOG("Error: invalid component ID");
		return;
    }

	status = pj_ice_strans_sendto(iceagent.icest, comp_id, pkt, size,
		&iceagent.rem.def_addr[comp_id-1],
		pj_sockaddr_get_len(&iceagent.rem.def_addr[comp_id-1]));

	if (status != PJ_SUCCESS)
 		iceagent_perror("Error sending data", status);
	else
		DEBUG("Data sent");
		return size;
}
