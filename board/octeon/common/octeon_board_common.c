/***********************license start************************************
 * Copyright (c) 2012 - 2014 Cavium Inc. (support@cavium.com). All rights
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

#include <common.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/octeon-model.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/cvmx-asm.h>
#include <asm/arch/cvmx-mio-defs.h>
#include <asm/arch/octeon_boot_bus.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/octeon_board_phy.h>
#ifdef CONFIG_OCTEON_ENABLE_PAL
#  include <asm/arch/octeon_boot.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

/* Generic bootloaders *MUST* have a TLV EEPROM */
#if !defined(CONFIG_OCTEON_GENERIC_EMMC_STAGE2)		&& \
    !defined(CONFIG_OCTEON_GENERIC_SPI_STAGE2)		&& \
    !defined(CONFIG_OCTEON_GENERIC_NAND2_STAGE2)	&& \
    !defined(CONFIG_OCTEON_GENERIC_NAND_STAGE2)
/* We can't do this check for generic bootloaders */
# if CONFIG_SYS_I2C_EEPROM_ADDR || CONFIG_SYS_DEF_EEPROM_ADDR
#  define USE_EEPROM	1
# else
#  define USE_EEPROM	0
# endif
#else
# define USE_EEPROM	1
#endif

/**
 * Dynamically adjust the board name, used for prompt generation
 * @param name - name string to be adjusted
 * @param max_len - maximum length of name
 *
 * This function can overwrite the name of a board, for example based on
 * the processor installed.
 */
void octeon_adjust_board_name(char *name, size_t max_len) __attribute__((weak));
void octeon_adjust_board_name(char *name, size_t max_len)
{
	return;
}

int checkboard(void)
{
	return 0;
}
int checkboard(void) __attribute__((weak));

int board_usb_preinit(void) __attribute__((weak));

int board_usb_preinit(void)
{
	return 0;
}

void board_usb_postinit(void) __attribute__((weak));

void board_usb_postinit(void)
{
}

#ifdef CONFIG_OCTEON_ENABLE_PAL
void show_pal_info(void) __attribute__((weak, alias("__show_pal_info")));
void __show_pal_info(void)
{
	int pal_rev_maj = 0;
	int pal_rev_min = 0;
	int voltage_100ths = 0;
	int voltage_1s = 0;
	int mcu_rev_maj = 0;
	int mcu_rev_min = 0;
	char tmp[10];
	uint64_t pal_addr;

	if (octeon_boot_bus_get_dev_info("/soc/bootbus/pal",
					 "cavium,ebt3000-pal", &pal_addr, NULL))
		pal_addr = gd->arch.pal_addr;

	if (!pal_addr) {
		debug("%s: PAL not found\n", __func__);
		octeon_led_str_write("Boot    ");
		return;
	}

	if (octeon_read64_byte(pal_addr) == 0xa5
	    && octeon_read64_byte(pal_addr + 1) == 0x5a) {
		pal_rev_maj = octeon_read64_byte(pal_addr + 2);
		pal_rev_min = octeon_read64_byte(pal_addr + 3);
		if ((octeon_read64_byte(pal_addr + 4)) > 0xf) {
			voltage_1s = 0;
			voltage_100ths =
			    (600 + (31 - octeon_read64_byte(pal_addr + 4)) * 25);
		} else {
			voltage_1s = 1;
			voltage_100ths =
			    ((15 - octeon_read64_byte(pal_addr + 4)) * 5);
		}
	}

	if (twsii_mcu_read(0x00) == 0xa5
	    && twsii_mcu_read(0x01) == 0x5a) {
		gd->arch.mcu_rev_maj = mcu_rev_maj = twsii_mcu_read(2);
		gd->arch.mcu_rev_min = mcu_rev_min = twsii_mcu_read(3);
	}

	printf("PAL rev: %d.%02d, MCU rev: %d.%02d, CPU voltage: %d.%02d\n",
	       pal_rev_maj, pal_rev_min, mcu_rev_maj, mcu_rev_min,
	       voltage_1s, voltage_100ths);

	/* Display CPU speed on display */
	sprintf(tmp, "%d %.1d.%.2lu ", (int)gd->cpu_clk,
		voltage_1s, (long unsigned int)voltage_100ths);
	octeon_led_str_write(tmp);
}

#endif /* CONFIG_OCTEON_ENABLE_PAL */

/**
 * Generate a random MAC address
 */
void octeon_board_create_random_mac_addr(void)
	__attribute__((weak,alias("__octeon_board_create_random_mac_addr")));

