
#define CR     0x0D

#define SERIAL_FLAG *(volatile unsigned char *) (0x8000)
#define SERIAL_OUT *(volatile unsigned char *) (0x8004)
#define SERIAL_IN *(volatile unsigned char *) (0x8008)


/* implementation of putchar (also used by printf function to output data)    */
int sendchar(int ch)                 /* Write character to Serial Port    */
{
    if (ch == '\n')  {
        while (SERIAL_FLAG & 0x01);
        SERIAL_OUT = CR;                          /* output CR */
    }
    while (SERIAL_FLAG & 0x01);
    return (SERIAL_OUT = ch);
}


int main(void)
{
    long int num = 12;
    num /= 2;
    double doub = 1.234567;
    // doub += doub;
    // doub *= doub;
    // doub -= doub;
    // doub /= doub;

    while(1) {
        sendchar('t');
        sendchar('e');
        sendchar('s');
        sendchar('t');
        sendchar('\n');
    }

}
