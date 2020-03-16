# arm_emulator

## Introduction

simple armv4 emulator with freertos and linux support, only two thousand lines, other code at  
1.[arm-emulator-linux](https://github.com/hxdyxd/arm-emulator-linux), [arm-emulator-linux(Gitee mirror)](https://gitee.com/hxdyxd/arm-emulator-linux)  
2.[buildroot(Generate embedded Linux systems)](https://github.com/hxdyxd/buildroot)  
3.[zImage.arm-emulator](https://drive.google.com/drive/folders/1W0milmr0MT9K7TXI4cvJHEbDRon9gp-X?usp=sharing)  

## Device Tree Support

[arm-emulator.dts](https://github.com/hxdyxd/arm-emulator-linux/blob/master/arch/arm/boot/dts/arm-emulator.dts)  

## Usage Example

> ./arm_emulator -m bin -f arm_hello_gcc/hello.bin  
> ./arm_emulator -m bin -f arm_freertos/hello.bin  
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
