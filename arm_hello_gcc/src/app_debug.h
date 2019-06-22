#ifndef _APP_DEBUG_H
#define _APP_DEBUG_H

#include <time.h>
#include "mini-printf.h"


#define BLUE_FONT "\033[40;34m%s\033[0m "
#define RED_FONT "\033[40;31m%s\033[0m "
#define GREEN_FONT "\033[40;32m%s\033[0m "
#define YELLOW_FONT "\033[40;33m%s\033[0m "
#define PURPLE_FONT "\033[40;35m%s\033[0m "
#define DGREEN_FONT "\033[40;36m%s\033[0m "
#define WHITE_FONT "\033[40;37m%s\033[0m "

#define MAX_UART_SBUF_SIZE   (512)
extern char sbuf[MAX_UART_SBUF_SIZE];


#define TIME_COUNT()  ((int)clock())
#define PRINTF(...) do{ int len = mini_snprintf(sbuf, sizeof sbuf, __VA_ARGS__); puts(sbuf); }while(0);



#define APP_ERROR(...) do {\
		             PRINTF("\033[40;32m[%d]\033[0m \033[2;40;33m%s(%d)\033[0m: ",\
                     TIME_COUNT(), __FUNCTION__, __LINE__);\
                     PRINTF("\033[1;40;31mERROR\033[0m ");\
                     PRINTF(__VA_ARGS__);}while(0)

#define APP_WARN(...) do {\
		            PRINTF("\033[40;32m[%d]\033[0m \033[2;40;33m%s(%d)\033[0m: ",\
                    TIME_COUNT(), __FUNCTION__, __LINE__);\
                    PRINTF("\033[1;40;33mWARN\033[0m ");\
                    PRINTF(__VA_ARGS__);}while(0)

#define APP_DEBUG(...) do {\
					 PRINTF("\033[40;32m[%d]\033[0m \033[2;40;33m%s(%d)\033[0m: ",\
                     TIME_COUNT(), __FUNCTION__, __LINE__);\
                     PRINTF(__VA_ARGS__);}while(0)



#endif
