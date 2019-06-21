#include <stdint.h>
#include <stdio.h>

static int gs_para = 0;

int main(void)
{
    uint64_t num = 12;
    num /= 2;
    double doub = 1.234567;
    doub += doub;
    doub *= doub;
    doub -= doub;
    doub /= doub;
    gs_para = 12;
    gs_para += 13;
    printf("helloworld\r\n");
    printf("hello world, %f\r\n", gs_para + doub + num);

    while(1) {
        uart_sendchar('t');
        uart_sendchar('e');
        uart_sendchar('s');
        uart_sendchar('t');
        uart_sendchar('\n');
    }

}
