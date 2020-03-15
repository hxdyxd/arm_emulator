/*
 * main.c of arm_emulator
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
#include <stdint.h>
#include <stdio.h>
#include <math.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"


//uart
#define SERIAL_THR   *(volatile unsigned char *) (0x40020000)

#define INT_MSK   *(volatile unsigned int *) (0x4001f040)
#define INT_PND   *(volatile unsigned int *) (0x4001f044)

#define TIM_CNT   *(volatile unsigned int *) (0x4001f020)
#define TIM_ENA   *(volatile unsigned int *) (0x4001f024)

#define CODE_COUNTER *(volatile unsigned int *) (0x4001f030)


/* implementation of putchar (also used by printf function to output data)    */
int uart_sendchar(char ch)                 /* Write character to Serial Port    */
{
    return (SERIAL_THR = ch);
}

int _write( int file, char *ptr, int len)
{
    int n;
    for (n = 0; n < len; n++) {
        uart_sendchar(*ptr++);
    }
    
    return len;
}




void pi_test(void)
{
    static int f[2801];
    int a=10000, b=0, c=2800, d, e=0, g;
    for(;b-c;)
        f[b++]=a/5;
    for(;d=0,g=c*2; c-=14,printf("%.4d",e + d/a),e=d%a)
        for(b=c; d+=f[b]*a,f[b]=d%--g,d/=g--,--b; d*=b);
    puts("\r\n");
}


static void math_task(void *param)
{
    while(1) {
        printf("\r\n--------------------------------[ARM9]--------------------------\r\n");
        vTaskDelay( 100 ); //1s
        printf("\r\n acos(0) %f \r\n", acos(0));
        vTaskDelay( 100 );
        printf("\r\n exp(1) %f \r\n", exp(1));
        vTaskDelay( 100 );
        printf("\r\n log(10) %f \r\n", log(10));
        vTaskDelay( 100 );
    }
}

static void pi_task(void *param)
{
    while(1) {
        pi_test();
        vTaskDelay( 10 );
    }
}

int main(void)
{
    printf("\r\n\r\n[ARM9 FREERTOS] Build , %s %s \r\n", __DATE__, __TIME__);
    INT_PND = 0;
    INT_MSK &= ~(3<<0); //enable timer interrupt
    TIM_ENA = 1;

    if(xTaskCreate( math_task, "MathTest", 256, NULL, ( tskIDLE_PRIORITY + 2 ), NULL ) != pdPASS) {
        goto exit;
    }
    printf("task1 create ok!\r\n");

    if(xTaskCreate( pi_task, "PiTest", 256, NULL, ( tskIDLE_PRIORITY + 2 ), NULL ) != pdPASS) {
        goto exit;
    }
    printf("task2 create ok!\r\n");

    vTaskStartScheduler();

exit:
    printf("task create error\r\n");
    while(1);
}



extern unsigned int _end_text;
extern unsigned int _sidata;
extern unsigned int _start_data;
extern unsigned int _end_data;
extern unsigned int _start_bss;
extern unsigned int _end_bss;


void reset(void)
{
    unsigned int *src, *dst;

    src = &_sidata;
    dst = &_start_data;
    while (dst < &_end_data) {
        *dst++ = *src++;
    }

    dst = &_start_bss;
    while (dst < &_end_bss) {
        *dst++ = 0;
    }

    printf("_end_text = %p\r\n", &_end_text);
    printf("_sidata = %p\r\n", &_sidata);
    printf("_start_data = %p\r\n", &_start_data);
    printf("_end_data = %p\r\n", &_end_data);
    printf("_start_bss = %p\r\n", &_start_bss);
    printf("_end_bss = %p\r\n", &_end_bss);
}

/*****************************END OF FILE***************************/
