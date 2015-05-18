/***********************license start************************************
 * Copyright (c) 2003-2011 Cavium Inc. (support@cavium.com). All rights
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

/**
 * @file
 *
 * $Id$
 *
 */

#include <common.h>
#include <command.h>
#include <exports.h>
#include <linux/ctype.h>
#include <net.h>
#include <elf.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/arch/cvmx-qlm.h>
#include <asm/arch/cvmx-helper-jtag.h>
#include <asm/arch/octeon_boot.h>

static void usage(char * const argv[])
{
	printf("\n"
	       "Usage:\n"
	       "  %s <qlm>\n"
	       "    Read and display all QLM jtag settings.\n"
	       "\n"
	       "  %s <qlm> <lane> <name> <value> ... <lane x> <name x> <value x>\n"
	       "    Write a QLM \"lane\" jtag setting of \"name\" as \"value\".\n"
	       "\n", argv[0], argv[0]);
	printf("    qlm     Which QLM to access.\n"
	       "    lane    Which lane number to write a setting for or \"all\" for all lanes.\n"
	       "    name    Name of qlm setting to write.\n"
	       "    value   The value can be in decimal of hex (0x...).\n"
	       "\n"
	       "Incorrect settings can damage chips, so be careful!\n" "\n");
}

static uint64_t inline convert_number(const char *str)
{
	unsigned long long result;
	result = simple_strtoul(str, NULL, 10);
	return result;
}

int do_qlm(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{

	int num_qlm;
	int qlm;
	int c_arg;

	/* Make sure we got the correct number of arguments */
	if (((argc - 2) % 3) != 0) {
		printf("Invalid number of arguments %d\n", argc);
		usage(argv);
		return -1;
	}

	num_qlm = cvmx_qlm_get_num();

	qlm = simple_strtoul(argv[1], NULL, 0);

	if ((qlm < 0) || (qlm >= num_qlm)) {
		printf("Invalid qlm number\n");
		return -1;
	}

	if (argc >= 5) {
		int lane;
		uint64_t value;
		int field_count;
		int num_fields = (argc - 2) / 3;
		c_arg = 2;
		for (field_count = 0; field_count < num_fields; field_count++) {
			char name[30];
			if (!strcmp(argv[c_arg], "all"))
				lane = -1;
			else {
				lane = (int)simple_strtol(argv[c_arg], NULL, 0);
				if (lane >= cvmx_qlm_get_lanes(qlm)) {
					printf("Invalid lane passed\n");
					return -1;
				}
			}
			strcpy(name, argv[c_arg + 1]);
			value = convert_number(argv[c_arg + 2]);
			cvmx_qlm_jtag_set(qlm, lane, name, value);

			/* Assert serdes_tx_byp to force the new settings to
  			   override the QLM default. */
			if (strncmp(name, "biasdrv_", 8) == 0 ||
				strncmp(name, "tcoeff_", 7) == 0)
				cvmx_qlm_jtag_set(qlm, lane, "serdes_tx_byp", 1);
			c_arg += 3;
			if (lane == -1)
				break;
		}
	} else
		cvmx_qlm_display_registers(qlm);
	return 0;
}

U_BOOT_CMD(qlm, (CONFIG_SYS_MAXARGS - 1), 0, do_qlm,
	   "Octeon QLM debug function (dangerous - remove from final product)",
	   "Usage:\n"
	   "  qlm <qlm>\n"
	   "    Read and display all QLM jtag settings.\n"
	   "\n"
	   "  qlm <qlm> <lane> <name> <value> ... <lane x> <name x> <value x>\n"
	   "    Write one or more QLM \"lane\" jtag setting of \"name\" as \"value\".\n"
	   "\n"
	   "    qlm     Which QLM to access.\n"
	   "    lane    Which lane number to write a setting for or \"all\" for all lanes.\n"
	   "    name    Name of qlm setting to write.\n"
	   "    value   The value can be in decimal or hex (0x...).\n"
	   " Note: multiple lane, name, value triplets may be specified on the same line.\n"
	   "\n"
	   "WARNING: Incorrect settings can damage chips, so be careful!\n\n");

enum command_ret_t
do_qlm_clock(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	int qlm;

	if (argc < 2)
		return CMD_RET_USAGE;

	qlm = simple_strtoul(argv[1], NULL, 10);

	printf("QLM %d clock speed: %d\n", qlm, cvmx_qlm_measure_clock(qlm));

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(qlmclock, 2, 0, do_qlm_clock,
	   "Print QLM clock speed",
	   "<qlm>\n");

