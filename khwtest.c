/* khwtest.c 
 *
 * This is a kernel module that provides read / write access to physical
 * memory in the system.  It also allows memory to be allocated for use as DMA
 * buffers.  I know this is ugly, and is only to be used to facilitate testing
 * hardware when you don't want to introduce any new variables with drivers.
 *
 * Written by Shaun Ruffell <sruffell@digium.com>
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include "khwtest.h"

#define HERE() printk(KERN_DEBUG "HERE: %s:%d\n", __FILE__, __LINE__)

static int debug = 0;
const int khwtest_major = 0;

struct allocation {
	struct list_head node;
	unsigned int size;
	void *memory;
	dma_addr_t dma_handle;
};

struct khwtest_pvt {
	spinlock_t lock;
	struct list_head allocations;
};

static void khwtest_init_pvt(struct khwtest_pvt *pvt) 
{
	INIT_LIST_HEAD(&pvt->allocations);
	spin_lock_init(&pvt->lock);
}

#ifndef ARCH_HAS_VALID_PHYS_ADDR_RANGE
static inline int valid_phys_addr_range(unsigned long addr, size_t count)
{
	if (addr + count > __pa(high_memory))
		return 0;

	return 1;
}

static inline int valid_mmap_phys_addr_range(unsigned long pfn, size_t size)
{
	return 1;
}
#endif

static int 
khwtest_open(struct inode *inode, struct file *file)
{
	struct khwtest_pvt *pvt;
	if (!(pvt=kmalloc(sizeof(struct khwtest_pvt), GFP_KERNEL))) {
		return -ENOMEM;
	}
	khwtest_init_pvt(pvt);
	file->private_data = pvt;
	return 0;
}

static int 
khwtest_release(struct inode *inode, struct file *file)
{
	struct khwtest_pvt *pvt = file->private_data;
	struct allocation *p, *tmp;
	LIST_HEAD(local_list);

	if (!pvt) return 0;

	spin_lock(&pvt->lock);
	list_splice_init(&pvt->allocations, &local_list);
	spin_unlock(&pvt->lock);

	list_for_each_entry_safe(p, tmp, &local_list, node) {
		if (debug) {
			printk(KERN_DEBUG "%s: Freeing memory at 0x%08lx.\n", 
			       THIS_MODULE->name, (unsigned long)p->dma_handle);
		}
		list_del(&p->node);
		pci_free_consistent(NULL, p->size, p->memory, p->dma_handle);
		kfree(p);
	}
	return 0;
}

static int 
khwtest_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data)
{
	struct khwtest_pvt *pvt = file->private_data;
	struct allocation *alloc;
	__u32 physical_memory;

	switch(cmd) {
	case KHWTEST_ALLOC_DMA_PAGE32:
		if (!(alloc = kmalloc(sizeof(*alloc), GFP_KERNEL))) {
			return -ENOMEM;
		}
		memset(alloc, 0, sizeof(*alloc));
		alloc->memory = pci_alloc_consistent(NULL, PAGE_SIZE, &alloc->dma_handle); 
		if (!alloc->memory) {
			printk(KERN_ERR "%s: Failed to allocate consistent memory.\n", THIS_MODULE->name);
			kfree(alloc);
			return -ENOMEM;
		}
		alloc->size = PAGE_SIZE;
		spin_lock(&pvt->lock);
		list_add_tail(&alloc->node, &pvt->allocations);
		spin_unlock(&pvt->lock);
		if (debug) {
			printk(KERN_DEBUG "%s: Allocating memory at 0x%08lx\n",
			       THIS_MODULE->name, (unsigned long)alloc->dma_handle);
		}
		*((unsigned long *)alloc->memory) = (unsigned long)alloc->dma_handle;
		physical_memory = alloc->dma_handle;
		return put_user(physical_memory, (unsigned long __user *)data);
		break;
	default:
		return -ENOTTY;
	};
	return -ENOTTY;
}

/*
 * This funcion reads the *physical* memory. The f_pos points directly to the
 * memory location.
 */
