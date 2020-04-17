/*
 * slip_user.c of arm_emulator
 * Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <slip_user.h>
#include <config.h>

#define LOG_NAME   "slirp"
#define DEBUG_PRINTF(...)     printf("\033[0;32m" LOG_NAME "\033[0m: " __VA_ARGS__)
#define ERROR_PRINTF(...)     printf("\033[1;31m" LOG_NAME "\033[0m: " __VA_ARGS__)



#ifdef USE_SLIRP_SUPPORT
#include <slip.h>
#include <libslirp.h>
#include <loop.h>


#define IP_PACK(a,b,c,d) htonl( ((a)<<24) | ((b)<<16) | ((c)<<8) | (d))

#define BUF_SIZE      (1800)

/* value is a power of two */
#define FIFO_SIZE     (4096)

/* host forward addr */
#define FWD_HOST_ADDR     (0)
#define FWD_HOST_PORT     (8080)
#define FWD_GUEST_ADDR    IP_PACK(10, 0, 0, 2)
#define FWD_GUEST_PORT    (80)


struct slip_user_t {
    struct __kfifo send;
    uint8_t send_buf[FIFO_SIZE];
    struct __kfifo recv;
    uint8_t recv_buf[FIFO_SIZE];

    uint32_t recv_time_cnt;
    Slirp *slirp;
    uint8_t slip_out_buf[BUF_SIZE];
};


static struct slip_user_t slip_user;

static int net_slirp_init(struct slip_user_t *u);
static void net_slirp_exit(struct slip_user_t *u);


static uint8_t slip_user_init(void)
{
    __kfifo_init(&slip_user.send, slip_user.send_buf, FIFO_SIZE, 1);
    __kfifo_init(&slip_user.recv, slip_user.recv_buf, FIFO_SIZE, 1);
    if(net_slirp_init(&slip_user) < 0) {
        return 0;
    }
    return 1;
}

static void slip_user_exit(void)
{
    net_slirp_exit(&slip_user);
}

static uint8_t slip_user_readable(void)
{
    return (slip_user.send.in != slip_user.send.out);
}

static uint8_t slip_user_read(void)
{
    uint8_t ch;
    __kfifo_out(&slip_user.send, &ch, 1);
    return ch;
}

static uint8_t slip_user_writeable(void)
{
    if(kfifo_unused(&slip_user.recv))
        return 1;
    else
        return 0;
}

static uint8_t slip_user_write(uint8_t ch)
{
    __kfifo_in(&slip_user.recv, &ch, 1);
    return 0;
}


/**************************extern end*******************************/
#if defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
#define SLIRP_PACKED __attribute__((gcc_struct, packed))
#else
#define SLIRP_PACKED __attribute__((packed))
#endif

#define ETH_ALEN 6
#define ETH_HLEN 14
#define ETH_P_IP (0x0800) /* Internet Protocol packet  */
#define ETH_P_ARP (0x0806) /* Address Resolution packet */
#define ETH_P_IPV6 (0x86dd)
#define ETH_P_VLAN (0x8100)
#define ETH_P_DVLAN (0x88a8)
#define ETH_P_NCSI (0x88f8)
#define ETH_P_UNKNOWN (0xffff)

#define ARPOP_REQUEST 1 /* ARP request */
#define ARPOP_REPLY 2 /* ARP reply   */

struct ethhdr {
    unsigned char h_dest[ETH_ALEN]; /* destination eth addr */
    unsigned char h_source[ETH_ALEN]; /* source ether addr    */
    unsigned short h_proto; /* packet type ID field */
};

struct slirp_arphdr {
    unsigned short ar_hrd; /* format of hardware address */
    unsigned short ar_pro; /* format of protocol address */
    unsigned char ar_hln; /* length of hardware address */
    unsigned char ar_pln; /* length of protocol address */
    unsigned short ar_op; /* ARP opcode (command)       */

    /*
     *  Ethernet looks like this : This bit is variable sized however...
     */
    unsigned char ar_sha[ETH_ALEN]; /* sender hardware address */
    uint32_t ar_sip; /* sender IP address       */
    unsigned char ar_tha[ETH_ALEN]; /* target hardware address */
    uint32_t ar_tip; /* target IP address       */
} SLIRP_PACKED;


const static uint8_t guesthdr[ETH_ALEN] = {0x90, 0xad, 0xf7, 0xb9, 0x30, 0x1b};
static uint8_t vhosthdr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


static void slirp_ip_send(struct slip_user_t *u, int ip_pkt_len)
{
    struct ethhdr *eh = (struct ethhdr *)u->slip_out_buf;
    memcpy(eh->h_dest, vhosthdr, ETH_ALEN);
    memcpy(eh->h_source, guesthdr, ETH_ALEN);
    eh->h_proto = htons(ETH_P_IP);

    slirp_input(u->slirp, u->slip_out_buf, ip_pkt_len + ETH_HLEN);
}

