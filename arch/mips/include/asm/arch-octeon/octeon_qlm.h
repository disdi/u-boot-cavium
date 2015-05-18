/**
 * (C) Copyright 2004-2013 Cavium Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __OCTEON_QLM_H__
#define __OCTEON_QLM_H__

#ifdef CONFIG_OCTEON_QLM

/**
 * Configure qlm/dlm speed and mode.
 * @param qlm     The QLM or DLM to configure
 * @param speed   The speed the QLM needs to be configured in Mhz.
 * @param mode    The QLM to be configured as SGMII/XAUI/PCIe.
 * @param rc      Only used for PCIe, rc = 1 for root complex mode, 0 for EP
 *		  mode.
 * @param pcie_mode Only used when qlm/dlm are in pcie mode.
 * @param ref_clk_sel Reference clock to use for 70XX where:
 *			0: 100MHz
 *			1: 125MHz
 *			2: 156.25MHz
 * @param ref_clk_input	This selects which reference clock input to use.  For
 *			cn70xx:
 *				0: DLMC_REF_CLK0
 *				1: DLMC_REF_CLK1
 *				2: DLM0_REF_CLK
 *			cn61xx: (not used)
 *			cn78xx/cn76xx/cn73xx:
 *				0: Internal clock (QLM[0-7]_REF_CLK)
 *				1: QLMC_REF_CLK0
 *				2: QLMC_REF_CLK1
 *
 * @return       Return 0 on success or -1.
 */
extern int octeon_configure_qlm(int qlm, int speed, int mode, int rc,
				int pcie_mode, int ref_clk_sel,
				int ref_clk_input);

extern int octeon_configure_qlm_cn78xx(int node, int qlm, int speed, int mode,
				       int rc, int pcie_mode, int ref_clk_sel,
				       int ref_clk_input);


extern void octeon_init_qlm(int node);

#else
# error CONFIG_OCTEON_QLM is not defined!
#endif	/* CONFIG_OCTEON_QLM */

#endif	/* __OCTEON_QLM_H__ */
