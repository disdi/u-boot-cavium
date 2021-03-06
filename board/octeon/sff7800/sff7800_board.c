/*
 * (C) Copyright 2014 Cavium Inc. <support@cavium.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/mipsregs.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/cvmx-sim-magic.h>
#include <asm/arch/cvmx-qlm.h>
#include <asm/arch/octeon_fdt.h>
#include <fdt_support.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/octeon_qlm.h>
#include <asm/gpio.h>
#include <pca953x.h>

extern void print_isp_volt(const char **labels, uint16_t isp_dev_addr, uint8_t adc_chan);

extern int read_ispPAC_mvolts(uint16_t isp_dev_addr, uint8_t adc_chan);

extern int read_ispPAC_mvolts_avg(int loops, uint16_t isp_dev_addr, uint8_t adc_chan);

DECLARE_GLOBAL_DATA_PTR;

static void kill_fdt_phy(void *fdt, int offset, void *arg)
{
	int len, phy_offset;
	const fdt32_t *php;
	uint32_t phandle;

	php = fdt_getprop(fdt, offset, "phy-handle", &len);
	if (php && len == sizeof(*php)) {
		phandle = fdt32_to_cpu(*php);
		fdt_nop_property(fdt, offset, "phy-handle");
		phy_offset = fdt_node_offset_by_phandle(fdt, phandle);
		if (phy_offset > 0)
			fdt_nop_node(fdt, phy_offset);
	}
}

int board_fixup_fdt(void)
{
	char fdt_key[16];
	int qlm;
	char env_var[16];
	const char *mode;
	int node;

	cvmx_coremask_for_each_node(node, gd->arch.node_mask) {
		if (gpio_get_value((node << 8) | 3))
			sprintf(fdt_key, "%d,pcie", node * 10);
		else
			sprintf(fdt_key, "%d,sgmii", node * 10);
		octeon_fdt_patch(gd->fdt_blob, fdt_key, NULL);
	}

	for (qlm = 4; qlm < 8; qlm++) {
		sprintf(env_var, "qlm%d_mode", qlm);
		mode = getenv(env_var);
		if (!mode) {
			sprintf(env_var, "qlm%d:%d_mode", qlm, 0);
			mode = getenv(env_var);
		}

		sprintf(fdt_key, "%d,none", qlm);
		if (!mode) {
			if (qlm >= 6)
				snprintf(fdt_key, sizeof(fdt_key),
					 "%d,xfi", qlm);
			else
				sprintf(fdt_key, "%d,none", qlm);
		} else if (!strncmp(mode, "sgmii", 5)) {
			sprintf(fdt_key, "%d,sgmii", qlm);
		} else if (!strncmp(mode, "xaui", 4)) {
			sprintf(fdt_key, "%d,xaui", qlm);
		} else if (!strncmp(mode, "dxaui", 5)) {
			sprintf(fdt_key, "%d,dxaui", qlm);
		} else if (!strcmp(mode, "ilk")) {
			sprintf(fdt_key, "%d,ilk", qlm);
		} else if (!strncmp(mode, "xlaui", 5)) {
			sprintf(fdt_key, "%d,xlaui", qlm);
		} else if (!strncmp(mode, "xfi", 3)) {
			sprintf(fdt_key, "%d,xfi", qlm);
		} else if (!strncmp(mode, "10G_KR", 6)) {
			sprintf(fdt_key, "%d,10G_KR", qlm);
		} else if (!strncmp(mode, "40G_KR4", 7)) {
			sprintf(fdt_key, "%d,40G_KR4", qlm);
		}
		debug("Patching qlm %d for %s\n", qlm, fdt_key);
		octeon_fdt_patch_rename((void *)gd->fdt_blob, fdt_key, NULL, true,
					strstr(mode, ",no_phy") ? kill_fdt_phy : NULL, NULL);
	}

	/* Test for node 1 */
	if (!(gd->arch.node_mask & (1 << 1))) {
		octeon_fdt_patch((void *)(gd->fdt_blob), "1,none",
				 "cavium,node-trim");
		return 0;
	}

	/* This won't do anything if node 1 is missing */
	for (qlm = 14; qlm < 18; qlm++) {
		sprintf(env_var, "qlm%d_mode", qlm);
		mode = getenv(env_var);
		if (!mode) {
			sprintf(env_var, "qlm%d:%d_mode", qlm - 10, 1);
			mode = getenv(env_var);
		}
		sprintf(fdt_key, "%d,none", qlm);
		if (!mode) {
			if (qlm >= 16)
				snprintf(fdt_key, sizeof(fdt_key),
					 "%d,xfi", qlm);
			else
				sprintf(fdt_key, "%d,none", qlm);
		} else if (!strncmp(mode, "sgmii", 5)) {
			sprintf(fdt_key, "%d,sgmii", qlm);
		} else if (!strncmp(mode, "xaui", 4)) {
			sprintf(fdt_key, "%d,xaui", qlm);
		} else if (!strncmp(mode, "dxaui", 5)) {
			sprintf(fdt_key, "%d,dxaui", qlm);
		} else if (!strcmp(mode, "ilk")) {
			sprintf(fdt_key, "%d,ilk", qlm);
		} else if (!strncmp(mode, "xlaui", 5)) {
			sprintf(fdt_key, "%d,xlaui", qlm);
		} else if (!strncmp(mode, "xfi", 3)) {
			sprintf(fdt_key, "%d,xfi", qlm);
		} else if (!strncmp(mode, "10G_KR", 6)) {
			sprintf(fdt_key, "%d,10G_KR", qlm);
		} else if (!strncmp(mode, "40G_KR4", 7)) {
			sprintf(fdt_key, "%d,40G_KR4", qlm);
		}
		debug("Patching qlm %d for %s\n", qlm, fdt_key);
		octeon_fdt_patch_rename((void *)gd->fdt_blob, fdt_key, NULL, true,
					strstr(mode, ",no_phy") ? kill_fdt_phy : NULL, NULL);
	}

	return 0;
}

