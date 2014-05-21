/* chwtest.c 
 *
 * Simple python extension and library used for testing hardware.  Currently
 * provides a library for reading / writing from io memory associated with PCI
 * devices.
 *
 * Copyright (c) 2009-2014, Shaun Ruffell <sruffell@sruffell.net>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "Python.h"
#include <sys/mman.h>
#include <sys/io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>
#include "khwtest.h"

#define MEMORY_MAP 2
#define ACCESS_MODE MEMORY_MAP

/* File handle to physical memory */
static int mem_fd = -1;
static int khwtest_fd = -1;
static unsigned long ram_high = 0x20000000;

static unsigned long base_address = 0;
static unsigned long base_offset = 0;
void *mmaped_ptr = NULL;

static void open_khwtest(void)
{
	if (khwtest_fd != -1)
		return;

	khwtest_fd = open("/dev/khwtest", O_RDWR);
	if (-1 == khwtest_fd) {
		PyErr_SetFromErrnoWithFilename(PyExc_ImportError, "/dev/khwtest");
		return;
	}
}

static int mmap_address(unsigned long address) 
{
	unsigned long local_base;
	unsigned long local_offset;
	const unsigned long PAGE_SIZE = getpagesize();

	local_base = address & ~(PAGE_SIZE-1);
	local_offset = address % PAGE_SIZE;

	if (base_address != local_base) {
		if (base_address) {
			munmap(mmaped_ptr, PAGE_SIZE);
			base_address = 0;
			base_offset = 0;
			mmaped_ptr = NULL;
		}
		mmaped_ptr = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, 
				  MAP_SHARED | MAP_FIXED, mem_fd, local_base);
		if (MAP_FAILED == mmaped_ptr) {
			PyErr_SetFromErrno(PyExc_IOError); 
			return -1;
		}

		base_address = local_base;
	} 

	base_offset = local_offset;

	/* We have a good mapping... */
	return 0;
}

static unsigned char 
readb(unsigned long address)
{
	if (address >= ram_high) {
		if (mmap_address(address)) {
			return -1;
		}
		return *((volatile unsigned char *)(mmaped_ptr + base_offset));
	} else {
		unsigned char value;
		if (-1 == lseek(khwtest_fd, address, SEEK_SET)) {
			PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
			return -1;
		} else {
			if (-1 == read(khwtest_fd, &value, sizeof(value))) {
				PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
				return -1;
			}
			return value;
		}
	}
}

static unsigned short int 
readw(unsigned long address)
{
	if (address >= ram_high) {
		if (mmap_address(address)) {
			return -1;
		}
		return *((volatile unsigned short int *)(mmaped_ptr + base_offset));
	} else {
		unsigned short int value;
		if (-1 == lseek(khwtest_fd, address, SEEK_SET)) {
			PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
			return -1;
		} else {
			if (-1 == read(khwtest_fd, &value, sizeof(value))) {
				PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
				return -1;
			}
			return value;
		}
	}
}

static unsigned long
readlw(unsigned long address)
{
	if (address >= ram_high) {
		if (mmap_address(address)) {
			return -1;
		}
		return *((volatile unsigned long *)(mmaped_ptr + base_offset));
	} else {
		unsigned long int value;
		open_khwtest();
		if (-1 == lseek(khwtest_fd, address, SEEK_SET)) {
			PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/khwtest");
			return -1;
		} else {
			if (-1 == read(khwtest_fd, &value, sizeof(value))) {
				PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/khwtest");
				return -1;
			}
			return value;
		}
	}
}

static void
writeb(unsigned long address, unsigned char val)
{
	if (address >= ram_high) {
		mmap_address(address);
		*(volatile unsigned char *)(mmaped_ptr + base_offset) = val;
	} else {
		if (-1 == lseek(khwtest_fd, address, SEEK_SET)) {
			PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
		} else {
			if (-1 == write(khwtest_fd, &val, sizeof(val))) {
				PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
			}
		}
	}
}

static void
writew(unsigned long address, unsigned short int val)
{
	if (address >= ram_high) {
		mmap_address(address);
		*(volatile unsigned short int *)(mmaped_ptr + base_offset) = val;
	} else {
		if (-1 == lseek(khwtest_fd, address, SEEK_SET)) {
			PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
		} else {
			if (-1 == write(khwtest_fd, &val, sizeof(val))) {
				PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/mem");
			}
		}
	}
}


static void
writelw(unsigned long address, unsigned long val)
{
	if (address >= ram_high) {
		mmap_address(address);
		*(volatile unsigned long *)(mmaped_ptr + base_offset) = val;
	} else {
		if (-1 == lseek(khwtest_fd, address, SEEK_SET)) {
			PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/khwtest");
		} else {
			if (-1 == write(khwtest_fd, &val, sizeof(val))) {
				PyErr_SetFromErrnoWithFilename(PyExc_IOError, "/dev/khwtest");
			}
		}
	}
}

static PyObject *
chwtest_readb(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned char value; 
	if (!PyArg_ParseTuple(args, "l", &address))
		return NULL;
	value = readb(address);
	return Py_BuildValue("b", value);
}

