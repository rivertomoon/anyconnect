#ifndef __PJMEDIA_TRANSPORT_ICE_H__
#define __PJMEDIA_TRANSPORT_ICE_H__

/**
 * @file transport_ice.h
 * @brief ICE capable media transport.
 */
#include <pjmedia/sdp.h>
#include <pjnath/ice_strans.h>


/* Variables to store parsed remote ICE info */
struct rem_info {
    char		 ufrag[80];
    char		 pwd[80];
    unsigned	 comp_cnt;
    pj_sockaddr	 def_addr[PJ_ICE_MAX_COMP];
    pjmedia_sdp_session *sdp_sess;
    const pj_str_t * ufrag_str;
    const pj_str_t * pwd_str;
    unsigned	 cand_cnt;
    pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
};

pj_status_t ice_parse(pj_pool_t *tmp_pool, struct rem_info * info, unsigned media_index);

#endif	/* __PJMEDIA_TRANSPORT_ICE_H__ */