static int checkboardinfo(void)
{
#if 0
	int core_mVolts, dram_mVolts0, dram_mVolts1;

	debug("In %s\n", __func__);
	if (octeon_show_info()) {

		core_mVolts  = read_ispPAC_mvolts_avg(10, BOARD_ISP_TWSI_ADDR, 8);
		dram_mVolts0 = read_ispPAC_mvolts_avg(10, BOARD_ISP_TWSI_ADDR, 7); /* DDR0 */
		dram_mVolts1 = read_ispPAC_mvolts_avg(10, BOARD_ISP_TWSI_ADDR, 6); /* DDR1 */

		printf("CPU voltage: %d.%03d DDR{0,1} voltages: %d.%03d,%d.%03d\n",
		       core_mVolts  / 1000, core_mVolts  % 1000,
		       dram_mVolts0 / 1000, dram_mVolts0 % 1000,
		       dram_mVolts1 / 1000, dram_mVolts1 % 1000);

	}
#endif
	return 0;
}

/* Here is the description of the parameters that are passed to QLM configuration
 * 	param0 : The QLM to configure
 * 	param1 : Speed to configure the QLM at
 * 	param2 : Mode the QLM to configure
 * 	param3 : 1 = RC, 0 = EP
 * 	param4 : 0 = GEN1, 1 = GEN2, 2 = GEN3
 * 	param5 : ref clock select, 0 = 100Mhz, 1 = 125MHz, 2 = 156MHz
 * 	param6 : ref clock input to use:
 * 		 0 - external reference (QLMx_REF_CLK)
 * 		 1 = common clock 0 (QLMC_REF_CLK0)
 * 		 2 = common_clock 1 (QLMC_REF_CLK1)
 */
