#include <stdio.h>
#include <json-c/json.h>
#include <stddef.h>
#include <string.h>
#include <pj/types.h>
#include <pj/errno.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include "config.h"
#include "util.h"

char * device_fields [] = { "name", "uuid", "token" };

void get_value(json_object * jobj, const  char  *sname)
{
    json_object *pval = NULL;
    enum json_type type;
    // pval = json_object_object_get(jobj, sname);
    if( json_object_object_get_ex(jobj, sname, &pval) ){
        type = json_object_get_type(pval);
        switch(type)
        {
            case json_type_string:
                printf("Key:%s  value: %s\n", sname, json_object_get_string(pval));
                break;
            case json_type_int:
                printf("Key:%s  value: %d\n", sname, json_object_get_int(pval));
                break;
            default:
                break;
        }
    }
}

pj_status_t parse_generic_section(struct json_object *file_obj, CONFIG_t * config)
{
struct json_object *lvl1_obj, *lvl2_obj;

	if( !json_object_object_get_ex(file_obj, "generic", &lvl1_obj) )
		goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "log_level", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_int) ) {
		config->generic.log_lvl = json_object_get_int(lvl2_obj);
	} else {
		goto on_error;
	}

	if( json_object_object_get_ex(lvl1_obj, "log_file", &lvl2_obj)) {
		config->generic.log_file = json_object_get_string(lvl2_obj);
	} else {
		goto on_error;
	}

	if( json_object_object_get_ex(lvl1_obj, "event_port", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_int)) {
		config->generic.event_port = json_object_get_int(lvl2_obj);
	} else {
		goto on_error;
	}

	// json_object_put(lvl2_obj);
	// json_object_put(lvl1_obj);
	return PJ_SUCCESS;

on_error :
	// json_object_put(lvl2_obj);
	// json_object_put(lvl1_obj);
	return PJ_EUNKNOWN;
}

pj_status_t parse_crypto_section(struct json_object *file_obj, CONFIG_t * config)
{
	struct json_object *lvl1_obj, *lvl2_obj;

	if( !json_object_object_get_ex(file_obj, "crypto", &lvl1_obj) )
		goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "password", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string)) {
		// config->crypto_config.password = malloc(json_object_get_string_len(lvl2_obj));
		// strcpy(config->crypto_config.password, json_object_get_string(lvl2_obj));
		config->crypto_config.password = (char *)json_object_get_string(lvl2_obj);
	} else {
		goto on_error;
	}

	// json_object_put(lvl2_obj);
	// json_object_put(lvl1_obj);
	return PJ_SUCCESS;

	on_error :
		// json_object_put(lvl2_obj);
		// json_object_put(lvl1_obj);
		return PJ_EUNKNOWN;

}

pj_status_t parse_mqtt_section(struct json_object *file_obj, CONFIG_t * config)
{
  struct json_object *lvl1_obj, *lvl2_obj;

  if( !json_object_object_get_ex(file_obj, "mqtt", &lvl1_obj) )
    goto on_error;

  if( json_object_object_get_ex(lvl1_obj, "srv_host", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) ) {
    // config->mqtt_config.host = malloc(strlen(json_object_get_string(lvl2_obj)));
    // strcpy(config->mqtt_config.host, json_object_get_string(lvl2_obj));
		config->mqtt_config.host = (char *)json_object_get_string(lvl2_obj);
  } else {
    goto on_error;
  }

  if( json_object_object_get_ex(lvl1_obj, "srv_port", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_int) ) {
    config->mqtt_config.port = json_object_get_int(lvl2_obj);
  } else {
    goto on_error;
  }

  // json_object_put(lvl2_obj);
  // json_object_put(lvl1_obj);
  return PJ_SUCCESS;

  on_error :
    // json_object_put(lvl2_obj);
    // json_object_put(lvl1_obj);
    return PJ_EUNKNOWN;
}

