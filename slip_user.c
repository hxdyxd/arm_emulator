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
#include <slip_user.h>


#include <kfifo.h>
#include <libslirp.h>
#include <time.h>
#include <pthread.h>
#ifdef USE_PRCTL_SET_THREAD_NAME
#include <sys/prctl.h>
#endif
#include <stddef.h>
#include <errno.h>
#include <glib.h>
#include <poll.h>

#define LOG_NAME   "slirp"
#define DEBUG_PRINTF(...)     printf("\033[0;32m" LOG_NAME "\033[0m: " __VA_ARGS__)
#define ERROR_PRINTF(...)     printf("\033[1;31m" LOG_NAME "\033[0m: " __VA_ARGS__)

#define container_of(ptr, type, member) ((void *)((char *)(ptr) - offsetof(type, member)))
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

    GArray *gpollfds;
    uint32_t poll_timeout;
    uint32_t timer_cnt;
    uint32_t recv_time_cnt;

    Slirp *slirp;
    char *thread_name;
    uint8_t is_run;
    pthread_t thread_id;
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

const static struct charwr_interface slip_user_interface = {
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

static void slip_send_packet(uint8_t *p, int len, uint8_t *is_run);
static int slip_recv_poll(struct slip_user_t *u, uint8_t *buf, int len);

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

    slip_send_packet(ip_pkt, ip_pkt_len, &u->is_run);
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
    struct slip_user_t *u = (struct slip_user_t *)opaque;
    return (int64_t)u->timer_cnt*1000;
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
    struct slip_user_t *u = (struct slip_user_t *)opaque;
    struct pollfd pfd = {
        .fd = fd,
        .events = 0,
    };

    if (events & SLIRP_POLL_IN) {
        pfd.events |= POLLIN;
    }
    if (events & SLIRP_POLL_OUT) {
        pfd.events |= POLLOUT;
    }
    if (events & SLIRP_POLL_PRI) {
        pfd.events |= POLLPRI;
    }
    if (events & SLIRP_POLL_ERR) {
        pfd.events |= POLLERR;
    }
    if (events & SLIRP_POLL_HUP) {
        pfd.events |= POLLHUP;
    }

    int idx = u->gpollfds->len;
    g_array_append_val(u->gpollfds, pfd);
    return idx;
}

static int get_revents(int idx, void *opaque)
{
    struct slip_user_t *u = (struct slip_user_t *)opaque;
    int revents = g_array_index(u->gpollfds, struct pollfd, idx).revents;

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

static void prepare_callback(struct slip_user_t *u)
{
    slirp_pollfds_fill(u->slirp, &u->poll_timeout, add_poll, u);
    
    int rlen = slip_recv_poll(u, u->slip_out_buf + ETH_HLEN, BUF_SIZE - ETH_HLEN);
    if(rlen) {
        slirp_ip_send(u, rlen);
        u->poll_timeout = 0;
        u->recv_time_cnt = u->timer_cnt;
    } else {
        if(u->poll_timeout != 1 && u->timer_cnt - u->recv_time_cnt >= 2) {
            u->poll_timeout = 1;
        }
    }
}

static void poll_callback(struct slip_user_t *u)
{
    slirp_pollfds_poll(u->slirp, 0, get_revents, u);
}

static void timer_callback(struct slip_user_t *u)
{
    /* do nothing */
}

static void *net_slirp_proc(void *base)
{
    struct slip_user_t *u = (struct slip_user_t *)base;
#ifdef USE_PRCTL_SET_THREAD_NAME
    prctl(PR_SET_NAME, u->thread_name);
#endif
    DEBUG_PRINTF("%s task enter success!\n", u->thread_name);

    u->poll_timeout = 2000;
    while(u->is_run) {
        g_array_set_size(u->gpollfds, 0);
        prepare_callback(u);
        int r = poll((struct pollfd *)u->gpollfds->data, u->gpollfds->len, u->poll_timeout);
        if(r < 0) {
            if(errno == EINTR)
                continue;
            ERROR_PRINTF("%s poll error\n", u->thread_name);
            break;
        } else if(r == 0) {
            timer_callback(u);
        } else {
            poll_callback(u);
        }
        u->timer_cnt = clock()/(CLOCKS_PER_SEC/1000);
    }

    u->is_run = 0;
    DEBUG_PRINTF("%s task exit!\n", u->thread_name);
    return NULL;
}

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

    u->gpollfds = g_array_new(FALSE, FALSE, sizeof(struct pollfd));
    if(!u->gpollfds) {
        ERROR_PRINTF("g array new err\n");
        goto err1;
    }

    u->thread_name = LOG_NAME;
    u->is_run = 1;
    if(pthread_create(&u->thread_id, 0, net_slirp_proc, u) < 0)
        goto err2;

    return 0;
err2:
    g_array_free(u->gpollfds, TRUE);
err1:
    slirp_cleanup(u->slirp);
    u->slirp = NULL;
err:
    return -1;
}