int checkboard(void)
{
	int qlm;
	char env_var[16];
	int node = 0;
	int pcie_gen;
	const char *mode_str;
	int rc;
	uint8_t qlm45_gpio;
	bool qlm4_present, qlm5_present;

	octeon_init_qlm(0);

	mdelay(100);

	cvmx_coremask_for_each_node(node, gd->arch.node_mask) {
		debug("Configuring node %d, QLM 0\n", node);

		if (gpio_get_value((node << 8) | 3) == 0) {
			rc = octeon_configure_qlm_cn78xx(node, 0, 1250,
							 CVMX_QLM_MODE_SGMII,
							 0, 0, 0, 0);
			if (rc)
				printf("Error configuring node %d, qlm 0 for SGMII\n",
				       node);
			else
				printf("Node %d, QLM 0 configured for SGMII mode\n", node);
		} else {
			snprintf(env_var, sizeof(env_var), "pcie0:%d_gen", node);
			pcie_gen = getenv_ulong(env_var, 10, 3) - 1;
			rc = octeon_configure_qlm_cn78xx(node, 0, 0,
						    CVMX_QLM_MODE_PCIE,
						    1, pcie_gen, 0, 0);
			if (rc)
				printf("Error configuring node %d, qlm 0 as PCIe endpoint\n",
				       node);
			else
				printf("Node %d, QLM 0 configured for PCIe generation %d endpoint mode\n",
				       node, pcie_gen + 1);
		}

		debug("Configuring node %d, QLM 1 for SGMII\n", node);
		rc = octeon_configure_qlm_cn78xx(node, 1, 1250,
						 CVMX_QLM_MODE_SGMII,
						 0, 0, 0, 0);
		if (rc)
			printf("Error configuring node %d, qlm 1 as SGMII\n",
			       node);

		debug("Configuring node %d, QLMs 2 and 3 for PCIe RC\n", node);
		snprintf(env_var, sizeof(env_var), "pcie2:%d_gen", node);
		pcie_gen = getenv_ulong(env_var, 10, 3) - 1;
		rc = octeon_configure_qlm_cn78xx(node, 2, 0,
						 CVMX_QLM_MODE_PCIE_1X8,
						 1, pcie_gen, 0, 0);
		rc |= octeon_configure_qlm_cn78xx(node, 3, 0,
						  CVMX_QLM_MODE_PCIE_1X8,
						  1, pcie_gen, 0, 0);
		if (rc)
			printf("Error configuring node %d QLMs 2 and 3 for PCIe generation %d root complex 1x8 mode\n",
			       node, pcie_gen + 1);

		debug("Configured node %d, QLMs 2 and 3 for PCIe generation %d\n",
		      node, pcie_gen + 1);
		if (pcie_gen != 2 && !rc)
			printf("Node %d: PCIe 1x8 configured for generation %d\n",
			       node, pcie_gen + 1);

		snprintf(env_var, sizeof(env_var), "qlm6:%d_mode", node);
		mode_str = getenv(env_var);

		if (!mode_str || !strcmp(mode_str, "xfi") ||
		    !strcmp(mode_str, "xaui")) {
			octeon_configure_qlm_cn78xx(node, 6, 103125,
						    CVMX_QLM_MODE_XFI,
						    0, 0, 2, 0);
			printf("Node %d, QLM 6 configured for XFI mode\n",
			       node);
		} else if (!strcmp(mode_str, "xlaui")) {
			octeon_configure_qlm_cn78xx(node, 6, 103125,
						    CVMX_QLM_MODE_XLAUI,
						    0, 0, 2, 0);
			printf("Node %d, QLM 6 configured for XLAUI mode\n",
			       node);
		}

		snprintf(env_var, sizeof(env_var), "qlm7:%d_mode", node);
		mode_str = getenv(env_var);
		if (!mode_str || !strcmp(mode_str, "xfi") ||
		    !strcmp(mode_str, "xaui")) {
			octeon_configure_qlm_cn78xx(node, 7, 103125,
						    CVMX_QLM_MODE_XFI,
						    0, 0, 2, 0);
			printf("Node %d, QLM 7 configured for XFI mode\n",
			       node);
		} else if (!strcmp(mode_str, "xlaui")) {
			octeon_configure_qlm_cn78xx(node, 7, 103125,
						    CVMX_QLM_MODE_XLAUI,
						    0, 0, 2, 0);
			printf("Node %d, QLM 7 configured for XLAUI mode\n",
			       node);
		}

		/* Power on QLMs 4 and 5 */
		pca953x_set_val(node << 1, 0x25, 0x4, 0x4);

		mdelay(250);

		qlm45_gpio = pca953x_get_val(node << 1, 0x26);
		debug("%s: node: %d, qlm45_gpio: 0x%x\n", __func__,
		      node, qlm45_gpio);
		qlm4_present = (qlm45_gpio & 0x19) == 1;
		qlm5_present = (qlm45_gpio & 0x62) == 2;

		if (qlm4_present || qlm5_present) {
			/* Power up QLMs */
			pca953x_set_val(node << 1, 0x25, 4, 4);
			/* Take QLMs out of reset */
			mdelay(500);
			if (qlm4_present)
				pca953x_set_val(node << 1, 0x25, 0x51, 0x51);
			if (qlm5_present)
				pca953x_set_val(node << 1, 0x25, 0xa2, 0xa2);
		} else {
			/* Remove power */
			debug("%s: node: %d shutting down power for QLMs 4 and 5\n",
			      __func__, node);
			pca953x_set_val(node << 1, 0x25, 0x4, 0);
		}

		for (qlm = 4; qlm < 6; qlm++) {
			const char *mode_str;
			int speed;
			int mode;
			int ref_clock_sel;
			int pcie_gen;
			int ref_clock_input;

			ref_clock_sel = 0;
			ref_clock_input = 0;
			pcie_gen = 2;
			mode = CVMX_QLM_MODE_DISABLED;
			sprintf(env_var, "qlm%d:%d_mode", qlm, node);
			mode_str = getenv(env_var);
			debug("%s: node %d, qlm %d mode string for %s is: %s\n",
			      __func__, node, qlm, env_var,
			      mode_str ? mode_str : "(none)");
			if (!mode_str ||
			    (qlm == 4 && !qlm4_present) ||
			    (qlm == 5 && !qlm5_present)) {
				mode_str = "disabled";
				mode = CVMX_QLM_MODE_DISABLED;
			} else if (!strncmp(mode_str, "sgmii", 5)) {
				speed = 1250;
				mode = CVMX_QLM_MODE_SGMII;
				printf("NODE %d:QLM %d: SGMII\n", node, qlm);
			} else if (!strncmp(mode_str, "xaui", 4)) {
				speed = 3125;
				mode = CVMX_QLM_MODE_XAUI;
				printf("NODE %d:QLM %d: XAUI\n", node, qlm);
			} else if (!strncmp(mode_str, "dxaui", 5)) {
				speed = 6250;
				mode = CVMX_QLM_MODE_XAUI;
				printf("NODE %d:QLM %d: DXAUI\n", node, qlm);
			} else if (!strncmp(mode_str, "rxaui", 5)) {
				speed = 6250;
				mode = CVMX_QLM_MODE_RXAUI;
				printf("NODE %d:QLM %d: RXAUI\n", node, qlm);
			} else if (!strcmp(mode_str, "ilk")) {
				int lanes = 0;
				char ilk_env[16];

				speed = 6250;
				mode = CVMX_QLM_MODE_ILK;
				printf("NODE %d:QLM %d: ILK\n", node, qlm);

				sprintf(ilk_env, "ilk%d:%d_lanes", node, qlm);
				if (getenv(ilk_env))
					lanes = getenv_ulong(ilk_env, 0, 16);
				if (lanes > 4)
					ref_clock_input = 2;
			} else if (!strncmp(mode_str, "xlaui", 5)) {
				speed = 103125;
				mode = CVMX_QLM_MODE_XLAUI;
				printf("NODE %d:QLM %d: XLAUI\n",
				       node, qlm);
			} else if (!strncmp(mode_str, "xfi", 3)) {
				speed = 103125;
				mode = CVMX_QLM_MODE_XFI;
				printf("NODE %d:QLM %d: XFI\n",
				       node, qlm);
			} else if (!strncmp(mode_str, "10G_KR", 6)) {
				speed = 103125;
				mode = CVMX_QLM_MODE_10G_KR;
				printf("NODE %d:QLM %d: 10G_KR\n",
				       node, qlm);
			} else if (!strncmp(mode_str, "40G_KR4", 7)) {
				speed = 103125;
				mode = CVMX_QLM_MODE_40G_KR4;
				printf("NODE %d:QLM %d: 40G_KR4\n",
				       node, qlm);
			} else if (!strcmp(mode_str, "pcie")) {
				sprintf(env_var, "pcie%d:%d_gen", qlm, node);
				pcie_gen = getenv_ulong(env_var, 0, 3);
				mode = CVMX_QLM_MODE_PCIE;
				printf("NODE %d:QLM %d: PCIe gen%d root complex\n",
				       node, qlm, pcie_gen + 1);
			} else {
				printf("NODE %d:QLM %d: disabled\n", node, qlm);
				mode = CVMX_QLM_MODE_DISABLED;
			}
			if (mode != CVMX_QLM_MODE_DISABLED) {
				debug("Configuring node %d qlm%d with speed(%d), mode(%d), Gen(%d), REF_CLK(%d), CLK_SOURCE(%d)\n",
				      node, qlm, speed, mode, pcie_gen,
				      ref_clock_sel, ref_clock_input);
				octeon_configure_qlm_cn78xx(node, qlm, speed, mode,
						    mode == CVMX_QLM_MODE_PCIE,
						    pcie_gen, ref_clock_sel,
						    ref_clock_input);
			}
		}
	}

        checkboardinfo();

#ifdef DEBUG
	debug("QLM 4 reference clock: %d\n"
	      "QLM 5 reference clock: %d\n"
	      "QLM 6 reference clock: %d\n"
	      "QLM 7 reference clock: %d\n",
	      cvmx_qlm_measure_clock(4),
	      cvmx_qlm_measure_clock(5),
	      cvmx_qlm_measure_clock(6),
	      cvmx_qlm_measure_clock(7));
#endif
	return 0;
}

