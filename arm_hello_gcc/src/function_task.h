/* 2019 04 10 */
/* By hxdyxd */
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
