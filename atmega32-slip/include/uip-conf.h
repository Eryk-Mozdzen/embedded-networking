#ifndef UIP_CONF_H
#define UIP_CONF_H

#include <stdint.h>
#include <string.h>

#define UIP_CONF_MAX_CONNECTIONS 3
#define UIP_CONF_MAX_LISTENPORTS 3
#define UIP_CONF_BUFFER_SIZE     400
#define UIP_CONF_BYTE_ORDER      LITTLE_ENDIAN
#define UIP_CONF_LOGGING         0
#define UIP_CONF_UDP             0
#define UIP_CONF_UDP_CHECKSUMS   0
#define UIP_CONF_STATISTICS      0
#define UIP_CONF_LLH_LEN         0

#define printf(format, ...) ;
#define tcpip_output()      ;
#define UIP_APPCALL         app_call

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint16_t uip_stats_t;
typedef void *uip_tcp_appstate_t;

void app_call();

#endif
