/*
 * (C) Copyright 2013 Cavium, Inc. <support@cavium.com>
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
 *
 * This file deals with working around various errata.
 */

#include <common.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-mio-defs.h>
#include <asm/mipsregs.h>

/**
 * This function works around various chip errata early in the initialization
 * process.
 *
 * @return 0 for success
 */
int __octeon_early_errata_workaround_f(void)
{
	/* Change default setting for CN58XX pass 2 */
	if (OCTEON_IS_MODEL(OCTEON_CN58XX_PASS2))
		cvmx_write_csr(CVMX_MIO_FUS_EMA, 2);
	return 0;
}

int octeon_early_errata_workaround_f(void)
	__attribute__((weak, alias("__octeon_early_errata_workaround_f")));

/**
 * This function works around various chip errata late in the initialization
 * process.
 *
 * @return 0 for success
 */
int __octeon_late_errata_workaround_f(void)
{
	if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X)) {
		/* Disable Fetch under fill on CN63XXp1 due to errata
		 * Core-14317
		 */
		u64 val;
		val = read_64bit_c0_cvmctl();
		val |= (1ull << 19);	/* CvmCtl[DEFET] */
		write_64bit_c0_cvmctl(val);
	}

	/* Apply workaround for Errata L2C-16862 */
	if (OCTEON_IS_MODEL(OCTEON_CN68XX_PASS1_X) ||
	      OCTEON_IS_MODEL(OCTEON_CN68XX_PASS2_0) ||
	      OCTEON_IS_MODEL(OCTEON_CN68XX_PASS2_1)) {
		cvmx_l2c_ctl_t l2c_ctl;
		cvmx_iob_to_cmb_credits_t iob_cmb_credits;
		cvmx_iob1_to_cmb_credits_t iob1_cmb_credits;

		l2c_ctl.u64 = cvmx_read_csr(CVMX_L2C_CTL);

		if (OCTEON_IS_MODEL(OCTEON_CN68XX_PASS1_X))
			l2c_ctl.cn68xxp1.maxlfb = 6;
		else
			l2c_ctl.cn68xx.maxlfb = 6;
		cvmx_write_csr(CVMX_L2C_CTL, l2c_ctl.u64);

		iob_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB_TO_CMB_CREDITS);
		iob_cmb_credits.s.ncb_rd = 6;
		cvmx_write_csr(CVMX_IOB_TO_CMB_CREDITS, iob_cmb_credits.u64);

		iob1_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB1_TO_CMB_CREDITS);
		iob1_cmb_credits.s.pko_rd = 6;
		cvmx_write_csr(CVMX_IOB1_TO_CMB_CREDITS, iob1_cmb_credits.u64);
#if 0
		l2c_ctl.u64 = cvmx_read_csr(CVMX_L2C_CTL);
		iob_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB_TO_CMB_CREDITS);
		iob1_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB1_TO_CMB_CREDITS);
		printf("L2C_CTL[MAXLFB] = %d, IOB_TO_CMB_CREDITS[ncb_rd] = %d, IOB1_TO_CMB_CREDITS[pko_rd] = %d\n",
			l2c_ctl.s.maxlfb, iob_cmb_credits.s.ncb_rd,
			iob1_cmb_credits.s.pko_rd);
#endif
	}
	return 0;
}

int octeon_late_errata_workaround_f(void)
	__attribute__((weak, alias("__octeon_late_errata_workaround_f")));
