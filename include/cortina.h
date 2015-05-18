/*
 * Copyright 2011 Freescale Semiconductor, Inc.
 *	Andy Fleming <afleming@freescale.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * This file pretty much stolen from Linux's mii.h/ethtool.h/phy.h
 */

#ifndef _CORTINA_H
#define _CORTINA_H

#include <linux/list.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/mdio.h>

int cortina_phy_init(void);

/**
 * Connect a new PHY to an Ethernet device
 *
 * @param bus		MDIO bus
 * @param addr		MDIO address of PHY
 * @param sub_addr	Sub-address used for some Cortina PHYs (i.e. cs4223 XFI)
 * @param dev		Ethernet device
 * @param interface	PHY interface type
 *
 * @return pointer to new PHY device or NULL if there is an error
 */
struct phy_device *cortina_phy_connect(struct mii_dev *bus,
				       int addr, char sub_addr,
				       struct eth_device *dev,
				       phy_interface_t interface);
int phy_startup(struct phy_device *phydev);
int phy_config(struct phy_device *phydev);
int phy_shutdown(struct phy_device *phydev);
int cortina_phy_register(struct phy_driver *drv);

#endif
