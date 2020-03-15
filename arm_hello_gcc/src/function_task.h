/*
 * function_task.h of arm_emulator
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
#ifndef _FUNCTION_TASK_H
#define _FUNCTION_TASK_H

#include <stdint.h>


#define  SWITCH_TASK_INIT(name)\
    uint32_t name##ii = 0;\
    static uint32_t name##ss = 0;

#define  SWITCH_TASK(name)\
    if(name##ss == name##ii++)

#define  SWITCH_TASK_END(name)\
    name##ss = (name##ss + 1) % name##ii;

#define  SWITCH_TASK_DO(name)\
    static int name##for = 0;

#define  SWITCH_TASK_WHILE(name,count)\
    name##for++;\
    if(name##for < count) { name##ss--;} else { name##for = 0;}



#define TIMER_TASK_GET_TICK_COUNT()  (cnt_counter)

#define  TIMER_TASK(name,time,condition)\
    static uint32_t name##timer = 0;\
    if( TIMER_TASK_GET_TICK_COUNT() - name##timer >= time)\
        if(name##timer = TIMER_TASK_GET_TICK_COUNT(),condition)


#endif
/*****************************END OF FILE***************************/
