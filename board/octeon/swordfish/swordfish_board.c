/*
 * (C) Copyright 2004-2014 Cavium, Inc. <support@cavium.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <i2c.h>
#include <asm/gpio.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/octeon_fdt.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-mdio.h>
#include <asm/arch/cvmx-helper-jtag.h>
#include <asm/arch/cvmx-helper.h>
#include <asm/arch/cvmx-helper-board.h>
#include <asm/arch/octeon_eth.h>
#include <asm/arch/cvmx-qlm.h>
#include <usb.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * Modify the device tree to remove all unused interface types.
 */
int board_fixup_fdt(void)
{
	const char *fdt_key;
	cvmx_mio_qlmx_cfg_t mio_qlmx;
	int dtt_addr = CONFIG_SYS_I2C_DTT_ADDR;
	int offset = -1;

	debug("%s ENTER\n", __func__);
	mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(0));

	if (mio_qlmx.s.qlm_spd == 15) {
		fdt_key = "0,none";	/* Disabled */
	} else if (mio_qlmx.s.qlm_cfg == 7) {
		fdt_key = "0,rxaui";
		debug("QLM 0 in RXAUI mode\n");
	} else if (mio_qlmx.s.qlm_cfg == 2) {
		fdt_key = "0,sgmii";
		debug("QLM 0 in SGMII mode\n");
	} else {
		debug("Unsupported QLM 0 configuration %d\n",
		      mio_qlmx.s.qlm_cfg);
		fdt_key = "0,none";	/* Disabled */
	}
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(3));
	if (mio_qlmx.s.qlm_spd == 15) {
		fdt_key = "3,none";	/* Disabled */
	} else if (mio_qlmx.s.qlm_cfg == 3) {
		debug("QLM 3 in XAUI mode\n");
		fdt_key = "3,xaui";
	} else if (mio_qlmx.s.qlm_cfg == 2) {
		fdt_key = "3,sgmii";
		debug("QLM 3 in SGMII mode\n");
	} else {
		debug("Unsupported QLM 3 configuration %d\n",
		      mio_qlmx.s.qlm_cfg);
		fdt_key = "3,none";
	}
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(4));
	if (mio_qlmx.s.qlm_spd == 15) {
		fdt_key = "4,none";	/* Disabled */
	} else if (mio_qlmx.s.qlm_cfg == 3) {
		fdt_key = "4,xaui";
		debug("QLM 4 in XAUI mode\n");
	} else if (mio_qlmx.s.qlm_cfg == 2) {
		fdt_key = "4,sgmii";
		debug("QLM 4 in SGMII mode\n");
	} else {
		debug("Unsupported QLM 4 configuration %d\n",
		      mio_qlmx.s.qlm_cfg);
		fdt_key = "4,none";
	}
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	if (i2c_reg_read(dtt_addr, 0xfe) != 0xa1) {
		dtt_addr += 2;
		if (i2c_reg_read(dtt_addr, 0xfe) != 0xa1) {
			puts("Warning: could not communicate with SA56004x temperature sensor\n");
		} else {
			debug("%s: Updating device tree temperature sensor address to 0x%x\n",
			      __func__, dtt_addr);
			offset = fdt_node_offset_by_compatible(gd->fdt_blob,
							       -1,
					  "nxp,saa56004x");
			if (offset < 0) {
				puts("Error: could not find NXP SAA56004X temperature sensor in device tree!\n");
			} else {
				char name[64];
				snprintf(name, sizeof(name), "tmp@%x", dtt_addr);
				dtt_addr = cpu_to_fdt32(dtt_addr);
				fdt_setprop(gd->fdt_blob, offset, "reg",
					    &dtt_addr, sizeof(dtt_addr));
				fdt_set_name(gd->fdt_blob, offset, name);
			}
		}
	}

	debug("%s EXIT\n", __func__);
	return 0;
}

int checkboard(void)
{
	cvmx_mio_qlmx_cfg_t mio_qlmx;

	debug("%s: ENTER\n", __func__);
	if (gd->arch.board_desc.board_type == CVMX_BOARD_TYPE_SWORDFISH) {
		mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(0));

		printf("NIC SFP+ ports configured for %s\n",
		       mio_qlmx.s.qlm_cfg == 7 ? "XAUI" : "SGMII");
	}

	/* Put the Vitesse PHYs in reset */
	gpio_direction_output(15, 0);
	/* Wait 10ms */
	mdelay(10);
	/* Take Vitesse PHYs out of reset */
	gpio_set_value(15, 1);
	gpio_set_value(8, 1);
#ifdef DEBUG
	debug("QLM 0 reference clock: %d\n"
	      "QLM 3 reference clock: %d\n"
	      "QLM 4 reference clock: %d\n",
	      cvmx_qlm_measure_clock(0),
	      cvmx_qlm_measure_clock(3),
	      cvmx_qlm_measure_clock(4));
