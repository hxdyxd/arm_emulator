#include <stdint.h>
#include <math.h>
#include <mini-printf.h>
#include <soft_timer.h>
#include <app_debug.h>

char sbuf[MAX_UART_SBUF_SIZE];


#define SERIAL_FLAG *(volatile unsigned char *) (0x18000)
#define SERIAL_OUT *(volatile unsigned char *) (0x18004)
#define SERIAL_IN *(volatile unsigned char *) (0x18008)


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
    doub = acos(0) * 100000;

    PRINTF("helloworld 123 %d\n", (int)doub );
}


int main(void)
{
    soft_timer_init();
    soft_timer_create(SOFT_TIMER_LED, 1, 1, led_timer_proc, 1000);

    uint32_t num = 12;
    num /= 3;

    PRINTF("helloworld %d\n", num);

    while(1) {
        soft_timer_proc();
    }
}