void __octeon_board_create_random_mac_addr(void)
{
#ifndef CONFIG_OCTEON_SIM
	uint8_t fuse_buf[128];
	cvmx_mio_fus_rcmd_t fus_rcmd;
	uint32_t poly = 0x04c11db7;
	uint32_t crc = 0xffffffff;
	uint64_t *ptr;
	int ser_len = strlen((char *)gd->arch.board_desc.serial_str);
	int i;

	memset(fuse_buf, 0, sizeof(fuse_buf));
	fuse_buf[0] = gd->arch.board_desc.board_type;
	fuse_buf[1] = (gd->arch.board_desc.rev_major << 4)
			| gd->arch.board_desc.rev_minor;
	fuse_buf[2] = ser_len;
	strncpy((char*)(fuse_buf+3), (char*)gd->arch.board_desc.serial_str,
		sizeof(fuse_buf)-3);

	/* For a random number we perform a CRC32 using the board type,
	 * revision, serial number length, serial number and for OCTEON 2 and 3
	 * the fuse settings.
	 */

	CVMX_MT_CRC_POLYNOMIAL(poly);
	CVMX_ES32(crc, crc);
	CVMX_MT_CRC_IV_REFLECT(crc);
	ptr = (uint64_t *)fuse_buf;
	for (i = 0; i < sizeof(fuse_buf); i += 8)
		CVMX_MT_CRC_DWORD_REFLECT(*ptr++);

	if (!OCTEON_IS_OCTEON1PLUS()) {
		fus_rcmd.u64 = 0;
		fus_rcmd.s.pend = 1;
		for (i = 0; i < sizeof(fuse_buf); i++) {
			do {
				fus_rcmd.u64 = cvmx_read_csr(CVMX_MIO_FUS_RCMD);
			} while (fus_rcmd.s.pend == 1);
			fuse_buf[i] = fus_rcmd.s.dat;
		}
		for (i = 0; i < sizeof(fuse_buf); i += 8)
			CVMX_MT_CRC_DWORD_REFLECT(*ptr++);
	}
	/* Get the final CRC32 */
	CVMX_MF_CRC_IV_REFLECT(crc);
	crc ^= 0xffffffff;

	gd->arch.mac_desc.count = 255;
	gd->arch.mac_desc.mac_addr_base[0] = 0x02;	/* locally administered */
	gd->arch.mac_desc.mac_addr_base[1] = crc & 0xff;
	gd->arch.mac_desc.mac_addr_base[2] = (crc >> 8) & 0xff;
	gd->arch.mac_desc.mac_addr_base[3] = (crc >> 16) & 0xff;
	gd->arch.mac_desc.mac_addr_base[4] = (crc >> 24) & 0xff;
	gd->arch.mac_desc.mac_addr_base[5] = 0;
	debug("Created random MAC address %pM", gd->arch.mac_desc.mac_addr_base);
#else
	gd->arch.mac_desc.mac_addr_base[0] = 2;
	gd->arch.mac_desc.mac_addr_base[1] = 0x00;
	gd->arch.mac_desc.mac_addr_base[2] = 0xDE;
	gd->arch.mac_desc.mac_addr_base[3] = 0xAD;
	gd->arch.mac_desc.mac_addr_base[4] = 0xBF;
	gd->arch.mac_desc.mac_addr_base[5] = 0x00;
	gd->arch.mac_desc.count = 255;
#endif
}

/**
 * Gets the MAC address for a board from the TLV EEPROM.  If not present a
 * random MAC address is generated.
 */
void octeon_board_get_mac_addr(void)
	__attribute__((weak, alias("__octeon_board_get_mac_addr")));

void __octeon_board_get_mac_addr(void)
{
#if USE_EEPROM
	uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
	int addr;

	addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
					 EEPROM_MAC_ADDR_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr >= 0)
		memcpy((void *)&(gd->arch.mac_desc), ee_buf,
		       sizeof(octeon_eeprom_mac_addr_t));
	else
#endif
		octeon_board_create_random_mac_addr();
}

/**
 * Gets the board clock info from the TLV EEPROM or uses the default value
 * if not available.
 *
 * @param def_ddr_clock_mhz	Default DDR clock speed in MHz
 */
void octeon_board_get_clock_info(uint32_t def_ddr_clock_mhz)
	__attribute__((weak, alias("__octeon_board_get_clock_info")));

void __octeon_board_get_clock_info(uint32_t def_ddr_clock_mhz)
{
	uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH] __attribute__((unused));
	int addr __attribute__((unused));
	/* Assume no descriptor is present */
	gd->mem_clk = def_ddr_clock_mhz;

	debug("%s(%u)\n", __func__, def_ddr_clock_mhz);
	/* Initialize DDR reference frequency if not already set. */
	if (!gd->arch.ddr_ref_hertz)
		gd->arch.ddr_ref_hertz = 50000000;

