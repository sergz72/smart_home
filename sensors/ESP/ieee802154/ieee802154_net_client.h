#ifndef _NET_CLIENT_H
#define _NET_CLIENT_H

#include "lwip/err.h"

#ifdef __cplusplus
extern "C" {
#endif

int net_client_init(void);
err_t send_udp(void *data, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif
