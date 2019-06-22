/* Soft_Timer 2018 06 27 END */
/* By hxdyxd */

#include "soft_timer.h"


static SOFT_TIMER timer[MAX_TIMER];

void soft_timer_default() {}

void soft_timer_init(void) 
{
       static uint8_t i;
       for(i=0;i<MAX_TIMER;i++) {
              timer[i].is_circle = 0;
              timer[i].on = 0;
              timer[i].timer_cb = soft_timer_default;
              timer[i].timeout = 1000;
              timer[i].count = 0;
       }
}

int soft_timer_create(char id, char on, char is_circle, void ( *timer_cb)(void), uint32_t timeout)
{
       if(id >= MAX_TIMER) {
              return -1;
       }
       timer[id].is_circle = is_circle;
       timer[id].on = on;
       timer[id].timer_cb = timer_cb;
       timer[id].timeout = timeout;
       timer[id].count = SOFT_TIMER_GET_TICK_COUNT();
       return id;
}

int soft_timer_delete(char id) 
{
       if(id >= MAX_TIMER) {
              return -1;
       } else if(timer[id].on == 0) {
              return -1;
       } else {
              timer[id].on = 0;
              return 0;
       }
}

/* 定时器处理 */
void soft_timer_proc(void)
{
       uint8_t i;
       
       for(i=0;i<MAX_TIMER;i++) {
              if( timer[i].on == 1 ) {
                     if( (SOFT_TIMER_GET_TICK_COUNT() - timer[i].count) >= timer[i].timeout) {
                            if( timer[i].is_circle == 0 ) {
                                   timer[i].on = 0;
                            }
                            timer[i].timer_cb();
                            timer[i].count = SOFT_TIMER_GET_TICK_COUNT();
                     }
              }
              /* for*/
       }
       /* void */
}
