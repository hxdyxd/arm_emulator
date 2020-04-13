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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _LINUX_KFIFO_H
#define _LINUX_KFIFO_H

/*
 * How to porting drivers to the new generic FIFO API:
 *
 * - Modify the declaration of the "struct kfifo *" object into a
 *   in-place "struct kfifo" object
 * - Init the in-place object with kfifo_alloc() or kfifo_init()
 *   Note: The address of the in-place "struct kfifo" object must be
 *   passed as the first argument to this functions
 * - Replace the use of __kfifo_put into kfifo_in and __kfifo_get
 *   into kfifo_out
 * - Replace the use of kfifo_put into kfifo_in_spinlocked and kfifo_get
 *   into kfifo_out_spinlocked
 *   Note: the spinlock pointer formerly passed to kfifo_init/kfifo_alloc
 *   must be passed now to the kfifo_in_spinlocked and kfifo_out_spinlocked
 *   as the last parameter
 * - The formerly __kfifo_* functions are renamed into kfifo_*
 */

/*
 * Note about locking : There is no locking required until only * one reader
 * and one writer is using the fifo and no kfifo_reset() will be * called
 *  kfifo_reset_out() can be safely used, until it will be only called
 * in the reader thread.
 *  For multiple writer and one reader there is only a need to lock the writer.
 * And vice versa for only one writer and multiple reader there is only a need
 * to lock the reader.
 */

#include <stdio.h>


#define smp_wmb()
#define gfp_t int

struct __kfifo {
    unsigned int    in;
    unsigned int    out;
    unsigned int    mask;
    unsigned int    esize;
    void        *data;
};


/**
 * kfifo_esize - returns the size of the element managed by the fifo
 * @fifo: address of the fifo to be used
 */
#define kfifo_esize(fifo)   ((fifo)->kfifo.esize)

/**
 * kfifo_size - returns the size of the fifo in elements
 * @fifo: address of the fifo to be used
 */
#define kfifo_size(fifo)    ((fifo)->kfifo.mask + 1)

/*
 * internal helper to calculate the unused elements in a fifo
 */
static inline unsigned int kfifo_unused(struct __kfifo *fifo)
{
    return (fifo->mask + 1) - (fifo->in - fifo->out);
}

/*
 *  Determine whether some value is a power of two, where zero is
 * *not* considered a power of two.
 */

static inline unsigned char is_power_of_2(unsigned long n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}


int __kfifo_alloc(struct __kfifo *fifo, unsigned int size,
        size_t esize, gfp_t gfp_mask);
void __kfifo_free(struct __kfifo *fifo);
int __kfifo_init(struct __kfifo *fifo, void *buffer,
        unsigned int size, size_t esize);
unsigned int __kfifo_in(struct __kfifo *fifo,
        const void *buf, unsigned int len);
unsigned int __kfifo_out_peek(struct __kfifo *fifo,
        void *buf, unsigned int len);
unsigned int __kfifo_out(struct __kfifo *fifo,
        void *buf, unsigned int len);
unsigned int __kfifo_out_peek_one(struct __kfifo *fifo,
        void *buf, unsigned int len);

#endif