#endif

	return 0;
}

int early_board_init(void)
{
	cvmx_mio_fus_pll_t fus_pll;
	int dtt_addr = CONFIG_SYS_I2C_DTT_ADDR;

	octeon_board_get_clock_info(SWORDFISH_DEF_DRAM_FREQ);

	octeon_board_get_descriptor(CVMX_BOARD_TYPE_SWORDFISH, 1, 0);

	octeon_board_get_mac_addr();

	/* Allow us to change the critical temperature */
	gpio_direction_output(0, 1);

	/* configure clk_out pin */
	fus_pll.u64 = cvmx_read_csr(CVMX_MIO_FUS_PLL);
	fus_pll.cn68xx.c_cout_rst = 1;
	cvmx_write_csr(CVMX_MIO_FUS_PLL, fus_pll.u64);

	cvmx_wait_io(10);
	/* Sel::  0:rclk, 1:pllout 2:psout 3:gnd */
	fus_pll.cn68xx.c_cout_sel = 0;
	cvmx_write_csr(CVMX_MIO_FUS_PLL, fus_pll.u64);
	cvmx_wait_io(10);
	fus_pll.cn68xx.c_cout_rst = 0;
	cvmx_write_csr(CVMX_MIO_FUS_PLL, fus_pll.u64);

	/* CN68XX has a fixed 50 MHz reference clock */
	gd->arch.ddr_ref_hertz = 50000000;

	if (i2c_reg_read(dtt_addr, 0xfe) != 0xa1) {
		dtt_addr += 2;
		if (i2c_reg_read(dtt_addr, 0xfe) != 0xa1) {
			puts("Warning: could not communicate with SA56004x temperature sensor\n");
		}
	}
	/* Note that the SA56004 will be programmed again later with DTT */
	/* Internal SA56004 temp, approximates ambient/board temp. */
	i2c_reg_write(dtt_addr, 0x20, 80);

	/* Internal Octeon temp, approximates junction temp. */
	i2c_reg_write(dtt_addr, 0x19, 100);

	/* Take I2C mux out of reset */
	gpio_direction_output(3, 1);
	/* Reset USB SD/MMC flash controller */
	gpio_direction_output(8, 1);
	gpio_direction_output(10, 0);
	/* Put eMMC in reset */
	gpio_set_value(8, 0);

	return 0;
}

extern void octeon_board_vcs8488_qlm_tune(int qlm);
extern void dtt_set_addr(unsigned dev, int8_t addr);

int early_board_init_r(void)
{
	/* Move temperature sensor if needed */
	if (i2c_reg_read(CONFIG_SYS_I2C_DTT_ADDR, 0xfe) != 0xa1) {
		dtt_set_addr(0, CONFIG_SYS_I2C_DTT_ADDR + 2);
		debug("Changed DTT address from %d to %d\n",
		      CONFIG_SYS_I2C_DTT_ADDR, CONFIG_SYS_I2C_DTT_ADDR + 2);
	}
	return 0;
}

int late_board_init(void)
{
	octeon_board_vcs8488_qlm_tune(0);
	octeon_board_vcs8488_qlm_tune(3);
	octeon_board_vcs8488_qlm_tune(4);
}

void board_usb_postinit(void)
{
	debug("Doing USB postinit\n");
	/* Take eMMC out of reset */
	gpio_set_value(8, 1);
	/* Take USB SD/MMC controller out of reset */
	gpio_set_value(10, 1);
	mdelay(500);
	usb_stor_scan(1);
}

/**
 * Reset the PHY.
 *
 * NOTE: that we have to replace the normal phy_reset function because even
 * in SGMII mode we use clause 45.
 *
 * @param phydev PHY device
 *
 * @return 0 for success, -1 on error
 */
int phy_reset(struct phy_device *phydev)
{
	int reg;
	int devad;
	int timeout = 500;

	if (!phydev->mmds)
		gen10g_discover_mmds(phydev);
	devad = ffs(phydev->mmds) - 1;

	reg = phy_read(phydev, devad, MII_BMCR);
	if (reg < 0)
		return -1;

	reg |= BMCR_RESET;

	if (phy_write(phydev, devad, MII_BMCR, reg) < 0)
		return -1;

	do {
		reg = phy_read(phydev, devad, MII_BMCR);
		if (reg < 0)
			return -1;
		mdelay(1);
	} while ((reg & BMCR_RESET) && timeout--);

	if (reg & BMCR_RESET) {
		printf("PHY reset timed out for %s (devad: %d, reg: 0x%x)\n",
		       phydev->drv->name, devad, reg);
		return -1;
	}
	return 0;
}