static PyObject *
chwtest_readw(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned short int value; 
	if (!PyArg_ParseTuple(args, "l", &address))
		return NULL;
	value = readw(address);
	return Py_BuildValue("h", value);
}


static PyObject *
chwtest_readlw(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned long int value; 
	if (!PyArg_ParseTuple(args, "l", &address))
		return NULL;
	value = readlw(address);
	return Py_BuildValue("l", value);
}

static PyObject *
chwtest_writeb(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned char value; 

	if (!PyArg_ParseTuple(args, "lb", &address, &value)) {
		return NULL;
	}
	writeb(address, value);
	Py_RETURN_NONE;
}

static PyObject *
chwtest_writew(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned short int value; 

	if (!PyArg_ParseTuple(args, "lh", &address, &value)) {
		return NULL;
	}
	writew(address, value);
	if (PyErr_Occurred() != NULL) {
		return NULL;
	} else {
		Py_RETURN_NONE;
	}
}

static PyObject *
chwtest_writelw(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned long int value;

	if (!PyArg_ParseTuple(args, "ll", &address, &value)) {
		return NULL;
	}
	writelw(address, value);
	if (PyErr_Occurred() != NULL) {
		return NULL;
	} else {
		Py_RETURN_NONE;
	}
}

static PyObject *
chwtest_inb(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned char value; 
	if (!PyArg_ParseTuple(args, "l", &address))
		return NULL;
	value = inb_p(address);
	return Py_BuildValue("b", value);
}

static PyObject *
chwtest_inw(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned short int value; 
	if (!PyArg_ParseTuple(args, "l", &address))
		return NULL;
	value = inw_p(address);
	return Py_BuildValue("h", value);
}


static PyObject *
chwtest_inlw(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned long int value; 
	if (!PyArg_ParseTuple(args, "l", &address))
		return NULL;
	value = inl_p(address);
	return Py_BuildValue("l", value);
}

static PyObject *
chwtest_outb(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned char value; 

	if (!PyArg_ParseTuple(args, "lb", &address, &value)) {
		return NULL;
	}
	outb_p(value, address);
	Py_RETURN_NONE;
}

static PyObject *
chwtest_outw(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned short int value; 

	if (!PyArg_ParseTuple(args, "lh", &address, &value)) {
		return NULL;
	}
	outw_p(value, address);
	Py_RETURN_NONE;
}

static PyObject *
chwtest_outlw(PyObject *self, PyObject *args)
{
	unsigned long int address;
	unsigned long int value;

	if (!PyArg_ParseTuple(args, "ll", &address, &value)) {
		return NULL;
	}
	outl_p(value, address);
	Py_RETURN_NONE;
}

static PyObject *
chwtest_allocdmapage(PyObject *self, PyObject *args)
{
	__u32 physical_address;
	open_khwtest();
	if (ioctl(khwtest_fd, KHWTEST_ALLOC_DMA_PAGE32, &physical_address)) {
		PyErr_SetFromErrno(PyExc_IOError);
		return NULL;
	}
	return Py_BuildValue("l", physical_address);
}

static PyMethodDef ChwtestMethods[] = {
	{"readb",   chwtest_readb,   METH_VARARGS, "Read a byte from physical memory."},
	{"readw",   chwtest_readw,   METH_VARARGS, "Read a word from physical memory."},
	{"readlw",  chwtest_readlw,  METH_VARARGS, "Read a long word from physical memory."},
	{"writeb",  chwtest_writeb,  METH_VARARGS, "Write a byte to physical memory."},
	{"writew",  chwtest_writew,  METH_VARARGS, "Write a word to physical memory."},
	{"writelw", chwtest_writelw, METH_VARARGS, "Write a long word to physical memory."},
	{"inb",     chwtest_inb,     METH_VARARGS, "Read a byte from I/O space."},
	{"inw",     chwtest_inw,     METH_VARARGS, "Read a word from I/O space."},
	{"inlw",    chwtest_inlw,    METH_VARARGS, "Read a long word from I/O space."},
	{"outb",    chwtest_outb,    METH_VARARGS, "Write a byte to I/O space."},
	{"outw",    chwtest_outw,    METH_VARARGS, "Write a word to I/O space."},
	{"outlw",   chwtest_outlw,   METH_VARARGS, "Write a long word to I/O space."},
	{"alloc_dma_page", 
	 chwtest_allocdmapage, 
	 METH_VARARGS, 
	 "Allocate a DMAable page of memory and return the physical address.\n"
	},
	{ NULL, NULL, 0, NULL},
};

PyMODINIT_FUNC initchwtest(void)
{
	(void)Py_InitModule("chwtest", ChwtestMethods);

	mem_fd = open("/dev/mem", O_RDWR);
	if (-1 == mem_fd) {
		PyErr_SetFromErrnoWithFilename(PyExc_ImportError, "/dev/mem");
		return;
	}
	if (iopl(3)) { /* So that we can access the io space */
		PyErr_SetString(PyExc_ImportError, 
			"Failed to enable permissions to access IO space for this process.");
		return;
	}
}