#if USE_EEPROM
	/* OCTEON I and OCTEON Plus use the old clock descriptor of which
	 * there are two versions.  OCTEON II uses a dedicated DDR clock
	 * descriptor instead.
	 */
	if (OCTEON_IS_OCTEON1PLUS()) {
		octeon_eeprom_header_t *header;
		struct octeon_eeprom_clock_desc_v1 *clock_v1;
		struct octeon_eeprom_clock_desc_v2 *clock_v2;

		addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
						 EEPROM_CLOCK_DESC_TYPE, 0,
						 ee_buf,
						 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
		if (addr < 0)
			return;

		header = (octeon_eeprom_header_t *)ee_buf;
		switch (header->major_version) {
		case 1:
			clock_v1 = (struct octeon_eeprom_clock_desc_v1 *)ee_buf;
			gd->mem_clk = clock_v1->ddr_clock_mhz;
			break;
		case 2:
			clock_v2 = (struct octeon_eeprom_clock_desc_v2 *)ee_buf;
			gd->mem_clk = clock_v2->ddr_clock_mhz;
			break;
		default:
			printf("Unknown TLV clock header version %d.%d\n",
			       header->major_version, header->minor_version);
		}
	} else {
		octeon_eeprom_ddr_clock_desc_t *ddr_clock_ptr;
		addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
						 EEPROM_DDR_CLOCK_DESC_TYPE, 0,
						 ee_buf,
						 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
		if (addr < 0)
			return;


		ddr_clock_ptr = (octeon_eeprom_ddr_clock_desc_t *)ee_buf;
		gd->mem_clk = ddr_clock_ptr->ddr_clock_hz / 1000000;
	}
	if (gd->mem_clk < 100 || gd->mem_clk > 2000)
		gd->mem_clk = def_ddr_clock_mhz;
#endif
}

/**
 * Reads the board descriptor from the TLV EEPROM or fills in the default
 * values.
 *
 * @param type		board type
 * @param rev_major	board major revision
 * @param rev_minor	board minor revision
 */
void octeon_board_get_descriptor(enum cvmx_board_types_enum type,
				 int rev_major, int rev_minor)
	__attribute__((weak, alias("__octeon_board_get_descriptor")));

void __octeon_board_get_descriptor(enum cvmx_board_types_enum type,
				   int rev_major, int rev_minor)
{
	struct bootloader_header *header;

	gd->arch.board_desc.board_type = CVMX_BOARD_TYPE_NULL;
	debug("%s(%s, %d, %d)\n", __func__, cvmx_board_type_to_string(type),
	      rev_major, rev_minor);

	if (type == CVMX_BOARD_TYPE_NULL) {
		debug("NULL board type passed, extracting board type from header\n");
		header = CASTPTR(struct bootloader_header, CONFIG_SYS_TEXT_BASE);
		if (validate_bootloader_header(header)) {
			gd->flags |= GD_FLG_BOARD_DESC_MISSING;
			gd->arch.board_desc.rev_major = header->maj_rev;
			gd->arch.board_desc.rev_minor = header->min_rev;
			gd->arch.board_desc.board_type = header->board_type;
			strncpy((char *)(gd->arch.board_desc.serial_str),
				"unknown", SERIAL_LEN);
			debug("header board type: %s, revision %d.%d\n",
			      cvmx_board_type_to_string(header->board_type),
			      header->maj_rev, header->min_rev);
		}
	}
#if USE_EEPROM
	{
		uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
		int addr;

		/* Determine board type/rev */
		strncpy((char *)(gd->arch.board_desc.serial_str), "unknown",
			SERIAL_LEN);
		addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
						 EEPROM_BOARD_DESC_TYPE, 0,
						 ee_buf,
						 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
		if (addr >= 0) {
			memcpy((void *)&(gd->arch.board_desc), ee_buf,
			       sizeof(octeon_eeprom_board_desc_t));
			gd->board_type = gd->arch.board_desc.board_type;
			gd->flags &= ~GD_FLG_BOARD_DESC_MISSING;
		}
	}
#endif
	if (gd->arch.board_desc.board_type == CVMX_BOARD_TYPE_NULL) {
		debug("Setting board type to passed-in type %s\n",
		      cvmx_board_type_to_string(type));
		gd->flags |= GD_FLG_BOARD_DESC_MISSING;
		gd->arch.board_desc.rev_major = rev_major;
		gd->arch.board_desc.rev_minor = rev_minor;
		gd->board_type = gd->arch.board_desc.board_type = type;
	}
}


/**
 * Function to write string to LED display
 * @param str - string up to 8 characters to write.
 */
void octeon_led_str_write(const char *str)
	__attribute__((weak, alias("__octeon_led_str_write")));

void __octeon_led_str_write(const char *str)
{
#ifdef CONFIG_OCTEON_ENABLE_LED_DISPLAY
	octeon_led_str_write_std(str);
#endif
}

void __board_mdio_init(void)
{
#ifdef CONFIG_OF_LIBFDT
	octeon_board_phy_init();
#endif
}

void board_mdio_init(void) __attribute__((weak, alias("__board_mdio_init")));


