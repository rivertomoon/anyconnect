#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pjmedia/sdp.h>
#include <pjnath.h>
#include "transport_ice.h"

#define THIS_FILE   "transport_ice.c"
#if 0
#   define TRACE__(expr)    PJ_LOG(5,expr)
#else
#   define TRACE__(expr)
#endif

static const pj_str_t STR_RTP_AVP	= { "RTP/AVP", 7 };
static const pj_str_t STR_CANDIDATE	= { "candidate", 9};
static const pj_str_t STR_REM_CAND	= { "remote-candidates", 17 };
static const pj_str_t STR_ICE_LITE	= { "ice-lite", 8};
static const pj_str_t STR_ICE_MISMATCH	= { "ice-mismatch", 12};
static const pj_str_t STR_ICE_UFRAG	= { "ice-ufrag", 9 };
static const pj_str_t STR_ICE_PWD	= { "ice-pwd", 7 };
static const pj_str_t STR_IP4		= { "IP4", 3 };
static const pj_str_t STR_IP6		= { "IP6", 3 };
static const pj_str_t STR_RTCP		= { "rtcp", 4 };
static const pj_str_t STR_BANDW_RR	= { "RR", 2 };
static const pj_str_t STR_BANDW_RS	= { "RS", 2 };


/* Get ice-ufrag and ice-pwd attribute */
static void get_ice_attr(const pjmedia_sdp_session *rem_sdp,
			 const pjmedia_sdp_media *rem_m,
			 const pj_str_t **p_ufrag_str,
			 const pj_str_t **p_pwd_str)
{
    pjmedia_sdp_attr *attr;

    /* Find ice-ufrag attribute in media descriptor */
    attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr,
				 &STR_ICE_UFRAG, NULL);
    if (attr == NULL) {
        /* Find ice-ufrag attribute in session descriptor */
        attr = pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
				     &STR_ICE_UFRAG, NULL);
    }
    *p_ufrag_str = &attr->value;

    /* Find ice-pwd attribute in media descriptor */
    attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr,
				 &STR_ICE_PWD, NULL);
    if (attr == NULL) {
        /* Find ice-pwd attribute in session descriptor */
        attr = pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
				     &STR_ICE_PWD, NULL);
    }
    *p_pwd_str = &attr->value;
}

/* Parse a=candidate line */
static pj_status_t parse_cand(const char *obj_name,
			      pj_pool_t *pool,
			      const pj_str_t *orig_input,
			      pj_ice_sess_cand *cand)
{
    pj_str_t input;
    char *token, *host;
    int af;
    pj_str_t s;
    pj_status_t status = PJNATH_EICEINCANDSDP;

    pj_bzero(cand, sizeof(*cand));
    pj_strdup_with_null(pool, &input, orig_input);

    PJ_UNUSED_ARG(obj_name);

    /* Foundation */
    token = strtok(input.ptr, " ");
    if (!token) {
        TRACE__((obj_name, "Expecting ICE foundation in candidate"));
        goto on_return;
    }
    pj_strdup2(pool, &cand->foundation, token);

    /* Component ID */
    token = strtok(NULL, " ");
    if (!token) {
        TRACE__((obj_name, "Expecting ICE component ID in candidate"));
        goto on_return;
    }
    cand->comp_id = (pj_uint8_t) atoi(token);

    /* Transport */
    token = strtok(NULL, " ");
    if (!token) {
        TRACE__((obj_name, "Expecting ICE transport in candidate"));
        goto on_return;
    }
    if (pj_ansi_stricmp(token, "UDP") != 0) {
        TRACE__((obj_name,
		 "Expecting ICE UDP transport only in candidate"));
        goto on_return;
    }

    /* Priority */
    token = strtok(NULL, " ");
    if (!token) {
        TRACE__((obj_name, "Expecting ICE priority in candidate"));
        goto on_return;
    }
    cand->prio = atoi(token);

    /* Host */
    host = strtok(NULL, " ");
    if (!host) {
        TRACE__((obj_name, "Expecting ICE host in candidate"));
        goto on_return;
    }
    /* Detect address family */
    if (pj_ansi_strchr(host, ':'))
        af = pj_AF_INET6();
    else
        af = pj_AF_INET();
    /* Assign address */
    if (pj_sockaddr_init(af, &cand->addr, pj_cstr(&s, host), 0)) {
        TRACE__((obj_name, "Invalid ICE candidate address"));
        goto on_return;
    }

    /* Port */
    token = strtok(NULL, " ");
    if (!token) {
        TRACE__((obj_name, "Expecting ICE port number in candidate"));
        goto on_return;
    }
    pj_sockaddr_set_port(&cand->addr, (pj_uint16_t)atoi(token));

    /* typ */
    token = strtok(NULL, " ");
    if (!token) {
        TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
        goto on_return;
    }
    if (pj_ansi_stricmp(token, "typ") != 0) {
        TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
        goto on_return;
    }

    /* candidate type */
    token = strtok(NULL, " ");
    if (!token) {
        TRACE__((obj_name, "Expecting ICE candidate type in candidate"));
        goto on_return;
    }

    if (pj_ansi_stricmp(token, "host") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_HOST;

    } else if (pj_ansi_stricmp(token, "srflx") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_SRFLX;

    } else if (pj_ansi_stricmp(token, "relay") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_RELAYED;

    } else if (pj_ansi_stricmp(token, "prflx") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_PRFLX;

    } else {
        PJ_LOG(5,(obj_name, "Invalid ICE candidate type %s in candidate",
		  token));
        goto on_return;
    }

    status = PJ_SUCCESS;

on_return:
    return status;
}

