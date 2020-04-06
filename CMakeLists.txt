# CMakeList.txt of arm_emulator
# Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
cmake_minimum_required (VERSION 2.8)

project(arm_emulator)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -std=gnu99")
set(CMAKE_EXE_LINKER_FLAGS -static)

option (TUN_SUPPORT "Use tun/tap device for host" ON)
option (FS_MMAP_MODE "Use mmap fs mode" ON)

if (TUN_SUPPORT)
add_definitions(-DTUN_SUPPORT)
endif ()

if (FS_MMAP_MODE)
add_definitions(-DFS_MMAP_MODE)
endif ()

set(ARMEMULATOR_SOURSE
		emulator.c
		armv4.c
		peripheral.c
		kfifo.c
		slip_tun.c
		)
include_directories("${PROJECT_SOURCE_DIR}/")

add_executable(armemulator ${ARMEMULATOR_SOURSE})

target_link_libraries(armemulator pthread)

install(TARGETS armemulator DESTINATION bin)