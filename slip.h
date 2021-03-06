/*
 * slip.h of arm_emulator
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
#ifndef _SLIP_H_
#define _SLIP_H_

#include <kfifo.h>
#include <stdint.h>

void slip_send_packet(struct __kfifo *fifo, uint8_t *buf, int len, uint8_t *is_run);
int slip_recv_poll(struct __kfifo *fifo, uint8_t *buf, int len);

#endif
/*****************************END OF FILE***************************/
