/*
 * Cortina PHY Management code
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Copyright 2011 Freescale Semiconductor, Inc.
 * author Andy Fleming
 *
 * Copyright 2014 Caviun, Inc. <support@cavium.com>
 * author Aaron Williams
 *
 * Based off of the generic PHY management code
 *
 * The Cortina PHYs differs from other phys in that they do not follow any of
 * the usual PHY standards.  The Cortina PHYs use devid 0 and the chip IDs are
 * stored in registers 0 and 1 instead of 2 and 3.
 *
 */

#include <common.h>
#include <malloc.h>
#include <net.h>
#include <command.h>
#include <miiphy.h>
#include <phy.h>
#include <cortina.h>
#include <errno.h>
#include <linux/err.h>
#ifdef CONFIG_PHY_CORTINA_CS4321
# include <cortina_cs4321.h>
#endif
/**
 * LSB of IEEE 1149.1 ID value.  Bits 15-12 are the LSB 4 bits of the 16-bit
 * product ID, 11-1 are the Cortina manufacturer ID, 0x1f2.  Bit 0 is always 0.
 */
#define CORTINA_GLOBAL_CHIP_ID_LSB	0
/**
 * MSB of IEEE 1149.1 ID value.
 */
#define CORTINA_GLOBAL_CHIP_ID_MSB	1

#define CS4224_PP_LINE_LINEMISC_SOFT_RESET		0x1000
#define CS4224_PP_LINE_LINEMISC_MPIF_RESET_DOTREG	0x1001
#define CS4224_PP_LINE_LINEMISC_GIGEPCS_SOFT_RESET	0x1002
#define CS4224_PP_HOST_HOSTMISC_SOFT_RESET		0x1800
#define CS4224_PP_HOST_HOSTMISC_MPIF_RESET_DOTREG	0x1801
#define CS4224_PP_HOST_HOSTMISC_GIGEPCS_SOFT_RESET	0x1802
#define CS4224_EEPROM_LOADER_CONTROL			0x5000
#define CS4224_EEPROM_LOADER_STATUS			0x5001

/** Private data for Cortina PHYs */
struct cortina_phy_info {
	/**
	 * Since the MDIO arrays of PHY devices won't work when there are
	 * sub-addresses we use a linked list.
	 */
	struct list_head list;

	/**
	 * PHY device we belong to (for list processing)
	 */
	struct phy_device *phydev;

	/**
	 * Some Cortina PHYs such as the CS4223 require sub addresses for
	 * each slice when running in XFI mode.  In this case, each Ethernet
	 * interface talks to the same PHY address on the MDIO bus but uses
	 * different register offsets.  This is true when there are multiple
	 * sub-addresses used.  In XLAUI mode sub addresses are not used.
	 */
	bool sub_addr_used;

	/**
	 * Sub-address of this interface, -1 if not used.
	 */
	uint8_t	sub_addr;
};

static LIST_HEAD(cortina_phy_drivers);

/**
 * Register a Cortina PHY driver.
 *
 * @param drv	Driver to register
 *
 * @return 0 for success
 */
int cortina_phy_register(struct phy_driver *drv)
{
	debug("%s(%s)\n", __func__, drv->name);
	INIT_LIST_HEAD(&drv->list);
	list_add_tail(&drv->list, &cortina_phy_drivers);

	return 0;
}

int cortina_phy_reset(struct phy_device *phydev)
{
	debug("%s(%s)\n", __func__, phydev->dev->name);
	return 0;
}

/**
 * get_phy_id - reads the specified addr for its ID.
 * @bus: the target MII bus
 * @addr: PHY address on the MII bus
 * @phy_id: where to store the ID retrieved.
 *
 * Description: Reads the ID registers of the PHY at @addr on the
 *   @bus, stores it in @phy_id and returns zero on success.
 */
static int cortina_get_phy_id(struct mii_dev *bus, int addr, uint32_t *phy_id)
{
	int phy_reg;

	/* The Cortina PHY stores the PHY id at addresses 0 and 1 */
	phy_reg = bus->read(bus, addr, 0, CORTINA_GLOBAL_CHIP_ID_MSB);
	if (phy_reg < 0)
		return -EIO;

	*phy_id = (phy_reg & 0xffff) << 16;
	phy_reg = bus->read(bus, addr, 0, CORTINA_GLOBAL_CHIP_ID_LSB);
	if (phy_reg < 0)
		return -EIO;
	*phy_id |= (phy_reg & 0xffff);

	debug("%s(%p, %d, %p): phy id: 0x%x\n", __func__, bus, addr,
	      phy_id, *phy_id);
	return 0;
}

static struct phy_driver *cortina_get_phy_driver(struct phy_device *phydev,
						 phy_interface_t interface)
{
	struct list_head *entry;
	int phy_id = phydev->phy_id;
	struct phy_driver *drv = NULL;

	debug("%s: Looking for UID: 0x%x\n", __func__, phy_id);
	list_for_each(entry, &cortina_phy_drivers) {
		drv = list_entry(entry, struct phy_driver, list);
		debug("  Checking driver %s, UID: 0x%x\n", drv->name, drv->uid);
		if ((drv->uid & drv->mask) == (phy_id & drv->mask))
			return drv;
	}
	return NULL;
}

static int cortina_phy_probe(struct phy_device *phydev)
{
	int err = 0;

	debug("%s(%s)\n", __func__, phydev->dev->name);
	phydev->advertising = phydev->supported = phydev->drv->features;

	if (phydev->drv->probe)
		err = phydev->drv->probe(phydev);

	return err;
}

/**
 * Create a new Cortina PHY device
 *
 * @param bus		MDIO bus
 * @param addr		MDIO address
 * @param sub_addr	Cortina sub-address used in some cases with Cortina
 *			PHYs such as the CS4223 in XFI mode.
 * @param phy_id	PHY ID reported by the PHY
 * @param interface	Interface type.  Note that this is currently useless
 *			for XLAUI and XFI modes.
 *
 * @return pointer to a new initialized PHY device or NULL if error
 */
struct phy_device *cortina_phy_device_create(struct mii_dev *bus,
					     int addr, char sub_addr,
					     u32 phy_id,
					     phy_interface_t interface)
{
	struct phy_device *dev;
	struct cortina_phy_info *phy_info;

	debug("%s(%p, %d, %d, 0x%x, %u)\n", __func__, bus, addr, sub_addr,
	      phy_id, interface);
	/* We allocate the device and initialize the default values
	 * We also kill two birds with one stone, allocating the private data
	 * immediately after the phy device.
	 */

	dev = calloc(sizeof(*dev) + sizeof(*phy_info), 1);
	if (!dev) {
		printf("Failed to allocate PHY device for %s:%d\n",
		       bus->name, addr);
		return NULL;
	}

	phy_info = (struct cortina_phy_info *)(dev + 1);
	debug("%s: dev: %p, phy_info: %p\n", __func__, dev, phy_info);
	dev->priv = phy_info;
	dev->duplex = -1;
	dev->link = 1;
	dev->interface = interface;
	dev->autoneg = AUTONEG_ENABLE;
	dev->addr = addr;
	dev->phy_id = phy_id;
	dev->bus = bus;
	debug("%s: Getting PHY driver\n", __func__);
	dev->drv = cortina_get_phy_driver(dev, interface);

	if (!dev->drv) {
		printf("PHY driver not found for Cortina PHY on %s, address %u:%d\n",
		       bus->name, addr, sub_addr);
		return NULL;
	}
	debug("%s: phy driver: %s\n", __func__, dev->drv->name);

	debug("Probing PHY driver\n");
	cortina_phy_probe(dev);

	INIT_LIST_HEAD(&phy_info->list);
	phy_info->phydev = dev;
	phy_info->sub_addr_used = sub_addr >= 0;
	phy_info->sub_addr = sub_addr;

	/* If there's already an entry here, add ourselves to the end of the
	 * list.
	 */
	if (bus->phymap[addr]) {
		struct cortina_phy_info *pi = bus->phymap[addr]->priv;

		if (!pi || !pi->sub_addr_used) {
			printf("Error: Cortina PHY information missing or sub "
			       "address not supported from existing entry at "
			       "address %d:%d\n",
			       addr, sub_addr);
			free(dev);
			return NULL;
		}
		debug("Adding phy driver to tail\n");
		list_add_tail(&pi->list, &phy_info->list);
	} else {
		bus->phymap[addr] = dev;
	}


	return dev;
}

static struct phy_device *cortina_create_phy_by_addr(struct mii_dev *bus,
				unsigned addr, char sub_addr,
				phy_interface_t interface)
{
	u32 phy_id = 0xffffffff;
	int r = cortina_get_phy_id(bus, addr, &phy_id);

	if (r < 0) {
		debug("%s(%s, 0x%x.%d, %d): could not get PHY id\n", __func__,
		      bus->name, addr, sub_addr);
		return ERR_PTR(r);
	}

	/* If the PHY ID is mostly F's, we didn't find anything */
	if ((phy_id & 0x1fffffff) != 0x1fffffff) {
		debug("%s: addr: %u:%d, creating device\n", __func__,
		      addr, sub_addr);
		return cortina_phy_device_create(bus, addr, sub_addr,
						 phy_id,
						 interface);
	}
	return NULL;
}

