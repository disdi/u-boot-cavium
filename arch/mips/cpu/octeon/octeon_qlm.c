/***********************license start************************************
 * Copyright (c) 2013-2015 Cavium, Inc. <support@cavium.com>.  All rights
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

#include <common.h>
#include <asm/mipsregs.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/cvmx-qlm.h>
#include <asm/arch/cvmx-pcie.h>
#include <asm/arch/cvmx-sata-defs.h>
#include <asm/arch/octeon_qlm.h>

DECLARE_GLOBAL_DATA_PTR;

/** 2.5GHz with 100MHz reference clock */
#define R_25G_REFCLK100             0x0
/** 5.0GHz with 100MHz reference clock */
#define R_5G_REFCLK100              0x1
/** 8.0GHz with 100MHz reference clock */
#define R_8G_REFCLK100              0x2
/** 1.25GHz with 156.25MHz reference clock */
#define R_125G_REFCLK15625_KX       0x3
/** 3.125Ghz with 156.25MHz reference clock (XAUI) */
#define R_3125G_REFCLK15625_XAUI    0x4
/** 10.3125GHz with 156.25MHz reference clock (XFI/XLAUI) */
#define R_103125G_REFCLK15625_KR    0x5
/** 1.25GHz with 156.25MHz reference clock (SGMII) */
#define R_125G_REFCLK15625_SGMII    0x6
/** 5GHz with 156.25MHz reference clock (QSGMII) */
#define R_5G_REFCLK15625_QSGMII     0x7
/** 6.25GHz with 156.25MHz reference clock (RXAUI) */
#define R_625G_REFCLK15625_RXAUI    0x8
/** 2.5GHz with 125MHz reference clock */
#define R_25G_REFCLK125             0x9
/** 5GHz with 125MHz reference clock */
#define R_5G_REFCLK125              0xa
/** 8GHz with 125MHz reference clock */
#define R_8G_REFCLK125              0xb
/** Must be last, number of modes */
#define R_NUM_LANE_MODES            0xc

int cvmx_qlm_is_ref_clock(int qlm, int reference_mhz)
{
	int ref_clock = cvmx_qlm_measure_clock(qlm);
	int mhz = ref_clock / 1000000;
	int range = reference_mhz / 10;
	return ((mhz >= reference_mhz - range) &&
		(mhz <= reference_mhz + range));
}

static int __get_qlm_spd(int qlm, int speed)
{
	int qlm_spd = 0xf;

	if (OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN5XXX))
		return -1;

	if (cvmx_qlm_is_ref_clock(qlm, 100)) {
		if (speed == 1250)
			qlm_spd = 0x3;
		else if (speed == 2500)
			qlm_spd = 0x2;
		else if (speed == 5000)
			qlm_spd = 0x0;
		else {
			qlm_spd = 0xf;
		}
	} else if (cvmx_qlm_is_ref_clock(qlm, 125)) {
		if (speed == 1250)
			qlm_spd = 0xa;
		else if (speed == 2500)
			qlm_spd = 0x9;
		else if (speed == 3125)
			qlm_spd = 0x8;
		else if (speed == 5000)
			qlm_spd = 0x6;
		else if (speed == 6250)
			qlm_spd = 0x5;
		else {
			qlm_spd = 0xf;
		}
	} else if (cvmx_qlm_is_ref_clock(qlm, 156)) {
		if (speed == 1250)
			qlm_spd = 0x4;
		else if (speed == 2500)
			qlm_spd = 0x7;
		else if (speed == 3125)
			qlm_spd = 0xe;
		else if (speed == 3750)
			qlm_spd = 0xd;
		else if (speed == 5000)
			qlm_spd = 0xb;
		else if (speed == 6250)
			qlm_spd = 0xc;
		else {
			qlm_spd = 0xf;
		}
	}
	return qlm_spd;
}

static void __set_qlm_pcie_mode_61xx(int pcie_port, int root_complex)
{
	int rc = root_complex ? 1 : 0;
	int ep = root_complex ? 0 : 1;
	cvmx_ciu_soft_prst1_t soft_prst1;
	cvmx_ciu_soft_prst_t soft_prst;
	cvmx_mio_rst_ctlx_t rst_ctl;

	if (pcie_port) {
		soft_prst1.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
		soft_prst1.s.soft_prst = 1;
		cvmx_write_csr(CVMX_CIU_SOFT_PRST1, soft_prst1.u64);
	} else {
		soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
		soft_prst.s.soft_prst = 1;
		cvmx_write_csr(CVMX_CIU_SOFT_PRST, soft_prst.u64);
	}

	rst_ctl.u64 = cvmx_read_csr(CVMX_MIO_RST_CTLX(pcie_port));

	rst_ctl.s.prst_link = rc;
	rst_ctl.s.rst_link = ep;
	rst_ctl.s.prtmode = rc;
	rst_ctl.s.rst_drv = rc;
	rst_ctl.s.rst_rcv = 0;
	rst_ctl.s.rst_chip = ep;
	cvmx_write_csr(CVMX_MIO_RST_CTLX(pcie_port), rst_ctl.u64);

	if (root_complex == 0) {
		if (pcie_port) {
			soft_prst1.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
			soft_prst1.s.soft_prst = 0;
			cvmx_write_csr(CVMX_CIU_SOFT_PRST1, soft_prst1.u64);
		} else {
			soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
			soft_prst.s.soft_prst = 0;
			cvmx_write_csr(CVMX_CIU_SOFT_PRST, soft_prst.u64);
		}
	}
}

/**
 * Configure qlm speed and mode. MIO_QLMX_CFG[speed,mode] are not set
 * for CN61XX.
 *
 * @param qlm     The QLM to configure
 * @param speed   The speed the QLM needs to be configured in Mhz.
 * @param mode    The QLM to be configured as SGMII/XAUI/PCIe.
 *                  QLM 0: 0 = PCIe0 1X4, 1 = Reserved, 2 = SGMII1, 3 = XAUI1
 *                  QLM 1: 0 = PCIe1 1x2, 1 = PCIe(0/1) 2x1, 2 - 3 = Reserved
 *                  QLM 2: 0 - 1 = Reserved, 2 = SGMII0, 3 = XAUI0
 * @param rc      Only used for PCIe, rc = 1 for root complex mode, 0 for EP
 *		  mode.
 * @param pcie2x1 Only used when QLM1 is in PCIE2x1 mode.  The QLM_SPD has a
 * 		  different value on how PEMx needs to be configured:
 *                   0x0 - both PEM0 & PEM1 are in gen1 mode.
 *                   0x1 - PEM0 in gen2 and PEM1 in gen1 mode.
 *                   0x2 - PEM0 in gen1 and PEM1 in gen2 mode.
 *                   0x3 - both PEM0 & PEM1 are in gen2 mode.
 *               SPEED value is ignored in this mode. QLM_SPD is set based on
 *               pcie2x1 value in this mode.
 *
 * @return       Return 0 on success or -1.
 */
static int octeon_configure_qlm_cn61xx(int qlm, int speed, int mode, int rc,
				       int pcie2x1)
{
	cvmx_mio_qlmx_cfg_t qlm_cfg;

	/* The QLM speed varies for SGMII/XAUI and PCIe mode. And depends on
	 * reference clock.
	 */
	if (!OCTEON_IS_MODEL(OCTEON_CN61XX))
		return -1;

	if (qlm < 3)
		qlm_cfg.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(qlm));
	else {
		cvmx_dprintf("WARNING: Invalid QLM(%d) passed\n", qlm);
		return -1;
	}

	switch (qlm) {
		/* SGMII/XAUI mode */
	case 2:
		{
			if (mode < 2) {
				qlm_cfg.s.qlm_spd = 0xf;
				break;
			}
			qlm_cfg.s.qlm_spd = __get_qlm_spd(qlm, speed);
			qlm_cfg.s.qlm_cfg = mode;
			break;
		}
	case 1:
		{
			if (mode == 1) {	/* 2x1 mode */
				cvmx_mio_qlmx_cfg_t qlm0;

				/* When QLM0 is configured as PCIe(QLM_CFG=0x0)
				 * and enabled (QLM_SPD != 0xf), QLM1 cannot be
				 * configured as PCIe 2x1 mode (QLM_CFG=0x1)
				 * and enabled (QLM_SPD != 0xf).
				 */
				qlm0.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(0));
				if (qlm0.s.qlm_spd != 0xf && qlm0.s.qlm_cfg == 0) {
					cvmx_dprintf("Invalid mode(%d) for QLM(%d) as QLM1 is PCIe mode\n", mode, qlm);
					qlm_cfg.s.qlm_spd = 0xf;
					break;
				}

				/* Set QLM_SPD based on reference clock and mode */
				if (cvmx_qlm_is_ref_clock(qlm, 100)) {
					if (pcie2x1 == 0x3)
						qlm_cfg.s.qlm_spd = 0x0;
					else if (pcie2x1 == 0x1)
						qlm_cfg.s.qlm_spd = 0x2;
					else if (pcie2x1 == 0x2)
						qlm_cfg.s.qlm_spd = 0x1;
					else if (pcie2x1 == 0x0)
						qlm_cfg.s.qlm_spd = 0x3;
					else
						qlm_cfg.s.qlm_spd = 0xf;
				} else if (cvmx_qlm_is_ref_clock(qlm, 125)) {
					if (pcie2x1 == 0x3)
						qlm_cfg.s.qlm_spd = 0x4;
					else if (pcie2x1 == 0x1)
						qlm_cfg.s.qlm_spd = 0x6;
					else if (pcie2x1 == 0x2)
						qlm_cfg.s.qlm_spd = 0x9;
					else if (pcie2x1 == 0x0)
						qlm_cfg.s.qlm_spd = 0x7;
					else
						qlm_cfg.s.qlm_spd = 0xf;
				}
				qlm_cfg.s.qlm_cfg = mode;
				cvmx_write_csr(CVMX_MIO_QLMX_CFG(qlm),
					       qlm_cfg.u64);

				/* Set PCIe mode bits */
				__set_qlm_pcie_mode_61xx(0, rc);
				__set_qlm_pcie_mode_61xx(1, rc);
				return 0;
			} else if (mode > 1) {
				cvmx_dprintf("Invalid mode(%d) for QLM(%d).\n",
					     mode, qlm);
				qlm_cfg.s.qlm_spd = 0xf;
				break;
			}

			/* Set speed and mode for PCIe 1x2 mode. */
			if (cvmx_qlm_is_ref_clock(qlm, 100)) {
				if (speed == 5000)
					qlm_cfg.s.qlm_spd = 0x1;
				else if (speed == 2500)
					qlm_cfg.s.qlm_spd = 0x2;
				else
					qlm_cfg.s.qlm_spd = 0xf;
			} else if (cvmx_qlm_is_ref_clock(qlm, 125)) {
				if (speed == 5000)
					qlm_cfg.s.qlm_spd = 0x4;
				else if (speed == 2500)
					qlm_cfg.s.qlm_spd = 0x6;
				else
					qlm_cfg.s.qlm_spd = 0xf;
			} else
				qlm_cfg.s.qlm_spd = 0xf;

			qlm_cfg.s.qlm_cfg = mode;
			cvmx_write_csr(CVMX_MIO_QLMX_CFG(qlm), qlm_cfg.u64);

			/* Set PCIe mode bits */
			__set_qlm_pcie_mode_61xx(1, rc);
			return 0;
		}
	case 0:
		{
			/* QLM_CFG = 0x1 - Reserved */
			if (mode == 1) {
				qlm_cfg.s.qlm_spd = 0xf;
				break;
			}
			/* QLM_CFG = 0x0 - PCIe 1x4(PEM0) */
			if (mode == 0 && speed != 5000 && speed != 2500) {
				qlm_cfg.s.qlm_spd = 0xf;
				break;
			}

			/* Set speed and mode */
			qlm_cfg.s.qlm_spd = __get_qlm_spd(qlm, speed);
			qlm_cfg.s.qlm_cfg = mode;
			cvmx_write_csr(CVMX_MIO_QLMX_CFG(qlm), qlm_cfg.u64);

			/* Set PCIe mode bits */
			if (mode == 0)
				__set_qlm_pcie_mode_61xx(0, rc);

			return 0;
		}
	default:
		cvmx_dprintf("WARNING: Invalid QLM(%d) passed\n", qlm);
		qlm_cfg.s.qlm_spd = 0xf;
	}
	cvmx_write_csr(CVMX_MIO_QLMX_CFG(qlm), qlm_cfg.u64);
	return 0;
}

/* qlm      : DLM to configure
 * baud_mhz : speed of the DLM
 * ref_clk_sel  :  reference clock speed selection where:
 *			0:	100MHz
 *			1:	125MHz
 *			2:	156.25MHz
 *
 * ref_clk_input:  reference clock input where:
 *			0:	DLMC_REF_CLK0_[P,N]
 *			1:	DLMC_REF_CLK1_[P,N]
 *			2:	DLM0_REF_CLK_[P,N] (only valid for QLM 0)
 * is_sff7000_rxaui : boolean to indicate whether qlm is RXAUI on SFF7000
 */
static int __dlm_setup_pll_cn70xx(int qlm, int baud_mhz, int ref_clk_sel,
				  int ref_clk_input, int is_sff7000_rxaui)
{
	cvmx_gserx_dlmx_test_powerdown_t dlmx_test_powerdown;
	cvmx_gserx_dlmx_ref_ssp_en_t dlmx_ref_ssp_en;
	cvmx_gserx_dlmx_mpll_en_t dlmx_mpll_en;
	cvmx_gserx_dlmx_phy_reset_t dlmx_phy_reset;
	cvmx_gserx_dlmx_tx_amplitude_t tx_amplitude;
	cvmx_gserx_dlmx_tx_preemph_t tx_preemph;
	cvmx_gserx_dlmx_rx_eq_t rx_eq;
	cvmx_gserx_dlmx_ref_clkdiv2_t ref_clkdiv2;
	cvmx_gserx_dlmx_mpll_multiplier_t mpll_multiplier;
	int gmx_ref_clk = 100;

	debug("%s(%d, %d, %d, %d, %d)\n", __func__, qlm, baud_mhz, ref_clk_sel,
	      ref_clk_input, is_sff7000_rxaui);
	if (ref_clk_sel == 1)
		gmx_ref_clk = 125;
	else if (ref_clk_sel == 2)
		gmx_ref_clk = 156;

	if (qlm != 0 && ref_clk_input == 2) {
		printf("%s: Error: can only use reference clock inputs 0 or 1 for DLM %d\n",
		       __func__, qlm);
		return -1;
	}

	/* Hardware defaults are invalid */
	tx_amplitude.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_TX_AMPLITUDE(qlm, 0));
	if (is_sff7000_rxaui) {
		tx_amplitude.s.tx0_amplitude = 100;
		tx_amplitude.s.tx1_amplitude = 100;
	} else {
		tx_amplitude.s.tx0_amplitude = 65;
		tx_amplitude.s.tx1_amplitude = 65;
	}

	cvmx_write_csr(CVMX_GSERX_DLMX_TX_AMPLITUDE(qlm, 0), tx_amplitude.u64);

	tx_preemph.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_TX_PREEMPH(qlm, 0));

	if (is_sff7000_rxaui) {
		tx_preemph.s.tx0_preemph = 0;
		tx_preemph.s.tx1_preemph = 0;
	} else {
		tx_preemph.s.tx0_preemph = 22;
		tx_preemph.s.tx1_preemph = 22;
	}
	cvmx_write_csr(CVMX_GSERX_DLMX_TX_PREEMPH(qlm, 0), tx_preemph.u64);

	rx_eq.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_RX_EQ(qlm, 0));
	rx_eq.s.rx0_eq = 0;
	rx_eq.s.rx1_eq = 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_RX_EQ(qlm, 0), rx_eq.u64);

	/* 1. Write GSER0_DLM0_REF_USE_PAD[REF_USE_PAD] = 1 (to select
	 *    reference-clock input)
	 *    The documentation for this register in the HRM is useless since
	 *    it says it selects between two different clocks that are not
	 *    documented anywhere.  What it really does is select between
	 *    DLM0_REF_CLK_[P,N] if 1 and DLMC_REF_CLK[0,1]_[P,N] if 0.
	 *
	 *    This register must be 0 for DLMs 1 and 2 and can only be 1 for
	 *    DLM 0.
	 */
	cvmx_write_csr(CVMX_GSERX_DLMX_REF_USE_PAD(0, 0),
		       ((ref_clk_input == 2) && (qlm == 0)) ? 1 : 0);

	/* Reference clock was already chosen before we got here */

	/* 2. Write GSER0_DLM0_REFCLK_SEL[REFCLK_SEL] if required for
	 *    reference-clock selection.
	 *
	 *    If GSERX_DLMX_REF_USE_PAD is 1 then this register is ignored.
	 */
	cvmx_write_csr(CVMX_GSERX_DLMX_REFCLK_SEL(0, 0), ref_clk_input & 1);

	/* Reference clock was already chosen before we got here */

	/* 3. If required, write GSER0_DLM0_REF_CLKDIV2[REF_CLKDIV2] (must be
	 *    set if reference clock > 100 MHz)
	 */
	/* Apply workaround for Errata (G-20669) MPLL may not come up. */
	ref_clkdiv2.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_REF_CLKDIV2(qlm, 0));
	if (gmx_ref_clk == 100)
		ref_clkdiv2.s.ref_clkdiv2 = 0;
	else
		ref_clkdiv2.s.ref_clkdiv2 = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_REF_CLKDIV2(qlm, 0), ref_clkdiv2.u64);

	/* 1. Ensure GSER(0)_DLM(0..2)_PHY_RESET[PHY_RESET] is set. */
	dlmx_phy_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0));
	dlmx_phy_reset.s.phy_reset = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0), dlmx_phy_reset.u64);

	/* 2. If SGMII or QSGMII or RXAUI (i.e. if DLM0) set
	      GSER(0)_DLM(0)_MPLL_EN[MPLL_EN] to one. */
	/* 7. Set GSER0_DLM0_MPLL_EN[MPLL_EN] = 1 */
	dlmx_mpll_en.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_MPLL_EN(0, 0));
	dlmx_mpll_en.s.mpll_en = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_MPLL_EN(0, 0), dlmx_mpll_en.u64);

	/* 3. Set GSER(0)_DLM(0..2)_MPLL_MULTIPLIER[MPLL_MULTIPLIER]
	      to the value in the preceding table, which is different
	      than the desired setting prescribed by the HRM. */
	mpll_multiplier.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0));
	if (gmx_ref_clk == 100)
		mpll_multiplier.s.mpll_multiplier = 35;
	else if (gmx_ref_clk == 125)
		mpll_multiplier.s.mpll_multiplier = 56;
	else
		mpll_multiplier.s.mpll_multiplier = 45;
	debug("%s: Setting mpll multiplier to %u for DLM%d, baud %d, clock rate %uMHz\n",
	      __func__, mpll_multiplier.s.mpll_multiplier, qlm, baud_mhz,
	      gmx_ref_clk);

	cvmx_write_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0), mpll_multiplier.u64);

	/* 5. Clear GSER0_DLM0_TEST_POWERDOWN[TEST_POWERDOWN] */
	dlmx_test_powerdown.u64 =
			cvmx_read_csr(CVMX_GSERX_DLMX_TEST_POWERDOWN(qlm, 0));
	dlmx_test_powerdown.s.test_powerdown = 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_TEST_POWERDOWN(qlm, 0),
		       dlmx_test_powerdown.u64);

	/* 6. Set GSER0_DLM0_REF_SSP_EN[REF_SSP_EN] = 1 */
	dlmx_ref_ssp_en.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_REF_SSP_EN(qlm, 0));
	dlmx_ref_ssp_en.s.ref_ssp_en = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_REF_SSP_EN(0, 0), dlmx_ref_ssp_en.u64);

	/* 8. Clear GSER0_DLM0_PHY_RESET[PHY_RESET] = 0 */
	dlmx_phy_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0));
	dlmx_phy_reset.s.phy_reset = 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0), dlmx_phy_reset.u64);

	/* 5. If PCIe or SATA (i.e. if DLM1 or DLM2), set both MPLL_EN
	   and MPLL_EN_OVRD to one in GSER(0)_PHY(1..2)_OVRD_IN_LO. */

	/* 6. Decrease MPLL_MULTIPLIER by one continually until it
	 * reaches the desired long-term setting, ensuring that each
	 * MPLL_MULTIPLIER value is constant for at least 1 msec before
	 * changing to the next value.  The desired long-term setting is
	 * as indicated in HRM tables 21-1, 21-2, and 21-3.  This is not
	 * required with the HRM sequence.
	 */
	mpll_multiplier.u64 =
			cvmx_read_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0));
	__cvmx_qlm_set_mult(qlm, baud_mhz,
			    mpll_multiplier.s.mpll_multiplier);

	/* 9. Poll until the MPLL locks. Wait for
	 *    GSER0_DLM0_MPLL_STATUS[MPLL_STATUS] = 1
	 */
	if ((gd->arch.board_desc.board_type != CVMX_BOARD_TYPE_SIM)
	     && CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_MPLL_STATUS(qlm, 0),
				      cvmx_gserx_dlmx_mpll_status_t,
				      mpll_status, ==, 1, 10000)) {
		cvmx_warn("PLL for DLM%d failed to lock\n", qlm);
		return -1;
	}
	return 0;
}

