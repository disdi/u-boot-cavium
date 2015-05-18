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

	/* First patch the PHY configuration based on board revision */
	octeon_fdt_patch_rename((void *)(gd->fdt_blob),
				gd->arch.board_desc.rev_major == 1
				? "0,rev1" : "0,notrev1",
				"cavium,board-trim", false, NULL, NULL);

	/* Next patch CPU revision chages */
	for (node = 0; node < CVMX_MAX_NODES; node++) {
		if (!(gd->arch.node_mask & (1 << node)))
			continue;
		if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0))
			sprintf(fdt_key, "%d,rev1.0", node);
		else
			sprintf(fdt_key, "%d,notrev1.0", node);
		octeon_fdt_patch((void *)(gd->fdt_blob), fdt_key, "cavium,cpu-trim");
	}

	for (qlm = 0; qlm < 8; qlm++) {
		sprintf(env_var, "qlm%d:%d_mode", qlm, 0);
		mode = getenv(env_var);
		sprintf(fdt_key, "%d,none", qlm);
		if (!mode) {
			sprintf(fdt_key, "%d,none", qlm);
		} else if (!strncmp(mode, "sgmii", 5)) {
			sprintf(fdt_key, "%d,sgmii", qlm);
		} else if (!strncmp(mode, "xaui", 4)) {
			sprintf(fdt_key, "%d,xaui", qlm);
		} else if (!strncmp(mode, "dxaui", 5)) {
			sprintf(fdt_key, "%d,dxaui", qlm);
		} else if (!strcmp(mode, "ilk")) {
			if (qlm < 4)
				printf("Error: ILK not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,ilk", qlm);
		} else if (!strncmp(mode, "rxaui", 5)) {
			sprintf(fdt_key, "%d,rxaui", qlm);
		} else if (!strncmp(mode, "xlaui", 5)) {
			if (qlm < 4)
				printf("Error: XLAUI not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,xlaui", qlm);
		} else if (!strncmp(mode, "xfi", 3)) {
			if (qlm < 4)
				printf("Error: XFI not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,xfi", qlm);
		} else if (!strncmp(mode, "10G_KR", 6)) {
			if (qlm < 4)
				printf("Error: 10G_KR not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,10G_KR", qlm);
		} else if (!strncmp(mode, "40G_KR4", 7)) {
			if (qlm < 4)
				printf("Error: 40G_KR4 not supported on QLM %d\n",
				       qlm);
			else
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
		goto final_cleanup;
	}

	/* This won't do anything if node 1 is missing */
	for (qlm = 10; qlm < 18; qlm++) {
		sprintf(env_var, "qlm%d:%d_mode", qlm - 10, 1);
		mode = getenv(env_var);

		sprintf(fdt_key, "%d,none", qlm);
		if (!mode) {
			sprintf(fdt_key, "%d,none", qlm);
		} else if (!strncmp(mode, "sgmii", 5)) {
			sprintf(fdt_key, "%d,sgmii", qlm);
		} else if (!strncmp(mode, "xaui", 4)) {
			sprintf(fdt_key, "%d,xaui", qlm);
		} else if (!strncmp(mode, "dxaui", 5)) {
			sprintf(fdt_key, "%d,dxaui", qlm);
		} else if (!strcmp(mode, "ilk")) {
			if (qlm < 4)
				printf("Error: ILK not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,ilk", qlm);
		} else if (!strncmp(mode, "rxaui", 5)) {
			sprintf(fdt_key, "%d,rxaui", qlm);
		} else if (!strncmp(mode, "xlaui", 5)) {
			if (qlm < 4)
				printf("Error: XLAUI not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,xlaui", qlm);
		} else if (!strncmp(mode, "xfi", 3)) {
			if (qlm < 4)
				printf("Error: XFI not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,xfi", qlm);
		} else if (!strncmp(mode, "10G_KR", 6)) {
			if (qlm < 4)
				printf("Error: 10G_KR not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,10G_KR", qlm);
		} else if (!strncmp(mode, "40G_KR4", 7)) {
			if (qlm < 4)
				printf("Error: 40G_KR4 not supported on QLM %d\n",
				       qlm);
			else
				sprintf(fdt_key, "%d,40G_KR4", qlm);
		}
		debug("Patching qlm %d for %s\n", qlm, fdt_key);
		octeon_fdt_patch_rename((void *)gd->fdt_blob, fdt_key, NULL, true,
					strstr(mode, ",no_phy") ? kill_fdt_phy : NULL, NULL);
	}

final_cleanup:
	node = fdt_node_offset_by_compatible((void *)gd->fdt_blob, -1, "cavium,octeon-7890-bgx");
	while (node != -FDT_ERR_NOTFOUND) {
		int depth = 0;
		int child = fdt_next_node((void *)gd->fdt_blob, node, &depth);
		if (depth != 1)
			fdt_nop_node((void *)gd->fdt_blob, node);
		node = fdt_node_offset_by_compatible((void *)gd->fdt_blob, node, "cavium,octeon-7890-bgx");
	}

	return 0;
}

