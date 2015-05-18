/*
 * (C) Copyright 2006-2013
 * Cavium Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/**
 * @file octeon_pci.h
 *
 * $Id$
 *
 */
#ifndef __OCTEON_PCI_H__
#define __OCTEON_PCI_H__

/* Give lots of space for U-Boot since it's not as effecient at allocating
 * space as Linux is.
 */
#define OCTEON_PCI_SLOT0_BAR_ADDR	0xE0000000
#define OCTEON_PCI_SLOT0_BAR_SIZE	0x08000000
#define OCTEON_PCI_SLOT1_BAR_ADDR	0xE8000000
#define OCTEON_PCI_SLOT1_BAR_SIZE	0x08000000
#define OCTEON_PCI_SLOT2_BAR_ADDR	0xF0000000
#define OCTEON_PCI_SLOT2_BAR_SIZE	0x08000000
#define OCTEON_PCI_SLOT3_BAR_ADDR	0xF8000000
#define OCTEON_PCI_SLOT3_BAR_SIZE	0x08000000
#define OCTEON_PCI_TOTAL_BAR_SIZE	(OCTEON_PCI_SLOT0_BAR_SIZE + \
					 OCTEON_PCI_SLOT1_BAR_SIZE + \
					 OCTEON_PCI_SLOT2_BAR_SIZE + \
					 OCTEON_PCI_SLOT3_BAR_SIZE)
void pci_dev_post_init(void);


int octeon_pci_io_readb (unsigned int reg);
void octeon_pci_io_writeb (int value, unsigned int reg);
int octeon_pci_io_readw (unsigned int reg);
void octeon_pci_io_writew (int value, unsigned int reg);
int octeon_pci_io_readl (unsigned int reg);
void octeon_pci_io_writel (int value, unsigned int reg);
int octeon_pci_mem1_readb (unsigned int reg);
void octeon_pci_mem1_writeb (int value, unsigned int reg);
int octeon_pci_mem1_readw (unsigned int reg);
void octeon_pci_mem1_writew (int value, unsigned int reg);
int octeon_pci_mem1_readl (unsigned int reg);
void octeon_pci_mem1_writel (int value, unsigned int reg);



/* In the TLB mapped case, these also work with virtual addresses,
** and do the required virt<->phys translations as well. */
uint32_t octeon_pci_phys_to_bus(uint32_t phys);
uint32_t octeon_pci_bus_to_phys(uint32_t bus);

#endif /* __OCTEON_PCI_H__ */