static int __dlm0_setup_tx_cn70xx(void)
{
	int need0, need1;
	cvmx_gmxx_inf_mode_t mode0, mode1;
	cvmx_gserx_dlmx_tx_rate_t rate;
	cvmx_gserx_dlmx_tx_en_t en;
	cvmx_gserx_dlmx_tx_cm_en_t cm_en;
	cvmx_gserx_dlmx_tx_data_en_t data_en;
	cvmx_gserx_dlmx_tx_reset_t tx_reset;

	mode0.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(0));
	mode1.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(1));

	/* Which lanes do we need? */
	need0 = (mode0.s.mode != CVMX_GMX_INF_MODE_DISABLED);
	need1 = (mode1.s.mode != CVMX_GMX_INF_MODE_DISABLED)
		 || (mode0.s.mode == CVMX_GMX_INF_MODE_RXAUI);

	/* 1. Write GSER0_DLM0_TX_RATE[TXn_RATE] (Set according to required
	      data rate (see Table 21-1). */
	rate.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_TX_RATE(0, 0));
	rate.s.tx0_rate = (mode0.s.mode == CVMX_GMX_INF_MODE_SGMII) ? 2 : 0;
	rate.s.tx1_rate = (mode1.s.mode == CVMX_GMX_INF_MODE_SGMII) ? 2 : 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_TX_RATE(0, 0), rate.u64);

	/* 2. Set GSER0_DLM0_TX_EN[TXn_EN] = 1 */
	en.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_TX_EN(0, 0));
	en.s.tx0_en = need0;
	en.s.tx1_en = need1;
	cvmx_write_csr(CVMX_GSERX_DLMX_TX_EN(0, 0), en.u64);

	/* 3 set GSER0_DLM0_TX_CM_EN[TXn_CM_EN] = 1 */
	cm_en.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_TX_CM_EN(0, 0));
	cm_en.s.tx0_cm_en = need0;
	cm_en.s.tx1_cm_en = need1;
	cvmx_write_csr(CVMX_GSERX_DLMX_TX_CM_EN(0, 0), cm_en.u64);

	/* 4. Set GSER0_DLM0_TX_DATA_EN[TXn_DATA_EN] = 1 */
	data_en.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_TX_DATA_EN(0, 0));
	data_en.s.tx0_data_en = need0;
	data_en.s.tx1_data_en = need1;
	cvmx_write_csr(CVMX_GSERX_DLMX_TX_DATA_EN(0, 0), data_en.u64);

	/* 5. Clear GSER0_DLM0_TX_RESET[TXn_DATA_EN] = 0 */
	tx_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_TX_RESET(0, 0));
	tx_reset.s.tx0_reset = !need0;
	tx_reset.s.tx1_reset = !need1;
	cvmx_write_csr(CVMX_GSERX_DLMX_TX_RESET(0, 0), tx_reset.u64);

	/* 6. Poll GSER0_DLM0_TX_STATUS[TXn_STATUS, TXn_CM_STATUS] until both
	 *    are set to 1. This prevents GMX from transmitting until the DLM
	 *    is ready.
	 */
	if ((gd->arch.board_desc.board_type != CVMX_BOARD_TYPE_SIM) && need0) {
		if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_TX_STATUS(0, 0),
			cvmx_gserx_dlmx_tx_status_t, tx0_status, ==, 1, 10000)) {
			cvmx_warn("DLM0 TX0 status fail\n");
			return -1;
		}
		if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_TX_STATUS(0, 0),
			cvmx_gserx_dlmx_tx_status_t, tx0_cm_status, ==, 1, 10000)) {
			cvmx_warn("DLM0 TX0 CM status fail\n");
			return -1;
		}
	}
	if ((gd->arch.board_desc.board_type != CVMX_BOARD_TYPE_SIM) && need1) {
		if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_TX_STATUS(0, 0),
			cvmx_gserx_dlmx_tx_status_t, tx1_status, ==, 1, 10000)) {
			cvmx_warn("DLM0 TX1 status fail\n");
			return -1;
		}
		if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_TX_STATUS(0, 0),
			cvmx_gserx_dlmx_tx_status_t, tx1_cm_status, ==, 1, 10000)) {
			cvmx_warn("DLM0 TX1 CM status fail\n");
			return -1;
		}
	}
	return 0;
}

static int __dlm0_setup_rx_cn70xx(void)
{
	int need0, need1;
	cvmx_gmxx_inf_mode_t mode0, mode1;
	cvmx_gserx_dlmx_rx_rate_t rate;
	cvmx_gserx_dlmx_rx_pll_en_t pll_en;
	cvmx_gserx_dlmx_rx_data_en_t data_en;
	cvmx_gserx_dlmx_rx_reset_t rx_reset;

	mode0.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(0));
	mode1.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(1));

	/* Which lanes do we need? */
	need0 = (mode0.s.mode != CVMX_GMX_INF_MODE_DISABLED);
	need1 = (mode1.s.mode != CVMX_GMX_INF_MODE_DISABLED)
		 || (mode0.s.mode == CVMX_GMX_INF_MODE_RXAUI);

	/* 1. Write GSER0_DLM0_RX_RATE[RXn_RATE] (must match the
	   GER0_DLM0_TX_RATE[TXn_RATE] setting). */
	rate.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_RX_RATE(0, 0));
	rate.s.rx0_rate = (mode0.s.mode == CVMX_GMX_INF_MODE_SGMII) ? 2 : 0;
	rate.s.rx1_rate = (mode1.s.mode == CVMX_GMX_INF_MODE_SGMII) ? 2 : 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_RX_RATE(0, 0), rate.u64);

	/* 2. Set GSER0_DLM0_RX_PLL_EN[RXn_PLL_EN] = 1 */
	pll_en.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_RX_PLL_EN(0, 0));
	pll_en.s.rx0_pll_en = need0;
	pll_en.s.rx1_pll_en = need1;
	cvmx_write_csr(CVMX_GSERX_DLMX_RX_PLL_EN(0, 0), pll_en.u64);

	/* 3. Set GSER0_DLM0_RX_DATA_EN[RXn_DATA_EN] = 1 */
	data_en.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_RX_DATA_EN(0, 0));
	data_en.s.rx0_data_en = need0;
	data_en.s.rx1_data_en = need1;
	cvmx_write_csr(CVMX_GSERX_DLMX_RX_DATA_EN(0, 0), data_en.u64);

	/* 4. Clear GSER0_DLM0_RX_RESET[RXn_DATA_EN] = 0. Now the GMX can be
	   enabled: set GMX(0..1)_INF_MODE[EN] = 1 */
	rx_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_RX_RESET(0, 0));
	rx_reset.s.rx0_reset = !need0;
	rx_reset.s.rx1_reset = !need1;
	cvmx_write_csr(CVMX_GSERX_DLMX_RX_RESET(0, 0), rx_reset.u64);

	return 0;
}

static const uint8_t sata_clkdiv_table[8] = {1, 2, 3, 4, 6, 8, 16, 24};

static int __dlm2_sata_uctl_init_cn70xx(void)
{
	int i;
	cvmx_sata_uctl_ctl_t uctl_ctl;

	/* Wait for all voltages to reach a stable stable. Ensure the
	 * reference clock is up and stable.
	 */

	/* 2. Wait for IOI reset to deassert. */

	/* 3. Optionally program the GPIO CSRs for SATA features.
	 *    a. For cold-presence detect:
	 *	 i. Select a GPIO for the input and program GPIO_SATA_CTL[sel]
	 *	    for port0 and port1.
	 *	 ii. Select a GPIO for the output and program
	 *	     GPIO_BIT_CFG*[OUTPUT_SEL] for port0 and port1.
	 *    b. For mechanical-presence detect, select a GPIO for the input
	 *	 and program GPIO_SATA_CTL[SEL] for port0/port1.
	 *    c. For LED activity, select a GPIO for the output and program
	 *	 GPIO_BIT_CFG*[OUTPUT_SEL] for port0/port1.
	 */

	/* 4. Assert all resets:
	 *    a. UAHC reset: SATA_UCTL_CTL[UAHC_RST] = 1
	 *    a. UCTL reset: SATA_UCTL_CTL[UCTL_RST] = 1
	 */

	uctl_ctl.u64 = cvmx_read_csr(CVMX_SATA_UCTL_CTL);
	uctl_ctl.s.sata_uahc_rst = 1;
	uctl_ctl.s.sata_uctl_rst = 1;
	cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

	/* 5. Configure the ACLK:
	 *    a. Reset the clock dividers: SATA_UCTL_CTL[A_CLKDIV_RST] = 1.
	 *    b. Select the ACLK frequency (400 MHz maximum)
	 *	 i. SATA_UCTL_CTL[A_CLKDIV] = desired value,
	 *	 ii. SATA_UCTL_CTL[A_CLKDIV_EN] = 1 to enable the ACLK,
	 *    c. Deassert the ACLK clock divider reset:
	 *	 SATA_UCTL_CTL[A_CLKDIV_RST] = 0
	 */
	uctl_ctl.s.a_clkdiv_rst = 1;
	cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

	uctl_ctl.s.a_clkdiv_sel = 0;
	for (i = 0; i < 8; i++) {
		if (cvmx_clock_get_rate(CVMX_CLOCK_SCLK) / sata_clkdiv_table[i] <= 400000000) {
			uctl_ctl.s.a_clkdiv_sel = i;
			break;
		}
	}
	uctl_ctl.s.a_clk_en = 1;
	uctl_ctl.s.a_clk_byp_sel = 0;
	cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

	uctl_ctl.s.a_clkdiv_rst = 0;
	cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

	/* 6. Deassert UCTL and UAHC resets:
	 *    a. SATA_UCTL_CTL[UCTL_RST] = 0
	 *    b. SATA_UCTL_CTL[UAHC_RST] = 0
	 *    c. Wait 10 ACLK cycles before accessing any ACLK-only registers.
	 */

	uctl_ctl.s.sata_uctl_rst = 0;
	cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

	uctl_ctl.s.sata_uahc_rst = 0;
	cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

	cvmx_wait(50);

	/* 7. Enable conditional SCLK of UCTL by writing
	      SATA_UCTL_CTL[CSCLK_EN] = 1 */

	uctl_ctl.s.csclk_en = 1;
	cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

#if 0
	/* 8. Configure PHY for SATA:
	 *    a. Enable SATA: SATA_CFG[SATA_EN] = 1
	 *    b. Program spread spectrum clock as desired:
	 *	 i. SATA_SSC_EN
	 *	 ii. SATA_SSC_RANGE
	 *	 iii. SATA_SSC_CLK_SEL
	 *    c. Clear lane resets: SATA_LANE_RST[L*RST] = 0
	 *    d. Enable reference clock: SATA_REF_SSP_EN[REF_SSP_EN] = 1
	 *    e. Program reference clock divide-by-2 based on reference clock
	 *	 frequence.
	 *	 i. DLM2_REF_CLKDIV2[REF_CLKDIV2] = 0 (for 100 Mhz ref clock)
	 *	 ii. DLM2_REF_CLKDIV2[REF_CLKDIV2] = 1 (for 125 Mhz ref clock)
	 */
#endif

	return 0;
}

static int __dlm2_sata_dlm_init_cn70xx(int qlm, int baud_mhz, int ref_clk_sel,
				int ref_clk_input)
{
	cvmx_gserx_sata_cfg_t sata_cfg;
	cvmx_gserx_sata_lane_rst_t sata_lane_rst;
	cvmx_gserx_dlmx_phy_reset_t dlmx_phy_reset;
	cvmx_gserx_dlmx_test_powerdown_t dlmx_test_powerdown;
	cvmx_gserx_sata_ref_ssp_en_t ref_ssp_en;
	cvmx_gserx_dlmx_mpll_multiplier_t mpll_multiplier;
	cvmx_gserx_dlmx_ref_clkdiv2_t ref_clkdiv2;
	cvmx_sata_uctl_shim_cfg_t shim_cfg;
	cvmx_gserx_phyx_ovrd_in_lo_t ovrd_in;
	int sata_ref_clk = 100;

	/* 5. Set GSERX0_SATA_CFG[SATA_EN] = 1 to configure DLM2 multiplexing.
	 */
	sata_cfg.u64 = cvmx_read_csr(CVMX_GSERX_SATA_CFG(0));
	sata_cfg.s.sata_en = 1;
	cvmx_write_csr(CVMX_GSERX_SATA_CFG(0), sata_cfg.u64);

	/* 6. Clear either/both lane0 and lane1 resets:
	 *    GSER0_SATA_LANE_RST[L0_RST, L1_RST] = 0.
	 */
	sata_lane_rst.u64 = cvmx_read_csr(CVMX_GSERX_SATA_LANE_RST(0));
	sata_lane_rst.s.l0_rst = 0;
	sata_lane_rst.s.l1_rst = 0;
	cvmx_write_csr(CVMX_GSERX_SATA_LANE_RST(0), sata_lane_rst.u64);

	/* 1. Write GSER(0)_DLM2_REFCLK_SEL[REFCLK_SEL] if required for
	 *    reference-clock selection.
	 */

	cvmx_write_csr(CVMX_GSERX_DLMX_REFCLK_SEL(qlm, 0), ref_clk_sel);

	ref_ssp_en.u64 = cvmx_read_csr(CVMX_GSERX_SATA_REF_SSP_EN(0));
	ref_ssp_en.s.ref_ssp_en = 1;
	cvmx_write_csr(CVMX_GSERX_SATA_REF_SSP_EN(0), ref_ssp_en.u64);

	/* Apply workaround for Errata (G-20669) MPLL may not come up. */

	/* Set REF_CLKDIV2 based on the Ref Clock */
	ref_clkdiv2.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_REF_CLKDIV2(qlm, 0));
	if (sata_ref_clk == 100)
		ref_clkdiv2.s.ref_clkdiv2 = 0;
	else
		ref_clkdiv2.s.ref_clkdiv2 = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_REF_CLKDIV2(qlm, 0), ref_clkdiv2.u64);

	/* 1. Ensure GSER(0)_DLM(0..2)_PHY_RESET[PHY_RESET] is set. */
	dlmx_phy_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0));
	dlmx_phy_reset.s.phy_reset = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0), dlmx_phy_reset.u64);

	/* 2. If SGMII or QSGMII or RXAUI (i.e. if DLM0) set
	      GSER(0)_DLM(0)_MPLL_EN[MPLL_EN] to one. */

	/* 3. Set GSER(0)_DLM(0..2)_MPLL_MULTIPLIER[MPLL_MULTIPLIER]
	      to the value in the preceding table, which is different
	      than the desired setting prescribed by the HRM. */

	mpll_multiplier.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0));
	if (sata_ref_clk == 100)
		mpll_multiplier.s.mpll_multiplier = 35;
	else
		mpll_multiplier.s.mpll_multiplier = 56;
	cvmx_write_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0), mpll_multiplier.u64);

	/* 3. Clear GSER0_DLM2_TEST_POWERDOWN[TEST_POWERDOWN] = 0 */
	dlmx_test_powerdown.u64 =
			cvmx_read_csr(CVMX_GSERX_DLMX_TEST_POWERDOWN(qlm, 0));
	dlmx_test_powerdown.s.test_powerdown = 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_TEST_POWERDOWN(qlm, 0),
				dlmx_test_powerdown.u64);

	/* 4. Clear GSER0_DLM2_PHY_RESET */
	dlmx_phy_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0));
	dlmx_phy_reset.s.phy_reset = 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0), dlmx_phy_reset.u64);

	/* 5. If PCIe or SATA (i.e. if DLM1 or DLM2), set both MPLL_EN
	   and MPLL_EN_OVRD to one in GSER(0)_PHY(1..2)_OVRD_IN_LO. */
	ovrd_in.u64 = cvmx_read_csr(CVMX_GSERX_PHYX_OVRD_IN_LO(qlm, 0));
	ovrd_in.s.mpll_en = 1;
	ovrd_in.s.mpll_en_ovrd = 1;
	cvmx_write_csr(CVMX_GSERX_PHYX_OVRD_IN_LO(qlm, 0), ovrd_in.u64);

	/* 6. Decrease MPLL_MULTIPLIER by one continually until it reaches
	     the desired long-term setting, ensuring that each MPLL_MULTIPLIER
	     value is constant for at least 1 msec before changing to the next
	     value. The desired long-term setting is as indicated in HRM tables
	     21-1, 21-2, and 21-3. This is not required with the HRM
	     sequence. */
	mpll_multiplier.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0));
	if (sata_ref_clk == 100)
		mpll_multiplier.s.mpll_multiplier = 0x1e;
	else
		mpll_multiplier.s.mpll_multiplier = 0x30;
	cvmx_write_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0), mpll_multiplier.u64);

	if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_MPLL_STATUS(qlm, 0),
		cvmx_gserx_dlmx_mpll_status_t, mpll_status, ==, 1, 10000)) {
		printf("ERROR: SATA MPLL failed to set\n");
		return -1;
	}

	if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_RX_STATUS(qlm, 0),
		cvmx_gserx_dlmx_rx_status_t, rx0_status, ==, 1, 10000)) {
		printf("ERROR: SATA RX0_STATUS failed to set\n");
		return -1;
	}
	if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_DLMX_RX_STATUS(qlm, 0),
		cvmx_gserx_dlmx_rx_status_t, rx1_status, ==, 1, 10000)) {
		printf("ERROR: SATA RX1_STATUS failed to set\n");
		return -1;
	}

	/* 10. Initialize UAHC as described in the AHCI Specification (UAHC_*
	 *     registers
	 */

	/* set-up endian mode */
	shim_cfg.u64 = cvmx_read_csr(CVMX_SATA_UCTL_SHIM_CFG);
	shim_cfg.s.dma_endian_mode = 1;
	shim_cfg.s.csr_endian_mode = 3;
	cvmx_write_csr(CVMX_SATA_UCTL_SHIM_CFG, shim_cfg.u64);

	return 0;
}

