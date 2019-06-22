#include <stdint.h>
#include <math.h>
#include <mini-printf.h>
#include <soft_timer.h>
#include <app_debug.h>


char sbuf[MAX_UART_SBUF_SIZE];


#define SERIAL_FLAG *(volatile unsigned char *) (0x18000)
#define SERIAL_OUT *(volatile unsigned char *) (0x18004)
#define SERIAL_IN *(volatile unsigned char *) (0x18008)

#define CODE_COUNTER *(volatile unsigned int *) (0x18030)


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

#define SOFT_TIMER_LED   (0)

void led_timer_proc(void)
{
    float doub;
    uint32_t code_counter =  CODE_COUNTER;
    CODE_COUNTER = 0;
    PRINTF("rate = %d KIPS \r\n", code_counter/1000 );

    doub = acos(0) * 100000;
    PRINTF("helloworld 123 %d \r\n", (int)doub );
}

void sin_test(void)
{
    for(float i=0; i<3.1415926/2; i+=0.1) {
        PRINTF("sin(%d) =\r\n", (int)(i *100) );
        //PRINTF("sin(%d) = %d \r\n", (int)(i *100), (int)(sin(i) * 100000) );
    }
}


int main(void)
{
    soft_timer_init();
    soft_timer_create(SOFT_TIMER_LED, 1, 1, led_timer_proc, 1000);

    uint32_t num = 12;
    num /= 3;

    PRINTF("helloworld %d \r\n", num);
    sin_test();

    while(1) {
        soft_timer_proc();
    }
}