int late_board_init(void)
{
	/* Enable all of the SFP+ transmitters */
	debug("%s: Enabling all SFP+ transmitters\n", __func__);
	pca953x_set_dir(4, 0x21, 0xff, 0x22);
	pca953x_set_dir(4, 0x22, 0xff, 0x22);
	pca953x_set_dir(4, 0x23, 0xff, 0x22);
	pca953x_set_dir(4, 0x24, 0xff, 0x22);
	pca953x_set_val(4, 0x21, 0x22, 0x00);
	pca953x_set_val(4, 0x22, 0x22, 0x00);
	pca953x_set_val(4, 0x23, 0x22, 0x00);
	pca953x_set_val(4, 0x24, 0x22, 0x00);
}

int early_board_init(void)
{
	int cpu_ref = (DEFAULT_CPU_REF_FREQUENCY_MHZ * 1000 * 1000ull) / 1000000;

	if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0)) {
		puts("ERROR: Use a pass 1.1 or later CN78XX with this board!\n");
		return -1;
	}

	octeon_board_get_clock_info(SFF7800_DEF_DRAM_FREQ);
	octeon_board_get_descriptor(CVMX_BOARD_TYPE_SFF7800, 1, 0);

	/* CN63XX has a fixed 50 MHz reference clock */
	gd->arch.ddr_ref_hertz = (DEFAULT_CPU_REF_FREQUENCY_MHZ * 1000 * 1000ull);

	/* Even though the CPU ref freq is stored in the clock descriptor, we
	 * don't read it here.  The MCU reads it and configures the clock, and
	 * we read how the clock is actually configured.
	 * The bootloader does not need to read the clock descriptor tuple for
	 * normal operation on rev 2 and later boards.
	 */
	octeon_board_get_mac_addr();

	/* Read CPU clock multiplier */
	gd->cpu_clk = octeon_get_cpu_multiplier() * cpu_ref * 1000000;

	/* Turn off power to USB 3.0 port on J6. */
	gpio_direction_output(4, 1);

	/* Take Cortina PHY out of reset */
	gpio_direction_output(1, 1);

	/* Take Marvell PHYs out of reset */
	gpio_direction_output(6, 1);

	gpio_direction_input(3);

	/* U55, all GPIO pins are outputs */
	pca953x_set_dir(0, 0x25, 0xff, 0x00);

	/* U98 Pins 0 and 1 are inputs for QLM 4, 5 DC OK */
	pca953x_set_dir(0, 0x26, 0xff, 0x03);

	/* Turn off power to QLM4 and 5 modules and enable reset */
	pca953x_set_val(0, 0x25, 0xf7, 0x00);

	if (gd->arch.node_mask & 2) {
		/* Turn off power to USB 3.0 port on J6. */
		gpio_direction_output(0x104, 1);

		/* Take Cortina PHY out of reset */
		gpio_direction_output(0x101, 1);

		/* Take Marvell PHYs out of reset */
		gpio_direction_output(0x106, 1);

		gpio_direction_input(0x103);

		/* U55, all GPIO pins are outputs */
		pca953x_set_dir(2, 0x25, 0xff, 0x00);

		/* U98 Pins 0 and 1 are inputs for QLM 4, 5 DC OK */
		pca953x_set_dir(2, 0x26, 0xff, 0x03);

		/* Turn off power to QLM4 and 5 modules and enable reset */
		pca953x_set_val(2, 0x25, 0xf7, 0x00);
	}
	mdelay(100);

	return 0;
}