static void net_slirp_exit(struct slip_user_t *u)
{
    if(u->slirp && u->gpollfds) {
        if (u->is_run == 1) {
            u->is_run = 0;
            pthread_join(u->thread_id, 0);
        } else {
            ERROR_PRINTF("%s stop failed!\n", u->thread_name);
        }

        slirp_cleanup(u->slirp);
        u->slirp = NULL;
        g_array_free(u->gpollfds, TRUE);
    }
}

/* Blockable */
static inline void send_char(int ch, uint8_t *is_run)
{
    while(__kfifo_in(&slip_user.send, &ch, 1) == 0 && *is_run) {
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 20,
        };
        select(0, NULL, NULL, NULL, &timeout);
    }
}


/* Blockable */
static inline void send_char_do(uint8_t *is_run)
{

}


#define FIFO_OUT_CHAR_PEEK(f,c,l)  __kfifo_out_peek_one(f,c,l)



/* SLIP special character codes */
#define END             0300    /* indicates end of packet */
#define ESC             0333    /* indicates byte stuffing */
#define ESC_END         0334    /* ESC ESC_END means END data byte */
#define ESC_ESC         0335    /* ESC ESC_ESC means ESC data byte */


/* SEND_PACKET: sends a packet of length "len", starting at
 * location "p".
 */
static void slip_send_packet(uint8_t *p, int len, uint8_t *is_run)
{
    /* send an initial END character to flush out any data that may
     * have accumulated in the receiver due to line noise
     */
    send_char(END, is_run);

    /* for each byte in the packet, send the appropriate character
     * sequence
    */
    while(len--) {
        switch(*p) {
        /* if it's the same code as an END character, we send a
         * special two character code so as not to make the
         * receiver think we sent an END */
        case END:
            send_char(ESC, is_run);
            send_char(ESC_END, is_run);
            break;

        /* if it's the same code as an ESC character,
        * we send a special two character code so as not
        * to make the receiver think we sent an ESC
        */
        case ESC:
            send_char(ESC, is_run);
            send_char(ESC_ESC, is_run);
            break;

        /* otherwise, we just send the character
        */
        default:
            send_char(*p, is_run);
        }

        p++;
    }

    /* tell the receiver that we're done sending the packet */
    send_char(END, is_run);
    send_char_do(is_run);
}

static int slip_recv_poll(struct slip_user_t *u, uint8_t *buf, int len)
{
    uint8_t ch;
    int fifo_out = 0;
    int received = 0;
    while(FIFO_OUT_CHAR_PEEK(&u->recv, &ch, fifo_out+1) == fifo_out+1) {
        fifo_out++;
        switch(ch) {
        case END:
            if(received) {
                u->recv.out += fifo_out;
                return received;
            } else {
                break;
            }
        case ESC:
            if(FIFO_OUT_CHAR_PEEK(&u->recv, &ch, fifo_out+1) == fifo_out+1) {
                fifo_out++;
                switch(ch) {
                case ESC_END:
                    ch = END;
                    break;
                case ESC_ESC:
                    ch = ESC;
                    break;
                }
            } else {
                return 0;
            }
        default:
            if(received < len) {
                buf[received++] = ch;
            } else {
                //long packet
                u->recv.out += fifo_out;
                return received;
            }
        }
    }
    return 0;
}

/**************************END OF FILE*****************************/
