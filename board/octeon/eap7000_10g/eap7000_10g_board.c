/*
 * (C) Copyright 2014 Cavium, Inc. <support@cavium.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/mipsregs.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/octeon_fdt.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/cvmx-helper-jtag.h>
#include <asm/gpio.h>
#include <asm/arch/cvmx-pcie.h>
#include <asm/arch/cvmx-qlm.h>
#include <asm/arch/octeon_qlm.h>
#include <i2c.h>
#include <phy.h>
#include <asm/arch/cvmx-mdio.h>

DECLARE_GLOBAL_DATA_PTR;

int checkboard(void)
{
	/* Take PHYs out of reset */
	gpio_direction_output(9, 1);
	mdelay(10);

	/* We'll configure the QLM later */
	/*octeon_configure_qlm(0, 3125, CVMX_QLM_MODE_RXAUI, 0, 0, 1, 0);*/
	/* Configure QLMs */
	/* qlm, speed, mode, rc, pcie_mode, ref_clk_sel, ref_clk_input */

	/* For some reason this doesn't work if it's configured for PCIe 1x1
	 * mode.
	 */
	octeon_configure_qlm(1, 5000, CVMX_QLM_MODE_PCIE_1X2, 1, 1, 1, 0);
	octeon_configure_qlm(2, 5000, CVMX_QLM_MODE_PCIE_1X2, 1, 1, 1, 0);

	return 0;
}

void board_net_preinit(void)
{
	int val;
	int con_state = -1;
	ulong start;
	bool sgmii = false;
	bool link_up;
	uint64_t sclock;
	union cvmx_smix_clk smix_clk;

	/* Init MDIO early */
	sclock = cvmx_clock_get_rate(CVMX_CLOCK_SCLK);
	smix_clk.u64 = cvmx_read_csr(CVMX_SMIX_CLK(0));
	smix_clk.s.phase = sclock / (2500000 * 2);
	cvmx_write_csr(CVMX_SMIX_CLK(0), smix_clk.u64);

	cvmx_write_csr(CVMX_SMIX_EN(0), 1);
	mdelay(10);

	start = get_timer(0);
	debug("%s: Waiting for link up...\n", __func__);
	do {
		val = cvmx_mdio_45_read(0, 1, 1, 0xe800);
		link_up = !!(val & 1);
	} while (!link_up && get_timer(start) < 10000);

	if (link_up) {
		start = get_timer(0);
		/* If we're in autonegotiation or training retry until it
		 * finishes.
		 */
		do {
			val = cvmx_mdio_45_read(0, 1, 7, 0xc810);
			if (val < 0) {
				printf("%s: Cannot read Aquantia PHY\n",
				       __func__);
				return;
			}
			/* Get the connection state, poll for a while if the
			 * state is in autonegotiation or calibrating
			 */
			con_state = (val >> 9) & 0x1f;
			if (con_state != 2 && con_state != 3 && con_state != 0xa)
				break;
		} while (get_timer(start) < 2000);

		debug("%s: connection state: 0x%x\n", __func__, con_state);
		debug("%s: Autonegotiation standard status 1: 0x%x, link is %s\n",
		      __func__, val, link_up ? "up" : "down");
	} else {
		debug("%s: link is down, enabling SGMII mode\n",
		      __func__);
		sgmii = true;
	}
	if (con_state == 4 && link_up) {
		val = cvmx_mdio_45_read(0, 1, 7, 0xc800);
		debug("%s: Autonegotiation Vendor Status 1: 0x%x\n",
		      __func__, val);
		if (val < 0) {
			printf("%s: Cannot read Aquantia PHY\n", __func__);
			return;
		}

		debug("%s: Connection rate: 0x%x\n", __func__, (val >> 1) & 7);
		switch ((val >> 1) & 0x7) {
		case 0:
			debug("%s: Detected 10Base-T, SGMII mode\n", __func__);
			puts("Detected 10Base-T connection.  This speed is not supported.  Defaulting to 100Base-TX/1000Base-T mode\n");
			sgmii = true;
			break;
		case 1:
			debug("%s: Detected 100Base-TX, SGMII mode\n", __func__);
			sgmii = true;
			break;
		case 2:
			debug("%s: Detected 1000Base-T, SGMII mode\n", __func__);
			sgmii = true;
			break;
		case 3:
			debug("%s: Detected 10GBase-T, RXAUI mode\n", __func__);
			sgmii = false;
			break;
		case 4:
			debug("%s: Detected 2.5G, RXAUI mode\n", __func__);
			sgmii = false;
		break;
		case 5:
			debug("%s: Detected 5G, RXAUI mode\n", __func__);
			sgmii = false;
		break;
		default:
			printf("Unknown connection rate, Autonegotiation Vendor Status 1: 0x%x\n",
			       val);
			sgmii = true;
		}
	}

	if (sgmii) {
		/* Put PHY SERDES into SGMII mode */
		debug("%s: Configuring PHY for SGMII mode\n", __func__);

		/* Force the start-up mode to always be SGMII with automatic
		 * rate and SGMII autonegotiation enabled.
		 */
		val = cvmx_mdio_45_read(0, 1, 7, 0xc410);
		val &= ~0xe000;
		val |= 0x4000;
		cvmx_mdio_45_write(0, 1, 7, 0xc410, val);
		octeon_configure_qlm(0, 2500, CVMX_QLM_MODE_SGMII_DISABLED,
				     0, 0, 1, 0);
	} else {
		debug("PHY in RXAUI mode\n");
		octeon_configure_qlm(0, 6250, CVMX_QLM_MODE_RXAUI,
				     0, 0, 1, 0);
	}
}

int early_board_init(void)
{
	debug("%s: ENTER\n", __func__);
	/* Enable USB power */
	gpio_direction_output(1, 1);

	/* Put the PHYs in reset */
	gpio_direction_output(9, 0);

	/* Wait at least 10ms */
	mdelay(11);

	/* Enable PCIe wireless cards */
	gpio_direction_output(5, 1);
	gpio_direction_output(12, 1);

	/* Populate global data from eeprom */
	octeon_board_get_clock_info(EAP7000_10G_DEF_DRAM_FREQ);

	octeon_board_get_descriptor(CVMX_BOARD_TYPE_EAP7000_10G,
				    CONFIG_OCTEON_EAP7000_10G_MAJOR,
				    CONFIG_OCTEON_EAP7000_10G_MINOR);


	/* CN63XX has a fixed 50 MHz reference clock */
	gd->arch.ddr_ref_hertz = DEFAULT_CPU_REF_FREQUENCY_MHZ;

	octeon_board_get_mac_addr();

	return 0;
}
