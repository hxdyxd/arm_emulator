/* hxdyxd 2019 07 09 */

#ifndef _CONIO_H
#define _CONIO_H

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>


static inline void enable_raw_mode(void)
{
    struct termios term;
    int oldf;

    tcgetattr(0, &term);
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    tcsetattr(0, TCSANOW, &term);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
}

static inline int kbhit(void)
{
    static int inited = 0;
    int ch;

    if(!inited) {
        inited = 1;
        enable_raw_mode();
    }

    ch = getchar();

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

static inline int getch(void)
{
     struct termios tm, tm_old;
     int fd = 0, ch;

     if (tcgetattr(fd, &tm) < 0) {
          return -1;
     }

     tm_old = tm;
     cfmakeraw(&tm);
     if (tcsetattr(fd, TCSANOW, &tm) < 0) {
          return -1;
     }

     ch = getchar();
     if (tcsetattr(fd, TCSANOW, &tm_old) < 0) {
          return -1;
     }

     return (ch==2)?3:ch; //ctrl+b map to ctrl+c
}


#endif