static struct phy_device *cortina_search_for_existing_phy(struct mii_dev *bus,
							  unsigned phy_addr,
							  char sub_addr,
							  phy_interface_t interface)
{
	struct phy_device *phydev = bus->phymap[phy_addr];
	struct cortina_phy_info *phy_info = phydev->priv;
	struct list_head *entry;

	if (!phydev || !phydev->priv)
		return NULL;

	if (sub_addr >= 0 && !phy_info->sub_addr_used) {
		printf("%s(%p, %u, %d, %u): found entry but sub address is not used!\n",
		       __func__, bus, phy_addr, sub_addr, interface);
		return NULL;
	}

	if (phy_info->sub_addr_used) {
		if (phy_info->sub_addr_used && phy_info->sub_addr == sub_addr) {
			debug("%s: Found match, addr: %u:%d\n", __func__,
			      phy_addr, sub_addr);
			return phydev;
		}

		list_for_each(entry, &(phy_info->list)) {
			struct cortina_phy_info *pi;
			pi = list_entry(entry, struct cortina_phy_info, list);
			debug("%s: entry: %p, info: %p\n", __func__, entry,
			      pi);
			if (pi->sub_addr_used &&
			    pi->sub_addr == sub_addr) {
				phydev = pi->phydev;
				phydev->interface = interface;
				debug("%s: Found address %u, sub address %d\n",
				      __func__, phy_addr, sub_addr);
				return phydev;
			}
		}
	} else {
		debug("%s: Found match, addr: %u\n", __func__, phy_addr);
		phydev->interface = interface;
		return phydev;
	}
	return NULL;
}

static struct phy_device *cortina_get_phy_device_by_addr(struct mii_dev *bus,
							 unsigned phy_addr,
							 char sub_addr,
							 phy_interface_t interface)
{
	struct phy_device *phydev;

	phydev = cortina_search_for_existing_phy(bus, phy_addr, sub_addr,
						 interface);
	if (phydev)
		return phydev;

	debug("%s: Creating new phy, addr: %u:%d\n", __func__,
	      phy_addr, sub_addr);
	phydev = cortina_create_phy_by_addr(bus, phy_addr, sub_addr, interface);
	if (IS_ERR(phydev))
		return NULL;
	if (phydev)
		return phydev;
	puts("Phy not found\n");
	return cortina_phy_device_create(bus, phy_addr, sub_addr, 0xffffffff,
					 interface);
}

/**
 * Find a Cortina PHY given its address and sub-address
 *
 * @param bus		MDIO bus PHY is connected to
 * @param phy_addr	Address of PHY on MDIO bus
 * @param sub_addr	Sub-address of PHY (i.e. cs4223 in XFI mode)
 * @param interface	Interface mode
 *
 * @return pointer to phy device
 */
struct phy_device *cortina_phy_find_by_addr(struct mii_dev *bus,
					    unsigned phy_addr,
					    char sub_addr,
					    phy_interface_t interface)
{
	if (bus->reset)
		bus->reset(bus);

	/* Wait 15ms to make sure the PHY has come out of hard reset */
	mdelay(15);
	return cortina_get_phy_device_by_addr(bus, phy_addr, sub_addr,
					      interface);
}

/**
 * Connect a PHY device to an Ethernet device
 *
 * @param phydev	PHY device
 * @param dev		Ethernet device
 */
void cortina_phy_connect_dev(struct phy_device *phydev, struct eth_device *dev)
{
	/* Configure the PHY */
	phy_config(phydev);
	/* Soft reset the PHY */
	cortina_phy_reset(phydev);

	if (phydev->dev)
		printf("%s:%d is connected to %s.  Reconnecting to %s\n",
		       phydev->bus->name, phydev->addr,
		       phydev->dev->name, dev->name);
	phydev->dev = dev;
	debug("%s connected to %s\n", dev->name, phydev->drv->name);
}

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
				       phy_interface_t interface)
{
	struct phy_device *phydev;

	phydev = cortina_phy_find_by_addr(bus, addr, sub_addr, interface);
	if (phydev)
		cortina_phy_connect_dev(phydev, dev);
	else
		printf("Could not get PHY for %s: addr %d\n", bus->name, addr);
	return phydev;
}

