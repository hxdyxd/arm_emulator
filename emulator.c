/* 2020 03 11 */
/* By hxdyxd */
#include <stdlib.h>
#include <armv4.h>
#include <peripheral.h>

#include <unistd.h>

#define USE_LINUX     0
#define USE_BINARY    1


//cpu memory
struct armv4_cpu_t cpu_handle;

//peripheral register
struct peripheral_t peripheral_reg_base;

#define PERIPHERAL_NUMBER    (4)
//peripheral address & function config
struct peripheral_link_t peripheral_config[PERIPHERAL_NUMBER] = {
    {
        .mask = ~(MEM_SIZE-1), //25bit
        .prefix = 0x00000000,
        .reg_base = &peripheral_reg_base.memory[0],
        .reset = memory_reset,
        .read = memory_read,
        .write = memory_write,
    },
    {
        .mask = ~(8-1), //3bit
        .prefix = 0x4001f040,
        .reg_base = &peripheral_reg_base.intc,
        .reset = intc_reset,
        .read = intc_read,
        .write = intc_write,
    },
    {
        .mask = ~(8-1), //3bit
        .prefix = 0x4001f020,
        .reg_base = &peripheral_reg_base.tim,
        .reset = tim_reset,
        .read = tim_read,
        .write = tim_write,
    },
    {
        .mask = ~(256-1), //8bit
        .prefix = 0x40020000,
        .reg_base = &peripheral_reg_base.uart[0],
        .reset = uart_8250_reset,
        .read = uart_8250_read,
        .write = uart_8250_write,
    },
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
        fread(&instruction, 4, 1, fp);
        write_word(cpu, address, instruction);
        address = address + 4;
    }
    WARN("load mem base 0x%x, size 0x%x\r\n", start, address - start - 4);
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
        "       -f <image_path>            Set Image or Binary file path.\n");
    printf(
        "       -t <device_tree_path>    Set Devices tree path.\n");
    printf(
        "       -d                       Display debug message.\n");
    printf(
        "       -s                       Step by step mode.\n");
    printf("\n");
    printf(
        "       [-v]                       Verbose mode.\n");
    printf(
        "       [-h, --help]               Print this message.\n");
    printf("\n");
    printf("Reference: https://github.com/hxdyxd/arm-emulator\n");
}


#define IMAGE_LOAD_ADDRESS   (0x8000)
#define DTB_BASE_ADDRESS     (MEM_SIZE - 0x4000)


int main(int argc, char **argv)
{
    struct armv4_cpu_t *cpu = &cpu_handle;
    //default value
    uint8_t mode = USE_BINARY;
    char *image_path = NULL;
    char *dtb_path = NULL;
    uint8_t step_by_step = 0;
    int ch;

    while((ch = getopt(argc, argv, "m:f:t:dshv")) != -1) {
        switch(ch) {
        case 't':
            dtb_path = optarg;
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
    if(!image_path || (mode == USE_LINUX && !dtb_path) ) {
        printf("parameter error \n");
        usage(argv[0]);
        exit(-1);
    }

    cpu_init(cpu);
    peripheral_register(cpu, peripheral_config, PERIPHERAL_NUMBER);

    switch(mode) {
    case USE_LINUX:
        load_program_memory(cpu, image_path, IMAGE_LOAD_ADDRESS);
        load_program_memory(cpu, dtb_path, DTB_BASE_ADDRESS);

        /* linux environment, Kernel boot conditions */
        register_write(cpu, 1, 0xffffffff);         //set r1
        register_write(cpu, 2, DTB_BASE_ADDRESS);  //set r2, dtb base Address
        register_write(cpu, 15, IMAGE_LOAD_ADDRESS);            //set pc, jump to Load Address
        break;
    case USE_BINARY:
        load_program_memory(cpu, image_path, 0);
        break;
    default:
        exit(-1);
    }

    for(;;) {
        cpu->code_counter++;
        //printf("%d 0x%x\n", cpu->code_counter, register_read(cpu, 15));

        fetch(cpu);
        if(mmu_check_status(&cpu->mmu)) {
            //check fetch instruction_word fault
            interrupt_exception(cpu, INT_EXCEPTION_PREABT);
            cpu->mmu.mmu_fault = 0;
            continue;
        }

        decode(cpu);
        if(cpu->decoder.swi_flag) {
            cpu->decoder.swi_flag = 0;
            interrupt_exception(cpu, INT_EXCEPTION_SWI);
        } else if(mmu_check_status(&cpu->mmu)) {
            //check memory data fault
            interrupt_exception(cpu, INT_EXCEPTION_DATAABT);
            cpu->mmu.mmu_fault = 0;
        } else {
            if(!cpsr_i(cpu) && user_event(&peripheral_reg_base, cpu->code_counter, 40000)) {
                interrupt_exception(cpu, INT_EXCEPTION_IRQ);
            }
        }

        if(step_by_step) {
            printf("[%d] Press any key to continue...\n", cpu->code_counter);
            getchar();
        }
    }

    return 0;
}


/*****************************END OF FILE***************************/
