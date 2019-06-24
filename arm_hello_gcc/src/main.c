#include <stdint.h>
#include <math.h>
#include <mini-printf.h>
#include <soft_timer.h>
#include <app_debug.h>
#include <stdio.h>


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

 
    printf("acos(0) %f \r\n", acos(0));
    printf("exp(1) %f \r\n", exp(1));
}

void sin_test(void)
{
    float i = 0;
    for(i=0; i<3.14159; i+=0.4) {
        float sv = i;
        printf("sin(%f) = %f\r\n", sv, sv);
    }
}


void float_test(void)
{
    float sum = 3.1;
    sum += 0.2;
    printf("+sum = %f \r\n", sum);
    sum += 0.2;
    printf("+sum = %f \r\n", sum);
    sum += 0.2;
    printf("+sum = %f \r\n", sum);
    sum -= 0.2;
    printf("-sum = %f \r\n", sum);
    sum *= 0.2;
    printf("*sum = %f \r\n", sum);
    sum /= 0.2;
    printf("/sum = %f \r\n", sum);
}



int num = 24;
uint32_t bss_test_val = 0;

int main(void)
{
    soft_timer_init();
    soft_timer_create(SOFT_TIMER_LED, 1, 1, led_timer_proc, 1000);

    num = num + 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    num = num - 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    num = num * 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    num = num / 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    printf("bss_test_val (0x%p) = 0x%x \r\n", &bss_test_val , bss_test_val);

    float_test();

    printf("helloworld %d \r\n", num);
    //sin_test();

    while(1) {
        soft_timer_proc();
    }
}


// extern unsigned int _end_text;
// extern unsigned int _start_data;
// extern unsigned int _end_data;
// extern unsigned int _start_bss;
// extern unsigned int _end_bss;

// void reset(void)
// {
//     unsigned int *src, *dst;

//     src = &_end_text;
//     dst = &_start_data;
//     while (dst < &_end_data) {
//         *dst++ = *src++;
//     }

//     dst = &_start_bss;
//     while (dst < &_end_bss) {
//         *dst++ = 1;
//     }

//     PRINTF("_end_text = 0x%x\r\n", _end_text);
//     PRINTF("_start_data = 0x%x\r\n", _start_data);
//     PRINTF("_end_data = 0x%x\r\n", _end_data);
//     PRINTF("_start_bss = 0x%x\r\n", _start_bss);
//     PRINTF("_end_bss = 0x%x\r\n", _end_bss);
//     main();
// }

/*****************************END OF FILE***************************/
