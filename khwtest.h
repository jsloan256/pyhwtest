/* khwtest.h 
 *
 * This is a kernel module that provides read / write access to physical
 * memory in the system.  It also allows memory to be allocated for use as DMA
 * buffers.
 *
 * Copyright (c) 2009-2014, Shaun Ruffell <sruffell@sruffell.net>
 * All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */
#define KHWTEST_CODE 'K'

/* Allocates a page of memory that is mapped for DMA access to 32 bit devices
 * and returns the 32-bit physical address. 
 *
 * NOTE:  There is no corresponding free.  All the pages allocated via a
 * particular open device file are closed when that file handle is closed.
 */
#define KHWTEST_ALLOC_DMA_PAGE32 _IOR(KHWTEST_CODE, 1, __u32)
