simple arm9 emulator with freertos and linux support and only two thousand lines, other code at
https://github.com/hxdyxd/arm-emulator-linux
https://github.com/hxdyxd/arm_emulator_linux_bin

console log:
KIPS_SPEED = 40000
load mem start 0x8000, size 0xa1d1a0
load mem start 0x1ffc000, size 0x4e4
[    0.000000] Booting Linux on physical CPU 0x0
[    0.000000] Linux version 5.1.15+ (root@ubuntu) (gcc version 7.2.1 20171011 (Linaro GCC 7.2-2017.11)) #113 Wed Jul 17 08:17:21 PDT 2019
[    0.000000] CPU: ARM920T [41029205] revision 5 (ARMv4T), cr=00002133
[    0.000000] CPU: VIVT data cache, VIVT instruction cache
[    0.000000] OF: fdt: Machine model: hxdyxd,armemulator
[    0.000000] printk: bootconsole [earlycon0] enabled
[    0.000000] Memory policy: Data cache buffered
[    0.000000] Map Peripheral address to 0xf001f000
[    0.000000] random: get_random_bytes called from start_kernel+0x84/0x41c with crng_init=0
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 8128
[    0.000000] Kernel command line: console=ttyS0 rootwait root=/dev/ram0 rw rdinit=/init earlyprintk
[    0.000000] Dentry cache hash table entries: 4096 (order: 2, 16384 bytes)
[    0.000000] Inode-cache hash table entries: 2048 (order: 1, 8192 bytes)
[    0.000000] Memory: 22832K/32768K available (4096K kernel code, 148K rwdata, 640K rodata, 4096K init, 206K bss, 9936K reserved, 0K cma-reserved, 0K highmem)
[    0.000000] SLUB: HWalign=32, Order=0-3, MinObjects=0, CPUs=1, Nodes=1
[    0.000000] NR_IRQS: 16, nr_irqs: 16, preallocated irqs: 16
[    0.000000] armemulator-ic base address = 0xf001f040
[    0.000000] armemulator-timer base address = 0xf001f020
[    0.001000] sched_clock: 32 bits at 1kHz, resolution 1000000ns, wraps every 2147483647500000ns
[    0.007000] clocksource: timer: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 1911260446275000 ns
[    0.011000] set periodic to 240000
[    0.019000] Console: colour dummy device 80x30
[    0.022000] Calibrating delay loop... 11.49 BogoMIPS (lpj=57472)
[    0.146000] pid_max: default: 32768 minimum: 301
[    0.161000] Mount-cache hash table entries: 1024 (order: 0, 4096 bytes)
[    0.165000] Mountpoint-cache hash table entries: 1024 (order: 0, 4096 bytes)
[    0.194000] CPU: Testing write buffer coherency: ok
[    0.211000] Setting up static identity map for 0x100000 - 0x100058
[    0.247000] devtmpfs: initialized
[    0.284000] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 19112604462750000 ns
[    0.291000] futex hash table entries: 256 (order: -1, 3072 bytes)
[    0.312000] NET: Registered protocol family 16
[    0.333000] DMA: preallocated 256 KiB pool for atomic coherent allocations
[    0.555000] clocksource: Switched to clocksource timer
[    0.820000] NET: Registered protocol family 2
[    0.851000] tcp_listen_portaddr_hash hash table entries: 512 (order: 0, 4096 bytes)
[    0.858000] TCP established hash table entries: 1024 (order: 0, 4096 bytes)
[    0.864000] TCP bind hash table entries: 1024 (order: 0, 4096 bytes)
[    0.868000] TCP: Hash tables configured (established 1024 bind 1024)
[    0.876000] UDP hash table entries: 256 (order: 0, 4096 bytes)
[    0.881000] UDP-Lite hash table entries: 256 (order: 0, 4096 bytes)
[    0.892000] NET: Registered protocol family 1
[    3.466000] random: fast init done
[   21.802000] workingset: timestamp_bits=30 max_order=13 bucket_order=0
[   22.740000] jitterentropy: Initialization failed with host not compliant with requirements: 2
[   22.746000] Block layer SCSI generic (bsg) driver version 0.4 loaded (major 254)
[   22.752000] io scheduler mq-deadline registered
[   22.755000] io scheduler kyber registered
[   22.772000] io scheduler bfq registered
[   22.898000] Serial: 8250/16550 driver, 4 ports, IRQ sharing disabled
[   22.941000] printk: console [ttyS0] disabled
[   22.953000] 40020000.serial: ttyS0 at MMIO 0x40020000 (irq = 17, base_baud = 230400) is a 8250
[   22.958000] printk: console [ttyS0] enabled
[   22.958000] printk: console [ttyS0] enabled
[   22.964000] printk: bootconsole [earlycon0] disabled
[   22.964000] printk: bootconsole [earlycon0] disabled
[   23.338000] brd: module loaded
[   23.370000] NET: Registered protocol family 17
[   23.373000] Key type dns_resolver registered
[   23.911000] Freeing unused kernel memory: 4096K
[   23.914000] Run /init as init process
Starting syslogd: OK
Starting klogd: OK
Initializing random number generator... [   27.572000] random: dd: uninitialized urandom read (512 bytes read)
done.
Starting network: OK

Welcome to ARM Emulator
armemulator login:
