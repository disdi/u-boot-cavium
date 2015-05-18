/***********************license start************************************
 * Copyright (c) 2011  Cavium Inc. (support@cavium.com).
 * All rights reserved.
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
 ***********************license end**************************************/

#include <common.h>
#include <command.h>
#include <asm/arch/cvmx.h>
#include <ata.h>
#include <part.h>
#include <fat.h>
#include <../disk/part_dos.h>
#include <asm/arch/cvmx-bootloader.h>
#include <asm/gpio.h>
#include <asm/arch/lib_octeon.h>
#ifdef CONFIG_OCTEON_SPI_STAGE2
# include <spi_flash.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

extern unsigned long do_go_exec(ulong (*entry)(int, char * const []), int argc,
				char * const argv[]);

#ifdef CONFIG_OCTEON_SPI_STAGE2
# ifndef CONFIG_SF_DEFAULT_SPEED
#  define CONFIG_SF_DEFAULT_SPEED	1000000
# endif
# ifndef CONFIG_SF_DEFAULT_MODE
#  define CONFIG_SF_DEFAULT_MODE		SPI_MODE_3
# endif
# ifndef CONFIG_SF_DEFAULT_CS
#  define CONFIG_SF_DEFAULT_CS		0
# endif
# ifndef CONFIG_SF_DEFAULT_BUS
#  define CONFIG_SF_DEFAULT_BUS		0
# endif
#endif

#ifdef CONFIG_OCTEON_EMMC_STAGE2
/**
 * Search for a bootable FAT partition
 */
static int find_bootable_fat_partition(block_dev_desc_t *dev_desc)
{
	dos_partition_t *pt;
	int i;

	ALLOC_CACHE_ALIGN_BUFFER(uint8_t, buffer, dev_desc->blksz);

	if ((dev_desc->block_read(dev_desc->dev, 0, 1, (ulong *)buffer) != 1) ||
	    (buffer[DOS_PART_MAGIC_OFFSET + 0] != 0x55) ||
	    (buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) ) {
		printf("Could not read DOS partition table\n");
		return -1;
	}

	pt = (dos_partition_t *)(buffer + DOS_PART_TBL_OFFSET);

	for (i = 0; i < 4; i++) {
		if (pt->boot_ind != 0x80)
			continue;
		switch (pt->sys_ind) {
		case 0x4:		/* FAT16 < 32M */
		case 0x6:		/* FAT16 */
		case 0x14:		/* FAT16 < 32M */
		case 0x16:		/* FAT16 */
		case 0xb:		/* FAT32 */
		case 0xc:		/* FAT32 */
		case 0xe:		/* LBA FAT16 */
		case 0x1b:		/* FAT32 */
		case 0x1c:		/* LBA FAT32 */
		case 0x1e:		/* LBA FAT16 */
			return i+1;
		default:
			break;
		}
	}
	return -1;
}

int do_octbootstage3(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	uint32_t addr, rc;
	int rcode = 0;
	char *filename;
	char *failsafe_filename;
	const char *dev_name;
	int failsafe;
	int size = 0;
	int max_size = 0;
	int part_no;
	int dev_no = 0;
	block_dev_desc_t *dev_desc = NULL;

	filename = getenv("octeon_stage3_bootloader");
	addr = getenv_ulong("octeon_stage3_load_addr", 16,
			    CONFIG_OCTEON_STAGE3_LOAD_ADDR);
	max_size = getenv_ulong("octeon_stage3_max_size", 0,
				CONFIG_OCTEON_STAGE3_MAX_SIZE);
	dev_no = getenv_ulong("octeon_stage3_devno", 0,
			   CONFIG_OCTEON_STAGE3_DEVNO);
	dev_name = getenv("octeon_stage3_devname");

	if (!dev_name)
		dev_name = CONFIG_OCTEON_STAGE3_DEVNAME;

	dev_desc = get_dev(dev_name, dev_no);
	if (!dev_desc) {
		printf("Could not find device %s %d\n", dev_name, dev_no);
		return -1;
	}

	part_no = find_bootable_fat_partition(dev_desc);
	if (part_no < 0) {
		printf("No bootable FAT partition found\n");
		return -1;
	}

	if (fat_register_device(dev_desc, part_no) != 0) {
		printf("Unable to use %s %d:%d for FAT partition\n", dev_name,
		       dev_no, part_no);
	}

#ifndef CONFIG_OCTEON_NO_STAGE3_FAILSAFE
	failsafe = gpio_direction_input(CONFIG_OCTEON_FAILSAFE_GPIO);

	if (!filename) {
		failsafe = 1;
	}

	if (!failsafe) {
		size = file_fat_read(filename, (unsigned char *)addr, max_size);
		if (size <= 0) {
			printf("Could not read %s, trying failsafe\n", filename);
			goto failsafe;
		}
		do_go_exec((void *)addr, argc - 1, argv + 1);
	}

failsafe:
	failsafe_filename = getenv("octeon_stage3_failsafe_bootloader");
	if (!failsafe_filename) {
		failsafe_filename = CONFIG_OCTEON_STAGE3_FAILSAFE_FILENAME;
		printf("Error: environment variable octeon_stage3_failsafe_bootloader is not set.\n");
		return 0;
	}
	if (failsafe_filename) {
		size = file_fat_read(filename, (unsigned char *)addr,
				     max_size);
		if (size <= 0) {
			printf("Could not read failsafe bootloader %s, "
			       "trying %s\n", failsafe_filename,
			       CONFIG_OCTEON_STAGE3_FAILSAFE_FILENAME);
			failsafe_filename = CONFIG_OCTEON_STAGE3_FAILSAFE_FILENAME;
		}
	} else {
		printf("No failsafe available!\n");
		return -1;
	}
#else
	if (!filename)
		filename = CONFIG_OCTEON_STAGE3_FILENAME;

	size = file_fat_read(filename, (unsigned char *)addr, max_size);
	if (size <= 0) {
		printf("Could not open stage 3 bootloader %s\n", filename);
		return -1;
	}
	do_go_exec((void *)addr, argc - 1, argv + 1);
#endif
	return -1;
}

