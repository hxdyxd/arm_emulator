/*
 * config.h of arm_emulator
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
#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifdef __linux__
#define USE_PRCTL_SET_THREAD_NAME
#define USE_TUN_SUPPORT
#endif

#define USE_SLIRP_SUPPORT
#define FS_MMAP_MODE

#ifndef _WIN32
#define USE_UNIX_TERMINAL_API
#endif

#endif /*_CONFIG_H_*/