pj_status_t parse_ice_section(struct json_object *file_obj, CONFIG_t * config) {
  struct json_object *lvl1_obj, *lvl2_obj;

  if( !json_object_object_get_ex(file_obj, "ice", &lvl1_obj) )
    goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "mode", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string)) {
    if( strcmp(json_object_get_string(lvl2_obj), "controlled") == 0)
      config->ice_config.mode = PJ_ICE_SESS_ROLE_CONTROLLED;
    else if ( strcmp(json_object_get_string(lvl2_obj), "controlling") == 0)
      config->ice_config.mode = PJ_ICE_SESS_ROLE_CONTROLLING;
		else
			config->ice_config.mode = PJ_ICE_SESS_ROLE_UNKNOWN;

		json_object_put(lvl2_obj);
  } else {
    goto on_error;
  }

  // json_object_put(lvl2_obj);
  // json_object_put(lvl1_obj);
  return PJ_SUCCESS;

  on_error :
    // json_object_put(lvl2_obj);
    // json_object_put(lvl1_obj);
    return PJ_EUNKNOWN;
}
pj_status_t parse_stun_section(struct json_object *file_obj, CONFIG_t * config)
{
	struct json_object *lvl1_obj, *lvl2_obj;

	if( !json_object_object_get_ex(file_obj, "stun", &lvl1_obj) )
			goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "srv_host", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) )
		config->stun_config.host = (char *)json_object_get_string(lvl2_obj);
	else
    goto on_error;

	// config->stun_config.host = malloc(strlen(json_object_get_string(lvl2_obj)));
	// strcpy(config->stun_config.host, json_object_get_string(lvl2_obj));

  if( json_object_object_get_ex(lvl1_obj, "srv_port", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_int) )
		config->stun_config.port = json_object_get_int(lvl2_obj);
	else
  	config->stun_config.port = PJ_STUN_PORT;

	//json_object_put(lvl2_obj);
	//json_object_put(lvl1_obj);
	return PJ_SUCCESS;

	on_error :
		//json_object_put(lvl2_obj);
		//json_object_put(lvl1_obj);
		return PJ_EUNKNOWN;
}

pj_status_t parse_turn_section(struct json_object *file_obj, CONFIG_t * config)
{
	struct json_object *lvl1_obj, *lvl2_obj;

	if( !json_object_object_get_ex(file_obj, "turn", &lvl1_obj) )
		goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "srv_host", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) )
		config->turn_config.host = (char *)json_object_get_string(lvl2_obj);
	else
    goto on_error;

  if( json_object_object_get_ex(lvl1_obj, "srv_port", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_int) )
			config->turn_config.port = json_object_get_int(lvl2_obj);
	else
    goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "use_tcp", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) ) {
		if( strcmp(json_object_get_string(lvl2_obj),"true") == 0 )
			config->turn_config.use_tcp = PJ_TRUE;
		else
			config->turn_config.use_tcp = PJ_FALSE;
		json_object_put(lvl2_obj);
	} else {
		config->turn_config.use_tcp = PJ_FALSE;
		goto on_error;
	}

  if( json_object_object_get_ex(lvl1_obj, "username", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) ) {
		config->turn_config.username = (char *)json_object_get_string(lvl2_obj);
	} else {
		goto on_error;
	}

	if( json_object_object_get_ex(lvl1_obj, "password", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) ) {
		config->turn_config.password = (char *)json_object_get_string(lvl2_obj);
  } else {
    goto on_error;
  }

	// json_object_put(lvl2_obj);
	// json_object_put(lvl1_obj);
	return PJ_SUCCESS;

	on_error :
		// json_object_put(lvl2_obj);
		// json_object_put(lvl1_obj);
		return PJ_EUNKNOWN;
}

