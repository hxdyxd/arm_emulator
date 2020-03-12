/* 2020 03 11 */
/* By hxdyxd */
#include <armv4.h>
#include <peripheral.h>


#if 1

//cpu memory
struct armv4_cpu_t cpu_handle;

//peripheral register
struct peripheral_t peripheral_reg_base;

#define PERIPHERAL_NUMBER    (5)
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
        .mask = ~(8-1), //3bit
        .prefix = 0x4001f000,
        .reg_base = &peripheral_reg_base.earlyuart,
        .reset = earlyuart_reset,
        .read = earlyuart_read,
        .write = earlyuart_write,
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
        ERROR("Error opening input mem file\n");
    }
    address = start;
    while(!feof(fp)) {
        fread(&instruction, 4, 1, fp);
        write_word(cpu, address, instruction);
        address = address + 4;
    }
    WARN("load mem start 0x%x, size 0x%x \r\n", start, address - start - 4);
    fclose(fp);
    return address - start - 4;
}


#define IMAGE_LOAD_ADDRESS   (0x8000)
#define DTB_BASE_ADDRESS     (MEM_SIZE - 0x4000)

const char *path = "./arm_linux/Image";
const char *dtb_path = "./arm_linux/arm-emulator.dtb";


int main(int argc, char **argv)
{
    printf("welcome to armv4 emulator\n");
    struct armv4_cpu_t *cpu = &cpu_handle;

    cpu_init(cpu);
    peripheral_register(cpu, peripheral_config, PERIPHERAL_NUMBER);
    load_program_memory(cpu, path, IMAGE_LOAD_ADDRESS);
    load_program_memory(cpu, dtb_path, DTB_BASE_ADDRESS);

    /* linux environment, Kernel boot conditions */
    register_write(cpu, 1, 0xffffffff);         //set r1
    register_write(cpu, 2, DTB_BASE_ADDRESS);  //set r2, dtb base Address
    register_write(cpu, 15, IMAGE_LOAD_ADDRESS);            //set pc, jump to Load Address

    for(;;) {
        cpu->code_counter++;

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
    }

    return 0;
}
#endif

/*****************************END OF FILE***************************/