static int __dlm2_sata_uahc_init_cn70xx(void)
{
	cvmx_sata_uahc_gbl_cap_t gbl_cap;
	cvmx_sata_uahc_px_sctl_t sctl;
	cvmx_sata_uahc_gbl_pi_t pi;
	cvmx_sata_uahc_px_cmd_t cmd;
	cvmx_sata_uahc_px_sctl_t sctl0, sctl1;
	cvmx_sata_uahc_px_ssts_t ssts;
	uint64_t done;
	int result = -1;

	/* Set-u global capabilities reg (GBL_CAP) */
	gbl_cap.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_GBL_CAP);
	gbl_cap.s.sss = 1;
	gbl_cap.s.smps = 1;
	cvmx_write64_int32(CVMX_SATA_UAHC_GBL_CAP, gbl_cap.u32);

	/* Set-up global hba control reg (interrupt enables) */
	/* Set-up port SATA control registers (speed limitation) */

	sctl.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_PX_SCTL(0));
	sctl.s.spd = 3;
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_SCTL(0), sctl.u32);
	sctl.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_PX_SCTL(1));
	sctl.s.spd = 3;
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_SCTL(1), sctl.u32);

	/* Set-up ports implemented reg. */
	pi.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_GBL_PI);
	pi.s.pi = 3;
	cvmx_write64_int32(CVMX_SATA_UAHC_GBL_PI, pi.u32);

	/* Clear port SERR and IS registers */
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_SERR(0),
			cvmx_read64_int32(CVMX_SATA_UAHC_PX_SERR(0)));
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_SERR(1),
			cvmx_read64_int32(CVMX_SATA_UAHC_PX_SERR(1)));
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_IS(0),
			cvmx_read64_int32(CVMX_SATA_UAHC_PX_IS(0)));
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_IS(1),
			cvmx_read64_int32(CVMX_SATA_UAHC_PX_IS(1)));

	/* Set spin-up, power on, FIS RX enable, start, active */
	cmd.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_PX_CMD(0));
	cmd.s.fre = 1;
	cmd.s.sud = 1;
	cmd.s.pod = 1;
	cmd.s.st = 1;
	cmd.s.icc = 1;
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_CMD(0), cmd.u32);
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_CMD(1), cmd.u32);

	sctl0.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_PX_SCTL(0));
	sctl0.s.det = 1;
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_SCTL(0), sctl0.u32);

	/* check status */
        done = cvmx_clock_get_count(CVMX_CLOCK_CORE) + 1000000 *
                        cvmx_clock_get_rate(CVMX_CLOCK_CORE) / 1000000;
	while (1) {
		ssts.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_PX_SSTS(0));

		if ((ssts.s.ipm == 1) && (ssts.s.det == 3)) {
			result = 0;
			break;
		} else if (cvmx_clock_get_count(CVMX_CLOCK_CORE) > done) {
			result = -1;
			break;
		} else
			cvmx_wait(100);
	}

	if (result == -1) {
		printf("SATA0: not available\n");
#if 0
		printf("ERROR: Failed to detect SATA0 device: 0x%lx\n",
		       cvmx_read64_int32(CVMX_SATA_UAHC_PX_SSTS(0)));

		return -1;
#endif
	} else
		printf("SATA0: available\n");

	sctl1.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_PX_SCTL(1));
	sctl1.s.det = 1;
	cvmx_write64_int32(CVMX_SATA_UAHC_PX_SCTL(1), sctl1.u32);

	/* check status */
        done = cvmx_clock_get_count(CVMX_CLOCK_CORE) + 1000000 *
                        cvmx_clock_get_rate(CVMX_CLOCK_CORE) / 1000000;
	while (1) {
		ssts.u32 = cvmx_read64_int32(CVMX_SATA_UAHC_PX_SSTS(1));

		if ((ssts.s.ipm == 1) && (ssts.s.det == 3)) {
			result = 0;
			break;
		} else if (cvmx_clock_get_count(CVMX_CLOCK_CORE) > done) {
			result = -1;
			break;
		} else
			cvmx_wait(100);
	}

	if (result == -1) {
		printf("SATA1: not available\n");
#if 0
		printf("ERROR: Failed to detect SATA1 device: 0x%lx\n",
		       cvmx_read64_int32(CVMX_SATA_UAHC_PX_SSTS(1)));
		return -1;
#endif
	} else
		printf("SATA1: available\n");

	return 0;
}

static int __sata_bist_cn70xx(int qlm, int baud_mhz, int ref_clk_sel,
			      int ref_clk_input)
{
	cvmx_sata_uctl_bist_status_t bist_status;
	cvmx_sata_uctl_ctl_t uctl_ctl;
	cvmx_sata_uctl_shim_cfg_t shim_cfg;
	uint64_t done;
	int result = -1;

	bist_status.u64 = cvmx_read_csr(CVMX_SATA_UCTL_BIST_STATUS);
	if (bist_status.s.uctl_xm_r_bist_ndone &&
	    bist_status.s.uctl_xm_w_bist_ndone &&
	    bist_status.s.uahc_p0_rxram_bist_ndone &&
	    bist_status.s.uahc_p1_rxram_bist_ndone &&
	    bist_status.s.uahc_p0_txram_bist_ndone &&
	    bist_status.s.uahc_p1_txram_bist_ndone) {
		if (__dlm2_sata_uctl_init_cn70xx()) {
			printf("ERROR: Failed to initialize SATA UCTL CSRs\n");
			return -1;
		}
		if (__dlm2_sata_dlm_init_cn70xx(qlm, baud_mhz, ref_clk_sel,
						ref_clk_input)) {
			printf("ERROR: Failed to initialize SATA GSER CSRs\n");
			return -1;
		}
		if (__dlm2_sata_uahc_init_cn70xx()) {
			printf("ERROR: Failed to initialize SATA UAHC CSRs\n");
			return -1;
		}
		uctl_ctl.u64 = cvmx_read_csr(CVMX_SATA_UCTL_CTL);
		uctl_ctl.s.start_bist = 1;
		cvmx_write_csr(CVMX_SATA_UCTL_CTL, uctl_ctl.u64);

		/* Set-up for a 1 sec timer. */
        	done = cvmx_clock_get_count(CVMX_CLOCK_CORE) + 1000000 *
                        cvmx_clock_get_rate(CVMX_CLOCK_CORE) / 1000000;
		while (1) {
			bist_status.u64 = cvmx_read_csr(CVMX_SATA_UCTL_BIST_STATUS);
			if ((bist_status.s.uctl_xm_r_bist_ndone |
			     bist_status.s.uctl_xm_w_bist_ndone |
			     bist_status.s.uahc_p0_rxram_bist_ndone |
			     bist_status.s.uahc_p1_rxram_bist_ndone |
			     bist_status.s.uahc_p0_txram_bist_ndone |
			     bist_status.s.uahc_p1_txram_bist_ndone) == 0) {
				result = 0;
				break;
			} else if (cvmx_clock_get_count(CVMX_CLOCK_CORE) > done) {
				result = -1;
				break;
			} else
				cvmx_wait(100);
		}
		if (result == -1) {
			cvmx_warn("ERROR: SATA_UCTL_BIST_STATUS = 0x%llx\n",
					 bist_status.u64);
			return -1;
		}
	}

	/* Change CSR_ENDIAN_MODE to big endian to use Open Source xHCI SATA
	   driver */
	shim_cfg.u64 = cvmx_read_csr(CVMX_SATA_UCTL_SHIM_CFG);
	shim_cfg.s.csr_endian_mode = 1;
	cvmx_write_csr(CVMX_SATA_UCTL_SHIM_CFG, shim_cfg.u64);

	printf("SATA BIST STATUS = 0x%llx\n",
	       cvmx_read_csr(CVMX_SATA_UCTL_BIST_STATUS));

	return 0;
}

static int __dlm2_setup_sata_cn70xx(int qlm, int baud_mhz, int ref_clk_sel,
				    int ref_clk_input)
{
	return __sata_bist_cn70xx(qlm, baud_mhz, ref_clk_sel, ref_clk_input);
}


static int __dlmx_setup_pcie_cn70xx(int qlm, enum cvmx_qlm_mode mode,
				int gen2, int rc, int ref_clk_sel,
				int ref_clk_input)
{
	cvmx_gserx_dlmx_phy_reset_t dlmx_phy_reset;
	cvmx_gserx_dlmx_test_powerdown_t dlmx_test_powerdown;
	cvmx_gserx_dlmx_mpll_multiplier_t mpll_multiplier;
	cvmx_gserx_dlmx_ref_clkdiv2_t ref_clkdiv2;

	if (rc == 0) {
		debug("Skipping initializing PCIe dlm %d in endpoint mode\n",
		      qlm);
		return 0;
	}

	/* 1. Write GSER0_DLM(1..2)_REFCLK_SEL[REFCLK_SEL] if required for
	 *    reference-clock selection
	 */

	cvmx_write_csr(CVMX_GSERX_DLMX_REFCLK_SEL(qlm, 0), ref_clk_sel);

	/* 2. If required, write GSER0_DLM(1..2)_REF_CLKDIV2[REF_CLKDIV2] = 1
	 *    (must be set if reference clock >= 100 MHz)
	 */

	/* 4. Configure the PCIE PIPE:
	 *  a. Write GSER0_PCIE_PIPE_PORT_SEL[PIPE_PORT_SEL] to configure the
	 *     PCIE PIPE.
	 *	0x0 = disables all pipes
	 *	0x1 = enables pipe0 only (PEM0 4-lane)
	 *	0x2 = enables pipes 0 and 1 (PEM0 and PEM1 2-lanes each)
	 *	0x3 = enables pipes 0, 1, 2, and 3 (PEM0, PEM1, and PEM3 are
	 *	      one-lane each)
	 *  b. Configure GSER0_PCIE_PIPE_PORT_SEL[CFG_PEM1_DLM2]. If PEM1 is
	 *     to be configured, this bit must reflect which DLM it is logically
	 *     tied to. This bit sets multiplexing logic in GSER, and it is used
	 *     by the RST logic to determine when the MAC can come out of reset.
	 *	0 = PEM1 is tied to DLM1 (for 3 x 1 PCIe mode).
	 *	1 = PEM1 is tied to DLM2 (for all other PCIe modes).
	 */
	if (qlm == 1) {
		cvmx_gserx_pcie_pipe_port_sel_t pipe_port;

		pipe_port.u64 = cvmx_read_csr(CVMX_GSERX_PCIE_PIPE_PORT_SEL(0));
		pipe_port.s.cfg_pem1_dlm2 =
				(mode == CVMX_QLM_MODE_PCIE_1X1) ? 1 : 0;
		pipe_port.s.pipe_port_sel =
				(mode == CVMX_QLM_MODE_PCIE) ? 1 : /* PEM0 only */
				(mode == CVMX_QLM_MODE_PCIE_1X2) ? 2 : /* PEM0-1 */
				(mode == CVMX_QLM_MODE_PCIE_1X1) ? 3 : /* PEM0-2 */
				(mode == CVMX_QLM_MODE_PCIE_2X1) ? 3 : /* PEM0-1 */
				0; /* PCIe disabled */
		cvmx_write_csr(CVMX_GSERX_PCIE_PIPE_PORT_SEL(0), pipe_port.u64);
	}

	/* Apply workaround for Errata (G-20669) MPLL may not come up. */

	/* Set REF_CLKDIV2 based on the Ref Clock */
	ref_clkdiv2.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_REF_CLKDIV2(qlm, 0));
	ref_clkdiv2.s.ref_clkdiv2 = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_REF_CLKDIV2(qlm, 0), ref_clkdiv2.u64);

	/* 1. Ensure GSER(0)_DLM(0..2)_PHY_RESET[PHY_RESET] is set. */
	dlmx_phy_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0));
	dlmx_phy_reset.s.phy_reset = 1;
	cvmx_write_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0), dlmx_phy_reset.u64);

	/* 2. If SGMII or QSGMII or RXAUI (i.e. if DLM0) set
	      GSER(0)_DLM(0)_MPLL_EN[MPLL_EN] to one. */

	/* 3. Set GSER(0)_DLM(0..2)_MPLL_MULTIPLIER[MPLL_MULTIPLIER]
	      to the value in the preceding table, which is different
	      than the desired setting prescribed by the HRM. */
	mpll_multiplier.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0));
	mpll_multiplier.s.mpll_multiplier = 56;
	cvmx_write_csr(CVMX_GSERX_DLMX_MPLL_MULTIPLIER(qlm, 0), mpll_multiplier.u64);
	/* 5. Clear GSER0_DLM(1..2)_TEST_POWERDOWN. Configurations that only
	 *    use DLM1 need not clear GSER0_DLM2_TEST_POWERDOWN
	 */
	dlmx_test_powerdown.u64 =
			cvmx_read_csr(CVMX_GSERX_DLMX_TEST_POWERDOWN(qlm, 0));
	dlmx_test_powerdown.s.test_powerdown = 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_TEST_POWERDOWN(qlm, 0),
		       dlmx_test_powerdown.u64);

	/* 6. Clear GSER0_DLM(1..2)_PHY_RESET. Configurations that use only
	 *    need DLM1 need not clear GSER0_DLM2_PHY_RESET
	 */
	dlmx_phy_reset.u64 = cvmx_read_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0));
	dlmx_phy_reset.s.phy_reset = 0;
	cvmx_write_csr(CVMX_GSERX_DLMX_PHY_RESET(qlm, 0), dlmx_phy_reset.u64);

	/* 6. Decrease MPLL_MULTIPLIER by one continually until it reaches
	     the desired long-term setting, ensuring that each MPLL_MULTIPLIER
	     value is constant for at least 1 msec before changing to the next
	     value. The desired long-term setting is as indicated in HRM tables
	     21-1, 21-2, and 21-3. This is not required with the HRM
	     sequence. */
	/* This is set when initializing PCIe after soft reset is asserted. */

	/* 7. Write the GSER0_PCIE_PIPE_RST register to take the appropriate
	 *    PIPE out of reset. There is a PIPEn_RST bit for each PIPE. Clear
	 *    the appropriate bits based on the configuration (reset is
	 *     active high).
	 */
	if (qlm == 1) {
		cvmx_pemx_cfg_t pemx_cfg;
		cvmx_pemx_on_t pemx_on;
		cvmx_gserx_pcie_pipe_rst_t pipe_rst;
		cvmx_rst_ctlx_t rst_ctl;

		switch(mode) {
		case CVMX_QLM_MODE_PCIE: /* PEM0 on DLM1 & DLM2 */
		case CVMX_QLM_MODE_PCIE_1X2: /* PEM0 on DLM1 */
		case CVMX_QLM_MODE_PCIE_1X1: /* PEM0 on DLM1 using lane 0 */
			pemx_cfg.u64 = cvmx_read_csr(CVMX_PEMX_CFG(0));
			pemx_cfg.cn70xx.hostmd = rc;
			if (mode == CVMX_QLM_MODE_PCIE_1X1) {
				pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_1LANE
						     : CVMX_PEM_MD_GEN1_1LANE;
			} else if (mode == CVMX_QLM_MODE_PCIE) {
				pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_4LANE
						     : CVMX_PEM_MD_GEN1_4LANE;
			} else {
				pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_2LANE
						     : CVMX_PEM_MD_GEN1_2LANE;
			}
			cvmx_write_csr(CVMX_PEMX_CFG(0), pemx_cfg.u64);

			rst_ctl.u64 = cvmx_read_csr(CVMX_RST_CTLX(0));
			rst_ctl.s.rst_drv = 1;
			cvmx_write_csr(CVMX_RST_CTLX(0), rst_ctl.u64);

			/* PEM0 is on DLM1&2 which is pipe0 */
			pipe_rst.u64 = cvmx_read_csr(CVMX_GSERX_PCIE_PIPE_RST(0));
			pipe_rst.s.pipe0_rst = 0;
			cvmx_write_csr(CVMX_GSERX_PCIE_PIPE_RST(0),
				       pipe_rst.u64);

			pemx_on.u64 = cvmx_read_csr(CVMX_PEMX_ON(0));
			pemx_on.s.pemon = 1;
			cvmx_write_csr(CVMX_PEMX_ON(0), pemx_on.u64);
			break;
		case CVMX_QLM_MODE_PCIE_2X1: /* PEM0 and PEM1 on DLM1 */
			pemx_cfg.u64 = cvmx_read_csr(CVMX_PEMX_CFG(0));
			pemx_cfg.cn70xx.hostmd = rc;
			pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_1LANE
					     : CVMX_PEM_MD_GEN1_1LANE;
			cvmx_write_csr(CVMX_PEMX_CFG(0), pemx_cfg.u64);

			rst_ctl.u64 = cvmx_read_csr(CVMX_RST_CTLX(0));
			rst_ctl.s.rst_drv = 1;
			cvmx_write_csr(CVMX_RST_CTLX(0), rst_ctl.u64);

			/* PEM0 is on DLM1 which is pipe0 */
			pipe_rst.u64 = cvmx_read_csr(CVMX_GSERX_PCIE_PIPE_RST(0));
			pipe_rst.s.pipe0_rst = 0;
			cvmx_write_csr(CVMX_GSERX_PCIE_PIPE_RST(0),
				       pipe_rst.u64);

			pemx_on.u64 = cvmx_read_csr(CVMX_PEMX_ON(0));
			pemx_on.s.pemon = 1;
			cvmx_write_csr(CVMX_PEMX_ON(0), pemx_on.u64);

			pemx_cfg.u64 = cvmx_read_csr(CVMX_PEMX_CFG(1));
			pemx_cfg.cn70xx.hostmd = 1;
			pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_1LANE
					     : CVMX_PEM_MD_GEN1_1LANE;
			cvmx_write_csr(CVMX_PEMX_CFG(1), pemx_cfg.u64);
			rst_ctl.u64 = cvmx_read_csr(CVMX_RST_CTLX(1));
			rst_ctl.s.rst_drv = 1;
			cvmx_write_csr(CVMX_RST_CTLX(1), rst_ctl.u64);
			/* PEM1 is on DLM2 which is pipe1 */
			pipe_rst.u64 = cvmx_read_csr(CVMX_GSERX_PCIE_PIPE_RST(0));
			pipe_rst.s.pipe1_rst = 0;
			cvmx_write_csr(CVMX_GSERX_PCIE_PIPE_RST(0),
				       pipe_rst.u64);
			pemx_on.u64 = cvmx_read_csr(CVMX_PEMX_ON(1));
			pemx_on.s.pemon = 1;
			cvmx_write_csr(CVMX_PEMX_ON(1), pemx_on.u64);
			break;
		default:
			break;
		}
	} else {
		cvmx_pemx_cfg_t pemx_cfg;
		cvmx_pemx_on_t pemx_on;
		cvmx_gserx_pcie_pipe_rst_t pipe_rst;
		cvmx_rst_ctlx_t rst_ctl;

		switch (mode) {
		case CVMX_QLM_MODE_PCIE_1X2:  /* PEM1 on DLM2 */
			pemx_cfg.u64 = cvmx_read_csr(CVMX_PEMX_CFG(1));
			pemx_cfg.cn70xx.hostmd = 1;
			pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_2LANE
					     : CVMX_PEM_MD_GEN1_2LANE;
			cvmx_write_csr(CVMX_PEMX_CFG(1), pemx_cfg.u64);

			rst_ctl.u64 = cvmx_read_csr(CVMX_RST_CTLX(1));
			rst_ctl.s.rst_drv = 1;
			cvmx_write_csr(CVMX_RST_CTLX(1), rst_ctl.u64);

			/* PEM1 is on DLM1 lane 0, which is pipe1 */
			pipe_rst.u64 = cvmx_read_csr(CVMX_GSERX_PCIE_PIPE_RST(0));
			pipe_rst.s.pipe1_rst = 0;
			cvmx_write_csr(CVMX_GSERX_PCIE_PIPE_RST(0),
				       pipe_rst.u64);

			pemx_on.u64 = cvmx_read_csr(CVMX_PEMX_ON(1));
			pemx_on.s.pemon = 1;
			cvmx_write_csr(CVMX_PEMX_ON(1), pemx_on.u64);
			break;
		case CVMX_QLM_MODE_PCIE_2X1:  /* PEM1 and PEM2 on DLM2 */
			pemx_cfg.u64 = cvmx_read_csr(CVMX_PEMX_CFG(1));
			pemx_cfg.cn70xx.hostmd = 1;
			pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_1LANE
					     : CVMX_PEM_MD_GEN1_1LANE;
			cvmx_write_csr(CVMX_PEMX_CFG(1), pemx_cfg.u64);

			rst_ctl.u64 = cvmx_read_csr(CVMX_RST_CTLX(1));
			rst_ctl.s.rst_drv = 1;
			cvmx_write_csr(CVMX_RST_CTLX(1), rst_ctl.u64);

			/* PEM1 is on DLM2 lane 0, which is pipe2 */
			pipe_rst.u64 = cvmx_read_csr(CVMX_GSERX_PCIE_PIPE_RST(0));
			pipe_rst.s.pipe2_rst = 0;
			cvmx_write_csr(CVMX_GSERX_PCIE_PIPE_RST(0),
				       pipe_rst.u64);

			pemx_on.u64 = cvmx_read_csr(CVMX_PEMX_ON(1));
			pemx_on.s.pemon = 1;
			cvmx_write_csr(CVMX_PEMX_ON(1), pemx_on.u64);

			pemx_cfg.u64 = cvmx_read_csr(CVMX_PEMX_CFG(2));
			pemx_cfg.cn70xx.hostmd = 1;
			pemx_cfg.cn70xx.md = gen2 ? CVMX_PEM_MD_GEN2_1LANE
					     : CVMX_PEM_MD_GEN1_1LANE;
			cvmx_write_csr(CVMX_PEMX_CFG(2), pemx_cfg.u64);

			rst_ctl.u64 = cvmx_read_csr(CVMX_RST_CTLX(2));
			rst_ctl.s.rst_drv = 1;
			cvmx_write_csr(CVMX_RST_CTLX(2), rst_ctl.u64);

			/* PEM2 is on DLM2 lane 1, which is pipe3 */
			pipe_rst.u64 = cvmx_read_csr(CVMX_GSERX_PCIE_PIPE_RST(0));
			pipe_rst.s.pipe3_rst = 0;
			cvmx_write_csr(CVMX_GSERX_PCIE_PIPE_RST(0),
				       pipe_rst.u64);

			pemx_on.u64 = cvmx_read_csr(CVMX_PEMX_ON(2));
			pemx_on.s.pemon = 1;
			cvmx_write_csr(CVMX_PEMX_ON(2), pemx_on.u64);
			break;
		default:
			break;
		}
	}
	return 0;
}

/**
 * Configure dlm speed and mode for cn70xx.
 *
 * @param qlm     The DLM to configure
 * @param speed   The speed the DLM needs to be configured in Mhz.
 * @param mode    The DLM to be configured as SGMII/XAUI/PCIe.
 *                  DLM 0: has 2 interfaces which can be configured as
 *                         SGMII/QSGMII/RXAUI. Need to configure both at the
 *                         same time. These are valid option
 *				CVMX_QLM_MODE_QSGMII,
 *				CVMX_QLM_MODE_SGMII_SGMII,
 *				CVMX_QLM_MODE_SGMII_DISABLED,
 *				CVMX_QLM_MODE_DISABLED_SGMII,
 *				CVMX_QLM_MODE_SGMII_QSGMII,
 *				CVMX_QLM_MODE_QSGMII_QSGMII,
 *				CVMX_QLM_MODE_QSGMII_DISABLED,
 *				CVMX_QLM_MODE_DISABLED_QSGMII,
 *				CVMX_QLM_MODE_QSGMII_SGMII,
 *				CVMX_QLM_MODE_RXAUI_1X2
 *
 *                  DLM 1: PEM0/1 in PCIE_1x4/PCIE_2x1/PCIE_1X1
 *                  DLM 2: PEM0/1/2 in PCIE_1x4/PCIE_1x2/PCIE_2x1/PCIE_1x1
 * @param rc      Only used for PCIe, rc = 1 for root complex mode, 0 for EP mode.
 * @param gen2    Only used for PCIe, gen2 = 1, in GEN2 mode else in GEN1 mode.
 *
 * @param ref_clk_input  The reference-clock input to use to configure QLM
 * @param ref_clk_sel    The reference-clock selection to use to configure QLM
 *
 * @return       Return 0 on success or -1.
 */
static int octeon_configure_qlm_cn70xx(int qlm, int speed, int mode, int rc,
				       int gen2, int ref_clk_sel,
				       int ref_clk_input)
{
	switch (qlm) {
	case 0:
		{
			int is_sff7000_rxaui = 0;
			cvmx_gmxx_inf_mode_t inf_mode0, inf_mode1;

			inf_mode0.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(0));
			inf_mode1.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(1));
			if (inf_mode0.s.en || inf_mode1.s.en) {
				cvmx_dprintf("DLM0 already configured\n");
				return -1;
			}

			switch (mode) {
			case CVMX_QLM_MODE_SGMII_SGMII:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_SGMII;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_SGMII;
				break;
			case CVMX_QLM_MODE_SGMII_QSGMII:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_SGMII;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_QSGMII;
				break;
			case CVMX_QLM_MODE_SGMII_DISABLED:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_SGMII;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_DISABLED;
				break;
			case CVMX_QLM_MODE_DISABLED_SGMII:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_DISABLED;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_SGMII;
				break;
			case CVMX_QLM_MODE_QSGMII_SGMII:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_QSGMII;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_SGMII;
				break;
			case CVMX_QLM_MODE_QSGMII_QSGMII:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_QSGMII;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_QSGMII;
				break;
			case CVMX_QLM_MODE_QSGMII_DISABLED:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_QSGMII;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_DISABLED;
				break;
			case CVMX_QLM_MODE_DISABLED_QSGMII:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_DISABLED;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_QSGMII;
				break;
			case CVMX_QLM_MODE_RXAUI:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_RXAUI;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_DISABLED;
				if (gd->arch.board_desc.board_type == CVMX_BOARD_TYPE_SFF7000) {
					is_sff7000_rxaui = 1;
				}

				break;
			default:
				inf_mode0.s.mode = CVMX_GMX_INF_MODE_DISABLED;
				inf_mode1.s.mode = CVMX_GMX_INF_MODE_DISABLED;
				break;
			}
			cvmx_write_csr(CVMX_GMXX_INF_MODE(0), inf_mode0.u64);
			cvmx_write_csr(CVMX_GMXX_INF_MODE(1), inf_mode1.u64);

			/* Bringup the PLL */
			if (__dlm_setup_pll_cn70xx(qlm, speed, ref_clk_sel,
					 ref_clk_input, is_sff7000_rxaui))
				return -1;

			/* TX Lanes */
			if (__dlm0_setup_tx_cn70xx())
				return -1;

			/* RX Lanes */
			if (__dlm0_setup_rx_cn70xx())
				return -1;

			/* Enable the interface */
			inf_mode0.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(0));
			if (inf_mode0.s.mode != CVMX_GMX_INF_MODE_DISABLED)
				inf_mode0.s.en = 1;
			cvmx_write_csr(CVMX_GMXX_INF_MODE(0), inf_mode0.u64);
			inf_mode1.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(1));
			if (inf_mode1.s.mode != CVMX_GMX_INF_MODE_DISABLED)
				inf_mode1.s.en = 1;
			cvmx_write_csr(CVMX_GMXX_INF_MODE(1), inf_mode1.u64);
			break;
		}
	case 1:
		switch (mode) {
		case CVMX_QLM_MODE_PCIE: /* PEM0 on DLM1 & DLM2 */
			if (__dlmx_setup_pcie_cn70xx(1, mode, gen2, rc,
						     ref_clk_sel,
						     ref_clk_input))
				return -1;
			if (__dlmx_setup_pcie_cn70xx(2, mode, gen2, rc,
						     ref_clk_sel,
						     ref_clk_input))
				return -1;
			break;
		case CVMX_QLM_MODE_PCIE_1X2: /* PEM0 on DLM1 */
		case CVMX_QLM_MODE_PCIE_2X1: /* PEM0 & PEM1 on DLM1 */
		case CVMX_QLM_MODE_PCIE_1X1: /* PEM0 on DLM1, only 1 lane */
			if (__dlmx_setup_pcie_cn70xx(qlm, mode, gen2, rc,
						     ref_clk_sel,
						     ref_clk_input))
				return -1;
			break;
		case CVMX_QLM_MODE_DISABLED:
			break;
		default:
			debug("DLM1 illegal mode specified\n");
			return -1;
		}
		break;
	case 2:
		switch (mode) {
		case CVMX_QLM_MODE_SATA_2X1:
			/* DLM2 is SATA, PCIE2 is disabled */
			if (__dlm2_setup_sata_cn70xx(qlm, speed, ref_clk_sel,
						     ref_clk_input))
				return -1;
			break;
		case CVMX_QLM_MODE_PCIE:
			/* DLM2 is PCIE0, PCIE1-2 are disabled. */
			/* Do nothing, its initialized in DLM1 */
			break;
		case CVMX_QLM_MODE_PCIE_1X2: /* PEM1 on DLM2 */
		case CVMX_QLM_MODE_PCIE_2X1: /* PEM1 & PEM2 on DLM2 */
			if (__dlmx_setup_pcie_cn70xx(qlm, mode, gen2, rc,
						     ref_clk_sel,
						     ref_clk_input))
				return -1;
			break;
		case CVMX_QLM_MODE_DISABLED:
			break;
		default:
			debug("DLM2 illegal mode specified\n");
			return -1;
		}
	default:
			return -1;
	}

	return 0;
}

/**
 * Some QLM speeds need to override the default tuning parameters
 *
 * @param node     Node to configure
 * @param qlm      QLM to configure
 * @param mode     Desired mode
 * @param baud_mhz Desired speed
 */
static void __qlm_tune_cn78xx(int node, int qlm, int mode, int baud_mhz)
{
	int lane;
	if (baud_mhz == 6250) {
		/* Change the default tuning for 6.25G, from lab measurements */
		cvmx_gserx_lanex_tx_cfg_0_t tx_cfg0;
		cvmx_gserx_lanex_tx_pre_emphasis_t pre_emphasis;
		cvmx_gserx_lanex_tx_cfg_1_t tx_cfg1;

		for (lane = 0; lane < 4; lane++) {
			if (mode == CVMX_QLM_MODE_OCI) {
				tx_cfg0.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(lane, qlm));
				tx_cfg0.s.cfg_tx_swing = 0xc;
				cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(lane, qlm), tx_cfg0.u64);
				pre_emphasis.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_PRE_EMPHASIS(lane, qlm));
				pre_emphasis.s.cfg_tx_premptap = 0xc0;
				cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_PRE_EMPHASIS(lane, qlm), pre_emphasis.u64);
			} else {
				tx_cfg0.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(lane, qlm));
				tx_cfg0.s.cfg_tx_swing = 0xd;
				cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(lane, qlm), tx_cfg0.u64);

				pre_emphasis.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_PRE_EMPHASIS(lane, qlm));
				pre_emphasis.s.cfg_tx_premptap = 0xd0;
				cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_PRE_EMPHASIS(lane, qlm), pre_emphasis.u64);
			}
			tx_cfg1.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_1(lane, qlm));
			tx_cfg1.s.tx_swing_ovrd_en = 1;
			tx_cfg1.s.tx_premptap_ovrd_val = 1;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_1(lane, qlm), tx_cfg1.u64);
		}
	} else if (baud_mhz == 103125) {
		/* Change the default tuning for 10.3125G, from lab measurements */
		cvmx_gserx_lanex_tx_cfg_0_t tx_cfg0;
		cvmx_gserx_lanex_tx_pre_emphasis_t pre_emphasis;
		cvmx_gserx_lanex_tx_cfg_1_t tx_cfg1;

		for (lane = 0; lane < 4; lane++) {
			tx_cfg0.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(lane, qlm));
			tx_cfg0.s.cfg_tx_swing = 0xd;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(lane, qlm), tx_cfg0.u64);
			pre_emphasis.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_PRE_EMPHASIS(lane, qlm));
			pre_emphasis.s.cfg_tx_premptap = 0xd0;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_PRE_EMPHASIS(lane, qlm), pre_emphasis.u64);
			tx_cfg1.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_1(lane, qlm));
			tx_cfg1.s.tx_swing_ovrd_en = 1;
			tx_cfg1.s.tx_premptap_ovrd_val = 1;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_1(lane, qlm), tx_cfg1.u64);
		}
	}
}

