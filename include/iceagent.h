#ifndef ICE_AGENT_H
#define ICE_AGENT_H

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include "transport_ice.h"
#include "config.h"
#include "shadowvpn.h"

/* This is our global variables */
struct app_t
{
  /* Command line options are stored here */
  struct options
  {
    unsigned    comp_cnt;
    pj_str_t    ns;
    int     max_host;
    pj_bool_t   regular;
    pj_str_t    stun_srv;
    pj_str_t    turn_srv;
    pj_bool_t   turn_tcp;
    pj_str_t    turn_username;
    pj_str_t    turn_password;
    pj_bool_t   turn_fingerprint;
    const char *log_file;
  } opt;

  /* Our global variables */
  pj_caching_pool  cp;
  pj_pool_t   *pool;
  pj_thread_t *thread;
  pj_bool_t    thread_quit_flag;
  pj_ice_strans_cfg  ice_cfg;
  pj_ice_strans *icest;
  FILE    *log_fhnd;

  pj_thread_t  *f_thread;

  /* Variables to store parsed remote ICE info */
  struct rem_info rem;

  vpn_ctx_t *ctx;
  shadowvpn_args_t *pargs;
};

extern struct app_t iceagent;

/*
 * This is the main application initialization function. It is called
 * once (and only once) during application initialization sequence by
 * main().
 */
pj_status_t iceagent_init();


/*
 * Create ICE stream transport instance, invoked from the menu.
 */
void iceagent_create_instance(void);

/*
 * Destroy ICE stream transport instance, invoked from the menu.
 */
void iceagent_destroy_instance(void);


/*
 * Create ICE session, invoked from the menu.
 */
/* void iceagent_init_session(unsigned rolechar); */
void iceagent_init_session(pj_uint8_t mode);

/*
 * Stop/destroy ICE session, invoked from the menu.
 */
void iceagent_stop_session(void);


/*
 * Show information contained in the ICE stream transport. This is
 * invoked from the menu.
 */
void iceagent_show_ice(void);


/*
 * Input and parse SDP from the remote (containing remote's ICE information)
 * and save it to global variables.
 */
void iceagent_input_remote(void);

/*
 * Start ICE negotiation! This function is invoked from the menu.
 */
void iceagent_start_nego(void);


/*
 * Display help for the menu.
 */
void iceagent_help_menu(void);


/*
 * Show information contained in the ICE stream transport. This is
 * invoked from the menu.
 */
void iceagent_show_ice(void);

/*
 * Send application data to remote agent.
 */
ssize_t iceagent_send_data(unsigned comp_id, const void *pkt, ssize_t size);

/* Utility: display error message and exit application (usually
 * because of fatal error.
 */
void err_exit(const char *title, pj_status_t status);

#endif




