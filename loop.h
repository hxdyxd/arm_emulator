/*
 * loop.h of arm_emulator
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
#ifndef _LOOP_H_
#define _LOOP_H_

#include <stdint.h>
#include <poll.h>
#ifdef NO_GLIB
#include <garray.h>
#else
#include <glib.h>
#endif
#include <pthread.h>

struct loopcb_t {
    void (* poll)(void *opaque);
    void (* prepare)(void *opaque);
    void (* timer)(void *opaque);
    void *opaque;
};

struct loop_t {
    GArray *gpollfds;
    uint32_t poll_timeout;
    uint32_t timer_cnt;

    char *thread_name;
    uint8_t is_run;
    pthread_t thread_id;
    GArray *callback;
};

#define LOOP_IS_RUN(a)   ((a)->is_run)
extern struct loop_t loop_default;

int loop_init(struct loop_t *lo);
int loop_exit(struct loop_t *lo);
int loop_start(struct loop_t *lo);

void loop_register(struct loop_t *lo, const struct loopcb_t *cb);
int loop_add_poll(struct loop_t *lo, int fd, int events);
int loop_get_revents(struct loop_t *lo, int idx);


static inline uint32_t loop_get_clock_ms(struct loop_t *lo)
{
    return lo->timer_cnt;
}

static inline void loop_set_timeout(struct loop_t *lo, uint32_t timeout)
{
    if(timeout < lo->poll_timeout) {
        lo->poll_timeout = timeout;
    }
}

#endif
/*****************************END OF FILE***************************/