static void __qlm_init_errata_20844(int node, int qlm)
{
	int lane;

	/* Only applies to CN78XX pass 1.x */
	if (!OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0))
		return;

	/* Errata GSER-20844: Electrical Idle logic can coast
	   1) After the link first comes up write the following
	   register on each lane to prevent the application logic
	   from stomping on the Coast inputs. This is a one time write,
	   or if you prefer you could put it in the link up loop and
	   write it every time the link comes up.
	   1a) Then write GSER(0..13)_LANE(0..3)_PCS_CTLIFC_2
	   Set CTLIFC_OVRRD_REQ (later)
	   Set CFG_RX_CDR_COAST_REQ_OVRRD_EN
	   Its not clear if #1 and #1a can be combined, lets try it
	   this way first. */
	if ((gd->arch.board_desc.board_type != CVMX_BOARD_TYPE_SIM)) {
		for (lane = 0; lane < 4; lane++) {
			cvmx_gserx_lanex_rx_misc_ovrrd_t misc_ovrrd;
			cvmx_gserx_lanex_pcs_ctlifc_2_t ctlifc_2;

        		ctlifc_2.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_PCS_CTLIFC_2(lane, qlm));
        		ctlifc_2.s.cfg_rx_cdr_coast_req_ovrrd_en = 1;
        		cvmx_write_csr_node(node, CVMX_GSERX_LANEX_PCS_CTLIFC_2(lane, qlm), ctlifc_2.u64);

        		misc_ovrrd.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, qlm));
        		misc_ovrrd.s.cfg_rx_eie_det_ovrrd_en = 1;
        		misc_ovrrd.s.cfg_rx_eie_det_ovrrd_val = 0;
        		cvmx_write_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, qlm), misc_ovrrd.u64);

			cvmx_wait_usec(1);

        		misc_ovrrd.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, qlm));
        		misc_ovrrd.s.cfg_rx_eie_det_ovrrd_en = 1;
        		misc_ovrrd.s.cfg_rx_eie_det_ovrrd_val = 1;
        		cvmx_write_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, qlm), misc_ovrrd.u64);
        		ctlifc_2.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_PCS_CTLIFC_2(lane, qlm));
        		ctlifc_2.s.ctlifc_ovrrd_req = 1;
        		cvmx_write_csr_node(node, CVMX_GSERX_LANEX_PCS_CTLIFC_2(lane, qlm), ctlifc_2.u64);
		}
	}
}

/** CN78xx reference clock register settings */
struct refclk_settings_cn78xx_t {
	bool valid;			/** Reference clock speed supported */
	union cvmx_gserx_pll_px_mode_0	mode_0;
	union cvmx_gserx_pll_px_mode_1	mode_1;
	union cvmx_gserx_lane_px_mode_0 pmode_0;
	union cvmx_gserx_lane_px_mode_1 pmode_1;
};

/** Default reference clock for various modes */
static const uint8_t def_ref_clk_cn78xx[R_NUM_LANE_MODES] =
{
	0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1
};

/**
 * This data structure stores the reference clock for each mode for each QLM.
 *
 * It is indexed first by the node number, then the QLM number and then the
 * lane mode.  It is initialized to the default values.
 */
static uint8_t ref_clk_cn78xx[CVMX_MAX_NODES][8][R_NUM_LANE_MODES] =
{
	{
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 }
	},
	{
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 }
	},
	{
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 }
	},
	{
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 },
		{ 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1 }
	}
};

/**
 * This data structure contains the register values for the cn78xx PLLs
 * It is indexed first by the reference clock and second by the mode.
 * Note that not all combinations are supported.
 */
static const struct refclk_settings_cn78xx_t
refclk_settings_cn78xx[R_NUM_LANE_MODES][3] =
{
	{	/* 0	R_25G_REFCLK100 */
		{	/* 100MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x4,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x5
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 125MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x5
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x1,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 156.25MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x5
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x10
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
	},
	{	/* 1	R_5G_REFCLK100 */
		{	/* 100MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x4,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 125MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x1,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 156.25MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x10
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		}

	},
	{	/* 2	R_8G_REFCLK100 */
		{	/* 100MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x5,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x1,
				.pll_opr = 0x1,
				.pll_div = 0x28
			},
			.pmode_0.s = {
				.ctle = 0x3,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xb,
				.ph_acc_adj = 0x23
			}
		},
		{	/* 125MHz reference clock */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x2,
				.pll_rloop = 0x5,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x1,
				.pll_pcie3en = 0x1,
				.pll_opr = 0x1,
				.pll_div = 0x20
			},
			.pmode_0.s = {
				.ctle = 0x3,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xb,
				.ph_acc_adj = 0x23
			}
		},
		{	/* 156.25MHz reference clock not supported */
			.valid = false
		}
	},
	{	/* 3	R_125G_REFCLK15625_KX */
		{	/* 100MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x28
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x2,
				.rx_ldiv = 0x2,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		},
		{	/* 125MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x28
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x2,
				.rx_ldiv = 0x2,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		},
		{	/* 156.25MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x28
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x3,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x10
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x2,
				.rx_ldiv = 0x2,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		}
	},
	{	/* 4	R_3125G_REFCLK15625_XAUI */
		{	/* 100MHz reference */
			.valid = false
		},
		{	/* 125MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x14
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		},
		{	/* 156.25MHz reference, default */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x14
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		}
	},
	{	/* 5	R_103125G_REFCLK15625_KR */
		{	/* 100MHz reference */
			.valid = false
		},
		{	/* 125MHz reference */
			.valid = false
		},
		{	/* 156.25MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x5,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x1,
				.pll_div = 0x21
			},
			.pmode_0.s = {
				.ctle = 0x3,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x1,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0xf
			}
		}
	},
	{	/* 6	R_125G_REFCLK15625_SGMII */
		{	/* 100MHz reference clock */
			.valid = 1,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x28
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x2,
				.rx_ldiv = 0x2,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		},
		{	/* 125MHz reference clock */
			.valid = 1,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x28
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x2,
				.rx_ldiv = 0x2,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		},
		{	/* 156.25MHz reference clock */
			.valid = 1,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x28
			},
			.mode_1.s = {
				.pll_16p5en = 0x1,
				.pll_cpadj = 0x3,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x10
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x2,
				.rx_ldiv = 0x2,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		}
	},
	{	/* 7	R_5G_REFCLK15625_QSGMII */
		{	/* 100MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x4,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		},
		{	/* 125MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x1,
				.pll_pcie3en = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		},
		{	/* 156.25MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_div = 0x10
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xc,
				.ph_acc_adj = 0x1e
			}
		}
	},
	{	/* 8	R_625G_REFCLK15625_RXAUI */
		{	/* 100MHz reference */
			.valid = false
		},
		{	/* 125MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 156.25MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x1,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		}
	},
	{	 /* 9	R_25G_REFCLK125 */
		{	/* 100MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x4,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x5
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 125MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x5
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x1,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 156,25MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0x5
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x10
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x1,
				.rx_ldiv = 0x1,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x1,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		}
	},
	{	/* 0xa	R_5G_REFCLK125 */
		{	/* 100MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x4,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x19
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 125MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x1,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x14
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		},
		{	/* 156.25MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x3,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x0,
				.pll_opr = 0x0,
				.pll_div = 0x10
			},
			.pmode_0.s = {
				.ctle = 0x0,
				.pcie = 0x1,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xa,
				.ph_acc_adj = 0x14
			}
		}
	},
	{	/* 0xb	R_8G_REFCLK125 */
		{	/* 100MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x3,
				.pll_rloop = 0x5,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x2,
				.pll_pcie3en = 0x1,
				.pll_opr = 0x1,
				.pll_div = 0x28
			},
			.pmode_0.s = {
				.ctle = 0x3,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xb,
				.ph_acc_adj = 0x23
			}
		},
		{	/* 125MHz reference */
			.valid = true,
			.mode_0.s = {
				.pll_icp = 0x2,
				.pll_rloop = 0x5,
				.pll_pcs_div = 0xa
			},
			.mode_1.s = {
				.pll_16p5en = 0x0,
				.pll_cpadj = 0x1,
				.pll_pcie3en = 0x1,
				.pll_opr = 0x1,
				.pll_div = 0x20
			},
			.pmode_0.s = {
				.ctle = 0x3,
				.pcie = 0x0,
				.tx_ldiv = 0x0,
				.rx_ldiv = 0x0,
				.srate = 0x0,
				.tx_mode = 0x3,
				.rx_mode = 0x3
			},
			.pmode_1.s = {
				.vma_fine_cfg_sel = 0x0,
				.vma_mm = 0x0,
				.cdr_fgain = 0xb,
				.ph_acc_adj = 0x23
			}
		},
		{	/* 156.25MHz reference */
			.valid = false
		}
	}
};

/* SATA only works at 100 MHz Reference Clock. */
static const struct refclk_settings_cn78xx_t
refclk_settings_sata[3] =
{
	{	/* 0	R_25G_REFCLK100 */
		.valid = true,
		.mode_0.s = {
			.pll_icp = 0x1,
			.pll_rloop = 0x3,
			.pll_pcs_div = 0x5
		},
		.mode_1.s = {
			.pll_16p5en = 0x0,
			.pll_cpadj = 0x2,
			.pll_pcie3en = 0x0,
			.pll_opr = 0x0,
			.pll_div = 0x19
		},
		.pmode_0.s = {
			.ctle = 0x0,
			.pcie = 0x1,
			.tx_ldiv = 0x0,
			.rx_ldiv = 0x2,
			.srate = 0x0,
			.tx_mode = 0x3,
			.rx_mode = 0x3
		},
		.pmode_1.s = {
			.vma_fine_cfg_sel = 0x0,
			.vma_mm = 0x1,
			.cdr_fgain = 0xa,
			.ph_acc_adj = 0x15
		}
	},
	{	/* 1	R_5G_REFCLK100 */
		.valid = true,
		.mode_0.s = {
			.pll_icp = 0x1,
			.pll_rloop = 0x3,
			.pll_pcs_div = 0x5
		},
		.mode_1.s = {
			.pll_16p5en = 0x0,
			.pll_cpadj = 0x2,
			.pll_pcie3en = 0x0,
			.pll_opr = 0x0,
			.pll_div = 0x1e
		},
		.pmode_0.s = {
			.ctle = 0x0,
			.pcie = 0x1,
			.tx_ldiv = 0x0,
			.rx_ldiv = 0x1,
			.srate = 0x0,
			.tx_mode = 0x3,
			.rx_mode = 0x3
		},
		.pmode_1.s = {
			.vma_fine_cfg_sel = 0x0,
			.vma_mm = 0x0,
			.cdr_fgain = 0xa,
			.ph_acc_adj = 0x15
		}
	},
	{	/* 2	R_8G_REFCLK100 */
		.valid = true,
		.mode_0.s = {
			.pll_icp = 0x1,
			.pll_rloop = 0x5,
			.pll_pcs_div = 0x5
		},
		.mode_1.s = {
			.pll_16p5en = 0x0,
			.pll_cpadj = 0x2,
			.pll_pcie3en = 0x1,
			.pll_opr = 0x0,
			.pll_div = 0x1e
		},
		.pmode_0.s = {
			.ctle = 0x3,
			.pcie = 0x0,
			.tx_ldiv = 0x0,
			.rx_ldiv = 0x0,
			.srate = 0x0,
			.tx_mode = 0x3,
			.rx_mode = 0x3
		},
		.pmode_1.s = {
			.vma_fine_cfg_sel = 0x0,
			.vma_mm = 0x0,
			.cdr_fgain = 0xb,
			.ph_acc_adj = 0x15
		}
	}
};

/**
 * Set a non-standard reference clock for a node, qlm and lane mode.
 *
 * @INTERNAL
 *
 * @param node		node number the reference clock is used with
 * @param qlm		qlm number the reference clock is hooked up to
 * @param lane_mode	current lane mode selected for the QLM
 * @param ref_clk_sel	0 = 100MHz, 1 = 125MHz, 2 = 156.25MHz
 *
 * @return 0 for success or -1 if the reference clock selector is not supported
 *
 * NOTE: This must be called before __qlm_setup_pll_cn78xx.
 */
static int __set_qlm_ref_clk_cn78xx(int node, int qlm, int lane_mode,
				    int ref_clk_sel)
{
	if ((ref_clk_sel > 2) || (ref_clk_sel < 0) ||
	    !refclk_settings_cn78xx[lane_mode][ref_clk_sel].valid) {
		debug("%s: Invalid reference clock %d for lane mode %d for node %d, QLM %d\n",
		      __func__, ref_clk_sel, lane_mode, node, qlm);
		return -1;
	}
	ref_clk_cn78xx[node][qlm][lane_mode] = ref_clk_sel;
	return 0;
}

/**
 * Configure all of the PLLs for a particular node and qlm
 * @INTERNAL
 *
 * @param node	Node number to configure
 * @param qlm	QLM number to configure
 */
static void __qlm_setup_pll_cn78xx(int node, int qlm)
{
	cvmx_gserx_pll_px_mode_0_t mode_0;
	cvmx_gserx_pll_px_mode_1_t mode_1;
	cvmx_gserx_lane_px_mode_0_t pmode_0;
	cvmx_gserx_lane_px_mode_1_t pmode_1;
	int lane_mode;
	int ref_clk;
	const struct refclk_settings_cn78xx_t *clk_settings;

	for (lane_mode = 0; lane_mode < R_NUM_LANE_MODES; lane_mode++) {
		mode_0.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PLL_PX_MODE_0(lane_mode, qlm));
		mode_1.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PLL_PX_MODE_1(lane_mode, qlm));
		pmode_0.u64 = 0;
		pmode_1.u64 = 0;
		ref_clk = ref_clk_cn78xx[node][qlm][lane_mode];
		clk_settings = &refclk_settings_cn78xx[lane_mode][ref_clk];

		if (!clk_settings->valid) {
			printf("%s: Error: reference clock %d is not supported for lane mode %d on qlm %d\n",
			       __func__, ref_clk, lane_mode, qlm);
			continue;
		}
		if (OCTEON_IS_MODEL(OCTEON_CN73XX) && qlm == 4) {
			if (lane_mode == R_25G_REFCLK100
			    || lane_mode == R_5G_REFCLK100
			    || lane_mode == R_8G_REFCLK100) {

				clk_settings = &refclk_settings_sata[lane_mode];

				mode_0.u64 = clk_settings->mode_0.u64;
				mode_1.u64 = clk_settings->mode_1.u64;
				pmode_0.u64 = clk_settings->pmode_0.u64;
				pmode_1.u64 = clk_settings->pmode_1.u64;
			} else {
				printf("Invalid SATA configuration\n");
				continue;
			}
		} else {

			mode_0.s.pll_icp = clk_settings->mode_0.s.pll_icp;
			mode_0.s.pll_rloop = clk_settings->mode_0.s.pll_rloop;
			mode_0.s.pll_pcs_div = clk_settings->mode_0.s.pll_pcs_div;

			mode_1.s.pll_16p5en = clk_settings->mode_1.s.pll_16p5en;
			mode_1.s.pll_cpadj = clk_settings->mode_1.s.pll_cpadj;
			mode_1.s.pll_pcie3en = clk_settings->mode_1.s.pll_pcie3en;
			mode_1.s.pll_opr = clk_settings->mode_1.s.pll_opr;
			mode_1.s.pll_div = clk_settings->mode_1.s.pll_div;

			pmode_0.u64 = clk_settings->pmode_0.u64;

			pmode_1.u64 = clk_settings->pmode_1.u64;
		}

		cvmx_write_csr_node(node, CVMX_GSERX_PLL_PX_MODE_0(lane_mode, qlm), mode_0.u64);
		cvmx_write_csr_node(node, CVMX_GSERX_PLL_PX_MODE_1(lane_mode, qlm), mode_1.u64);
		cvmx_write_csr_node(node, CVMX_GSERX_LANE_PX_MODE_0(lane_mode, qlm), pmode_0.u64);
		cvmx_write_csr_node(node, CVMX_GSERX_LANE_PX_MODE_1(lane_mode, qlm), pmode_1.u64);
	}
}

/**
 * Get the lane mode for the specified node and QLM.
 *
 * @param ref_clk_sel	The reference-clock selection to use to configure QLM
 * 			 0 = REF_100MHZ
 * 			 1 = REF_125MHZ
 * 			 2 = REF_156MHZ
 * @param baud_mhz   The speed the QLM needs to be configured in Mhz.
 * @param[out] alt_pll_settings	If non-NULL this will be set if non-default PLL
 *				settings are required for the mode.
 *
 * @return lane mode to use or -1 on error
 *
 * NOTE: In some modes
 */
static int __get_lane_mode_for_speed_and_ref_clk(int ref_clk_sel,
						 int baud_mhz,
						 bool *alt_pll_settings)
{
	if (alt_pll_settings)
		*alt_pll_settings = false;
	if (baud_mhz <= 1250) {
		if (alt_pll_settings)
			*alt_pll_settings = (ref_clk_sel != 2);
		return R_125G_REFCLK15625_SGMII;
	}

	if (baud_mhz <= 2500) {
		if (ref_clk_sel == 0)
			return R_25G_REFCLK100;
		else {
			if (alt_pll_settings)
				*alt_pll_settings = (ref_clk_sel != 1);
			return R_25G_REFCLK125;
		}
	}
	if (baud_mhz <= 3125) {
		if (ref_clk_sel == 2)
			return R_3125G_REFCLK15625_XAUI;
		else if (ref_clk_sel == 1) {
			if (alt_pll_settings)
				*alt_pll_settings = true;
			return R_3125G_REFCLK15625_XAUI;
		} else {
			printf("Error: Invalid speed\n");
			return -1;
		}
	}
	if (baud_mhz <= 5000) {
		if (ref_clk_sel == 0) {
			return R_5G_REFCLK100;
		} else {
			if (alt_pll_settings)
				*alt_pll_settings = (ref_clk_sel != 1);
			return R_5G_REFCLK125;
		}
	}
	if (baud_mhz <= 6250) {
		if (ref_clk_sel != 0) {
			if (alt_pll_settings)
				*alt_pll_settings = (ref_clk_sel != 2);
			return R_625G_REFCLK15625_RXAUI;
		} else {
			printf("Error: Invalid speed\n");
			return -1;
		}
	}
	if (baud_mhz <= 8000) {
		if (ref_clk_sel == 0)
			return R_8G_REFCLK100;
		else if (ref_clk_sel == 1)
			return R_8G_REFCLK125;
		else {
			printf("Error: Invalid speed\n");
			return -1;
		}
	}
	if (ref_clk_sel == 2)
		return R_103125G_REFCLK15625_KR;

	return -1;
}

static void __cvmx_qlm_pcie_errata_ep_cn78xx(int node, int pem)
{
	cvmx_pciercx_cfg031_t cfg031;
	cvmx_pciercx_cfg032_t cfg032;
	cvmx_pciercx_cfg040_t cfg040;
	cvmx_pemx_cfg_t pemx_cfg;
	cvmx_pemx_on_t pemx_on;
	int low_qlm, high_qlm;
	int qlm, lane;
	uint64_t start_cycle;

	pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(pem));

	/* Errata (GSER-21178) PCIe gen3 doesn't work, continued */

	/* Wait for the link to come up as Gen1 */
	printf("PCIe%d: Waiting for EP out of reset\n", pem);
	while (pemx_on.s.pemoor == 0) {
		cvmx_wait_usec(1000);
		pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(pem));
	}
	/* Enable gen3 speed selection */
	printf("PCIe%d: Enabling Gen3 for EP\n", pem);
	/* Force Gen1 for initial link bringup. We'll fix it later */
	pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(pem));
	pemx_cfg.s.md = 2;
	cvmx_write_csr_node(node, CVMX_PEMX_CFG(pem), pemx_cfg.u64);
	cfg031.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG031(pem));
	cfg031.s.mls = 2;
	cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG031(pem), cfg031.u32);
	cfg040.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG040(pem));
	cfg040.s.tls = 3;
	cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG040(pem), cfg040.u32);

	/* Wait up to 10ms for the link speed change to complete */
	start_cycle = cvmx_get_cycle();
	do {
		if (cvmx_get_cycle() - start_cycle > cvmx_clock_get_rate(CVMX_CLOCK_CORE))
			return;
		cvmx_wait(10000);
		cfg032.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG032(pem));
	} while (cfg032.s.ls != 3);

	pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(pem));
	low_qlm = pem;  /* FIXME */
	high_qlm = (pemx_cfg.cn78xx.lanes8) ? low_qlm+1 : low_qlm;

	/* Toggle cfg_rx_dll_locken_ovvrd_en and rx_resetn_ovrrd_en across
	   all QM lanes in use */
	for (qlm = low_qlm; qlm <= high_qlm; qlm++) {
		for (lane = 0; lane < 4; lane++) {
			cvmx_gserx_lanex_rx_misc_ovrrd_t misc_ovrrd;
			cvmx_gserx_lanex_pwr_ctrl_t pwr_ctrl;

			misc_ovrrd.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, pem));
			misc_ovrrd.s.cfg_rx_dll_locken_ovvrd_en = 1;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, pem), misc_ovrrd.u64);
			pwr_ctrl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_PWR_CTRL(lane, pem));
			pwr_ctrl.s.rx_resetn_ovrrd_en = 1;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_PWR_CTRL(lane, pem), pwr_ctrl.u64);
		}
	}
	for (qlm = low_qlm; qlm <= high_qlm; qlm++) {
		for (lane = 0; lane < 4; lane++) {
			cvmx_gserx_lanex_rx_misc_ovrrd_t misc_ovrrd;
			cvmx_gserx_lanex_pwr_ctrl_t pwr_ctrl;

			misc_ovrrd.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, pem));
			misc_ovrrd.s.cfg_rx_dll_locken_ovvrd_en = 0;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_RX_MISC_OVRRD(lane, pem), misc_ovrrd.u64);
			pwr_ctrl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_PWR_CTRL(lane, pem));
			pwr_ctrl.s.rx_resetn_ovrrd_en = 0;
			cvmx_write_csr_node(node, CVMX_GSERX_LANEX_PWR_CTRL(lane, pem), pwr_ctrl.u64);
		}
	}

	//printf("PCIe%d: Waiting for EP link up at Gen3\n", pem);
	if (CVMX_WAIT_FOR_FIELD64_NODE(node, CVMX_PEMX_ON(pem), cvmx_pemx_on_t, pemoor, ==, 1, 1000000)) {
		printf("PCIe%d: Timeout waiting for EP link up at Gen3\n", pem);
		return;
	}
}

static void __cvmx_qlm_pcie_errata_cn78xx(int node, int qlm)
{
	int pem, i, q;
	int is_8lanes;
	int is_high_lanes;
	int low_qlm, high_qlm, is_host;
	int need_ep_monitor;
	cvmx_pemx_cfg_t pem_cfg,pem3_cfg;
	cvmx_gserx_slice_cfg_t slice_cfg;
	cvmx_gserx_rx_pwr_ctrl_p1_t pwr_ctrl_p1;
	cvmx_rst_soft_prstx_t soft_prst;

	/* Only applies to CN78XX pass 1.0 */
	if (!OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_X))
		return;

	/* Determine the PEM for this QLM, whether we're in 8 lane mode,
	   and whether these are the top lanes of the 8 */
	switch (qlm) {
	case 0:	/* First 4 lanes of PEM0 */
		pem_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(0));
		pem = 0;
		is_8lanes = pem_cfg.cn78xx.lanes8;
		is_high_lanes = 0;
		break;
	case 1:	/* Either last 4 lanes of PEM0, or PEM1 */
		pem_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(0));
		pem = (pem_cfg.cn78xx.lanes8) ? 0 : 1;
		is_8lanes = pem_cfg.cn78xx.lanes8;
		is_high_lanes = is_8lanes;
		break;
	case 2:	/* First 4 lanes of PEM2 */
		pem_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(2));
		pem = 2;
		is_8lanes = pem_cfg.cn78xx.lanes8;
		is_high_lanes = 0;
		break;
	case 3:	/* Either last 4 lanes of PEM2, or PEM3 */
		pem_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(2));
		pem3_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(3));
		pem = (pem_cfg.cn78xx.lanes8) ? 2 : 3;
		is_8lanes = (pem == 2) ? pem_cfg.cn78xx.lanes8 : pem3_cfg.cn78xx.lanes8;
		is_high_lanes = (pem == 2) && is_8lanes;
		break;
	case 4: /* Last 4 lanes of PEM3 */
		pem = 3;
		is_8lanes = 1;
		is_high_lanes = 1;
		break;
	default:
		return;
	}

	/* These workaround must be applied once per PEM. Since we're called per
	   QLM, wait for the 2nd half of 8 lane setups before doing the workaround */
	if (is_8lanes && !is_high_lanes)
		return;

	pem_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(pem));
	is_host = pem_cfg.cn78xx.hostmd;
	low_qlm = (is_8lanes) ? qlm - 1 : qlm;
	high_qlm = qlm;
	qlm = -1;

	if (!is_host) {
		/* Read the current slice config value. If its at the value we will
		   program then skip doing the workaround. We're probably doing a
		   hot reset and the workaround is already applied */
		slice_cfg.u64 = cvmx_read_csr_node(node, CVMX_GSERX_SLICE_CFG(low_qlm));
		if (slice_cfg.s.tx_rx_detect_lvl_enc == 7
		    && OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0))
			return;
	}

	if (is_host) {
		/* (GSER-XXXX) GSER PHY needs to be reset at initialization */
		cvmx_gserx_phy_ctl_t phy_ctl;

		for (q = low_qlm; q <= high_qlm; q++) {
			phy_ctl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PHY_CTL(q));
			phy_ctl.s.phy_reset = 1;
			cvmx_write_csr_node(node, CVMX_GSERX_PHY_CTL(q), phy_ctl.u64);
		}
		cvmx_wait_usec(5);

		for (q = low_qlm; q <= high_qlm; q++) {
			phy_ctl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PHY_CTL(q));
			phy_ctl.s.phy_reset = 0;
			cvmx_write_csr_node(node, CVMX_GSERX_PHY_CTL(q), phy_ctl.u64);
		}
		cvmx_wait_usec(5);
	}

	if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0)) {
		/* (GSER-20936) GSER has wrong PCIe RX detect reset value */
		for (q = low_qlm; q <= high_qlm; q++) {
			slice_cfg.u64 = cvmx_read_csr_node(node, CVMX_GSERX_SLICE_CFG(q));
			slice_cfg.s.tx_rx_detect_lvl_enc = 7;
			cvmx_write_csr_node(node, CVMX_GSERX_SLICE_CFG(q), slice_cfg.u64);
		}

		/* Clear the bit in GSERX_RX_PWR_CTRL_P1[p1_rx_subblk_pd]
		   that coresponds to "Lane DLL" */
		for (q = low_qlm; q <= high_qlm; q++) {
			pwr_ctrl_p1.u64 = cvmx_read_csr_node(node, CVMX_GSERX_RX_PWR_CTRL_P1(q));
			pwr_ctrl_p1.s.p1_rx_subblk_pd &= ~4;
			cvmx_write_csr_node(node, CVMX_GSERX_RX_PWR_CTRL_P1(q), pwr_ctrl_p1.u64);
		}

		/* Errata (GSER-20888) GSER incorrect synchronizers hurts PCIe
		   Override TX Power State machine TX reset control signal */
		for (q = low_qlm; q <= high_qlm; q++) {
			for (i = 0; i < 4; i++) {
				cvmx_gserx_lanex_tx_cfg_0_t tx_cfg;
                		cvmx_gserx_lanex_pwr_ctrl_t pwr_ctrl;

				tx_cfg.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(i, q));
				tx_cfg.s.tx_resetn_ovrd_val = 1;
				cvmx_write_csr_node(node, CVMX_GSERX_LANEX_TX_CFG_0(i, q), tx_cfg.u64);
				pwr_ctrl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANEX_PWR_CTRL(i,q));
				pwr_ctrl.s.tx_p2s_resetn_ovrrd_en = 1;
				cvmx_write_csr_node(node, CVMX_GSERX_LANEX_PWR_CTRL(i, q), pwr_ctrl.u64);
			}
		}
	}

	if (!is_host) {
		cvmx_pciercx_cfg089_t cfg089;
		cvmx_pciercx_cfg090_t cfg090;
		cvmx_pciercx_cfg091_t cfg091;
		cvmx_pciercx_cfg092_t cfg092;
		cvmx_pciercx_cfg548_t cfg548;
		cvmx_pciercx_cfg554_t cfg554;

		if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0)) {
			/* Errata (GSER-21178) PCIe gen3 doesn't work */
			/* The starting equalization hints are incorrect on CN78XX pass 1.x. Fix
			   them for the 8 possible lanes. It doesn't hurt to program them even for
			   lanes not in use */
			cfg089.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG089(pem));
			cfg089.s.l1urph= 2;
			cfg089.s.l1utp = 7;
			cfg089.s.l0urph = 2;
			cfg089.s.l0utp = 7;
			cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG089(pem), cfg089.u32);
			cfg090.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG090(pem));
			cfg090.s.l3urph= 2;
			cfg090.s.l3utp = 7;
			cfg090.s.l2urph = 2;
			cfg090.s.l2utp = 7;
			cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG090(pem), cfg090.u32);
			cfg091.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG091(pem));
			cfg091.s.l5urph= 2;
			cfg091.s.l5utp = 7;
			cfg091.s.l4urph = 2;
			cfg091.s.l4utp = 7;
			cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG091(pem), cfg091.u32);
			cfg092.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG092(pem));
			cfg092.s.l7urph= 2;
			cfg092.s.l7utp = 7;
			cfg092.s.l6urph = 2;
			cfg092.s.l6utp = 7;
			cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG092(pem), cfg092.u32);
			/* FIXME: Disable phase 2 and phase 3 equalization */
			cfg548.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG548(pem));
			cfg548.s.ep2p3d = 1;
			cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG548(pem), cfg548.u32);
		}
		/* Errata (GSER-21331) GEN3 Equalization may fail */
		/* Disable preset #10 and disable the 2ms timeout */
		cfg554.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG554(pem));
		if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0))
			cfg554.s.p23td = 1;
		cfg554.s.prv = 0x3ff;
		cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG554(pem), cfg554.u32);

		if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0)) {
			need_ep_monitor = (pem_cfg.s.md == 2);
			if (need_ep_monitor) {
				cvmx_pciercx_cfg031_t cfg031;
				cvmx_pciercx_cfg040_t cfg040;

				/* Force Gen1 for initial link bringup. We'll
				   fix it later */
				pem_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(pem));
				pem_cfg.s.md = 0;
				cvmx_write_csr_node(node, CVMX_PEMX_CFG(pem), pem_cfg.u64);
				cfg031.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG031(pem));
				cfg031.s.mls = 0;
				cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG031(pem), cfg031.u32);
				cfg040.u32 = cvmx_pcie_cfgx_read_node(node, pem, CVMX_PCIERCX_CFG040(pem));
				cfg040.s.tls = 1;
				cvmx_pcie_cfgx_write_node(node, pem, CVMX_PCIERCX_CFG040(pem), cfg040.u32);
				__cvmx_qlm_pcie_errata_ep_cn78xx(node, pem);
			}
			return;
		}
	}

	if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_0)) {
		/* De-assert the SOFT_RST bit for this QLM (PEM), causing the PCIe
		   workarounds code above to take effect. */
		soft_prst.u64 = cvmx_read_csr_node(node, CVMX_RST_SOFT_PRSTX(pem));
		soft_prst.s.soft_prst = 0;
		cvmx_write_csr_node(node, CVMX_RST_SOFT_PRSTX(pem), soft_prst.u64);
		cvmx_wait_usec(1);

		/* Assert the SOFT_RST bit for this QLM (PEM), putting the PCIe back into
		   reset state with disturbing the workarounds. */
		soft_prst.u64 = cvmx_read_csr_node(node, CVMX_RST_SOFT_PRSTX(pem));
		soft_prst.s.soft_prst = 1;
		cvmx_write_csr_node(node, CVMX_RST_SOFT_PRSTX(pem), soft_prst.u64);
	}
	cvmx_wait_usec(1);
}

/**
 * Setup the PEM to either driver or receive reset from PRST based on RC or EP
 *
 * @param node   Node to use in a Numa setup
 * @param pem    Which PEM to setuo
 * @param is_endpoint
 *               Non zero if PEM is a EP
 */
static void __setup_pem_reset(int node, int pem, int is_endpoint)
{
	cvmx_rst_ctlx_t rst_ctl;

	/* Make sure is_endpoint is either 0 or 1 */
	is_endpoint = (is_endpoint != 0);
	rst_ctl.u64 = cvmx_read_csr_node(node, CVMX_RST_CTLX(pem));
	rst_ctl.s.prst_link = 0;          /* Link down causes soft reset */
	rst_ctl.s.rst_link = is_endpoint; /* EP PERST causes a soft reset */
	rst_ctl.s.rst_drv = !is_endpoint; /* Drive if RC */
	rst_ctl.s.rst_rcv = is_endpoint;  /* Only read PERST in EP mode */
	rst_ctl.s.rst_chip = 0;          /* PERST doesn't pull CHIP_RESET */
	cvmx_write_csr_node(node, CVMX_RST_CTLX(pem), rst_ctl.u64);
}

/**
 * Configure QLM speed and mode for cn78xx.
 *
 * @param node    Node to configure the QLM
 * @param qlm     The QLM to configure
 * @param baud_mhz   The speed the QLM needs to be configured in Mhz.
 * @param mode    The QLM to be configured as SGMII/XAUI/PCIe.
 * @param rc      Only used for PCIe, rc = 1 for root complex mode, 0 for EP mode.
 * @param gen3    Only used for PCIe
 * 			gen3 = 2 GEN3 mode
 * 			gen3 = 1 GEN2 mode
 * 			gen3 = 0 GEN1 mode
 *
 * @param ref_clk_sel    The reference-clock selection to use to configure QLM
 * 			 0 = REF_100MHZ
 * 			 1 = REF_125MHZ
 * 			 2 = REF_156MHZ
 * @param ref_clk_input  The reference-clock input to use to configure QLM
 *
 * @return       Return 0 on success or -1.
 */
