# PYHWTEST

This is a python module for reading and writing to memory on devices in python.
Currently it will only work on Linux. The intent is for users to be able to make
quick python scripts and import them into the interpreter with the '-i' flag to
python.

This makes some basic hardware tests very simple if you have registers mapped
into the host memory space.

# INSTALLATION

To install the hwtest extension make sure the python development headers are
installed for you system.  On yum based systems:

```ShellSession
# yum install python-devel
# python setup.py install
```

You will also need to build and install the driver separately right now. First make sure your kernel headers are installed. On RPM based systems:

```ShellSession
# yum install kernel-devel
```
or on Debian based systems:

```ShellSession
# apt-get install linux-headers-`uname -r`
```

Now build and install the khwtest module:

```ShellSession
# make -C /lib/modules/`uname -r`/build M=`pwd` modules
# cp khwtest.ko /lib/modules/`uname -r`/kernel/
# depmod -a
```

Also, please disable selinux or apparmor on your system. pyhwtest tries to map
memory, which can be prevented by selinux depending on how pyhwtest is
installed.

# USAGE

You must be root in order to use the hwtest module since it can access
physical memory and IO ports.  

You can always get the built in help information with:

```python
>>> import hwtest
>>> help(hwtest)
```

Assume you have a custom PCI device with BAR0 mapped into 0xfc000000. You can
access those registers like: 

```python
>>> from hwtest import *
>>> readlw(0xfc000000)
4294967295L
>>> hex(_)
'0xFFFFFFFFL'
>>> hex(readlw(0xfc000000))
'0xFFFFFFFFL'
>>> writelw(0xfc000000, 0x80000000)
```

The hwtest module also provides a way to allocate host memory for use in
testing out DMA operations:

```python
>>> phys_address = hwtest.alloc_dma_page()
```

The phys_address value can be written into device registers, and readlw /
writelw can be used to access those pages just like any other memory.
