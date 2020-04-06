# arm_emulator

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

## Other codes
1.[arm-emulator-linux](https://github.com/hxdyxd/arm-emulator-linux), [arm-emulator-linux(gitee mirror)](https://gitee.com/hxdyxd/arm-emulator-linux)  
2.[buildroot(generate embedded Linux systems)](https://github.com/hxdyxd/buildroot)  
3.[Image(google drive)](https://drive.google.com/drive/folders/1W0milmr0MT9K7TXI4cvJHEbDRon9gp-X?usp=sharing)  
4.[arm-emulator.dts](https://github.com/hxdyxd/arm-emulator-linux/blob/master/arch/arm/boot/dts/arm-emulator.dts)  

## Usage Example

> ./arm_emulator -m bin -f hello.bin  
> ./arm_emulator -m bin -f hello.bin  
> ./arm_emulator -m linux -f zImage -r rootfs.ext2  
> ./arm_emulator -m linux -f Image -t arm-emulator.dtb -r rootfs.ext2  

## Usage

```
  usage:

  arm_emulator
       -m <mode>                  Select 'linux' or 'bin' mode, default is 'bin'.
       -f <image_path>            Set image or binary programme file path.
       [-r <romfs_path>]          Set ROM filesystem path.
       [-t <device_tree_path>]    Set Devices tree path.
       [-d]                       Display debug message.
       [-s]                       Step by step mode.

       [-v]                       Verbose mode.
       [-h, --help]               Print this message.

Reference: https://github.com/hxdyxd/arm_emulator
```

## Build armemulator

```
CFLAGS=-static make
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
1.[ARM Architecture Reference Manual](https://developer.arm.com/docs/ddi0100/i/armv5-architecture-reference-manual)  