int octeon_configure_qlm_cn78xx(int node, int qlm, int baud_mhz,
				int mode, int rc,
				int gen3, int ref_clk_sel,
				int ref_clk_input)
{
	cvmx_gserx_phy_ctl_t phy_ctl;
	cvmx_gserx_lane_mode_t lmode;
	cvmx_gserx_cfg_t cfg;
	cvmx_gserx_refclk_sel_t refclk_sel;

	int is_pcie = 0;
	int is_ilk = 0;
	int is_bgx = 0;
	int lane_mode = 0;
	int lmac_type = 0;
	bool alt_pll = false;

	debug("%s(node: %d, qlm: %d, baud_mhz: %d, mode: %d, rc: %d, gen3: %d, ref_clk_sel: %d, ref_clk_input: %d\n",
	      __func__, node, qlm, baud_mhz, mode, rc, gen3, ref_clk_sel,
	      ref_clk_input);
	if (OCTEON_IS_MODEL(OCTEON_CN76XX) && qlm > 4)
		return -1;

	cfg.u64 = cvmx_read_csr_node(node, CVMX_GSERX_CFG(qlm));
	/* If PEM is in EP, no need to do anything */

	if (cfg.s.pcie && rc == 0) {
		debug("%s: node %d, qlm %d is in PCIe endpoint mode, returning\n",
		      __func__, node, qlm);
		return 0;
	}

	/* Set the reference clock to use */
	refclk_sel.u64 = 0;
	if (ref_clk_input == 0) { /* External ref clock */
		refclk_sel.s.com_clk_sel = 0;
		refclk_sel.s.use_com1 = 0;
	} else if (ref_clk_input == 1) {
		refclk_sel.s.com_clk_sel = 1;
		refclk_sel.s.use_com1 = 0;
	} else {
		refclk_sel.s.com_clk_sel = 1;
		refclk_sel.s.use_com1 = 1;
	}

	cvmx_write_csr_node(node, CVMX_GSERX_REFCLK_SEL(qlm), refclk_sel.u64);

	/* Reset the QLM after changing the reference clock */
	phy_ctl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PHY_CTL(qlm));
	phy_ctl.s.phy_reset = 1;
	phy_ctl.s.phy_pd = 1;
	cvmx_write_csr_node(node, CVMX_GSERX_PHY_CTL(qlm), phy_ctl.u64);

	cvmx_wait(1000);

	/* Always restore the reference clocks for a QLM */
	memcpy(ref_clk_cn78xx[node][qlm], def_ref_clk_cn78xx,
	       sizeof(def_ref_clk_cn78xx));
	switch (mode) {
	case CVMX_QLM_MODE_PCIE:
	case CVMX_QLM_MODE_PCIE_1X8:
	{
		cvmx_pemx_cfg_t pemx_cfg;
		cvmx_pemx_on_t pemx_on;

		is_pcie = 1;

		if (ref_clk_sel == 0) {
			refclk_sel.u64 = cvmx_read_csr_node(node, CVMX_GSERX_REFCLK_SEL(qlm));
			refclk_sel.s.pcie_refclk125 = 0;
			cvmx_write_csr_node(node, CVMX_GSERX_REFCLK_SEL(qlm), refclk_sel.u64);
			if (gen3 == 0) /* Gen1 mode */
				lane_mode = R_25G_REFCLK100;
			else if (gen3 == 1) /* Gen2 mode */
				lane_mode = R_5G_REFCLK100;
			else
				lane_mode = R_8G_REFCLK100;
		} else if (ref_clk_sel == 1) {
			refclk_sel.u64 = cvmx_read_csr_node(node, CVMX_GSERX_REFCLK_SEL(qlm));
			refclk_sel.s.pcie_refclk125 = 1;
			cvmx_write_csr_node(node, CVMX_GSERX_REFCLK_SEL(qlm), refclk_sel.u64);
			if (gen3 == 0) /* Gen1 mode */
				lane_mode = R_25G_REFCLK125;
			else if (gen3 == 1) /* Gen2 mode */
				lane_mode = R_5G_REFCLK125;
			else
				lane_mode = R_8G_REFCLK125;
		} else {
			printf("Invalid reference clock for PCIe on QLM%d\n", qlm);
			return -1;
		}

		switch (qlm) {
		case 0:	/* Either x4 or x8 based on PEM0 */
		{
			cvmx_rst_soft_prstx_t rst_prst;
			rst_prst.u64 = cvmx_read_csr_node(node, CVMX_RST_SOFT_PRSTX(0));
			rst_prst.s.soft_prst = rc;
			cvmx_write_csr_node(node, CVMX_RST_SOFT_PRSTX(0), rst_prst.u64);
			__setup_pem_reset(node, 0, !rc);

			pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(0));
			pemx_cfg.cn78xx.lanes8 = (mode == CVMX_QLM_MODE_PCIE_1X8);
			pemx_cfg.cn78xx.hostmd = rc;
			pemx_cfg.cn78xx.md = gen3;
			cvmx_write_csr_node(node, CVMX_PEMX_CFG(0), pemx_cfg.u64);
			/* x8 mode waits for QLM1 setup before turning on the PEM */
			if (mode == CVMX_QLM_MODE_PCIE) {
				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(0));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(0), pemx_on.u64);
			}
			break;
		}
		case 1:	/* Either PEM0 x8 or PEM1 x4 */
		{
			if (mode == CVMX_QLM_MODE_PCIE) {
				cvmx_rst_soft_prstx_t rst_prst;
				cvmx_pemx_cfg_t pemx_cfg;
				rst_prst.u64 = cvmx_read_csr_node(node, CVMX_RST_SOFT_PRSTX(1));
				rst_prst.s.soft_prst = rc;
				cvmx_write_csr_node(node, CVMX_RST_SOFT_PRSTX(1), rst_prst.u64);
				__setup_pem_reset(node, 1, !rc);

				pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(1));
				pemx_cfg.cn78xx.lanes8 = 0;
				pemx_cfg.cn78xx.hostmd = rc;
				pemx_cfg.cn78xx.md = gen3;
				cvmx_write_csr_node(node, CVMX_PEMX_CFG(1), pemx_cfg.u64);

				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(1));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(1), pemx_on.u64);
			} else {
				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(0));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(0), pemx_on.u64);
			}
			break;
		}
		case 2:	/* Either PEM2 x4 or PEM2 x8 */
		{
			cvmx_rst_soft_prstx_t rst_prst;
			rst_prst.u64 = cvmx_read_csr_node(node, CVMX_RST_SOFT_PRSTX(2));
			rst_prst.s.soft_prst = rc;
			cvmx_write_csr_node(node, CVMX_RST_SOFT_PRSTX(2), rst_prst.u64);
			__setup_pem_reset(node, 2, !rc);

			pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(2));
			pemx_cfg.cn78xx.lanes8 = (mode == CVMX_QLM_MODE_PCIE_1X8);
			pemx_cfg.cn78xx.hostmd = rc;
			pemx_cfg.cn78xx.md = gen3;
			cvmx_write_csr_node(node, CVMX_PEMX_CFG(2), pemx_cfg.u64);
			/* x8 mode waits for QLM3 setup before turning on the PEM */
			if (mode == CVMX_QLM_MODE_PCIE) {
				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(2));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(2), pemx_on.u64);
			}
			break;
		}
		case 3:	/* Either PEM2 x8 or PEM3 x4 */
		{
			pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(2));
			if (pemx_cfg.cn78xx.lanes8) {
				/* Last 4 lanes of PEM2 */
				/* PEMX_CFG already setup */
				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(2));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(2), pemx_on.u64);
			}
			/* Check if PEM3 uses QLM3 and in x4 lane mode */
			if (mode == CVMX_QLM_MODE_PCIE) {
				cvmx_rst_soft_prstx_t rst_prst;
				rst_prst.u64 = cvmx_read_csr_node(node, CVMX_RST_SOFT_PRSTX(3));
				rst_prst.s.soft_prst = rc;
				cvmx_write_csr_node(node, CVMX_RST_SOFT_PRSTX(3), rst_prst.u64);
				__setup_pem_reset(node, 3, !rc);

				pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(3));
				pemx_cfg.cn78xx.lanes8 = 0;
				pemx_cfg.cn78xx.hostmd = rc;
				pemx_cfg.cn78xx.md = gen3;
				cvmx_write_csr_node(node, CVMX_PEMX_CFG(3), pemx_cfg.u64);

				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(3));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(3), pemx_on.u64);
			}
			break;
		}
		case 4:	/* Either PEM3 x4 or PEM3 x8 */
		{
			if (mode == CVMX_QLM_MODE_PCIE_1X8) {
				/* Last 4 lanes of PEM3 */
				/* PEMX_CFG already setup */
				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(3));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(3), pemx_on.u64);
			} else {
				/* 4 lanes of PEM3 */
				cvmx_pemx_qlm_t pemx_qlm;
				cvmx_rst_soft_prstx_t rst_prst;
				rst_prst.u64 = cvmx_read_csr_node(node, CVMX_RST_SOFT_PRSTX(3));
				rst_prst.s.soft_prst = rc;
				cvmx_write_csr_node(node, CVMX_RST_SOFT_PRSTX(3), rst_prst.u64);
				__setup_pem_reset(node, 3, !rc);

				pemx_cfg.u64 = cvmx_read_csr_node(node, CVMX_PEMX_CFG(3));
				pemx_cfg.cn78xx.lanes8 = 0;
				pemx_cfg.cn78xx.hostmd = rc;
				pemx_cfg.cn78xx.md = gen3;
				cvmx_write_csr_node(node, CVMX_PEMX_CFG(3), pemx_cfg.u64);
				/* PEM3 is on QLM4 */
				pemx_qlm.u64 = cvmx_read_csr_node(node, CVMX_PEMX_QLM(3));
				pemx_qlm.cn78xx.pem3qlm = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_QLM(3), pemx_qlm.u64);
				pemx_on.u64 = cvmx_read_csr_node(node, CVMX_PEMX_ON(3));
				pemx_on.s.pemon = 1;
				cvmx_write_csr_node(node, CVMX_PEMX_ON(3), pemx_on.u64);

			}
			break;
		}
		default:
			break;
		}
		break;
	}
	case CVMX_QLM_MODE_ILK:
		is_ilk = 1;
		lane_mode = __get_lane_mode_for_speed_and_ref_clk(ref_clk_sel,
								  baud_mhz,
								  &alt_pll);
		if (lane_mode == -1)
			return -1;
		/* FIXME: Set lane_mode for other speeds */
		break;
	case CVMX_QLM_MODE_SGMII:
		is_bgx = 1;
		lmac_type = 0;
		lane_mode = __get_lane_mode_for_speed_and_ref_clk(ref_clk_sel,
								  baud_mhz,
								  &alt_pll);
		debug("%s: SGMII lane mode: %d, alternate PLL: %s\n", __func__,
		      lane_mode, alt_pll ? "true" : "false");
		if (lane_mode == -1)
			return -1;
		break;
	case CVMX_QLM_MODE_XAUI:
		is_bgx = 5;
		lmac_type = 1;
		lane_mode = __get_lane_mode_for_speed_and_ref_clk(ref_clk_sel,
								  baud_mhz,
								  &alt_pll);
		debug("%s: XAUI lane mode: %d\n", __func__, lane_mode);
		if (lane_mode == -1)
			return -1;
		break;
	case CVMX_QLM_MODE_RXAUI:
		is_bgx = 3;
		lmac_type = 2;
		debug("%s: RXAUI lane mode: %d\n", __func__, lane_mode);
		lane_mode = __get_lane_mode_for_speed_and_ref_clk(ref_clk_sel,
								  baud_mhz,
								  &alt_pll);
		if (lane_mode == -1)
			return -1;
		break;
	case CVMX_QLM_MODE_XFI:		/* 10GR_4X1 */
	case CVMX_QLM_MODE_10G_KR:
		is_bgx = 1;
		lmac_type = 3;
		lane_mode = __get_lane_mode_for_speed_and_ref_clk(ref_clk_sel,
								  baud_mhz,
								  &alt_pll);
		debug("%s: XFI/10G_KR lane mode: %d\n", __func__, lane_mode);
		if (lane_mode == -1)
			return -1;
		break;
	case CVMX_QLM_MODE_XLAUI:	/* 40GR4_1X4 */
	case CVMX_QLM_MODE_40G_KR4:
		is_bgx = 5;
		lmac_type = 4;
		lane_mode = __get_lane_mode_for_speed_and_ref_clk(ref_clk_sel,
								  baud_mhz,
								  &alt_pll);
		debug("%s: LXAUI/40G_KR4 lane mode: %d\n", __func__, lane_mode);
		if (lane_mode == -1)
			return -1;
		break;
	default:
		break;
	}

	if (alt_pll) {
		debug("%s: alternate PLL settings used for node %d, qlm %d, lane mode %d, reference clock %d\n",
		      __func__, node, qlm, lane_mode, ref_clk_sel);
		if (__set_qlm_ref_clk_cn78xx(node, qlm, lane_mode,
					    ref_clk_sel)) {
			printf("%s: Error: reference clock %d is not supported for node %d, qlm %d\n",
			       __func__, ref_clk_sel, node, qlm);
			return -1;
		}
	}
		/* Power up PHY, but keep it in reset */
	phy_ctl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PHY_CTL(qlm));
	phy_ctl.s.phy_pd = 0;
	phy_ctl.s.phy_reset = 1;
	cvmx_write_csr_node(node, CVMX_GSERX_PHY_CTL(qlm), phy_ctl.u64);

	/* Errata GSER-20788: GSER(0..13)_CFG[BGX_QUAD]=1 is broken. Force the
	   BGX_QUAD bit to be clear for CN78XX pass 1.x */
	if (OCTEON_IS_MODEL(OCTEON_CN78XX_PASS1_X))
		is_bgx &= 3;

	/* Set GSER for the interface mode */
	cfg.u64 = cvmx_read_csr_node(node, CVMX_GSERX_CFG(qlm));
	cfg.s.ila = is_ilk;
	cfg.s.bgx = is_bgx & 1;
	cfg.s.bgx_quad = (is_bgx >> 2) & 1;
	cfg.s.bgx_dual = (is_bgx >> 1) & 1;
	cfg.s.pcie = is_pcie;
	cvmx_write_csr_node(node, CVMX_GSERX_CFG(qlm), cfg.u64);

	/* Lane mode */
	lmode.u64 = cvmx_read_csr_node(node, CVMX_GSERX_LANE_MODE(qlm));
	lmode.s.lmode = lane_mode;
	cvmx_write_csr_node(node, CVMX_GSERX_LANE_MODE(qlm), lmode.u64);

	/* BGX0-1 can connect to QLM0-1 or QLM 2-3. Program the select bit if we're
	   one of these QLMs and we're using BGX */
	if ((qlm < 4) && is_bgx) {
		int bgx = qlm & 1;
		int use_upper = (qlm >> 1) & 1;
		cvmx_bgxx_cmr_global_config_t global_cfg;

		global_cfg.u64 = cvmx_read_csr_node(node, CVMX_BGXX_CMR_GLOBAL_CONFIG(bgx));
		global_cfg.s.pmux_sds_sel = use_upper;
		cvmx_write_csr_node(node, CVMX_BGXX_CMR_GLOBAL_CONFIG(bgx), global_cfg.u64);
	}

	/* Bring phy out of reset */
	phy_ctl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PHY_CTL(qlm));
	phy_ctl.s.phy_reset = 0;
	cvmx_write_csr_node(node, CVMX_GSERX_PHY_CTL(qlm), phy_ctl.u64);
	cvmx_read_csr_node(node, CVMX_GSERX_PHY_CTL(qlm));

	/*
	 * Wait 250 ns until the managment interface is ready to accept
	 * read/write commands.
	 */
	cvmx_wait_usec(3);

	/* Because of the Errata where quad mode does not work, program
	   lmac_type to figure out the type of BGX interface configured */
	if (is_bgx) {
		int bgx = (qlm < 2) ? qlm : qlm - 2;
		cvmx_bgxx_cmrx_config_t cmr_config;

		cmr_config.u64 = cvmx_read_csr_node(node, CVMX_BGXX_CMRX_CONFIG(0, bgx));
		cmr_config.s.lmac_type = lmac_type;
		cvmx_write_csr_node(node, CVMX_BGXX_CMRX_CONFIG(0, bgx), cmr_config.u64);

		/* Enable/disable training for 10G_KR/40G_KR4/XFI/XLAUI modes */
		if (mode == CVMX_QLM_MODE_10G_KR || mode == CVMX_QLM_MODE_40G_KR4) {
			cvmx_bgxx_spux_br_pmd_control_t spu_pmd_control;
			spu_pmd_control.u64 = cvmx_read_csr_node(node, CVMX_BGXX_SPUX_BR_PMD_CONTROL(0, bgx));
			spu_pmd_control.s.train_en = 1;
			cvmx_write_csr_node(node, CVMX_BGXX_SPUX_BR_PMD_CONTROL(0, bgx), spu_pmd_control.u64);
		} else if (mode == CVMX_QLM_MODE_XFI || mode == CVMX_QLM_MODE_XLAUI) {
			cvmx_bgxx_spux_br_pmd_control_t spu_pmd_control;
			spu_pmd_control.u64 = cvmx_read_csr_node(node, CVMX_BGXX_SPUX_BR_PMD_CONTROL(0, bgx));
			spu_pmd_control.s.train_en = 0;
			cvmx_write_csr_node(node, CVMX_BGXX_SPUX_BR_PMD_CONTROL(0, bgx), spu_pmd_control.u64);
		}
	}


	/* Configure the gser pll */
	__qlm_setup_pll_cn78xx(node, qlm);

	/* Wait for reset to complete and the PLL to lock */
	if (CVMX_WAIT_FOR_FIELD64_NODE(node, CVMX_GSERX_PLL_STAT(qlm), cvmx_gserx_pll_stat_t, pll_lock, ==, 1, 10000)) {
		cvmx_warn("%d:QLM%d: Timeout waiting for GSERX_PLL_STAT[pll_lock]\n", node, qlm);
		return -1;
	}

	/* Perform PCIe errata workaround */
	if (is_pcie)
		__cvmx_qlm_pcie_errata_cn78xx(node, qlm);
	else
		__qlm_init_errata_20844(node, qlm);

	/* Wait for reset to complete and the PLL to lock */
	/* PCIe mode doesn't become ready until the PEM block attempts to bring
 	   the interface up. Skip this check for PCIe */
	if (!is_pcie && CVMX_WAIT_FOR_FIELD64_NODE(node, CVMX_GSERX_QLM_STAT(qlm), cvmx_gserx_qlm_stat_t, rst_rdy, ==, 1, 10000)) {
		cvmx_warn("%d:QLM%d: Timeout waiting for GSERX_QLM_STAT[rst_rdy]\n", node, qlm);
		return -1;
	}

	/* Apply any custom tuning */
	__qlm_tune_cn78xx(node, qlm, mode, baud_mhz);
	return 0;
}


static int __is_qlm_valid_bgx_cn73xx(int qlm)
{
   if (qlm == 2 || qlm == 3 || qlm == 5 || qlm == 6)
	return 0;
   return 1;
}

