#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS        1
#define MEM_ALIGNMENT 4
#define MEM_SIZE      (8 * 1024)
#define LWIP_RAW      0
#define LWIP_NETCONN  0
#define LWIP_SOCKET   0
#define LWIP_ICMP     1
#define LWIP_UDP      0
#define LWIP_TCP      1
#define LWIP_IPV4     1

#define PPP_SUPPORT   1
#define PPPOS_SUPPORT 1

#endif