static void slirp_arp_send(struct slip_user_t *u, const uint8_t *src_ha, const uint8_t *dst_ha,
 const uint32_t src_ip, const uint32_t dst_ip, const uint16_t op)
{
    uint8_t arp_reply[ETH_HLEN + sizeof(struct slirp_arphdr)];
    struct ethhdr *reh = (struct ethhdr *)arp_reply;
    struct slirp_arphdr *rah = (struct slirp_arphdr *)(arp_reply + ETH_HLEN);

    memcpy(reh->h_dest, dst_ha, ETH_ALEN);
    memcpy(reh->h_source, src_ha, ETH_ALEN);
    reh->h_proto = htons(ETH_P_ARP);

    rah->ar_hrd = htons(1);
    rah->ar_pro = htons(ETH_P_IP);
    rah->ar_hln = ETH_ALEN;
    rah->ar_pln = 4;
    rah->ar_op = htons(op);
    memcpy(rah->ar_sha, src_ha, ETH_ALEN);
    rah->ar_sip = src_ip;
    memcpy(rah->ar_tha, dst_ha, ETH_ALEN);
    rah->ar_tip = dst_ip;
    slirp_input(u->slirp, arp_reply, sizeof(arp_reply));
}

static int slirp_arp_input(struct slip_user_t *u, const void *buf, size_t len)
{
    struct ethhdr *eh = (struct ethhdr *)buf;
    struct slirp_arphdr *ah = (struct slirp_arphdr *)((uint8_t *)buf + ETH_HLEN);
    int ar_op = ntohs(ah->ar_op);

    switch(ar_op) {
    case ARPOP_REQUEST:
        memcpy(vhosthdr, ah->ar_sha, ETH_ALEN);
        slirp_arp_send(u, guesthdr, eh->h_source, 
            ah->ar_tip, ah->ar_sip, ARPOP_REPLY);

        break;
    case ARPOP_REPLY:
        break;
    default:
        ERROR_PRINTF("unknow arp pack %04x\n", ar_op);
    }
    return len;
}

static int slirp_ip_input(struct slip_user_t *u, const void *buf, size_t len)
{
    uint8_t *ip_pkt = (uint8_t *)buf + ETH_HLEN;
    int ip_pkt_len = len - ETH_HLEN;

    slip_send_packet(&u->send, ip_pkt, ip_pkt_len, &LOOP_IS_RUN(&loop_default));
    return len;
}

static ssize_t net_slirp_send_packet(const void *buf, size_t len, void *opaque)
{
    struct slip_user_t *u = (struct slip_user_t *)opaque;
    const uint8_t *pkt = buf;
    int proto = (((uint16_t)pkt[12]) << 8) + pkt[13];
    switch(proto) {
    case ETH_P_ARP:
        return slirp_arp_input(u, buf, len);
    case ETH_P_IP:
    case ETH_P_IPV6:
        return slirp_ip_input(u, buf, len);
    default:
        ERROR_PRINTF("unknow pack\n");
        break;
    }
    
    return 0;
}

static void net_slirp_guest_error(const char *msg, void *opaque)
{
    ERROR_PRINTF("%s\n", msg);
}

static int64_t net_slirp_clock_get_ns(void *opaque)
{
    return (int64_t)loop_get_clock_ms(&loop_default)*1000;
}

static void *net_slirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque)
{
    ERROR_PRINTF("timer_new stub\n");
    return NULL;
}

static void net_slirp_timer_free(void *timer, void *opaque)
{
    ERROR_PRINTF("timer_free stub\n");
}

static void net_slirp_timer_mod(void *timer, int64_t expire_timer,
                                void *opaque)
{
    ERROR_PRINTF("timer_mod stub\n");
}

static void net_slirp_register_poll_fd(int fd, void *opaque)
{
    /* no register */
}

static void net_slirp_unregister_poll_fd(int fd, void *opaque)
{
    /* no unregister */
}

static void net_slirp_notify(void *opaque)
{
    /* no net_slirp_notify */
}

static const SlirpCb slirp_cb = {
    .send_packet = net_slirp_send_packet,
    .guest_error = net_slirp_guest_error,
    .clock_get_ns = net_slirp_clock_get_ns,
    .timer_new = net_slirp_timer_new,
    .timer_free = net_slirp_timer_free,
    .timer_mod = net_slirp_timer_mod,
    .register_poll_fd = net_slirp_register_poll_fd,
    .unregister_poll_fd = net_slirp_unregister_poll_fd,
    .notify = net_slirp_notify,
};


static int add_poll(int fd, int events, void *opaque)
{
    int poll_events = 0;

    if (events & SLIRP_POLL_IN) {
        poll_events |= POLLIN;
    }
    if (events & SLIRP_POLL_OUT) {
        poll_events |= POLLOUT;
    }
    if (events & SLIRP_POLL_PRI) {
        poll_events |= POLLPRI;
    }
    if (events & SLIRP_POLL_ERR) {
        poll_events |= POLLERR;
    }
    if (events & SLIRP_POLL_HUP) {
        poll_events |= POLLHUP;
    }

    return loop_add_poll(&loop_default, fd, poll_events);
}