/**
 * Configure QLM/DLM speed and mode for cn73xx.
 *
 * @param qlm     The QLM to configure
 * @param baud_mhz   The speed the QLM needs to be configured in Mhz.
 * @param mode    The QLM to be configured as SGMII/XAUI/PCIe.
 * @param rc      Only used for PCIe, rc = 1 for root complex mode, 0 for EP mode.
 * @param gen3    Only used for PCIe
 * 			gen3 = 2 GEN3 mode
 * 			gen3 = 1 GEN2 mode
 * 			gen3 = 0 GEN1 mode
 *
 * @param ref_clk_sel    The reference-clock selection to use to configure QLM
 * 			 0 = REF_100MHZ
 * 			 1 = REF_125MHZ
 * 			 2 = REF_156MHZ
 * @param ref_clk_input  The reference-clock input to use to configure QLM
 *
 * @return       Return 0 on success or -1.
 */
static int octeon_configure_qlm_cn73xx(int qlm, int baud_mhz,
				int mode, int rc,
				int gen3, int ref_clk_sel,
				int ref_clk_input)
{
	cvmx_gserx_phy_ctl_t phy_ctl;
	cvmx_gserx_lane_mode_t lmode;
	cvmx_gserx_cfg_t cfg;
	cvmx_gserx_refclk_sel_t refclk_sel;
	int is_pcie = 0;
	int is_sata = 0;
	int is_bgx = 0;
	int lane_mode = 0;
	short lmac_type[4] = {0};
	short sds_lane[4] = {0};
	bool alt_pll = false;
	int enable_training = 0;

	debug("%s(qlm: %d, baud_mhz: %d, mode: %d, rc: %d, gen3: %d, ref_clk_sel: %d, ref_clk_input: %d\n",
	      __func__, qlm, baud_mhz, mode, rc, gen3, ref_clk_sel,
	      ref_clk_input);

	cfg.u64 = cvmx_read_csr(CVMX_GSERX_CFG(qlm));

	/* If PEM is in EP, no need to do anything */
	if (cfg.s.pcie && rc == 0) {
		debug("%s: qlm %d is in PCIe endpoint mode, returning\n",
		      __func__, qlm);
		return 0;
	}

	/* Set the reference clock to use */
	refclk_sel.u64 = 0;
	if (ref_clk_input == 0) { /* External ref clock */
		refclk_sel.s.com_clk_sel = 0;
		refclk_sel.s.use_com1 = 0;
	} else if (ref_clk_input == 1) {
		refclk_sel.s.com_clk_sel = 1;
		refclk_sel.s.use_com1 = 0;
	} else {
		refclk_sel.s.com_clk_sel = 1;
		refclk_sel.s.use_com1 = 1;
	}

	cvmx_write_csr(CVMX_GSERX_REFCLK_SEL(qlm), refclk_sel.u64);

	/* Reset the QLM after changing the reference clock */
	phy_ctl.u64 = cvmx_read_csr(CVMX_GSERX_PHY_CTL(qlm));
	phy_ctl.s.phy_reset = 1;
	phy_ctl.s.phy_pd = 1;
	cvmx_write_csr(CVMX_GSERX_PHY_CTL(qlm), phy_ctl.u64);

	cvmx_wait(1000);

	/* Check if QLM is a valid BGX interface */
	if (mode != CVMX_QLM_MODE_PCIE
	    && mode != CVMX_QLM_MODE_PCIE_1X8
	    && mode != CVMX_QLM_MODE_SATA_2X1) {
		if (__is_qlm_valid_bgx_cn73xx(qlm))
			return -1;
	}

	switch (mode) {
	case CVMX_QLM_MODE_PCIE:
	case CVMX_QLM_MODE_PCIE_1X8:
		/* FIXME later */
		break;
	case CVMX_QLM_MODE_SATA_2X1:
		if (qlm != 4)
			return -1;
		is_sata = 1;
		break;
	case CVMX_QLM_MODE_SGMII:
		is_bgx = 1;
		lmac_type[0] = 0;
		lmac_type[1] = 0;
		lmac_type[2] = 0;
		lmac_type[3] = 0;
		sds_lane[0] = 0;
		sds_lane[1] = 1;
		sds_lane[2] = 2;
		sds_lane[3] = 3;
		break;
	case CVMX_QLM_MODE_XAUI:
		is_bgx = 5;
		lmac_type[0] = 1;
		lmac_type[1] = -1;
		lmac_type[2] = -1;
		lmac_type[3] = -1;
		sds_lane[0] = 0xe4;
		break;
	case CVMX_QLM_MODE_RXAUI:
		is_bgx = 3;
		lmac_type[0] = 2;
		lmac_type[1] = 2;
		lmac_type[2] = -1;
		lmac_type[3] = -1;
		sds_lane[0] = 0x4;
		sds_lane[1] = 0xe;
		break;
	case CVMX_QLM_MODE_10G_KR:
		enable_training = 1;
	case CVMX_QLM_MODE_XFI:		/* 10GR_4X1 */
		is_bgx = 1;
		lmac_type[0] = 3;
		lmac_type[1] = 3;
		lmac_type[2] = 3;
		lmac_type[3] = 3;
		sds_lane[0] = 0;
		sds_lane[1] = 1;
		sds_lane[2] = 2;
		sds_lane[3] = 3;
		break;
	case CVMX_QLM_MODE_40G_KR4:
		enable_training = 1;
	case CVMX_QLM_MODE_XLAUI:	/* 40GR4_1X4 */
		is_bgx = 5;
		lmac_type[0] = 4;
		lmac_type[1] = -1;
		lmac_type[2] = -1;
		lmac_type[3] = -1;
		sds_lane[0] = 0xe4;
		break;
	case CVMX_QLM_MODE_RGMII_SGMII:
		is_bgx = 1;
		lmac_type[0] = 5;
		lmac_type[1] = 0;
		lmac_type[2] = 0;
		lmac_type[3] = 0;
		sds_lane[0] = 0;
		sds_lane[1] = 1;
		sds_lane[2] = 2;
		sds_lane[3] = 3;
		break;
	case CVMX_QLM_MODE_RGMII_10G_KR:
		enable_training = 1;
	case CVMX_QLM_MODE_RGMII_XFI:
		is_bgx = 1;
		lmac_type[0] = 5;
		lmac_type[1] = 3;
		lmac_type[2] = 3;
		lmac_type[3] = 3;
		sds_lane[0] = 0;
		sds_lane[1] = 1;
		sds_lane[2] = 2;
		sds_lane[3] = 3;
		break;
	case CVMX_QLM_MODE_RGMII_40G_KR4:
		enable_training = 1;
	case CVMX_QLM_MODE_RGMII_XLAUI:
		is_bgx = 5;
		lmac_type[0] = 5;
		lmac_type[1] = 4;
		lmac_type[2] = -1;
		lmac_type[3] = -1;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0xe4;
		break;
	case CVMX_QLM_MODE_RGMII_RXAUI:
		is_bgx = 3;
		lmac_type[0] = 5;
		lmac_type[1] = 2;
		lmac_type[2] = 2;
		lmac_type[3] = -1;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x4;
		sds_lane[2] = 0xe;
		break;
	case CVMX_QLM_MODE_RGMII_XAUI:
		is_bgx = 5;
		lmac_type[0] = 5;
		lmac_type[1] = 1;
		lmac_type[2] = -1;
		lmac_type[3] = -1;
		sds_lane[0] = 0;
		sds_lane[1] = 0xe4;
		break;
	case CVMX_QLM_MODE_SGMII_10G_KR:
		enable_training = 1;
	case CVMX_QLM_MODE_SGMII_XFI:
		is_bgx = 3;
		lmac_type[0] = 0;
		lmac_type[1] = 0;
		lmac_type[2] = 3;
		lmac_type[3] = 3;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x1;
		sds_lane[2] = 0x2;
		sds_lane[3] = 0x3;
		break;
	case CVMX_QLM_MODE_SGMII_RXAUI:
		is_bgx = 3;
		lmac_type[0] = 0;
		lmac_type[1] = 0;
		lmac_type[2] = 2;
		lmac_type[3] = -1;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x1;
		sds_lane[2] = 0xe;
		break;
	case CVMX_QLM_MODE_10G_KR_RXAUI:
		enable_training = 1;
	case CVMX_QLM_MODE_XFI_RXAUI:
		is_bgx = 3;
		lmac_type[0] = 3;
		lmac_type[1] = 3;
		lmac_type[2] = 2;
		lmac_type[3] = -1;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x1;
		sds_lane[2] = 0xe;
		break;
	case CVMX_QLM_MODE_RGMII_SGMII_10G_KR:
		enable_training = 1;
	case CVMX_QLM_MODE_RGMII_SGMII_XFI:
		is_bgx = 1;
		lmac_type[0] = 5;
		lmac_type[1] = 0;
		lmac_type[2] = 3;
		lmac_type[3] = 3;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x1;
		sds_lane[2] = 0x2;
		sds_lane[3] = 0x3;
		break;
	case CVMX_QLM_MODE_RGMII_SGMII_RXAUI:
		is_bgx = 1;
		lmac_type[0] = 5;
		lmac_type[1] = 0;
		lmac_type[2] = 2;
		lmac_type[3] = -1;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x1;
		sds_lane[2] = 0xe;
		break;
	case CVMX_QLM_MODE_RGMII_10G_KR_SGMII:
		enable_training = 1;
	case CVMX_QLM_MODE_RGMII_XFI_SGMII:
		is_bgx = 1;
		lmac_type[0] = 5;
		lmac_type[1] = 3;
		lmac_type[2] = 0;
		lmac_type[3] = 0;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x1;
		sds_lane[2] = 0x2;
		sds_lane[3] = 0x3;
		break;
	case CVMX_QLM_MODE_RGMII_10G_KR_RXAUI:
		enable_training = 1;
	case CVMX_QLM_MODE_RGMII_XFI_RXAUI:
		is_bgx = 1;
		lmac_type[0] = 5;
		lmac_type[1] = 3;
		lmac_type[2] = 2;
		lmac_type[3] = -1;
		sds_lane[0] = 0x0;
		sds_lane[1] = 0x1;
		sds_lane[2] = 0xe;
		break;
	default:
		break;
	}

	lane_mode = __get_lane_mode_for_speed_and_ref_clk(ref_clk_sel,
							  baud_mhz,
							  &alt_pll);
	debug("%s: %d lane mode: %d, alternate PLL: %s\n", __func__,
	      mode, lane_mode, alt_pll ? "true" : "false");
	if (lane_mode == -1)
		return -1;

	if (alt_pll) {
		debug("%s: alternate PLL settings used for qlm %d, lane mode %d, reference clock %d\n",
		      __func__, qlm, lane_mode, ref_clk_sel);
		if (__set_qlm_ref_clk_cn78xx(0, qlm, lane_mode,
					    ref_clk_sel)) {
			printf("%s: Error: reference clock %d is not supported for qlm %d\n",
			       __func__, ref_clk_sel, qlm);
			return -1;
		}
	}

	/* Power up PHY, but keep it in reset */
	phy_ctl.u64 = cvmx_read_csr(CVMX_GSERX_PHY_CTL(qlm));
	phy_ctl.s.phy_pd = 0;
	phy_ctl.s.phy_reset = 1;
	cvmx_write_csr(CVMX_GSERX_PHY_CTL(qlm), phy_ctl.u64);

	/* Set GSER for the interface mode */
	cfg.u64 = cvmx_read_csr(CVMX_GSERX_CFG(qlm));
	cfg.s.bgx = is_bgx & 1;
	cfg.s.bgx_quad = (is_bgx >> 2) & 1;
	cfg.s.bgx_dual = (is_bgx >> 1) & 1;
	cfg.s.pcie = is_pcie;
	cfg.s.sata = is_sata;
	cvmx_write_csr(CVMX_GSERX_CFG(qlm), cfg.u64);

	/* Lane mode */
	lmode.u64 = cvmx_read_csr(CVMX_GSERX_LANE_MODE(qlm));
	lmode.s.lmode = lane_mode;
	cvmx_write_csr(CVMX_GSERX_LANE_MODE(qlm), lmode.u64);

	/* Bring phy out of reset */
	phy_ctl.u64 = cvmx_read_csr(CVMX_GSERX_PHY_CTL(qlm));
	phy_ctl.s.phy_reset = 0;
	cvmx_write_csr(CVMX_GSERX_PHY_CTL(qlm), phy_ctl.u64);
	cvmx_read_csr(CVMX_GSERX_PHY_CTL(qlm));

	/*
	 * Wait 250 ns until the managment interface is ready to accept
	 * read/write commands.
	 */
	cvmx_wait_usec(3);

	/* Because of the Errata where quad mode does not work, program
	   lmac_type to figure out the type of BGX interface configured */
	if (is_bgx) {
		int bgx = (qlm < 2) ? qlm : qlm - 2;
		cvmx_bgxx_cmrx_config_t cmr_config;
		cvmx_bgxx_spux_br_pmd_control_t spu_pmd_control;
		int index, total_lmacs = 0;

		for (index = 0; index < 4; index++) {
			cmr_config.u64 = cvmx_read_csr(CVMX_BGXX_CMRX_CONFIG(index, bgx));
			cmr_config.s.enable = 0;
			cmr_config.s.data_pkt_rx_en = 0;
			cmr_config.s.data_pkt_tx_en = 0;
			if (lmac_type[index] != -1) {
				cmr_config.s.lmac_type = lmac_type[index];
				cmr_config.s.lane_to_sds = sds_lane[index];
				total_lmacs++;
			} else {
				cmr_config.s.lmac_type = 0;
				cmr_config.s.lane_to_sds = 0;
			}
			cvmx_write_csr(CVMX_BGXX_CMRX_CONFIG(index, bgx), cmr_config.u64);

			/* Enable training for 10G_KR/40G_KR4 modes */
			if (enable_training == 1
			    && (lmac_type[index] == 3 || lmac_type[index] == 4)) {

				spu_pmd_control.u64 = cvmx_read_csr(CVMX_BGXX_SPUX_BR_PMD_CONTROL(index, bgx));
				spu_pmd_control.s.train_en = 1;
				cvmx_write_csr(CVMX_BGXX_SPUX_BR_PMD_CONTROL(index, bgx), spu_pmd_control.u64);
			}
		}

		/* Update the total number of lmacs */
		cvmx_write_csr(CVMX_BGXX_CMR_RX_LMACS(bgx), total_lmacs);
		cvmx_write_csr(CVMX_BGXX_CMR_TX_LMACS(bgx), total_lmacs);
	}

	/* Configure the gser pll */
	__qlm_setup_pll_cn78xx(0, qlm);

	/* Wait for reset to complete and the PLL to lock */
	if (CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_PLL_STAT(qlm), cvmx_gserx_pll_stat_t, pll_lock, ==, 1, 10000)) {
		cvmx_warn("QLM%d: Timeout waiting for GSERX_PLL_STAT[pll_lock]\n", qlm);
		return -1;
	}

	/* Wait for reset to complete and the PLL to lock */
	/* PCIe mode doesn't become ready until the PEM block attempts to bring
 	   the interface up. Skip this check for PCIe */
	if (!is_pcie && CVMX_WAIT_FOR_FIELD64(CVMX_GSERX_QLM_STAT(qlm), cvmx_gserx_qlm_stat_t, rst_rdy, ==, 1, 10000)) {
		cvmx_warn("QLM%d: Timeout waiting for GSERX_QLM_STAT[rst_rdy]\n", qlm);
		return -1;
	}
	return 0;
}

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
int octeon_configure_qlm(int qlm, int speed, int mode, int rc, int pcie_mode,
			int ref_clk_sel, int ref_clk_input)
{
	int node = cvmx_get_node_num();
	debug("%s(%d, %d, %d, %d, %d, %d, %d)\n", __func__, qlm, speed, mode,
	      rc, pcie_mode, ref_clk_sel, ref_clk_input);
	if (OCTEON_IS_MODEL(OCTEON_CN61XX) || OCTEON_IS_MODEL(OCTEON_CNF71XX))
		return octeon_configure_qlm_cn61xx(qlm, speed, mode, rc,
						   pcie_mode);
	else if (OCTEON_IS_MODEL(OCTEON_CN70XX))
		return octeon_configure_qlm_cn70xx(qlm, speed, mode, rc,
						   pcie_mode, ref_clk_sel,
						   ref_clk_input);
	else if (OCTEON_IS_MODEL(OCTEON_CN78XX))
		return octeon_configure_qlm_cn78xx(node, qlm, speed, mode, rc,
						   pcie_mode, ref_clk_sel,
						   ref_clk_input);
	else if (OCTEON_IS_MODEL(OCTEON_CN73XX))
		return octeon_configure_qlm_cn73xx(qlm, speed, mode, rc,
						   pcie_mode, ref_clk_sel,
						   ref_clk_input);
	else
		return -1;
}

void octeon_init_qlm(int node)
{
	int qlm;
	cvmx_gserx_phy_ctl_t phy_ctl;
	cvmx_gserx_cfg_t cfg;
	int baud_mhz;
	int mode, pem;

	if (!OCTEON_IS_MODEL(OCTEON_CN78XX))
		return;

	for (qlm = 0; qlm < 8; qlm++) {
		phy_ctl.u64 = cvmx_read_csr_node(node, CVMX_GSERX_PHY_CTL(qlm));
		if (phy_ctl.s.phy_reset == 0) {
			cfg.u64 = cvmx_read_csr_node(node, CVMX_GSERX_CFG(qlm));
			if (cfg.s.pcie)
				__cvmx_qlm_pcie_errata_cn78xx(node, qlm);
			else
				__qlm_init_errata_20844(node, qlm);

			mode = cvmx_qlm_get_mode_cn78xx(node, qlm);
			baud_mhz = cvmx_qlm_get_gbaud_mhz_node(node, qlm);
			__qlm_tune_cn78xx(node, qlm, mode, baud_mhz);
		}
	}

	/* Setup how each PEM drives the PERST lines */
	for (pem = 0; pem < 4; pem++) {
		cvmx_rst_ctlx_t rst_ctl;
		rst_ctl.u64 = cvmx_read_csr_node(node, CVMX_RST_CTLX(pem));
		__setup_pem_reset(node, pem, !rst_ctl.s.host_mode);
	}
}