#elif defined(CONFIG_OCTEON_SPI_STAGE2)
/**
 * This will search for one or more bootloaders in the SPI NOR and boot
 * either the failsafe one or the last valid one found.
 *
 * @param cmdtp		Command data structure
 * @param flag		flags, not used
 * @param argc		argument count, not used, passed on
 * @param argv		arguments, not used, passed on
 *
 * @return		-1 on error otherwise no return.
 */
int do_octbootstage3(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct spi_flash *flash;
	struct bootloader_header *header;
	uint32_t addr;
	const int bus = 0;
	const int cs = 0;
	const int speed = CONFIG_SF_DEFAULT_SPEED;
	const int mode = CONFIG_SF_DEFAULT_MODE;
	void *buf;
	int len;
	int offset;
	int found_offset = -1;
	int found_size = 0;
	int rc;
	int failsafe;

	flash = spi_flash_probe(bus, cs, speed, mode);
	if (!flash) {
		printf("Failed to initialize SPI flash at %u:%u", bus, cs);
		return -1;
	}
	addr = getenv_ulong("octeon_stage3_load_addr", 16,
			    CONFIG_OCTEON_STAGE3_LOAD_ADDR);

	header = CASTPTR(struct bootloader_header, addr);
	buf = (void *)header;
#ifdef CONFIG_OCTEON_FAILSAFE_GPIO
	failsafe = gpio_direction_input(CONFIG_OCTEON_FAILSAFE_GPIO);
#else
	failsafe = 0;
#endif

	offset = CONFIG_OCTEON_SPI_BOOT_START;
	do {
		if (spi_flash_read(flash, offset, sizeof(*header), header)) {
			printf("Could not read SPI flash to find bootloader\n");
			return -1;
		}

		if (!validate_bootloader_header(header) ||
		    (header->board_type != gd->arch.board_desc.board_type)) {
			offset += flash->erase_size;
			continue;
		}

		len = header->hlen + header->dlen - sizeof(*header);
		if (len < 0) {
			printf("Invalid length calculated, hlen: %d, dlen: %d\n",
			       header->hlen, header->dlen);
			offset += flash->erase_size;
			continue;
		}
		/* Read rest of bootloader */
		rc = spi_flash_read(flash, offset + sizeof(*header), len,
				    &header[1]);
		if (rc) {
			printf("Could not read %d bytes from SPI flash\n",
			       header->dlen + header->hlen);
			return -1;
		}
		if (calculate_image_crc(header) != header->dcrc) {
			printf("Found corrupted image at offset 0x%x, continuing search\n",
			       offset);
			offset += flash->erase_size;
			continue;
		}
		found_offset = offset;
		found_size = header->hlen + header->dlen;
		printf("Found valid SPI bootloader at offset: 0x%x, size: %d bytes\n",
		       found_offset, found_size);
		if (failsafe)
			break;
		/* Skip past the current image to the next one */
		offset += (found_size + flash->erase_size - 1) &
							~(flash->erase_size);
	} while (offset < CONFIG_OCTEON_SPI_BOOT_END);

	if (found_offset < 0) {
		printf("Could not find stage 3 bootloader\n");
		return -1;
	}

	/* If we searched for multiple bootloaders and didn't stop at the first
	 * one (i.e. failsafe) then re-read the last good one found into
	 * memory.
	 */
	if (found_offset != offset) {
		rc = spi_flash_read(flash, found_offset, found_size, header);
		if (rc) {
			printf("Error reading bootloader from offset 0x%x, size: 0x%x\n",
			       found_offset, found_size);
			return -1;
		}
	}

	do_go_exec(buf, argc - 1, argv + 1);

	return 0;
}
#endif

U_BOOT_CMD(bootstage3, 1, 0, do_octbootstage3,
	   "Load and execute the stage 3 bootloader",
	   "Load and execute the stage 3 bootloader");
