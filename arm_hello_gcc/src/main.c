/* 2019 06 26 */
/* By hxdyxd */
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <function_task.h>


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


//systick
volatile uint32_t cnt_counter = 0;

void handle_irq(void)
{
    cnt_counter++;
    //putchar('i');
}

void handle_swi(int number)
{
    printf("swi: 0x%x \r\n", number);
}


void led_timer_proc(void)
{
    static uint32_t last_code_counter = 0;
    uint32_t code_counter =  CODE_COUNTER;

    printf("--------------------------------[ARM9]--------------------------\r\n");
    static uint32_t last_time = 0;

    printf("[%lld] rate = %.3f KIPS \r\n", 
        (uint64_t)TIMER_TASK_GET_TICK_COUNT(),
        (code_counter - last_code_counter)*1.0/(TIMER_TASK_GET_TICK_COUNT() - last_time)
    );
    last_time = TIMER_TASK_GET_TICK_COUNT();
    last_code_counter = code_counter;

    printf("acos(0) %f \r\n", acos(0));
    printf("exp(1) %f \r\n", exp(1));
    printf("log(10) %f \r\n", log(10));
}


void int32_test(void)
{
    int num = -24;
    num = num + 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    num = num + 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    num = num - 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    num = num * 3;
    printf("num (0x%p) = %d \r\n", &num , num);
    num = num / 3;
    printf("num (0x%p) = %d \r\n", &num , num);
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


void sin_test(void)
{
    double i = 0;
    for(i=0; i<2*3.14159; i+=0.2) {
        printf("sin(%f) = %f\r\n", i, sin(i));
    }
}

void pi_test(void)
{
    static int a=10000,b,c=2800,d,e,f[2801],g;
    for(;b-c;)
        f[b++]=a/5; 
    for(;d=0,g=c*2; c-=14,printf("%.4d",e + d/a),e=d%a) 
        for(b=c; d+=f[b]*a,f[b]=d%--g,d/=g--,--b; d*=b);
    puts("\r\n");
}


uint16_t bss_test_val[10];


int main(void)
{
    printf("\r\n\r\n[ARM9] Build , %s %s \r\n", __DATE__, __TIME__);

    for(int i=0; i<10; i++) {
        printf("bss_test_val (%p) = 0x%x \r\n", &bss_test_val[i] , bss_test_val[i]);
    }

    int32_test();
    float_test();
    sin_test();
    pi_test();

    while(1) {
        TIMER_TASK(timer1, 2000, 1) {
            led_timer_proc();
        }
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
