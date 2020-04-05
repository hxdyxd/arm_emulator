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
#include <peripheral.h>
#include <conio.h>

#include <unistd.h>
#include <slip_tun.h>


#define USE_LINUX     0
#define USE_BINARY    1

#define IMAGE_LOAD_ADDRESS   (0x8000)
#define DTB_BASE_ADDRESS     (MEM_SIZE - 0x4000)

#ifndef MAX_FS_SIZE
#define MAX_FS_SIZE   (1 << 29)   //512M
#endif


//peripheral register
struct peripheral_t peripheral_reg_base = {
    .tim = {
        .interrupt_id = 0,
    },
    .uart = {
        {
            .interrupt_id = 1,
            .readable = kbhit,
            .read = getch,
            .writeable = uart_8250_rw_enable,
            .write = putchar,
        },
        {
            .interrupt_id = 2,
            .readable = slip_tun_readable,
            .read = slip_tun_read,
            .writeable = slip_tun_writeable,
            .write = slip_tun_write,
        },
    },
};

#define PERIPHERAL_NUMBER    (6)
//peripheral address & function config
struct peripheral_link_t peripheral_config[PERIPHERAL_NUMBER] = {
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


//load_program_memory reads the input memory, and populates the instruction 
// memory
uint32_t load_program_memory(struct armv4_cpu_t *cpu, const char *file_name, uint32_t start)
{
    FILE *fp;
    unsigned int address, instruction;
    fp = fopen(file_name, "rb");
    if(fp == NULL) {
        ERROR("Error opening input mem file %s\n", file_name);
        exit(-1);
    }
    address = start;
    while(!feof(fp)) {
        (void)fread(&instruction, 4, 1, fp);
        write_word(cpu, address, instruction);
        address = address + 4;
    }
    WARN("load mem base 0x%x, size %d\r\n", start, address - start - 4);
    fclose(fp);
    return address - start - 4;
}


void usage(const char *file)
{
    printf("\n");
    printf("%s\n\n", file);
    printf("  usage:\n\n");
    printf("  arm_emulator\n");
    printf(
        "       -m <mode>                  Select 'linux' or 'bin' mode, default is 'bin'.\n");
    printf(
        "       -f <image_path>            Set image or binary programme file path.\n");
    printf(
        "       [-r <romfs_path>]          Set ROM filesystem path.\n");
    printf(
        "       [-t <device_tree_path>]    Set Devices tree path.\n");
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
    printf("Reference: https://github.com/hxdyxd/arm_emulator\n");
}


int main(int argc, char **argv)
{
    struct armv4_cpu_t cpu_handle;
    struct armv4_cpu_t *cpu = &cpu_handle;
    //default value
    uint8_t mode = USE_BINARY;
    char *image_path = NULL;
    char *dtb_path = NULL;
    uint8_t step_by_step = 0;
    int ch;

    peripheral_reg_base.fs.filename = NULL;
    while((ch = getopt(argc, argv, "m:f:r:t:dshv")) != -1) {
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
            if(strcmp(optarg, "linux") == 0) {
                mode = USE_LINUX;
            } else if(strcmp(optarg, "bin") == 0) {
                mode = USE_BINARY;
            } else {
                printf("unknown mode option :%s\n", optarg);
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
            exit(-1);
        case '?': // 输入未定义的选项, 都会将该选项的值变为 ?
            printf("unknown option \n");
            usage(argv[0]);
            exit(-1);
        }
    }
    if(!image_path) {
        printf("parameter error \n");
        usage(argv[0]);
        exit(-1);
    }

    slip_tun_init();
    cpu_init(cpu);
    peripheral_register(cpu, peripheral_config, PERIPHERAL_NUMBER);

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
    printf("Start...\n");

    for(;;) {
        if(step_by_step) {
            static uint32_t skip_num = 0;
            if(skip_num) {
                --skip_num;
                goto RUN;
            }
            printf("[%d] cmd>\n", cpu->code_counter);
            char cmd_str[64] = {0, };
            uint8_t cmd_len = 0;
            for(cmd_len=0; cmd_len<64; cmd_len++) {
                while(!kbhit())
                    usleep(1000);
                if((cmd_str[cmd_len] = getch()) == '\n')
                    break;
                putchar(cmd_str[cmd_len]);
            }
            cmd_str[cmd_len] = 0;
            printf("\n");
            switch(cmd_str[0]) {
            case 'm':
                printf("MMU table base: 0x%08x\n", cpu->mmu.reg[2]);
                for(int i=0; i<4096; i++) {
                    if(i && i % 16 == 0)
                        printf("\n");
                    printf("%08x, ", read_word_without_mmu(cpu,
                     cpu->mmu.reg[2]+(i<<2)));
                }
                printf("\n-----------MMU table end--------------\n");
                break;
            case 'r':
                if(sscanf(cmd_str, "r %d", &skip_num) != 1) {
                    skip_num = 0;
                }
                printf("skip %d ...\n", skip_num);
                goto RUN;
                break;
            case 'd':
                global_debug_flag = !global_debug_flag;
                printf("[%s] debug info\n", global_debug_flag ? "x" : " ");
                break;
            case 'l':
                tlb_show(&cpu->mmu);
                break;
            case 'q':
                printf("quit\n");
                exit(0);
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
            if(!cpsr_i(cpu) && user_event(&peripheral_reg_base, cpu->code_counter, 65536)) {
                interrupt_exception(cpu, INT_EXCEPTION_IRQ);
            }
        }
    }

    return 0;
}


/*****************************END OF FILE***************************/
