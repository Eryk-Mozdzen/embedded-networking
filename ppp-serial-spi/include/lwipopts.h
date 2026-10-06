#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS            1
#define MEM_ALIGNMENT     4
#define MEM_SIZE          (32 * 1024)
#define LWIP_RAW          0
#define LWIP_NETCONN      0
#define LWIP_SOCKET       0
#define LWIP_ICMP         1
#define LWIP_UDP          0
#define LWIP_TCP          1
#define LWIP_IPV4         1
#define LWIP_SINGLE_NETIF 0

#define IP_FORWARD    1
#define IP_REASSEMBLY 1
#define IP_FRAG       1

#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_SEG 128
#define MEMP_NUM_PPP_PCB 4
#define TCP_MSS          1400
#define TCP_WND          (4 * TCP_MSS)
#define TCP_SND_BUF      (4 * TCP_MSS)
#define TCP_SND_QUEUELEN ((8 * TCP_SND_BUF) / TCP_MSS)

#define PPP_SUPPORT   1
#define PPPOS_SUPPORT 1

#define LWIP_STATS         1
#define LWIP_STATS_DISPLAY 1
#define MEM_STATS          1
#define MEMP_STATS         1
#define LINK_STATS         1
#define SYS_STATS          0

#endif
