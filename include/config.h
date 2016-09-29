#ifndef CONFIG_H
#define CONFIG_H

#include <pj/types.h>
#include "device.h"

typedef struct GenericConfig {
	pj_uint8_t log_lvl;
	const char *log_file;
	pj_int32_t event_port;
}GenericConfig_t;

typedef struct CryptoConfig {
	const char *password;
}CryptoConfig_t;

typedef struct MQTTConfig {
	const char *host;
	pj_int32_t port;
}MQTTConfig_t;

typedef struct ICEConfig {
  pj_uint8_t mode;
}ICEConfig_t;

typedef struct STUNConfig {
	char *host;
	pj_int32_t port;
}STUNConfig_t;

typedef struct TURNConfig {
	char *host;
	pj_int32_t port;
	pj_bool_t use_tcp;
	char *username;
	char *password;
}TURNConfig_t;

typedef struct VPNConfig {
  const char *config_file;
}VPNConfig_t;

typedef struct CONFIG {
	GenericConfig_t generic;
	CryptoConfig_t crypto_config;
	MQTTConfig_t mqtt_config;
	ICEConfig_t   ice_config;
	STUNConfig_t stun_config;
	TURNConfig_t turn_config;
	VPNConfig_t   vpn_config;

	DEVICE_t device;
}CONFIG_t;

pj_status_t parse_file(const char *filename, CONFIG_t * config);

pj_status_t parse_message(const char *msg_buffer, CONFIG_t * config);

#endif
