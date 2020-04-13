/*
 * A generic kernel FIFO implementation
 *
 * Copyright (C) 2009/2010 Stefani Seibold <stefani@seibold.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <kfifo.h>
#include <string.h>


#include <stdlib.h>
#define kmalloc(a,b) malloc(a)
#define kfree free


#define min(a,b) ((a)<(b)?(a):(b))

#define EINVAL  1
#define ENOMEM  2


int __kfifo_alloc(struct __kfifo *fifo, unsigned int size,
        size_t esize, gfp_t gfp_mask)
{
    /*
     * round down to the next power of 2, since our 'let the indices
     * wrap' technique works only in this case.
     */
    if (!is_power_of_2(size))
        return -EINVAL;

    fifo->in = 0;
    fifo->out = 0;
    fifo->esize = esize;

    if (size < 2) {
        fifo->data = NULL;
        fifo->mask = 0;
        return -EINVAL;
    }

    fifo->data = kmalloc(size * esize, gfp_mask);

    if (!fifo->data) {
        fifo->mask = 0;
        return -ENOMEM;
    }
    fifo->mask = size - 1;

    return 0;
}

void __kfifo_free(struct __kfifo *fifo)
{
    kfree(fifo->data);
    fifo->in = 0;
    fifo->out = 0;
    fifo->esize = 0;
    fifo->data = NULL;
    fifo->mask = 0;
}

int __kfifo_init(struct __kfifo *fifo, void *buffer,
        unsigned int size, size_t esize)
{
    size /= esize;

    if (!is_power_of_2(size))
        return -EINVAL;

    fifo->in = 0;
    fifo->out = 0;
    fifo->esize = esize;
    fifo->data = buffer;

    if (size < 2) {
        fifo->mask = 0;
        return -EINVAL;
    }
    fifo->mask = size - 1;

    return 0;
}

static void kfifo_copy_in(struct __kfifo *fifo, const void *src,
        unsigned int len, unsigned int off)
{
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int l;

    off &= fifo->mask;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    memcpy( (unsigned char *)fifo->data + off, src, l);
    memcpy( fifo->data, (unsigned char *)src + l, len - l);
    /*
     * make sure that the data in the fifo is up to date before
     * incrementing the fifo->in index counter
     */
    smp_wmb();
}

unsigned int __kfifo_in(struct __kfifo *fifo,
        const void *buf, unsigned int len)
{
    unsigned int l;

    l = kfifo_unused(fifo);
    if (len > l)
        len = l;

    kfifo_copy_in(fifo, buf, len, fifo->in);
    fifo->in += len;
    return len;
}

static void kfifo_copy_out(struct __kfifo *fifo, void *dst,
        unsigned int len, unsigned int off)
{
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int l;

    off &= fifo->mask;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    memcpy(dst, (unsigned char *)fifo->data + off, l);
    memcpy((unsigned char *)dst + l, fifo->data, len - l);
    /*
     * make sure that the data is copied before
     * incrementing the fifo->out index counter
     */
    smp_wmb();
}

unsigned int __kfifo_out_peek(struct __kfifo *fifo,
        void *buf, unsigned int len)
{
    unsigned int l;

    l = fifo->in - fifo->out;
    if (len > l)
        len = l;

    kfifo_copy_out(fifo, buf, len, fifo->out);
    return len;
}

unsigned int __kfifo_out(struct __kfifo *fifo,
        void *buf, unsigned int len)
{
    len = __kfifo_out_peek(fifo, buf, len);
    fifo->out += len;
    return len;
}


static void kfifo_copy_out_one(struct __kfifo *fifo, void *dst,
        unsigned int len, unsigned int off)
{
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int l;

    off &= fifo->mask;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    if(l)
        memcpy(dst, (unsigned char *)(fifo->data + off) + l - esize, esize);
    if(len - l)
        memcpy(dst, (unsigned char *)(fifo->data) + (len - l) - esize, esize);
}


unsigned int __kfifo_out_peek_one(struct __kfifo *fifo,
        void *buf, unsigned int len)
{
    unsigned int l;

    l = fifo->in - fifo->out;
    if (len > l)
        len = l;

    kfifo_copy_out_one(fifo, buf, len, fifo->out);
    return len;
}