static ssize_t khwtest_read(struct file * file, char __user * buf,
                        size_t count, loff_t *ppos)
{
        unsigned long p = *ppos;
        ssize_t read, sz;
        char *ptr;

        if (!valid_phys_addr_range(p, count))
                return -EFAULT;
        read = 0;
#ifdef __ARCH_HAS_NO_PAGE_ZERO_MAPPED
        /* we don't have page 0 mapped on sparc and m68k.. */
        if (p < PAGE_SIZE) {
                sz = PAGE_SIZE - p;
                if (sz > count)
                        sz = count;
                if (sz > 0) {
                        if (clear_user(buf, sz))
                                return -EFAULT;
                        buf += sz;
                        p += sz;
                        count -= sz;
                        read += sz;
                }
        }
#endif
        while (count > 0) {
                /*
                 * Handle first page in case it's not aligned
                 */
                if (-p & (PAGE_SIZE - 1))
                        sz = -p & (PAGE_SIZE - 1);
                else
                        sz = PAGE_SIZE;

                sz = min_t(unsigned long, sz, count);

                /*
                 * On ia64 if a page has been mapped somewhere as
                 * uncached, then it must also be accessed uncached
                 * by the kernel or data corruption may occur
                 */
                ptr = xlate_dev_mem_ptr(p);
                if (!ptr)
                        return -EFAULT;

                if (copy_to_user(buf, ptr, sz)) {
                        return -EFAULT;
                }

                buf += sz;
                p += sz;
                count -= sz;
                read += sz;
        }

        *ppos += read;
        return read;
}



/* NOTE:  This is copied from the write_mem implementation in mem.c */
static ssize_t khwtest_write(struct file * file, const char __user * buf, 
			 size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	ssize_t written, sz;
	unsigned long copied;
	void *ptr;

	if (!valid_phys_addr_range(p, count))
		return -EFAULT;

	written = 0;

	while (count > 0) {
		/*
		 * Handle first page in case it's not aligned
		 */
		if (-p & (PAGE_SIZE - 1))
			sz = -p & (PAGE_SIZE - 1);
		else
			sz = PAGE_SIZE;

		sz = min_t(unsigned long, sz, count);

		/*
		 * On ia64 if a page has been mapped somewhere as
		 * uncached, then it must also be accessed uncached
		 * by the kernel or data corruption may occur
		 */
		ptr = xlate_dev_mem_ptr(p);

		copied = copy_from_user(ptr, buf, sz);
		if (copied) {
			written += sz - copied;
			if (written)
				break;
			return -EFAULT;
		}
		buf += sz;
		p += sz;
		count -= sz;
		written += sz;
	}

	*ppos += written;
	return written;
}

static loff_t khwtest_lseek(struct file * file, loff_t offset, int orig)
{
	switch (orig) {
	case 0:
		file->f_pos = offset;
		break;
	case 1:
		file->f_pos += offset;
		break;
	default:
		return -EINVAL;
	}
	return file->f_pos;
}

struct file_operations khwtest_fops = {
	owner: THIS_MODULE,
	open: khwtest_open,
	release: khwtest_release,
	ioctl: khwtest_ioctl,
	write: khwtest_write,
	llseek: khwtest_lseek,
	read: khwtest_read,
};

static struct miscdevice khwtest_dev = {
	MISC_DYNAMIC_MINOR,
	"khwtest",
	&khwtest_fops,
};

static int __init
khwtest_init(void)
{
	int res = 0;

	res = misc_register(&khwtest_dev);
	if (res) {
		printk(KERN_ERR "%s: Failed call to misc_register.\n", THIS_MODULE->name);
		return -EFAULT;
	}
	printk(KERN_WARNING "%s: This module is completely unsafe and " \
	       "should only be used for hardware testing and bring up.\n",
	       THIS_MODULE->name);
	return 0;
}

static void __exit
khwtest_exit(void)
{
	misc_deregister(&khwtest_dev);
}

module_param(debug, int, 0644);
module_init(khwtest_init);
module_exit(khwtest_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("sruffell@digium.com");
