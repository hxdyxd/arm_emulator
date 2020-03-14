# arm_emulator  

## Introduction  
simple armv4 emulator with freertos and linux support, only two thousand lines, other code at  
1.[arm-emulator-linux](https://github.com/hxdyxd/arm-emulator-linux)  
2.[arm_emulator_linux_bin](https://github.com/hxdyxd/arm_emulator_linux_bin)  
3.[buildroot](https://github.com/hxdyxd/buildroot)  

## Device Tree Support:  
https://github.com/hxdyxd/arm-emulator-linux/blob/master/arch/arm/boot/dts/arm-emulator.dts  

## Usage Example:
> ./arm_emulator -m bin -f arm_hello_gcc/hello.bin  
> ./arm_emulator -m bin -f arm_freertos/hello.bin  
> ./arm_emulator -m linux -f arm_linux/Image -t arm_linux/arm-emulator.dtb  
