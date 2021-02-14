/*
 * garray.h of arm_emulator
 * Copyright (C) 2019-2021  hxdyxd <hxdyxd@gmail.com>
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
#ifndef _GARRAY_H_
#define _GARRAY_H_

#include <stdint.h>
#include <string.h>

#define MIN_ARRAY_SIZE 0
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define TRUE   1
#define FALSE  0

typedef struct {
    uint8_t *data;
    uint32_t alloc;
    uint32_t len;
    uint32_t elt_size;
}GArray;

#define g_array_elt_len(array,i) ((array)->elt_size * (i))
#define g_array_elt_pos(array,i) ((array)->data + g_array_elt_len((array),(i)))
#define g_array_append_val(a,v)   g_array_append_vals (a, &(v), 1)
//#define g_array_prepend_val(a,v)  g_array_prepend_vals (a, &(v), 1)
//#define g_array_insert_val(a,i,v) g_array_insert_vals (a, i, &(v), 1)
#define g_array_index(a,t,i)      (((t*) (void *) (a)->data) [(i)])

static inline GArray *g_array_new(uint32_t zero_terminated, uint32_t clear, uint32_t elt_size)
{
    GArray *array;

    array = malloc(sizeof(GArray));
    if(!array)
        return NULL;

    array->data = NULL;
    array->alloc = 0;
    array->len = 0;
    array->elt_size = elt_size;

    return array;
}

static inline void g_array_free(GArray *array, uint8_t free_segment)
{
    if(array) {
        if(array->data)
            free(array->data);
        free(array);
    }
}

/* Returns the smallest power of 2 greater than n, or n if
  947  * such power does not fit in a guint
  948  */
static inline uint32_t g_nearest_pow(uint32_t num)
{
    uint32_t n = num - 1;

    if(n == 0)
        return 1;

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    return n + 1;
}

static inline void g_array_maybe_expand(GArray *array, uint32_t len)
{
    uint32_t want_alloc;

    want_alloc = g_array_elt_len(array, array->len + len);

    if (want_alloc > array->alloc) {
        want_alloc = g_nearest_pow(want_alloc);
        want_alloc = MAX(want_alloc, MIN_ARRAY_SIZE);

        array->data = realloc(array->data, want_alloc);
        array->alloc = want_alloc;
    }
}

static inline GArray *g_array_append_vals(GArray *array, void *data, uint32_t len)
{
    if(len == 0)
        return array;

    g_array_maybe_expand(array, len);

    memcpy( g_array_elt_pos(array, array->len), data, 
            g_array_elt_len(array, len));
    array->len += len;
    return array;
}

static inline GArray *g_array_remove_range (GArray *array, uint32_t index_, uint32_t length)
{
    if (index_ + length != array->len)
        memmove (g_array_elt_pos (array, index_),
                 g_array_elt_pos (array, index_ + length),
                 (array->len - (index_ + length)) * array->elt_size);

    array->len -= length;
    return array;
}

static inline GArray *g_array_set_size(GArray *array, uint32_t length)
{
    if(length > array->len) {
        g_array_maybe_expand(array, length - array->len);
    } else if(length < array->len) {
        g_array_remove_range(array, length, array->len - length);
    }

    array->len = length;
    return array;
}

#endif
