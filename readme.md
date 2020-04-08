# arm_emulator

![C/C++ CI](https://github.com/hxdyxd/arm_emulator/workflows/C/C++%20CI/badge.svg)

## Introduction

Simple armv4 emulator with embedded freertos and linux operating system support  

## Currently supported features
* All ARMv4 instructions  
* Interrupts (timer interrupt, 8250 serial interrupts)  
* Prefetch Abort, Data Abort, Undefined instruction, IRQ ,FIQ exceptions  
* CP15 coprocessor, Memory Management Unit(MMU) and Translation Lookaside Buffer(TLB)  
* Network support via serial port and TUN/TAP devices for host  
* Console support via serial port  
* Step by step running  
* Disassembler  

## Other codes
* [arm-emulator-linux](https://github.com/hxdyxd/arm-emulator-linux), [arm-emulator-linux(gitee mirror)](https://gitee.com/hxdyxd/arm-emulator-linux)  
* [buildroot(generate embedded Linux systems)](https://github.com/hxdyxd/buildroot)  
* [Image(google drive)](https://drive.google.com/drive/folders/1W0milmr0MT9K7TXI4cvJHEbDRon9gp-X?usp=sharing)  
* [arm-emulator.dts](https://github.com/hxdyxd/arm-emulator-linux/blob/master/arch/arm/boot/dts/arm-emulator.dts)  

## Usage Example

> armemulator -ds -m bin -f hello.bin                             ;Run 'hello.bin' by step by step mode  
> armemulator -m disassembly -f hello.bin                         ;Show assembly code of 'hello.bin'  
> armemulator -m linux -f zImage -r rootfs.ext2                   ;Run linux kernel  
> armemulator -m linux -f Image -t arm-emulator.dtb -r rootfs.ext2  

## Usage

```
./armemulator

  usage:

  armemulator
       -m <mode>                  Select 'linux', 'bin' or 'disassembly' mode, default is 'bin'.
       -f <image_path>            Set image or binary programme file path.
       [-r <romfs_path>]          Set ROM filesystem path.
       [-t <device_tree_path>]    Set Devices tree path.
       [-d]                       Display debug message.
       [-s]                       Step by step mode.

       [-v]                       Verbose mode.
       [-h, --help]               Print this message.

  Build , [time] 
  Reference: https://github.com/hxdyxd/arm_emulator

```
Step by step mode command:  
```
  usage:

  armemulator
       m                Print MMU page table
       r [n]            Run skip n step
       d                Set/Clear debug message flag
       l                Print TLB table
       g                Print register table
       s                Set step by step flag, prease ctrl+b to clear
       p[p|v] [a]       Print physical/virtual address at 0x[a]
       t                Print run time
       h                Print this message
       q                Quit program

  Build , [time]
```

## Build armemulator

```
make V=1 STATIC=1
sudo make install
```

## Build linux zImage with buildroot

```
sudo apt-get install libncurses-dev flex bison bc
sudo apt-get install unzip rsync python3 texinfo
git clone https://github.com/hxdyxd/buildroot
cd buildroot
make armemulator_defconfig
make menuconfig
make
ls output/images/
```

## Reference
* [ARM Architecture Reference Manual](https://developer.arm.com/docs/ddi0100/i/armv5-architecture-reference-manual)  
