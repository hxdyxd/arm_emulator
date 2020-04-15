/*
 * emulator.c of arm_emulator
 * Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdlib.h>
#include <armv4.h>
#include <disassembly.h>
#include <peripheral.h>

#include <unistd.h>
#include <signal.h>
#include <console.h>
#include <slip_tun.h>
#include <slip_user.h>
#include <config.h>


#define LOG_NAME   "emulator"
#define PRINTF(...)           printf(LOG_NAME ": " __VA_ARGS__)
#define DEBUG_PRINTF(...)     printf("\033[0;32m" LOG_NAME "\033[0m: " __VA_ARGS__)
#define ERROR_PRINTF(...)     printf("\033[1;31m" LOG_NAME "\033[0m: " __VA_ARGS__)



#define USE_LINUX          0
#define USE_BINARY         1
#define USE_DISASSEMBLY    2

#define USE_NET_USER       0
#define USE_NET_TUN        1


#define IMAGE_LOAD_ADDRESS   (0x8000)
#define DTB_BASE_ADDRESS     (MEM_SIZE - 0x4000)

#ifndef MAX_FS_SIZE
#define MAX_FS_SIZE   (1 << 29)   //512M
#endif


static uint8_t step_by_step = 0;


//peripheral register
struct peripheral_t peripheral_reg_base = {
    .tim = {
        .interrupt_id = 0,
    },
    .uart = {
        {
            .interrupt_id = 1,
            .interface_register_cb = console_register,
        },
        {
            .interrupt_id = 2,
            .interface_register_cb = slip_user_register,
        },
    },
};

#define SIZEOF_PERIPHERAL_CONFIG(cfg)    (sizeof(cfg)/sizeof(struct peripheral_link_t))
//peripheral address & function config
struct peripheral_link_t peripheral_config[] = {
    {
        .name = "Ram",
        .mask = ~(MEM_SIZE-1), //25bit
        .prefix = 0x00000000,
        .reg_base = &peripheral_reg_base.mem,
        .reset = memory_reset,
        .read = memory_read,
        .write = memory_write,
    },
    {
        .name = "Romfs",
        .mask = ~(MAX_FS_SIZE-1),
        .prefix = 0x80000000,
        .reg_base = &peripheral_reg_base.fs,
        .reset = fs_reset,
        .read = fs_read,
        .write = fs_write,
    },
    {
        .name = "Interrupt controller",
        .mask = ~(8-1), //3bit
        .prefix = 0x4001f040,
        .reg_base = &peripheral_reg_base.intc,
        .reset = intc_reset,
        .read = intc_read,
        .write = intc_write,
    },
    {
        .name = "Timer",
        .mask = ~(8-1), //3bit
        .prefix = 0x4001f020,
        .reg_base = &peripheral_reg_base.tim,
        .reset = tim_reset,
        .read = tim_read,
        .write = tim_write,
    },
    {
        .name = "Uart0",
        .mask = ~(256-1), //8bit
        .prefix = 0x40020000,
        .reg_base = &peripheral_reg_base.uart[0],
        .reset = uart_8250_reset,
        .read = uart_8250_read,
        .write = uart_8250_write,
    },
    {
        .name = "Uart1_slip",
        .mask = ~(256-1), //8bit
        .prefix = 0x40020100,
        .reg_base = &peripheral_reg_base.uart[1],
        .reset = uart_8250_reset,
        .read = uart_8250_read,
        .write = uart_8250_write,
    }
};


static void enable_step_by_step(int sig)
{
    step_by_step = 1;
    printf("\n");
    PRINTF("[%s] step by step mode\n", step_by_step ? "x" : " ");
}


static void peripheral_exit(void)
{
    fs_exit(0, &peripheral_reg_base.fs);
    tim_exit(0, &peripheral_reg_base.tim);
    memory_exit(0, &peripheral_reg_base.mem);
    uart_8250_exit(0, &peripheral_reg_base.uart[0]);
    uart_8250_exit(0, &peripheral_reg_base.uart[1]);
}


//load_program_memory reads the input memory, and populates the instruction 
// memory
uint32_t load_program_memory(struct armv4_cpu_t *cpu, const char *file_name, uint32_t start)
{
    FILE *fp;
    int ret = 0;
    unsigned int address, instruction;
    fp = fopen(file_name, "rb");
    if(fp == NULL) {
        ERROR_PRINTF("Error opening input mem file %s\n", file_name);
        exit(-1);
    }
    address = start;
    while(!feof(fp)) {
        if((ret = fread(&instruction, 4, 1, fp)) < 0) {
            ERROR_PRINTF("Error fread mem file %s, %d\n", file_name, ret);
            exit(-1);
        }
        write_word(cpu, address, instruction);
        address = address + 4;
    }
    DEBUG_PRINTF("load mem base 0x%x, size %u\r\n", start, address - start - 4);
    fclose(fp);
    return address - start - 4;
}


uint32_t load_disassembly(const char *file_name)
{
    FILE *fp;
    int ret = 0;
    unsigned int address, instruction;
    char code_buff[AS_CODE_LEN];
    fp = fopen(file_name, "rb");
    if(fp == NULL) {
        ERROR_PRINTF("Error opening input mem file %s\n", file_name);
        exit(-1);
    }
    address = 0;
    while(!feof(fp)) {
        if((ret = fread(&instruction, 4, 1, fp)) < 0) {
            ERROR_PRINTF("Error fread mem file %s, %d\n", file_name, ret);
            exit(-1);
        }
        code_disassembly(instruction, address, code_buff, AS_CODE_LEN);
        printf(AS_CODE_FORMAT, address, instruction, code_buff);
        address = address + 4;
    }
    DEBUG_PRINTF("code size %u\n", address);
    fclose(fp);
    return address;
}


void usage(const char *file)
{
    printf("\n");
    printf("%s\n\n", file);
    printf("  usage:\n\n");
    printf("  armemulator\n");
    printf(
        "       -m <mode>                  Select 'linux', 'bin' or 'disassembly' mode, default is 'bin'.\n");
    printf(
        "       -f <image_path>            Set image or binary programme file path.\n");
    printf(
        "       [-r <romfs_path>]          Set ROM filesystem path.\n");
    printf(
        "       [-t <device_tree_path>]    Set Devices tree path.\n");
    printf(
        "       [-n <net_mode>]            Select 'user' or 'tun' network mode, default is 'user'.\n");
    printf(
        "       [-d]                       Display debug message.\n");
    printf(
        "       [-s]                       Step by step mode.\n");
    printf("\n");
    printf(
        "       [-v]                       Verbose mode.\n");
    printf(
        "       [-h, --help]               Print this message.\n");
    printf("\n");
    printf("  features: "
#ifdef FS_MMAP_MODE
        "mmap "
#endif
#ifdef USE_SLIRP_SUPPORT
        "slirp "
#endif
#ifdef USE_TUN_SUPPORT
        "tun "
#endif
        "\n");
    printf("  build: %s %s %s \n", ARMEMULATOR_VERSION_STRING, __DATE__, __TIME__);
    printf("  reference: https://github.com/hxdyxd/arm_emulator\n");
}


void usage_s(void)
{
    printf("  usage:\n\n");
    printf("  armemulator\n");
    printf(
        "       m                Print MMU page table\n");
    printf(
        "       r [n]            Run skip n step\n");
    printf(
        "       d                Set/Clear debug message flag\n");
    printf(
        "       l                Print TLB table\n");
    printf(
        "       g                Print register table\n");
    printf(
        "       s                Set step by step flag, press ctrl+b to clear\n");
    printf(
        "       p[p|v] [a]       Print physical/virtual address at 0x[a]\n");
    printf(
        "       t[s]             Print run time and speed, set/clear realtime show flag\n");
    printf(
        "       h                Print this message\n");
    printf(
        "       q                Quit program\n");
    printf("\n");
    printf("  Build , %s %s \n", __DATE__, __TIME__);
}


void print_addr(struct armv4_cpu_t *cpu, char *ps)
{
    uint32_t printaddr = 0;
    char code_buff[AS_CODE_LEN];
    switch(*ps++) {
    case 'p':
        while(*ps == ' ')
            ps++;
        if(sscanf(ps, "%x", &printaddr) == 1) {
            uint32_t data = read_word_without_mmu(cpu, printaddr);
            PRINTF("p, *(0x%08x) = 0x%08x\n", printaddr, data);
            code_disassembly(data, printaddr, code_buff, AS_CODE_LEN);
            PRINTF("disassembly:\n");
            printf(AS_CODE_FORMAT, printaddr, data, code_buff);
        }
        break;
    case 'v':
        while(*ps == ' ')
            ps++;
        if(sscanf(ps, "%x", &printaddr) == 1) {
            uint32_t data = read_word(cpu, printaddr);
            if(!cpu->mmu.mmu_fault) {
                PRINTF("v, *(0x%08x) = 0x%08x\n", printaddr, data);
                code_disassembly(data, printaddr, code_buff, AS_CODE_LEN);
                PRINTF("disassembly:\n");
                printf(AS_CODE_FORMAT, printaddr, data, code_buff);
            } else {
                PRINTF("v, *(0x%08x) mmu fault, fsr=0x%x\n",
                 printaddr, cp15_fsr(&cpu->mmu));
            }
        }
        break;
    default:
        --ps;
        PRINTF("unknown option p'%c', 0x%x\n", *ps, *ps);
        usage_s();
        break;
    }
}


#define CLOCK_UPDATE_RATE    (0x1ffffff)
static void clock_speed_detect(struct armv4_cpu_t *cpu, const uint8_t rt_debug)
{
    static uint32_t previous_time = 0;
    if(!(cpu->code_counter & CLOCK_UPDATE_RATE)) {
        uint32_t current_time = GET_TICK();
        cpu->code_time = current_time - previous_time;
        previous_time = current_time;
        if(rt_debug) {
            PRINTF("[RT] %u ms, %.6f MIPS\n", GET_TICK(),
             (CLOCK_UPDATE_RATE+1)/(1000.0*cpu->code_time) );
        }
    }
}


int main(int argc, char **argv)
{
    struct armv4_cpu_t cpu_handle;
    struct armv4_cpu_t *cpu = &cpu_handle;
    uint8_t realtime_speed_show = 0;
    //default value
    uint8_t mode = USE_BINARY;
    uint8_t net_mode = USE_NET_USER;
    char *image_path = NULL;
    char *dtb_path = NULL;
    int ch;

    peripheral_reg_base.fs.filename = NULL;
    while((ch = getopt(argc, argv, "m:n:f:r:t:dshv")) != -1) {
        switch(ch) {
        case 't':
            dtb_path = optarg;
            break;
        case 'r':
            peripheral_reg_base.fs.filename = optarg;
            break;
        case 'f':
            image_path = optarg;
            break;
        case 'm':
            if(optarg[0] == 'l' || strcmp(optarg, "linux") == 0) {
                mode = USE_LINUX;
            } else if(optarg[0] == 'b' || strcmp(optarg, "bin") == 0) {
                mode = USE_BINARY;
            } else if(optarg[0] == 'd' || strcmp(optarg, "disassembly") == 0) {
                mode = USE_DISASSEMBLY;
            } else {
                ERROR_PRINTF("unknown mode option :%s\n", optarg);
                usage(argv[0]);
                exit(-1);
            }
            break;
        case 'n':
            if(optarg[0] == 'u' || strcmp(optarg, "user") == 0) {
                net_mode = USE_NET_USER;
            } else if(optarg[0] == 't' || strcmp(optarg, "tun") == 0) {
                net_mode = USE_NET_TUN;
            } else {
                ERROR_PRINTF("unknown net mode option :%s\n", optarg);
                usage(argv[0]);
                exit(-1);
            }
            break;
        case 's':
            step_by_step = 1;
            break;
        case 'd':
            global_debug_flag = 1;
            break;
        case 'v':
        case 'h':
            usage(argv[0]);
            exit(0);
        case '?':
            ERROR_PRINTF("unknown option \n");
            usage(argv[0]);
            exit(-1);
        }
    }
    if(!image_path) {
        ERROR_PRINTF("parameter error \n");
        usage(argv[0]);
        exit(-1);
    }
    if(USE_DISASSEMBLY == mode) {
        load_disassembly(image_path);
        exit(0);
    }

    signal(SIGINT, enable_step_by_step); //ctrl+b
    signal(SIGPIPE, SIG_IGN);

    switch(net_mode) {
    case USE_NET_USER:
        peripheral_reg_base.uart[1].interface_register_cb = slip_user_register;
        break;
    case USE_NET_TUN:
        peripheral_reg_base.uart[1].interface_register_cb = slip_tun_register;
        break;
    default:
        exit(-1);
    }

    cpu_init(cpu);
    peripheral_register(cpu, peripheral_config, SIZEOF_PERIPHERAL_CONFIG(peripheral_config));
    atexit(peripheral_exit);

    switch(mode) {
    case USE_LINUX:
        load_program_memory(cpu, image_path, IMAGE_LOAD_ADDRESS);
        if(dtb_path) {
            load_program_memory(cpu, dtb_path, DTB_BASE_ADDRESS);
        }

        /* linux environment, Kernel boot conditions */
        register_write(cpu, 1, 0xffffffff);         //set r1
        if(dtb_path) {
            register_write(cpu, 2, DTB_BASE_ADDRESS);  //set r2, dtb base Address
        }
        register_write(cpu, 15, IMAGE_LOAD_ADDRESS);   //set pc, jump to Load Address
        break;
    case USE_BINARY:
        load_program_memory(cpu, image_path, 0);
        break;
    default:
        exit(-1);
    }
    DEBUG_PRINTF("Start...\n");

    for(;;) {
        if(step_by_step) {
            static uint32_t skip_num = 0;
            if(skip_num) {
                --skip_num;
                goto RUN;
            }
            char cmd_str[64] = {0, };
            uint8_t cmd_len = 0;

            printf("\n[%u] cmd>", cpu->code_counter);
            //console read
            for(cmd_len=0; cmd_len<64;) {
                while(!peripheral_reg_base.uart[0].interface->readable()) {
                    usleep(1000);
                }
                cmd_str[cmd_len] = peripheral_reg_base.uart[0].interface->read();
                if(cmd_str[cmd_len] == '\n' || cmd_str[cmd_len] == '\r') {
                    cmd_str[cmd_len] = 0;
                    if(cmd_len != 0)
                        break;
                    printf("\n[%u] cmd>", cpu->code_counter);
                } else if(cmd_str[cmd_len] == 0x7f) {
                    //delete
                    if(cmd_len != 0)
                        cmd_len--;
                    cmd_str[cmd_len] = '\0'; //clear
                    printf("\n[%u] cmd>%s", cpu->code_counter, cmd_str);
                } else if(cmd_str[cmd_len] == '\033') {
                    
                } else {
                    putchar(cmd_str[cmd_len]);
                    cmd_len++;
                }
            }
            
            printf("\n");
            char *ps = &cmd_str[0];
            switch(*ps++) {
            case 'm':
                PRINTF("MMU table base: 0x%08x\n", cpu->mmu.reg[2]);
                for(int i=0; i<4096; i++) {
                    if(i && i % 16 == 0)
                        printf("\n");
                    printf("%08x, ", read_word_without_mmu(cpu,
                     cpu->mmu.reg[2]+(i<<2)));
                }
                printf("\n-----------MMU table end--------------\n");
                break;
            case 'd':
                global_debug_flag = !global_debug_flag;
                PRINTF("[%s] debug info\n", global_debug_flag ? "x" : " ");
                break;
            case 'g':
                reg_show(cpu);
                break;
            case 'l':
                tlb_show(&cpu->mmu);
                break;
            case 'p':
                print_addr(cpu, ps);
                break;
            case 'r':
                while(*ps == ' ')
                    ps++;
                if(sscanf(ps, "%d", &skip_num) != 1) {
                    skip_num = 0;
                }
                PRINTF("skip %d ...\n", skip_num);
                goto RUN;
                break;
            case 's':
                step_by_step = !step_by_step;
                PRINTF("[%s] step by step mode\n", step_by_step ? "x" : " ");
                break;
            case 't':
                PRINTF("Run time: %u ms\n", GET_TICK());
                PRINTF("Run speed: %u i/%u ms = %.3f MIPS\n", CLOCK_UPDATE_RATE, cpu->code_time,
                 (CLOCK_UPDATE_RATE+1)/(1000.0*cpu->code_time) );
                switch(*ps) {
                case 's':
                    realtime_speed_show = !realtime_speed_show;
                    PRINTF("[%s] Show realtime clock speed\n", realtime_speed_show ? "x" : " ");
                    break;
                }
                break;
            case 'h':
            case '?':
                usage_s();
                break;
            case 'q':
                PRINTF("quit\n");
                exit(0);
                break;
            case '\0':
                break;
            default:
                ERROR_PRINTF("unknown option '%c', 0x%x\n", cmd_str[0], cmd_str[0]);
                usage_s();
                break;
            }
            continue;
        }
RUN:
        cpu->code_counter++;
        cpu->decoder.event_id = EVENT_ID_IDLE;

        fetch(cpu);
        if(EVENT_ID_IDLE == cpu->decoder.event_id)
            decode(cpu);

        switch(cpu->decoder.event_id) {
        case EVENT_ID_UNDEF:
            interrupt_exception(cpu, INT_EXCEPTION_UNDEF);
            DEBUG_PRINTF("undef:%08x\n", cpu->decoder.instruction_word);
            break;
        case EVENT_ID_SWI:
            interrupt_exception(cpu, INT_EXCEPTION_SWI);
            break;
        case EVENT_ID_DATAABT:
            interrupt_exception(cpu, INT_EXCEPTION_DATAABT);
            break;
        case EVENT_ID_PREAABT:
            interrupt_exception(cpu, INT_EXCEPTION_PREABT);
            break;
        default:
            if(!cpsr_i(cpu) && user_event(&peripheral_reg_base, cpu->code_counter)) {
                interrupt_exception(cpu, INT_EXCEPTION_IRQ);
            }
        }
        clock_speed_detect(cpu, realtime_speed_show);
    }

    return 0;
}


/*****************************END OF FILE***************************/
