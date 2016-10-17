/**
  vpn.h

  Copyright (C) 2015 clowwindy

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef VPN_H
#define VPN_H

#include <time.h>

#include "args.h"
#include "nat.h"

typedef ssize_t (*send_func_ptr_t)(unsigned, const void *, ssize_t);

typedef struct {
  int running;
  int nsock;
  int *socks;
  int tun;
  /* select() in winsock doesn't support file handler */
#ifndef TARGET_WIN32
  int control_pipe[2];
#else
  int control_fd;
  struct sockaddr control_addr;
  socklen_t control_addrlen;
  HANDLE cleanEvent;
#endif
  unsigned char *tun_buf;
  unsigned char *udp_buf;

  /* the address we currently use (client only) */
  struct sockaddr_storage remote_addr;
  /* points to above, just for convenience */
  struct sockaddr *remote_addrp;
  socklen_t remote_addrlen;

  /* bool ice_connected; */
  int comp_id;

  shadowvpn_args_t *args;

  /* server with NAT enabled only */
  nat_ctx_t *nat_ctx;

  /* Ice Tunnel */
  send_func_ptr_t ice_tunnel_send;

  /* */
} vpn_ctx_t;

/* return -1 on error. no need to destroy any resource */
int vpn_ctx_init(vpn_ctx_t *ctx, shadowvpn_args_t *args);

/* return -1 on error. no need to destroy any resource */
int vpn_run(vpn_ctx_t *ctx);

int vpn_run_new(void *arg);

/* return -1 on error. no need to destroy any resource */
int vpn_stop(vpn_ctx_t *ctx);

/* these low level functions are exposed for Android jni */
#ifndef TARGET_WIN32
int vpn_tun_alloc(const char *dev);
#endif
int vpn_udp_alloc(int if_bind, const char *host, int port,
                  struct sockaddr *addr, socklen_t* addrlen);

/*
 * Darwin & OpenBSD use utun which is slightly
 * different from standard tun device. It adds
 * a uint32 to the beginning of the IP header
 * to designate the protocol.
 *
 * We use utun_read to strip off the header
 * and utun_write to put it back.
 */
#ifdef TARGET_DARWIN
#define tun_read(...) utun_read(__VA_ARGS__)
#define tun_write(...) utun_write(__VA_ARGS__)
#elif !defined(TARGET_WIN32)
#define tun_read(...) read(__VA_ARGS__)
#define tun_write(...) write(__VA_ARGS__)
#endif

#endif