/* Raise an integer to a power */
static uint64_t ipow(uint64_t base, uint64_t exp)
{
	uint64_t result = 1;
	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

static int checkboardinfo(void)
{
        int core_mVolts, dram_mVolts0, dram_mVolts1;
	char mcu_ip_msg[64] = { 0 };
	char tmp[10];
	int characters, idx = 0, value = 0;

	debug("In %s\n", __func__);
	if (octeon_show_info()) {

		int mcu_rev_maj = 0;
		int mcu_rev_min = 0;

		if (twsii_mcu_read(0x00) == 0xa5
		    && twsii_mcu_read(0x01) == 0x5a) {
			gd->arch.mcu_rev_maj = mcu_rev_maj = twsii_mcu_read(2);
			gd->arch.mcu_rev_min = mcu_rev_min = twsii_mcu_read(3);
		} else {
                    return -1;     /* Abort if we can't access the MCU */
                }

		core_mVolts  = read_ispPAC_mvolts_avg(10, BOARD_ISP_TWSI_ADDR, 8);
		dram_mVolts0 = read_ispPAC_mvolts_avg(10, BOARD_ISP_TWSI_ADDR, 7); /* DDR0 */
		dram_mVolts1 = read_ispPAC_mvolts_avg(10, BOARD_ISP_TWSI_ADDR, 6); /* DDR1 */

		if (twsii_mcu_read(0x14) == 1)
			sprintf(mcu_ip_msg, "MCU IPaddr: %d.%d.%d.%d, ",
				twsii_mcu_read(0x10),
				twsii_mcu_read(0x11),
				twsii_mcu_read(0x12), twsii_mcu_read(0x13));
		printf("MCU rev: %d.%02d, %sCPU voltage: %d.%03d DDR{0,1} voltages: %d.%03d,%d.%03d\n",
		       gd->arch.mcu_rev_maj, gd->arch.mcu_rev_min, mcu_ip_msg,
		       core_mVolts  / 1000, core_mVolts  % 1000,
		       dram_mVolts0 / 1000, dram_mVolts0 % 1000,
		       dram_mVolts1 / 1000, dram_mVolts1 % 1000);

#define LED_CHARACTERS 8
		value = core_mVolts;
		idx = sprintf(tmp, "%lu ", gd->cpu_clk/(1000*1000));
		characters = LED_CHARACTERS - idx;

		if (value / 1000) {
			idx += sprintf(tmp + idx, "%d", value / 1000);
			characters = LED_CHARACTERS - idx;
		}

		characters -= 1;	/* Account for decimal point */

		value %= 1000;
		value = DIV_ROUND_UP(value, ipow(10, max(3 - characters, 0)));

		idx += sprintf(tmp + idx, ".%0*d", min(3, characters), value);

		/* Display CPU speed and voltage on display */
		octeon_led_str_write(tmp);
	} else {
		octeon_led_str_write("Boot    ");
	}

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

	/* Since i2c is broken on pass 1.0 we use an environment variable */
	if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0)) {
		ulong board_version;
		board_version = getenv_ulong("board_version", 10, 1);

		gd->arch.board_desc.rev_major = board_version;
	}

	octeon_init_qlm(0);

	for (node = 0; node < CVMX_MAX_NODES; node++) {
		int speed[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		int mode[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
		int pcie_rc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		int pcie_gen[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		int ref_clock_sel[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		int ref_clock_input[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		if (!(gd->arch.node_mask & (1 << node)))
			continue;

		for (qlm = 0; qlm < 8; qlm++) {
			const char *mode_str;

			mode[qlm] = CVMX_QLM_MODE_DISABLED;
			sprintf(env_var, "qlm%d:%d_mode", qlm, node);
			mode_str = getenv(env_var);
			if (!mode_str)
				continue;
			if (!strncmp(mode_str, "sgmii", 5)
			    || !strncmp(mode_str, "xaui", 4)
			    || !strncmp(mode_str, "dxaui", 5)
			    || !strncmp(mode_str, "rxaui", 5)) {
				/* BGX0 is QLM0 or QLM2 */
				if (qlm == 2
				    && mode[0] != -1
				    && mode[0] != CVMX_QLM_MODE_PCIE
				    && mode[0] != CVMX_QLM_MODE_PCIE_1X8) {
					printf("NODE %d: Not configuring QLM2,  as QLM0 is already set for BGX0\n", node);
					continue;
				}
				/* BGX1 is QLM1 or QLM3 */
				if (qlm == 3
				    && mode[1] != -1
				    && mode[1] != CVMX_QLM_MODE_PCIE
				    && mode[1] != CVMX_QLM_MODE_PCIE_1X8) {
					printf("NODE %d: Not configuring QLM2,  as QLM1 is already set for BGX1\n", node);
					continue;
				}
			}
			if (!strncmp(mode_str, "sgmii", 5)) {
				speed[qlm] = 1250;
				mode[qlm] = CVMX_QLM_MODE_SGMII;
				ref_clock_sel[qlm] = 2;
				printf("NODE %d:QLM %d: SGMII\n", node, qlm);
			} else if (!strncmp(mode_str, "xaui", 4)) {
				speed[qlm] = 3125;
				mode[qlm] = CVMX_QLM_MODE_XAUI;
				ref_clock_sel[qlm] = 2;
				printf("NODE %d:QLM %d: XAUI\n", node, qlm);
			} else if (!strncmp(mode_str, "dxaui", 5)) {
				speed[qlm] = 6250;
				mode[qlm] = CVMX_QLM_MODE_XAUI;
				ref_clock_sel[qlm] = 2;
				printf("NODE %d:QLM %d: DXAUI\n", node, qlm);
			} else if (!strncmp(mode_str, "rxaui", 5)) {
				speed[qlm] = 6250;
				mode[qlm] = CVMX_QLM_MODE_RXAUI;
				ref_clock_sel[qlm] = 2;
				printf("NODE %d:QLM %d: RXAUI\n", node, qlm);
			} else if (!strcmp(mode_str, "ila")) {
				if (qlm != 2 && qlm != 3) {
					printf("Error: ILA not supported on NODE%d:QLM %d\n",
						node, qlm);
				} else {
					speed[qlm] = 6250;
					mode[qlm] = CVMX_QLM_MODE_ILK;
					ref_clock_sel[qlm] = 2;
					printf("NODE %d:QLM %d: ILA\n", node, qlm);
				}
			} else if (!strcmp(mode_str, "ilk")) {
				int lanes = 0;
				char ilk_env[16];

				if (qlm < 4) {
					printf("Error: ILK not supported on NODE%d:QLM %d\n",
				       node, qlm);
				} else {
					speed[qlm] = 6250;
					mode[qlm] = CVMX_QLM_MODE_ILK;
					ref_clock_sel[qlm] = 2;
					printf("NODE %d:QLM %d: ILK\n", node, qlm);
				}
				sprintf(ilk_env, "ilk%d:%d_lanes", qlm, node);
				if (getenv(ilk_env))
					lanes = getenv_ulong(ilk_env, 0, 16);
				if (lanes > 4)
					ref_clock_input[qlm] = 2;
			} else if (!strncmp(mode_str, "xlaui", 5)) {
				if (qlm < 4) {
					printf("Error: XLAUI not supported on NODE%d:QLM %d\n",
				       	node, qlm);
				} else {
					speed[qlm] = 103125;
					mode[qlm] = CVMX_QLM_MODE_XLAUI;
					ref_clock_sel[qlm] = 2;
					printf("NODE %d:QLM %d: XLAUI\n", node, qlm);
				}
			} else if (!strncmp(mode_str, "xfi", 3)) {
				if (qlm < 4) {
					printf("Error: XFI not supported on NODE%d:QLM %d\n",
				       node, qlm);
				} else {
					speed[qlm] = 103125;
					mode[qlm] = CVMX_QLM_MODE_XFI;
					ref_clock_sel[qlm] = 2;
					printf("NODE %d:QLM %d: XFI\n", node, qlm);
				}
			} else if (!strncmp(mode_str, "10G_KR", 6)) {
				if (qlm < 4) {
					printf("Error: 10G_KR not supported on NODE%d:QLM %d\n",
				       node, qlm);
				} else {
					speed[qlm] = 103125;
					mode[qlm] = CVMX_QLM_MODE_10G_KR;
					ref_clock_sel[qlm] = 2;
					printf("NODE %d:QLM %d: 10G_KR\n", node, qlm);
				}
			} else if (!strncmp(mode_str, "40G_KR4", 7)) {
				if (qlm < 4) {
					printf("Error: 40G_KR4 not supported on NODE%d:QLM %d\n",
				       node, qlm);
				} else {
					speed[qlm] = 103125;
					mode[qlm] = CVMX_QLM_MODE_40G_KR4;
					ref_clock_sel[qlm] = 2;
					printf("NODE %d:QLM %d: 40G_KR4\n", node, qlm);
				}
			} else if (!strcmp(mode_str, "pcie")) {
				char *pmode;
				int lanes = 0;

				sprintf(env_var, "pcie%d:%d_mode", qlm, node);
				pmode = getenv(env_var);
				if (pmode && !strcmp(pmode, "ep"))
					pcie_rc[qlm] = 0;
				else
					pcie_rc[qlm] = 1;
				sprintf(env_var, "pcie%d:%d_gen", qlm, node);
				pcie_gen[qlm] = getenv_ulong(env_var, 0, 3);
				sprintf(env_var, "pcie%d:%d_lanes", qlm, node);
				lanes = getenv_ulong(env_var, 0, 8);
				if (lanes == 8)
					mode[qlm] = CVMX_QLM_MODE_PCIE_1X8;
				else
					mode[qlm] = CVMX_QLM_MODE_PCIE;
				ref_clock_sel[qlm] = 0;
				printf("NODE %d:QLM %d: PCIe gen%d %s\n", node, qlm, pcie_gen[qlm] + 1,
					pcie_rc[qlm] ? "root complex" : "endpoint");
			} else {
				printf("NODE %d:QLM %d: disabled\n", node, qlm);
			}
		}

		for (qlm = 0; qlm < 8; qlm++) {
			if (mode[qlm] == -1)
				continue;
			debug("Configuring node%d qlm%d with speed(%d), mode(%d), RC(%d), Gen(%d), REF_CLK(%d), CLK_SOURCE(%d)\n",
			 	node, qlm, speed[qlm], mode[qlm], pcie_rc[qlm],
			 	pcie_gen[qlm] + 1, ref_clock_sel[qlm], ref_clock_input[qlm]);
			octeon_configure_qlm_cn78xx(node, qlm, speed[qlm], mode[qlm], pcie_rc[qlm],
					    pcie_gen[qlm], ref_clock_sel[qlm],
					    ref_clock_input[qlm]);
		}
	}

        checkboardinfo();

	return 0;
}

int early_board_init(void)
{
	int cpu_ref = (DEFAULT_CPU_REF_FREQUENCY_MHZ * 1000 * 1000ull) / 1000000;

#ifdef CONFIG_OCTEON_EBB7804
	octeon_board_get_clock_info(EBB7804_DEF_DRAM_FREQ);
	octeon_board_get_descriptor(CVMX_BOARD_TYPE_EBB7804, 1, 0);
#else
	octeon_board_get_clock_info(EBB7800_DEF_DRAM_FREQ);
	octeon_board_get_descriptor(CVMX_BOARD_TYPE_EBB7800, 1, 0);
#endif
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

	return 0;
}
