#include <pjnath/ice_strans.h>
#include <pjnath/errno.h>
#include <pj/addr_resolv.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ip_helper.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/compat/socket.h>

/**
 * Maximum number of STUN transports for each ICE stream transport component.
 * Valid values are 1 - 64.
 *
 * Default: 2
 */
#ifndef PJ_ICE_MAX_STUN
#   define PJ_ICE_MAX_STUN			    2
#endif


/**
 * Maximum number of TURN transports for each ICE stream transport component.
 * Valid values are 1 - 64.
 *
 * Default: 2
 */
#ifndef PJ_ICE_MAX_TURN
#   define PJ_ICE_MAX_TURN			    2
#endif

/**
 * This structure describes an ICE stream transport component. A component
 * in ICE stream transport typically corresponds to a single socket created
 * for this component, and bound to a specific transport address. This
 * component may have multiple alias addresses, for example one alias
 * address for each interfaces in multi-homed host, another for server
 * reflexive alias, and another for relayed alias. For each transport
 * address alias, an ICE stream transport candidate (#pj_ice_sess_cand) will
 * be created, and these candidates will eventually registered to the ICE
 * session.
 */
typedef struct pj_ice_strans_comp
{
    pj_ice_strans	*ice_st;	/**< ICE stream transport.	*/
    unsigned		 comp_id;	/**< Component ID.		*/

    struct {
	pj_stun_sock	*sock;		/**< STUN transport.		*/
    } stun[PJ_ICE_MAX_STUN];

    struct {
	pj_turn_sock	*sock;		/**< TURN relay transport.	*/
	pj_bool_t	 log_off;	/**< TURN loggin off?		*/
	unsigned	 err_cnt;	/**< TURN disconnected count.	*/
    } turn[PJ_ICE_MAX_TURN];

    unsigned		 cand_cnt;	/**< # of candidates/aliaes.	*/
    pj_ice_sess_cand	 cand_list[PJ_ICE_ST_MAX_CAND];	/**< Cand array	*/

    unsigned		 default_cand;	/**< Default candidate.		*/

} pj_ice_strans_comp;

/**
 * This structure represents the ICE stream transport.
 */
struct pj_ice_strans
{
    char		    *obj_name;	/**< Log ID.			*/
    pj_pool_t		    *pool;	/**< Pool used by this object.	*/
    void		    *user_data;	/**< Application data.		*/
    pj_ice_strans_cfg	     cfg;	/**< Configuration.		*/
    pj_ice_strans_cb	     cb;	/**< Application callback.	*/
    pj_grp_lock_t	    *grp_lock;  /**< Group lock.		*/

    pj_ice_strans_state	     state;	/**< Session state.		*/
    pj_ice_sess		    *ice;	/**< ICE session.		*/
    pj_time_val		     start_time;/**< Time when ICE was started	*/

    unsigned		     comp_cnt;	/**< Number of components.	*/
    pj_ice_strans_comp	   **comp;	/**< Components array.		*/

    pj_timer_entry	     ka_timer;	/**< STUN keep-alive timer.	*/

    pj_bool_t		     destroy_req;/**< Destroy has been called?	*/
    pj_bool_t		     cb_called;	/**< Init error callback called?*/
};

PJ_DEF(pj_status_t) pj_ice_strans_prepare_ice( pj_ice_strans *ice_st,
               const pj_str_t *rem_ufrag,
               const pj_str_t *rem_passwd,
               unsigned rem_cand_cnt,
               const pj_ice_sess_cand rem_cand[])
{
	unsigned n;
	pj_status_t status;

	PJ_ASSERT_RETURN(ice_st && rem_ufrag && rem_passwd && rem_cand_cnt && rem_cand, PJ_EINVAL);

	/* Build check list */
	status = pj_ice_sess_create_check_list(ice_st->ice, rem_ufrag, rem_passwd, rem_cand_cnt, rem_cand);

	if (status != PJ_SUCCESS)
		return status;

#if 0
	/*if we have TURN candidate, now is the time to create the permissions */
	for (n = 0; n < ice_st->cfg.turn_tp_cnt; ++n) {
	/* if (ice_st->comp[0]->turn_sock) { */
		unsigned i;

		for (i=0; i<ice_st->comp_cnt; ++i) {
			pj_ice_strans_comp *comp = ice_st->comp[i];
			pj_sockaddr addrs[PJ_ICE_ST_MAX_CAND];
			unsigned j, count=0;

			/* Gather remote addresses for this component */
			for (j=0; j<rem_cand_cnt && count<PJ_ARRAY_SIZE(addrs); ++j) {
				if (rem_cand[j].comp_id==i+1 && rem_cand[j].addr.addr.sa_family==ice_st->cfg.turn_tp[n].af) {
					pj_sockaddr_cp(&addrs[count++], &rem_cand[j].addr);
				}
			}

			if (count) {
				status = pj_turn_sock_set_perm(comp->turn[n].sock, count, addrs, 0);
				if (status != PJ_SUCCESS) {
					pj_ice_strans_stop_ice(ice_st);
					return status;
				}
			}
		}
	}
#endif

	return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ice_strans_start_ice_2( pj_ice_strans *ice_st)
{
	pj_status_t status;
	/* Start ICE negotiation! */
	status = pj_ice_sess_start_check(ice_st->ice);
	if (status != PJ_SUCCESS) {
		pj_ice_strans_stop_ice(ice_st);
		return status;
	}

	ice_st->state = PJ_ICE_STRANS_STATE_NEGO;
	return status;
}