pj_status_t parse_vpn_section(struct json_object *file_obj, CONFIG_t * config)
{
  struct json_object *lvl1_obj, *lvl2_obj;

  if( !json_object_object_get_ex(file_obj, "vpn", &lvl1_obj) )
    goto on_error;

  if( json_object_object_get_ex(lvl1_obj, "config_file", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) ) {
    // config->vpn_config.config_file = malloc(strlen(json_object_get_string(lvl2_obj)));
    // strcpy(config->vpn_config.config_file, json_object_get_string(lvl2_obj));
    config->vpn_config.config_file = json_object_get_string(lvl2_obj);
  } else {
    goto on_error;
  }

  // json_object_put(lvl2_obj);
  // json_object_put(lvl1_obj);
  return PJ_SUCCESS;

  on_error :
    // json_object_put(lvl2_obj);
    // json_object_put(lvl1_obj);
    return PJ_EUNKNOWN;
}



pj_status_t parse_file(const char *filename, CONFIG_t * config)
{

	struct json_object *file_obj, *lvl1_obj = NULL, *lvl2_obj = NULL;

	if( (file_obj = json_object_from_file(filename)) == NULL )
		goto on_error;

	if( parse_generic_section(file_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_crypto_section(file_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_mqtt_section(file_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_ice_section(file_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_stun_section(file_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_turn_section(file_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_vpn_section(file_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( !json_object_object_get_ex(file_obj, "device", &lvl1_obj) )
		goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "registered", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) ) {
		if( strcmp(json_object_get_string(lvl2_obj), "true") == 0 )
			config->device.registered = PJ_TRUE;
		else
			config->device.registered = PJ_FALSE;
		json_object_put(lvl2_obj);
	}
	else {
		goto on_error;
	}

	if( json_object_object_get_ex(lvl1_obj, "uuid", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string)  )
		config->device.uuid = json_object_get_string(lvl2_obj);
	else
		goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "token", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) )
		config->device.token = json_object_get_string(lvl2_obj);
	else
		goto on_error;

	config->device.state = OFFLINE;

	// json_object_put(lvl2_obj);
	// json_object_put(lvl1_obj);
	// json_object_put(file_obj);
	return PJ_SUCCESS;

	on_error:
		// json_object_put(lvl2_obj);
		// json_object_put(lvl1_obj);
		// json_object_put(file_obj);
		return PJ_EUNKNOWN;
}

pj_status_t parse_message(const char *msg_buffer, CONFIG_t * config)
{

	struct json_object *msg_obj, *lvl1_obj = NULL, *lvl2_obj = NULL;

	if( (msg_obj = json_tokener_parse(msg_buffer)) == NULL )
		goto on_error;

	if( parse_generic_section(msg_obj, config) != PJ_SUCCESS )
		goto on_error;

	DEBUG("Parse Generic Section Success");

	if( parse_crypto_section(msg_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_mqtt_section(msg_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_ice_section(msg_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_stun_section(msg_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_turn_section(msg_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( parse_vpn_section(msg_obj, config) != PJ_SUCCESS )
		goto on_error;

	if( !json_object_object_get_ex(msg_obj, "device", &lvl1_obj) )
		goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "registered", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) ) {
		if( strcmp(json_object_get_string(lvl2_obj), "true") == 0 )
			config->device.registered = PJ_TRUE;
		else
			config->device.registered = PJ_FALSE;
		json_object_put(lvl2_obj);
	}
	else {
		goto on_error;
	}

	if( json_object_object_get_ex(lvl1_obj, "uuid", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string)  )
		config->device.uuid = json_object_get_string(lvl2_obj);
	else
		goto on_error;

	if( json_object_object_get_ex(lvl1_obj, "token", &lvl2_obj) && json_object_is_type(lvl2_obj, json_type_string) )
		config->device.token = json_object_get_string(lvl2_obj);
	else
		goto on_error;

	config->device.state = OFFLINE;

	return PJ_SUCCESS;

	on_error:
		return PJ_EUNKNOWN;
}

void save(struct json_object *file_obj)
{

}

void encrypt()
{

}

void decrypt()
{

}
