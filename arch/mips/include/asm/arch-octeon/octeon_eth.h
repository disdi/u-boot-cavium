/***********************license start************************************
 * Copyright (c) 2005-2012 Cavium, Inc. (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Cavium Inc. nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM INC. MAKES NO PROMISES, REPRESENTATIONS
 * OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 * RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 * REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 * DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 * PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 * POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 * OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 * For any questions regarding licensing please contact
 * marketing@cavium.com
 *
 ***********************license end**************************************/
#ifndef __OCTEON_ETH_H__
#define __OCTEON_ETH_H__

#include <asm/u-boot.h>
#include <phy.h>
#include <miiphy.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-helper.h>
#include <asm/arch/cvmx-helper-board.h>

struct eth_device;

/** Ethernet device private data structure for octeon ethernet */
struct octeon_eth_info {
	uint64_t	link_state;
	uint32_t	port;		/** ipd port */
	uint32_t	interface;	/** Port interface */
	uint32_t	index;		/** port index on interface */
	uint32_t	initted_flag;	/** 0 if port not initialized */
	struct mii_dev *mii_bus;	/** MII bus for PHY */
	struct phy_device *phydev;	/** PHY device */
	struct eth_device *ethdev;	/** Eth device this priv is part of */
	int		mii_addr;
	int		phy_fdt_offset;	/** Offset of PHY info in device tree */
	int		fdt_offset;	/** Offset of Eth interface in DT */
	enum cvmx_phy_type phy_device_type;	/** Type of PHY */
	/* current link status, use to reconfigure on status changes */
	uint64_t	packets_sent;
	uint64_t	packets_received;
	uint32_t	link_speed:2;
	uint32_t	link_duplex:1;
	uint32_t	link_status:1;
	uint32_t	loopback:1;
	uint32_t	enabled:1;
	uint32_t	is_c45:1;	/** Set if we need to use clause 45 */
	uint32_t	vitesse_sfp_config:1;	/** Need Vitesse SFP config */
	uint32_t	ti_gpio_config:1;	/** Need TI GPIO configuration */
	uint64_t	gmx_base;	/** Base address to access GMX CSRs */
};

/**
 * Searches for an ethernet device based on interface and index.
 *
 * @param interface - interface number to search for
 * @param index - index to search for
 *
 * @returns pointer to ethernet device or NULL if not found.
 */
struct eth_device *octeon_find_eth_by_interface_index(int interface, int index);

int octeon_eth_initialize (bd_t *);

/**
 * User-defined function called when the link state changes
 *
 * @param[in]	dev		Ethernet device
 * @param 	link_state	new link state
 *
 * NOTE: This is defined as a weak function.
 */
void board_net_set_link(struct eth_device *dev,
			cvmx_helper_link_info_t link_state);

#endif /* __OCTEON_ETH_H__ */