static int get_revents(int idx, void *opaque)
{
    int revents = loop_get_revents(&loop_default, idx);

    int ret = 0;
    if (revents & POLLIN) {
        ret |= SLIRP_POLL_IN;
    }
    if (revents & POLLOUT) {
        ret |= SLIRP_POLL_OUT;
    }
    if (revents & POLLPRI) {
        ret |= SLIRP_POLL_PRI;
    }
    if (revents & POLLERR) {
        ret |= SLIRP_POLL_ERR;
    }
    if (revents & POLLHUP) {
        ret |= SLIRP_POLL_HUP;
    }
    return ret;
}

static void prepare_callback(void *opaque)
{
    struct slip_user_t *u = opaque;
    uint32_t timeout = 0;
    slirp_pollfds_fill(u->slirp, &timeout, add_poll, u);
    
    int rlen = slip_recv_poll(&u->recv, u->slip_out_buf + ETH_HLEN, BUF_SIZE - ETH_HLEN);
    if(rlen) {
        slirp_ip_send(u, rlen);
        loop_set_timeout(&loop_default, 0);
        u->recv_time_cnt = loop_get_clock_ms(&loop_default);
    } else {
        if(loop_get_clock_ms(&loop_default) - u->recv_time_cnt >= 2) {
            loop_set_timeout(&loop_default, 1);
        } else {
            loop_set_timeout(&loop_default, 0);
        }
    }
}

static void poll_callback(void *opaque)
{
    struct slip_user_t *u = opaque;
    slirp_pollfds_poll(u->slirp, 0, get_revents, u);
}

static void timer_callback(void *opaque)
{
    /* do nothing */
}

static const struct loopcb_t loop_slip_user_cb = {
    .prepare = prepare_callback,
    .poll = poll_callback,
    .timer = timer_callback,
    .opaque = &slip_user,
};


static int net_slirp_init(struct slip_user_t *u)
{
    SlirpConfig cfg = {
        .version = SLIRP_CONFIG_VERSION_MAX,
        .in_enabled = 1,
        .vnetwork = { .s_addr = IP_PACK(10, 0, 0, 0) },
        .vnetmask = { .s_addr = IP_PACK(255, 255, 255, 0) },
        .vhost = { .s_addr = IP_PACK(10, 0, 0, 1) },
        .vhostname = "slirp_vhost",
    };
    struct in_addr fwd_host_addr = { .s_addr = FWD_HOST_ADDR };
    struct in_addr fwd_guest_addr = { .s_addr = FWD_GUEST_ADDR };

    DEBUG_PRINTF("version %s\n", slirp_version_string());

    DEBUG_PRINTF("vnetwork %s\n", inet_ntoa(cfg.vnetwork));
    DEBUG_PRINTF("vnetmask %s\n", inet_ntoa(cfg.vnetmask));
    DEBUG_PRINTF("vhost %s\n", inet_ntoa(cfg.vhost));
    u->slirp = slirp_new(&cfg, &slirp_cb, u);
    if(!u->slirp)
        goto err;

    DEBUG_PRINTF("hostfwd host addr %s:%d\n", inet_ntoa(fwd_host_addr), FWD_HOST_PORT);
    DEBUG_PRINTF("hostfwd guest addr %s:%d\n", inet_ntoa(fwd_guest_addr), FWD_GUEST_PORT);
    int r = slirp_add_hostfwd(u->slirp, 0, fwd_host_addr, FWD_HOST_PORT, fwd_guest_addr, FWD_GUEST_PORT);
    if(r < 0) {
        ERROR_PRINTF("add hostfwd err\n");
    }

    loop_register(&loop_default, &loop_slip_user_cb);
    return 0;

err:
    return -1;
}

static void net_slirp_exit(struct slip_user_t *u)
{
    if(u->slirp) {
        slirp_cleanup(u->slirp);
        u->slirp = NULL;
    }
}


#else /* !USE_SLIRP_SUPPORT */

//tun stub
static uint8_t slip_tun_init(void)
{
    ERROR_PRINTF("slirp is not supported in this build\n");
    return 1;
}

static void slip_user_exit(void)
{
}

static uint8_t slip_user_readable(void) 
{
    return 0;
}

static uint8_t slip_user_read(void)
{
    return 0;
}

static uint8_t slip_user_writeable(void)
{
    return 1;
}

static uint8_t slip_user_write(uint8_t ch)
{
    return 0;
}
#endif /* USE_SLIRP_SUPPORT */

static const struct charwr_interface slip_user_interface = {
    .init = slip_user_init,
    .exit = slip_user_exit,
    .readable = slip_user_readable,
    .read = slip_user_read,
    .writeable = slip_user_writeable,
    .write = slip_user_write,
};

int slip_user_register(struct uart_register *uart)
{
    uart_8250_register(uart, &slip_user_interface);
    return 0;
}


/**************************END OF FILE*****************************/