/* Start ICE session with the specified remote SDP */
pj_status_t ice_parse(pj_pool_t *tmp_pool, struct rem_info * info, unsigned media_index)
{
	pj_status_t status;
	pjmedia_sdp_session *rem_sdp = info->sdp_sess;
	pjmedia_sdp_media * rem_m = rem_sdp->media[media_index];
	// const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;
	unsigned i, cand_cnt;

	get_ice_attr(rem_sdp, rem_m, &info->ufrag_str, &info->pwd_str);

	/* Get all candidates in the media */
	cand_cnt = 0;
	for (i=0; i<rem_m->attr_count && cand_cnt < PJ_ICE_MAX_CAND; ++i) {
		pjmedia_sdp_attr *attr;

		attr = rem_m->attr[i];

		if (pj_strcmp(&attr->name, &STR_CANDIDATE)!=0)
			continue;

		/* Parse candidate */
		status = parse_cand(THIS_FILE, tmp_pool, &attr->value, &(info->cand[cand_cnt]));

		if (status != PJ_SUCCESS) {
			PJ_LOG(4,(THIS_FILE,
									"Error in parsing SDP candidate attribute '%.*s', ""candidate is ignored",
									(int)attr->value.slen, attr->value.ptr));
			continue;
		}

		cand_cnt++;
	}
	info->cand_cnt = cand_cnt;

	return status;
}


/* Start ICE session with the specified remote SDP */
#if 0
pj_status_t start_ice(pj_ice_strans *icest,
			     pj_pool_t *tmp_pool,
			     const pjmedia_sdp_session *rem_sdp,
			     unsigned media_index)
{
    pjmedia_sdp_media *rem_m = rem_sdp->media[media_index];
    const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;
    pj_ice_sess_cand *cand;
    unsigned i, cand_cnt;
    pj_status_t status;

    get_ice_attr(rem_sdp, rem_m, &ufrag_attr, &pwd_attr);

    /* Allocate candidate array */
    /* cand = (pj_ice_sess_cand*)
	   pj_pool_calloc(tmp_pool, PJ_ICE_MAX_CAND, sizeof(pj_ice_sess_cand)); */

    /* Get all candidates in the media */
    cand_cnt = 0;
    for (i=0; i<rem_m->attr_count && cand_cnt < PJ_ICE_MAX_CAND; ++i) {
        pjmedia_sdp_attr *attr;

        attr = rem_m->attr[i];

        if (pj_strcmp(&attr->name, &STR_CANDIDATE)!=0)
            continue;

        /* Parse candidate */
        status = parse_cand(THIS_FILE, tmp_pool, &attr->value,
			    &cand[cand_cnt]);
        if (status != PJ_SUCCESS) {
            PJ_LOG(4,(THIS_FILE,
		      "Error in parsing SDP candidate attribute '%.*s', "
		      "candidate is ignored",
		      (int)attr->value.slen, attr->value.ptr));
            continue;
        }

        cand_cnt++;
    }

    /* Start ICE */
    return pj_ice_strans_start_ice(icest, &ufrag_attr->value,
				   &pwd_attr->value, cand_cnt, cand);
}
#endif
