/* MN10300 PCI definitions
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_PCI_H
#define _ASM_PCI_H

#ifdef __KERNEL__
#include <linux/mm.h>		/* for struct page */

#if 0
#define __pcbdebug(FMT, ADDR, ...) \
	printk(KERN_DEBUG "PCIBRIDGE[%08x]: "FMT"\n", \
	       (u32)(ADDR), ##__VA_ARGS__)

#define __pcidebug(FMT, BUS, DEVFN, WHERE,...)		\
do {							\
	printk(KERN_DEBUG "PCI[%02x:%02x.%x + %02x]: "FMT"\n",	\
	       (BUS)->number,					\
	       PCI_SLOT(DEVFN),					\
	       PCI_FUNC(DEVFN),					\
	       (u32)(WHERE), ##__VA_ARGS__);			\
} while (0)

#else
#define __pcbdebug(FMT, ADDR, ...)		((void)0)
#define __pcidebug(FMT, BUS, DEVFN, WHERE, ...)	((void)0)
#endif

/* Can be used to override the logic in pci_scan_bus for skipping
 * already-configured bus numbers - to be used for buggy BIOSes or
 * architectures with incomplete PCI setup by the loader */

#ifdef CONFIG_PCI
#define pcibios_assign_all_busses()	1
extern void unit_pci_init(void);
#else
#define pcibios_assign_all_busses()	0
#endif

#define PCIBIOS_MIN_IO		0xBE000004
#define PCIBIOS_MIN_MEM		0xB8000000

void pcibios_set_master(struct pci_dev *dev);

/* Dynamic DMA mapping stuff.
 * i386 has everything mapped statically.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>

struct pci_dev;

/* The PCI address space does equal the physical memory
 * address space.  The networking and block device layers use
 * this boolean for bounce buffer decisions.
 */
#define PCI_DMA_BUS_IS_PHYS	(1)

/* Return the index of the PCI controller for device. */
static inline int pci_controller_num(struct pci_dev *dev)
{
	return 0;
}

#define HAVE_PCI_MMAP
#define ARCH_GENERIC_PCI_MMAP_RESOURCE

#endif /* __KERNEL__ */

static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	return channel ? 15 : 14;
}

#include <asm-generic/pci_iomap.h>

#endif /* _ASM_PCI_H */
