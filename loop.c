/*
 * loop.c of arm_emulator
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
#include <loop.h>

#include <time.h>
#include <errno.h>


#define LOG_NAME   "loop"
#define DEBUG_PRINTF(...)     printf("\033[0;32m" LOG_NAME "\033[0m: " __VA_ARGS__)
#define ERROR_PRINTF(...)     printf("\033[1;31m" LOG_NAME "\033[0m: " __VA_ARGS__)


static void loop_prepare_callback(struct loop_t *lo)
{
    for(int i=0; i<lo->callback->len; i++) {
        struct loopcb_t *cb = g_array_index(lo->callback, struct loopcb_t *, i);
        if(cb->prepare)
            cb->prepare(cb->opaque);
    }
}

static void loop_poll_callback(struct loop_t *lo)
{
    for(int i=0; i<lo->callback->len; i++) {
        struct loopcb_t *cb = g_array_index(lo->callback, struct loopcb_t *, i);
        if(cb->poll)
            cb->poll(cb->opaque);
    }
}

static void loop_timer_callback(struct loop_t *lo)
{
    for(int i=0; i<lo->callback->len; i++) {
        struct loopcb_t *cb = g_array_index(lo->callback, struct loopcb_t *, i);
        if(cb->timer)
            cb->timer(cb->opaque);
    }
}

static void *loop_proc(void *base)
{
    struct loop_t *lo = (struct loop_t *)base;
#ifdef USE_PRCTL_SET_THREAD_NAME
    prctl(PR_SET_NAME, lo->thread_name);
#endif
    DEBUG_PRINTF("%s task enter success!\n", lo->thread_name);
    
    while(lo->is_run) {
        lo->poll_timeout = 2000;
        g_array_set_size(lo->gpollfds, 0);
        loop_prepare_callback(lo);
        int r = poll((struct pollfd *)lo->gpollfds->data, lo->gpollfds->len, lo->poll_timeout);
        if(r < 0) {
            if(errno == EINTR)
                continue;
            ERROR_PRINTF("%s poll error\n", lo->thread_name);
            break;
        } else if(r == 0) {
            loop_timer_callback(lo);
        } else {
            loop_poll_callback(lo);
        }
        lo->timer_cnt = clock()/(CLOCKS_PER_SEC/1000);
    }

    lo->is_run = 0;
    DEBUG_PRINTF("%s task exit!\n", lo->thread_name);
    return NULL;
}

int loop_add_poll(struct loop_t *lo, int fd, int events)
{
    struct pollfd pfd = {
        .fd = fd,
        .events = events,
    };
    int idx = lo->gpollfds->len;
    g_array_append_val(lo->gpollfds, pfd);
    return idx;
}

int loop_get_revents(struct loop_t *lo, int idx)
{
    return g_array_index(lo->gpollfds, struct pollfd, idx).revents;
}

void loop_register(struct loop_t *lo, const struct loopcb_t *cb)
{
    g_array_append_val(lo->callback, cb);
}

int loop_init(struct loop_t *lo)
{
    lo->thread_name = LOG_NAME;
    lo->gpollfds = g_array_new(FALSE, FALSE, sizeof(struct pollfd));
    if(!lo->gpollfds) {
        ERROR_PRINTF("g array new err\n");
        goto err0;
    }

    lo->callback = g_array_new(FALSE, FALSE, sizeof(struct loopcb_t *));
    if(!lo->callback) {
        ERROR_PRINTF("g array new err\n");
        goto err1;
    }
    return 0;

    g_array_free(lo->callback, TRUE);
    lo->callback = NULL;
err1:
    g_array_free(lo->gpollfds, TRUE);
    lo->gpollfds = NULL;
err0:
    return -1;
}

int loop_start(struct loop_t *lo)
{
    lo->is_run = 1;
    if(pthread_create(&lo->thread_id, 0, loop_proc, lo) < 0) {
        ERROR_PRINTF("create thread err\n");
        return -1;
    }
    return 0;
}

int loop_exit(struct loop_t *lo)
{
    if (lo->is_run == 1) {
        lo->is_run = 0;
        pthread_join(lo->thread_id, 0);
    } else {
        ERROR_PRINTF("%s stop failed!\n", lo->thread_name);
        return -1;
    }
    if(lo->gpollfds)
        g_array_free(lo->gpollfds, TRUE);
    if(lo->callback)
        g_array_free(lo->callback, TRUE);
    return 0;
}

struct loop_t loop_default;
