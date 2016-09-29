#ifndef EXTERN_CMD_H
#define EXTERN_CMD_H

#include <pjlib.h>

#define INIT_NEGO_CODE 1
#define START_ICE_CODE 2

typedef struct {
  int code;
}cmd_t;

void on_cmd_socket_read_complete(pj_ioqueue_key_t *key,
	pj_ioqueue_op_key_t *op_key,
	pj_ssize_t bytes_received);

void on_cmd_socket_write_complete(pj_ioqueue_key_t *key,
	pj_ioqueue_op_key_t *op_key,
	pj_ssize_t bytes_sent);

#endif
