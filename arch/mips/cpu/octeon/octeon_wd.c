/***********************license start************************************
 * Copyright (c) 2011-2013 Cavium Inc. (support@cavium.com). All rights
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
 * support@cavium.com
 *
 ***********************license end**************************************/

/**
 *
 * $Id$
 *
 * Watchdog support imported from the Linux kernel driver
 */

#include <common.h>
#include <watchdog.h>
#include <asm/mipsregs.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-access.h>
#include <asm/arch/cvmx-ciu-defs.h>
#include <asm/arch/cvmx-mio-defs.h>
#include <asm/arch/octeon_boot.h>
#ifndef CONFIG_SYS_NO_FLASH
#include <mtd/cfi_flash.h>
#endif
#include <asm/arch/octeon_boot_bus.h>

DECLARE_GLOBAL_DATA_PTR;

#define K0		26
#define C0_CVMMEMCTL	11, 7
#define C0_STATUS	12, 0
#define C0_EBASE	15, 1
#define C0_DESAVE	31, 0

extern u64 octeon_get_io_clock_rate(void);

/* Defined in start.S */
extern void nmi_exception_handler(void);
extern void asm_reset(void);

static uint32_t octeon_watchdog_get_period(unsigned int msecs)
{
	uint32_t timeout_cnt;
	/* Divide by 2 since the first interrupt is ignored in U-Boot */
	msecs >>= 1;
	if (msecs > 30000)
		msecs = 30000;
	do {
		timeout_cnt = (((octeon_get_io_clock_rate() >> 8) / 1000) * msecs) >> 8;
		msecs -= 500;
		/* CN68XX seems to run at half the rate */
		if (OCTEON_IS_MODEL(OCTEON_CN68XX))
			timeout_cnt >>= 1;
	} while (timeout_cnt > 65535);
	debug("%s: Setting timeout count to %d\n", __func__, timeout_cnt);
	return timeout_cnt;
}

/**
 * Starts the watchdog with the specified interval.
 *
 * @param msecs - time until NMI is hit.  If zero then use watchdog_timeout
 *                environment variable or default value.
 *
 * NOTE: if the time exceeds the maximum supported by the hardware then it
 * will be limited to the maximum supported by the hardware.
 */
void hw_watchdog_start(int msecs)
{
	union cvmx_ciu_wdogx ciu_wdog;
	static int initialized = 0;

	debug("%s(%d)\n", __func__, msecs);

	if (!initialized) {
		/* Initialize the stage1 code */
		boot_init_vector_t *p = octeon_get_boot_vector();
		if (!p) {
			/* Something bad happened */
			return;
		}
		p[0].code_addr = (uint64_t)(int64_t)(long)&nmi_exception_handler;
		initialized = 1;
	}

	if (!msecs)
		msecs = getenv_ulong("watchdog_timeout", 10,
				     CONFIG_OCTEON_WD_TIMEOUT * 1000);

	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);

	ciu_wdog.u64 = 0;
	ciu_wdog.s.len = octeon_watchdog_get_period(msecs);
	ciu_wdog.s.mode = 3;	/* Interrupt + NMI + soft-reset */

	cvmx_write_csr(CVMX_CIU_WDOGX(0), ciu_wdog.u64);
	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);
}

/**
 * Puts the dog to sleep
 */
void hw_watchdog_disable(void)
{
	union cvmx_ciu_wdogx ciu_wdog;
	/* Disable the hardware. */
	ciu_wdog.u64 = 0;

	/* Poke the watchdog to clear out its state */
	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);
	cvmx_write_csr(CVMX_CIU_WDOGX(0), ciu_wdog.u64);

	debug("%s: Watchdog stopped\n", __func__);
}

/**
 * Pets the watchdog
 */
void hw_watchdog_reset(void)
{
	/* Pet the dog.  Good dog! */
	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);
}

