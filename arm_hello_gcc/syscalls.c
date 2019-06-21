#include <_ansi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <errno.h>

int errno;

#define CR     0x0D

#define SERIAL_FLAG *(volatile unsigned char *) (0x8000)
#define SERIAL_OUT *(volatile unsigned char *) (0x8004)
#define SERIAL_IN *(volatile unsigned char *) (0x8008)


/* implementation of putchar (also used by printf function to output data)    */
int uart_sendchar(char ch)                 /* Write character to Serial Port    */
{
    if (ch == '\n')  {
        while (SERIAL_FLAG & 0x01);
        SERIAL_OUT = CR;                          /* output CR */
    }
    while (SERIAL_FLAG & 0x01);
    return (SERIAL_OUT = ch);
}



/* This is used by _sbrk.  */
//register char *stack_ptr asm ("r15");

int _read (int file, char *ptr, int len)
{
    uart_sendchar('r');
    return 0;
}

int _lseek (int file, int ptr, int dir)
{
    return 0;
}

int _write ( int file, char *ptr, int len)
{
    int n;
    for (n = 0; n < len; n++) {
        uart_sendchar(*ptr++);
    }
    
    return len;
}

int _close (int file)
{
    return 0;
}

int _link(char *old, char *new)
{
    errno = EMLINK;
    return -1;
}

caddr_t _sbrk (int incr)
{
    //extern char _ebss;      /* Defined by the linker */
    uart_sendchar('s');
    uart_sendchar('\n');
    static char *heap_end = 0;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = (char *)(0x4000);
    }
    prev_heap_end = heap_end;
    if ( (uint32_t)(heap_end + incr) > 0x8000) {
        //_write (1, "Heap and stack collision\n", 25);
       //abort ();
    }
    heap_end += incr;
    return (caddr_t) prev_heap_end;
}


int _fstat (int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

// int _open (const char *path, int flags)
// {
//     uart_sendchar('o');
//     return 0;
// }

// int
// _creat (const char *path,
//     int mode)
// {
//   return 0;
// }

int _unlink(char *name)
{
    errno = ENOENT;
    return -1;
}


int _isatty(int file)
{
    switch (file) {
    case STDOUT_FILENO:
    case STDERR_FILENO:
    case STDIN_FILENO:
        return 1;
    default:
        //errno = ENOTTY;
        errno = EBADF;
        return 0;
    }
}

// _exit (n)
// {
//   return __trap34 (SYS_exit, n, 0, 0);
// }

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return (-1);
}

int _getpid() {
    return 1;
}

// _raise ()
// {
// }

// int
// _stat (const char *path, struct stat *st)

// {
//   return __trap34 (SYS_stat, path, st, 0);
// }

// int
// _chmod (const char *path, short mode)
// {
//   return __trap34 (SYS_chmod, path, mode);
// }

// int
// _chown (const char *path, short owner, short group)
// {
//   return __trap34 (SYS_chown, path, owner, group);
// }

// int
// _utime (path, times)
//      const char *path;
//      char *times;
// {
//   return __trap34 (SYS_utime, path, times);
// }

int _fork()
{
    errno = EAGAIN;
    return -1;
}

// int
// _wait (statusp)
//      int *statusp;
// {
//   return __trap34 (SYS_wait);
// }

// int
// _execve (const char *path, char *const argv[], char *const envp[])
// {
//   return __trap34 (SYS_execve, path, argv, envp);
// }

// int
// _execv (const char *path, char *const argv[])
// {
//   return __trap34 (SYS_execv, path, argv);
// }

// int
// _pipe (int *fd)
// {
//   return __trap34 (SYS_pipe, fd);
// }

/* This is only provided because _gettimeofday_r and _times_r are
   defined in the same module, so we avoid a link error.  */
// clock_t
// _times (struct tms *tp)
// {
//   return -1;
// }

// int
// _gettimeofday (struct timeval *tv, void *tz)
// {
//   tv->tv_usec = 0;
//   tv->tv_sec = __trap34 (SYS_time);
//   return 0;
// }

// static inline int
// __setup_argv_for_main (int argc)
// {
//   char **argv;
//   int i = argc;

//   argv = __builtin_alloca ((1 + argc) * sizeof (*argv));

//   argv[i] = NULL;
//   while (i--) {
//     argv[i] = __builtin_alloca (1 + __trap34 (SYS_argnlen, i));
//     __trap34 (SYS_argn, i, argv[i]);
//   }

//   return main (argc, argv);
// }

// int
// __setup_argv_and_call_main ()
// {
//   int argc = __trap34 (SYS_argc);

//   if (argc <= 0)
//     return main (argc, NULL);
//   else
//     return __setup_argv_for_main (argc);
// }
