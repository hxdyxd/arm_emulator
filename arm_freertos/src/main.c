/* 2019 06 26 */
/* By hxdyxd */
#include <stdint.h>
#include <stdio.h>
#include <math.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"


//uart
#define SERIAL_FLAG *(volatile unsigned char *) (0x1f000)
#define SERIAL_OUT *(volatile unsigned char *) (0x1f004)
#define SERIAL_IN *(volatile unsigned char *) (0x1f008)

#define CODE_COUNTER *(volatile unsigned int *) (0x1f030)


/* implementation of putchar (also used by printf function to output data)    */
int uart_sendchar(char ch)                 /* Write character to Serial Port    */
{
    while (SERIAL_FLAG & 0x01);
    return (SERIAL_OUT = ch);
}

int _write( int file, char *ptr, int len)
{
    int n;
    for (n = 0; n < len; n++) {
        uart_sendchar(*ptr++);
    }
    
    return len;
}




static void vTaskTest(void *param)
{
    while(1) {
        printf("\r\n--------------------------------[ARM9]--------------------------\r\n");
        vTaskDelay( 1000 );
        printf("\r\n acos(0) %f \r\n", acos(0));
        vTaskDelay( 1000 );
        printf("\r\n exp(1) %f \r\n", exp(1));
        vTaskDelay( 1000 );
        printf("\r\n log(10) %f \r\n", log(10));
        vTaskDelay( 1000 );
    }
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


static void vTaskSinTest(void *param)
{
    while(1) {
        pi_test();
        vTaskDelay( 100 );
    }
}


int main(void)
{
    printf("\r\n\r\n[ARM9 FREERTOS] Build , %s %s \r\n", __DATE__, __TIME__);

    if(xTaskCreate( vTaskTest, "Test", 512, NULL, ( tskIDLE_PRIORITY + 2 ), NULL ) != pdPASS) {
        printf("task1 create failed\r\n");
    }
    printf("task1 create ok!\r\n");

    if(xTaskCreate( vTaskSinTest, "SinTest", 256, NULL, ( tskIDLE_PRIORITY + 2 ), NULL ) != pdPASS) {
        printf("task2 create failed\r\n");
    }
    printf("task2 create ok!\r\n");

    vTaskStartScheduler();
    while(1) {
        putchar('w');
    }
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