/**
 * Probe for a Cortina CS4223 PHY device
 *
 * @param phydev	Pointer to phy device data structure
 *
 * @return		0 if probe was successful, error otherwise.
 */
static int cs4223_probe(struct phy_device *phydev)
{
	int id_lsb, id_msb;

	id_lsb = phy_read(phydev, 0, CORTINA_GLOBAL_CHIP_ID_LSB);
	if (id_lsb < 0)
		return id_lsb;
	id_msb = phy_read(phydev, 0, CORTINA_GLOBAL_CHIP_ID_MSB);
	if (id_msb < 0)
		return -1;

	debug("%s(%u): ID: 0x%x\n", __func__, phydev->addr,
	      (id_msb << 16) | id_lsb);
	if (id_lsb != 0x3e5 || id_msb != 0x3003)
		return -ENODEV;
	return 0;
}

static int cs4223_config(struct phy_device *phydev)
{
	/* This PHY supports both 10G and 40G and can run as XLAUI or XFI.
	 * Currently the U-Boot infrastructure (and Linux for that matter)
	 * doesn't really provide a way to detect these higher speed interfaces.
	 */
	phydev->supported = phydev->advertising =
		SUPPORTED_40000baseKR4_Full |
		ADVERTISED_40000baseKR4_Full;
	phydev->duplex = DUPLEX_FULL;
	phydev->speed = 10000;
	return 0;
}

/**
 * Resets the PHY
 *
 * @param phydev	Phy device to reset
 *
 * @return 0 for success.
 */
static int cs4223_reset(struct phy_device *phydev)
{
	u16 reg;
	u16 value;
	ulong start;

	struct cortina_phy_info *phy_info = phydev->priv;

	/* First check that the EEPROM has been properly loaded */
	value = phy_read(phydev, 0, CS4224_EEPROM_LOADER_STATUS);
	if (value & 7 != 1) {
		/* Start downloading the firmware */
		reg = CS4224_EEPROM_LOADER_CONTROL;
		phy_write(phydev, 0, reg, 1);

		reg = CS4224_EEPROM_LOADER_STATUS;
		start = get_timer(0);
		do {
			value = phy_read(phydev, 0, reg);
		} while ((get_timer(start) < 500) && !(value & 1));
		if (value & 7) {
			printf("Error: %s firmware download timeout\n",
			       phydev->dev->name);
			return -1;
		}
	}

	/* Reset both the host and line sides */
	reg = CS4224_PP_HOST_HOSTMISC_SOFT_RESET;
	value = 0xb;
	phy_write(phydev, 0, reg, value);
	phy_write(phydev, 0, reg + 1000, value);
	phy_write(phydev, 0, reg + 2000, value);
	phy_write(phydev, 0, reg + 3000, value);

	reg = CS4224_PP_LINE_LINEMISC_SOFT_RESET;
	value = 0xb;
	phy_write(phydev, 0, reg, value);
	phy_write(phydev, 0, reg + 1000, value);
	phy_write(phydev, 0, reg + 2000, value);
	phy_write(phydev, 0, reg + 3000, value);

	mdelay(100);

	reg = CS4224_PP_HOST_HOSTMISC_SOFT_RESET;
	value = 0;
	phy_write(phydev, 0, reg, value);
	phy_write(phydev, 0, reg + 1000, value);
	phy_write(phydev, 0, reg + 2000, value);
	phy_write(phydev, 0, reg + 3000, value);

	reg = CS4224_PP_LINE_LINEMISC_SOFT_RESET;
	value = 0;
	phy_write(phydev, 0, reg, value);
	phy_write(phydev, 0, reg + 1000, value);
	phy_write(phydev, 0, reg + 2000, value);
	phy_write(phydev, 0, reg + 3000, value);

	return 0;
}

static struct phy_driver cortina_cs4223 = {
	.uid = 0x300303e5,
	.mask = 0x0fffffff,
	.name = "Cortina CS4223",
	.features = PHY_10G_FEATURES,
	.config = cs4223_config,
	.startup = NULL,
	.shutdown = NULL,
	.probe = cs4223_probe,
	.reset = cs4223_reset,
};

int cortina_phy_init(void)
{
	int rc = 0;
#ifdef CONFIG_PHY_CORTINA_CS4321
	rc |= cortina_cs4321_phy_init();
#endif
	cortina_phy_register(&cortina_cs4223);

	return rc;
}
