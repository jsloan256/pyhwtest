#!/usr/bin/env python
'''
Contains functions for testing registers on physical boards.
'''

import os

if os.getuid() != 0:
    raise ImportError("You must be root to use the hwtest module")

if os.system("modprobe khwtest"):
    raise ImportError("Failed to load the khwtest module.")

import chwtest

def __convert(address):
    # This is a bit of ugliness due to the fact that python integerrs aren't
    # limited by 32-bits.  If bit 31 is set, we need to make sure that we pass
    # a negative number to the extension.
    if address & 0x80000000:
        address = -((address^0xffffffff)+1)
    return address

def readb(address):
    '''
    Reads a byte in from memory mapped IO registers.
    '''
    return chwtest.readb(__convert(address)) & 0xff
def readw(address):
    '''
    Reads a word in from memory mapped IO registers.
    '''
    return chwtest.readw(__convert(address)) & 0xffff
def readlw(address):
    '''
    Reads a long word in from memory mapped IO registers.
    '''
    return chwtest.readlw(__convert(address)) & 0xffffffff
def writeb(address, value):
    '''
    Writes a byte to a memory mapped IO register.
    '''
    chwtest.writeb(__convert(address), __convert(value))
def writew(address, value):
    ''' 
    Writes a word to a memory mapped IO register.
    '''
    chwtest.writeb(__convert(address), __convert(value))
def writelw(address, value):
    ''' 
    Writes a long word to a memory mapped IO register.
    '''
    chwtest.writelw(__convert(address), __convert(value))
def inb(address):
    ''' 
    Reads a byte from an IO port.
    '''
    return chwtest.inb(__convert(address)) & 0xff
def inw(address):
    '''
    Reads a word from an IO port.
    '''
    return chwtest.inw(__convert(address)) & 0xffff
def inlw(address):
    '''
    Reads a long word from an IO port.
    '''
    return chwtest.inlw(__convert(address)) & 0xffffffff
def outb(address, value):
    '''
    Writes a byte to an IO port.
    '''
    chwtest.outb(__convert(address), __convert(value))
def outw(address, value):
    '''
    Writes a word to an IO port.
    '''
    chwtest.outw(__convert(address), __convert(value))
def outlw(address, value):
    '''
    Writes a long word to an IO port.
    '''
    chwtest.outl(__convert(address), __convert(value))

def alloc_dma_page():
    '''
    Allocates a page of physical memory and returns the physical address of
    the page which can be passed to a device or read with the other physical
    memory access functions.  The memory is freed when this module is
    unloaded.
    '''
    return chwtest.alloc_dma_page() & 0xffffffff

def dump(address, words):
    for i in range(0, words, 1):
        print "%08x:" % (address+ 4*i),
        print hex(readlw(address + 4*i))

class IORegion(object):
    '''
    Provides an interface for reading and writing to IO from a given offset.
    '''
    def __init__(self, **kwargs):
        self.base = kwargs.get("base")
        if not self.base:
            raise Exception("You must specify a base address for the memory region.")
    def inlw(self, address):
        return inlw(self.base + address)
    def outlw(self, address):
        outlw(self.base + address)

class MemoryRegion(object):
    '''
    Provides an interface for reading and writing to meory from a given offset.
    '''
    def __init__(self, **kwargs):
        self.base = kwargs.get("base")
        if not self.base:
            raise Exception("You must specify a base address for the memory region.")
    def readlw(self, address):
        return readlw(self.base + address)
    def writelw(self, address):
        writelw(self.base + address)
    
    
# vim: ai ts=4 sts=4 et sw=4
