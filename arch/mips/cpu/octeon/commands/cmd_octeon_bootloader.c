/***********************license start************************************
 * Copyright (c) 2008-2014  Cavium Inc. (support@cavium.com).
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
#include <linux/stddef.h>

#ifdef CONFIG_CMD_OCTEON_BOOTLOADER_UPDATE

#include <watchdog.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <asm/addrspace.h>
#include <asm/arch/octeon_boot.h>
#ifdef CONFIG_CMD_OCTEON_NAND
#include <asm/arch/octeon_nand.h>
#endif
#endif
#ifdef CONFIG_OCTEON_SPI_BOOT_END
#include "spi_flash.h"
#endif
#include <asm/arch/cvmx.h>
#ifdef CONFIG_CMD_OCTEON_NAND
#include <asm/arch/cvmx-nand.h>
#endif
#include <asm/arch/cvmx-bootloader.h>
#ifdef CONFIG_OCTEON_HW_BCH
# include <nand.h>
# include <linux/mtd/nand.h>


# ifndef CONFIG_OCTEON_NAND_BOOT_DEV
#  define CONFIG_OCTEON_NAND_BOOT_DEV		0
# endif
# ifndef CONFIG_OCTEON_NAND_BOOT_START
#  define CONFIG_OCTEON_NAND_BOOT_START		0
# endif
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_OCTEON_SPI_BOOT_END
/**
 * Data structure used when analyzing the SPI NOR for bootloader images and
 * free space.
 */
struct bootloader_map {
	uint32_t offset;	/** Offset into SPI NOR */
	size_t size;		/** Size of image in SPI NOR including padding */
	size_t image_size;	/** Size of image in SPI NOR (no padding) */
	bootloader_image_t type;/** Type of bootloader image */
	bool free_space;	/** Block is free space (not necessarily erased) */
};
#endif

extern ulong load_addr;

/*
 * Helpers
 */
uint32_t calculate_header_crc(const bootloader_header_t * header);
uint32_t calculate_image_crc(const bootloader_header_t * header);

#ifdef CONFIG_CMD_OCTEON_BOOTLOADER_UPDATE
uint32_t get_image_size(const bootloader_header_t * header);
int validate_header(const bootloader_header_t * header);
int validate_data(const bootloader_header_t * header);

#if !defined(CONFIG_SYS_NO_FLASH)
extern flash_info_t flash_info[];
#endif

#ifdef DEBUG
#define DBGUPD printf
#else
#define DBGUPD(_args...)
#endif

/* This needs to be overridden for some boards, but this is correct for most.
 * This is only used for dealing with headerless images (and headerless failsafe
 * images currently on boards.)
 */
#ifndef CONFIG_SYS_NORMAL_BOOTLOADER_BASE
# define CONFIG_SYS_NORMAL_BOOTLOADER_BASE 0x1fd00000
#endif
/**
 * Prepares the update of the bootloader in NOR flash.  Performs any
 * NOR specific checks (block alignment, etc). Patches up the image for
 * relocation and prepares the environment for bootloader_flash_update command.
 *
 * @param image_addr Address of image in DRAM.  May not have a bootloader header
 *                   in it.
 * @param length     Length of image in bytes if the image does not have a header.
 *                   0 if image has a hader.
 * @param burn_addr  Address in flash (not remapped) to programm the image to
 * @param failsafe   Flag to allow failsafe image burning.
 *
 * @return 0 on success
 *         1 on failure
 */
#define ERR_ON_OLD_BASE
int do_bootloader_update_nor(uint32_t image_addr, int length,
			     uint32_t burn_addr, int failsafe)
{
#if defined(CONFIG_SYS_NO_FLASH)
	printf("ERROR: Bootloader not compiled with NOR flash support\n");
	return 1;
#else
	uint32_t failsafe_size, failsafe_top_remapped;
	uint32_t burn_addr_remapped, image_size, normal_top_remapped;
	flash_info_t *info;
	char tmp[16] __attribute__ ((unused));	/* to hold 32 bit numbers in hex */
	int sector = 0;
	bootloader_header_t *header;
	int rc;

	header = cvmx_phys_to_ptr(image_addr);

	DBGUPD("%s(0x%x, 0x%x, 0x%x, %s)\n", __func__, image_addr, length,
	       burn_addr, failsafe ? "failsafe" : "normal");
	DBGUPD("LOOKUP_STEP                0x%x\n", LOOKUP_STEP);
	DBGUPD("CFG_FLASH_BASE             0x%x\n", CONFIG_SYS_FLASH_BASE);

	/* File with rev 1.1 headers are not relocatable, so _must_ be burned
	 * at the address that they are linked at.
	 */
	if (header->maj_rev == 1 && header->min_rev == 1) {
		if (burn_addr && burn_addr != header->address) {
			printf("ERROR: specified address (0x%x) does not match "
			       "required burn address (0x%llx\n)\n",
			       burn_addr, header->address);
			return 1;
		}
		burn_addr = header->address;
	}

	/* If we have at least one bank of non-zero size, we have some NOR */
	if (!flash_info[0].size) {
		puts("ERROR: No NOR Flash detected on board, can't burn NOR "
		     "bootloader image\n");
		return 1;
	}

	/* check the burn address allignement */
	if ((burn_addr & (LOOKUP_STEP - 1)) != 0) {
		printf("Cannot programm normal image at 0x%x: address must be\n"
		       " 0x%x bytes alligned for normal boot lookup\n",
		       burn_addr, LOOKUP_STEP);
		return 1;
	}

	/* for failsage checks are easy */
	if ((failsafe) && (burn_addr != FAILSAFE_BASE)) {
		printf("ERROR: Failsafe image must be burned to address 0x%x\n",
		       FAILSAFE_BASE);
		return 1;
	}

	if (burn_addr && (burn_addr < FAILSAFE_BASE)) {
		printf("ERROR: burn address 0x%x out of boot range\n",
		       burn_addr);
		return 1;
	}

	if (!failsafe) {
#ifndef CONFIG_OCTEON_NO_FAILSAFE
		/* find out where failsafe ends */
		failsafe_size = get_image_size((bootloader_header_t *)
					       CONFIG_SYS_FLASH_BASE);
		if (failsafe_size == 0) {
			/* failsafe does not have header - assume fixed size
			 * old image
			 */
			puts("Failsafe has no valid header, assuming old image. "
			     "Using default failsafe size\n");
			failsafe_size =
			    CONFIG_SYS_NORMAL_BOOTLOADER_BASE - FAILSAFE_BASE;

			/* must default to CONFIG_SYS_NORMAL_BOOTLOADER_BASE */
			if (!burn_addr)
				burn_addr = CONFIG_SYS_NORMAL_BOOTLOADER_BASE;
			else if (CONFIG_SYS_NORMAL_BOOTLOADER_BASE != burn_addr) {
				printf("WARNING: old failsafe image will not be able to start\n"
				       "image at any address but 0x%x\n",
				       CONFIG_SYS_NORMAL_BOOTLOADER_BASE);
#ifdef ERR_ON_OLD_BASE
				return 1;
#endif
			}
		}		/* old failsafe */
#else
		failsafe_size = 0;
#endif		/* CONFIG_OCTEON_NO_FAILSAFE */

		DBGUPD("failsafe size is 0x%x\n", failsafe_size);
		DBGUPD("%s: burn address: 0x%x\n", __func__, burn_addr);
		/* Locate the next flash sector */
		failsafe_top_remapped = CONFIG_SYS_FLASH_BASE + failsafe_size;
		DBGUPD("failsafe_top_remapped 0x%x\n", failsafe_top_remapped);
		info = &flash_info[0];	/* no need to look into any other banks */
		/* scan flash bank sectors */
		for (sector = 0; sector < info->sector_count; ++sector) {
			DBGUPD("%d: 0x%lx\n", sector, info->start[sector]);
			if (failsafe_top_remapped <= info->start[sector])
				break;
		}

		if (sector == info->sector_count) {
			puts("Failsafe takes all the flash??  Can not burn normal image\n");
			return 1;
		}

		/* Move failsafe top up to the sector boundary */
		failsafe_top_remapped = info->start[sector];

		DBGUPD("Found next sector after failsafe is at remapped addr 0x%x\n",
		       failsafe_top_remapped);
		failsafe_size = failsafe_top_remapped - CONFIG_SYS_FLASH_BASE;
		DBGUPD("Alligned up failsafe size is 0x%x\n", failsafe_size);

		/* default to the first sector after the failsafe */
		if (!burn_addr) {
			burn_addr = FAILSAFE_BASE + failsafe_size;
			DBGUPD("Setting burn address to 0x%x, failsafe size: 0x%x\n",
			       burn_addr, failsafe_size);
		/* check for overlap */
		} else if (FAILSAFE_BASE + failsafe_size > burn_addr) {
			puts("ERROR: can not burn: image overlaps with failsafe\n");
			printf("burn address is 0x%x, in-flash failsafe top is 0x%x\n",
			       burn_addr, FAILSAFE_BASE + failsafe_size);
			return 1;
		}
		/* done with failsafe checks */
	}

	if (length)
		image_size = length;
	else
		image_size = get_image_size((bootloader_header_t *)image_addr);
	if (!image_size) {
		/* this is wierd case. Should never happen with good image */
		printf("ERROR: image has size field set to 0??\n");
		return 1;
	}

	/* finally check the burn address' CKSSEG limit */
	if ((burn_addr + image_size) >= (uint64_t) CKSSEG) {
		puts("ERROR: can not burn: image exceeds KSEG1 area\n");
		printf("burnadr is 0x%x, top is 0x%x\n", burn_addr,
		       burn_addr + image_size);
		return 1;
	}
	DBGUPD("burn_addr: 0x%x, image_size: 0x%x\n", burn_addr, image_size);
	/* Look up the last sector to use by the new image */
	burn_addr_remapped = burn_addr - FAILSAFE_BASE + CONFIG_SYS_FLASH_BASE;
	DBGUPD("burn_addr_remapped 0x%x\n", burn_addr_remapped);
	normal_top_remapped = burn_addr_remapped + image_size;
	/* continue flash scan - now for normal image top */
	if (failsafe)
		sector = 0;	/* is failsafe, we start from first sector here */
	for (; sector < info->sector_count; ++sector) {
		DBGUPD("%d: 0x%lx\n", sector, info->start[sector]);
		if (normal_top_remapped <= info->start[sector])
			break;
	}
	if (sector == info->sector_count) {
		puts("ERROR: not enough room in flash bank for the image??\n");
		return 1;
	}
	/* align up for environment variable set up */
	normal_top_remapped = info->start[sector];

	DBGUPD("normal_top_remapped 0x%x\n", normal_top_remapped);
	/* if there is no header (length != 0) - check burn address and
	 * give warning
	 */
	if (length && CONFIG_SYS_NORMAL_BOOTLOADER_BASE != burn_addr) {
#ifdef ERR_ON_OLD_BASE
		puts("ERROR: burning headerless image at other that defailt address\n"
		     "Image look up will not work.\n");
		printf("Default burn address: 0x%x requested burn address: 0x%x\n",
		       CONFIG_SYS_NORMAL_BOOTLOADER_BASE, burn_addr);
		return 1;
#else
		puts("WARNING: burning headerless image at other that defailt address\n"
		     "Image look up will not work.\n");
		printf("Default burn address: 0x%x requested burn address: 0x%x\n",
		       CONFIG_SYS_NORMAL_BOOTLOADER_BASE, burn_addr);
#endif
	}

	printf("Image at 0x%x is ready for burning\n", image_addr);
	printf("           Header version: %d.%d\n", header->maj_rev,
	       header->min_rev);
	printf("           Header size %d, data size %d\n", header->hlen,
	       header->dlen);
	printf("           Header crc 0x%x, data crc 0x%x\n", header->hcrc,
	       header->dcrc);
	printf("           Image link address is 0x%llx\n", header->address);
	printf("           Image burn address on flash is 0x%x\n", burn_addr);
	printf("           Image size on flash 0x%x\n",
	       normal_top_remapped - burn_addr_remapped);

	DBGUPD("burn_addr_remapped 0x%x normal_top_remapped 0x%x\n",
	       burn_addr_remapped, normal_top_remapped);
	if (flash_sect_protect(0, burn_addr_remapped, normal_top_remapped - 1)) {
		puts("Flash unprotect failed\n");
		return 1;
	}
	if (flash_sect_erase(burn_addr_remapped, normal_top_remapped - 1)) {
		puts("Flash erase failed\n");
		return 1;
	}

	puts("Copy to Flash... ");
	/* Note: Here we copy more than we should - whatever is after the image
	 * in memory gets copied to flash.
	 */
	rc = flash_write((char *)image_addr, burn_addr_remapped,
			 normal_top_remapped - burn_addr_remapped);
	if (rc != 0) {
		flash_perror(rc);
		return 1;
	}
	puts("done\n");

#ifndef CONFIG_ENV_IS_IN_NAND
	/* Erase the environment so that older bootloader will use its default
	 * environment.  This will ensure that the default
	 * 'bootloader_flash_update' macro is there.  HOWEVER, this is only
	 * useful if a legacy sized failsafe u-boot image is present.
	 * If a new larger failsafe is present, then that macro will be incorrect
	 * and will erase part of the failsafe.
	 * The 1.9.0 u-boot needs to have its link address and
	 * normal_bootloader_size/base modified to work with this...
	 */
	if (header->maj_rev == 1 && header->min_rev == 1) {
		puts("Erasing environment due to u-boot downgrade.\n");
		flash_sect_protect(0, CONFIG_ENV_ADDR,
				   CONFIG_ENV_ADDR + CONFIG_ENV_SIZE - 1);
		if (flash_sect_erase
		    (CONFIG_ENV_ADDR, CONFIG_ENV_ADDR + CONFIG_ENV_SIZE - 1)) {
			puts("Environment erase failed\n");
			return 1;
		}

	}
#endif
	return 0;
#endif
}

#if defined(CONFIG_CMD_OCTEON_NAND) && !defined(CONFIG_OCTEON_HW_BCH)
/**
 * NAND specific update routine.  Handles erasing the previous
 * image if it exists.
 *
 * @param image_addr Address of image in DRAM.  Always
 *                   has an image header.
 *
 * @return 0 on success
 *         1 on failure
 */
int do_bootloader_update_nand(uint32_t image_addr)
{
	const bootloader_header_t *new_header;
	const bootloader_header_t *header;
	int chip = oct_nand_get_cur_chip();
	int page_size = cvmx_nand_get_page_size(chip);
	int oob_size = octeon_nand_get_oob_size(chip);
	int pages_per_block = cvmx_nand_get_pages_per_block(chip);
	int bytes;

	uint64_t block_size = page_size * pages_per_block;

	uint64_t nand_addr = block_size;
	uint64_t buf_storage[2200 / 8] = { 0 };
	unsigned char *buf = (unsigned char *)buf_storage;
	int read_size = CVMX_NAND_BOOT_ECC_BLOCK_SIZE + 8;
	uint64_t old_image_nand_addr = 0;
	int old_image_size = 0;
	int required_len;
	int required_blocks;
	int conseq_blank_blocks;
	uint64_t erase_base;
	uint64_t erase_end;
	header = (void *)buf;
	new_header = (void *)image_addr;

	if (!cvmx_nand_get_active_chips()) {
		puts("ERROR: No NAND Flash detected on board, can't burn "
		     "NAND bootloader image\n");
		return 1;
	}

	/* Find matching type (failsafe/normal, stage2/stage3) of image that
	 * is currently in NAND, if present.  Save location for later erasing
	 */
	while ((nand_addr =
		oct_nand_image_search(nand_addr, MAX_NAND_SEARCH_ADDR,
				      new_header->image_type))) {
		/* Read new header */
		bytes =
		    cvmx_nand_page_read(chip, nand_addr, cvmx_ptr_to_phys(buf),
					read_size);
		if (bytes != read_size) {
			printf("Error reading NAND addr 0x%llx (bytes_read: %d, expected: %d)\n",
			       nand_addr, bytes, read_size);
			return 1;
		}
		/* Check a few more fields from the headers */

		if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_GENERIC
		    && header->board_type != CVMX_BOARD_TYPE_GENERIC) {
			/* If the board type of the running image is generic,
			 * don't do any board matching.  When looking for images
			 * in NAND to overwrite, treat generic board type images
			 * as matching all board types.
			 */
			if (new_header->board_type != header->board_type) {
				puts("WARNING: A bootloader for a different "
				     "board type was found and skipped (not erased.)\n");
				/* Different board type, so skip (this is
				 * strange to find.....
				 */
				nand_addr +=
				    ((header->hlen + header->dlen + page_size - 1)
				    & ~(page_size - 1));
				continue;
			}
		}
		if ((new_header->flags & BL_HEADER_FLAG_FAILSAFE) !=
		    (new_header->flags & BL_HEADER_FLAG_FAILSAFE)) {
			/* Not a match, so skip */
			nand_addr +=
			    ((header->hlen + header->dlen + page_size -
			      1) & ~(page_size - 1));
			continue;
		}

		/* A match, so break out */
		old_image_nand_addr = nand_addr;
		old_image_size = header->hlen + header->dlen;
		printf("Found existing bootloader image of same type at NAND addr: 0x%llx\n",
		       old_image_nand_addr);
		break;
	}
	/* nand_addr is either 0 (no image found), or has the address of the
	 * image we will delete after the write of the new image.
	 */
	if (!nand_addr)
		puts("No existing matching bootloader found in flash\n");

	/* Find a blank set of _blocks_ to put the new image in.  We want
	 * to make sure that we don't put any part of it in a block with
	 * something else, as we want to be able to erase it later.
	 */
	required_len = new_header->hlen + new_header->dlen;
	required_blocks = (required_len + block_size - 1) / block_size;

	conseq_blank_blocks = 0;
	read_size = page_size + oob_size;
	for (nand_addr = block_size; nand_addr < MAX_NAND_SEARCH_ADDR;
	     nand_addr += block_size) {
		if (oct_nand_block_is_blank(nand_addr)) {
			conseq_blank_blocks++;
			if (conseq_blank_blocks == required_blocks) {
				/* We have a large enough blank spot */
				nand_addr -=
				    (conseq_blank_blocks - 1) * block_size;
				break;
			}
		} else
			conseq_blank_blocks = 0;
	}

	if (nand_addr >= MAX_NAND_SEARCH_ADDR) {
		puts("ERROR: unable to find blank space for new bootloader\n");
		return 1;
	}
	printf("New bootloader image will be written at blank address 0x%llx, length 0x%x\n",
	       nand_addr, required_len);

	/* Write the new bootloader to blank location. */
	if (0 > oct_nand_boot_write(nand_addr, (void *)image_addr, required_len, 0)) {
		puts("ERROR: error while writing new image to flash.\n");
		return 1;
	}

	/* Now erase the old bootloader of the same type.
	 * We know these are not bad NAND blocks since they have valid data
	 * in them.
	 */
	erase_base = old_image_nand_addr & ~(block_size - 1);
	erase_end =
	    ((old_image_nand_addr + old_image_size + block_size -
	      1) & ~(block_size - 1));
	for (nand_addr = erase_base; nand_addr < erase_end;
	     nand_addr += block_size) {
		if (cvmx_nand_block_erase(chip, nand_addr)) {
			printf("cvmx_nand_block_erase() failed, addr 0x%08llx\n",
			       nand_addr);
			return 1;
		}

	}

	puts("Bootloader update in NAND complete.\n");
	return 0;
}

#elif defined(CONFIG_OCTEON_HW_BCH) && defined(CONFIG_OCTEON_NAND_BOOT_END)

/**
 * NAND specific update routine.  Handles erasing the previous
 * image if it exists.  Note that this uses the standard U-Boot NAND
 * facilities which make use of the hardware BCH engine.
 *
 * @param image_addr Address of image in DRAM.  Always
 *                   has an image header.
 *
 * @return 0 on success
 *         1 on failure
 */
int do_bootloader_update_nand(uint32_t image_addr)
{
	const int dev = CONFIG_OCTEON_NAND_BOOT_DEV;
	const loff_t end = CONFIG_OCTEON_NAND_BOOT_END;
	const bootloader_header_t *new_header = (void *)image_addr;
	nand_info_t *nand = &nand_info[CONFIG_OCTEON_NAND_BOOT_DEV];
	int page_size = nand->writesize;
	int pages_per_block = nand->erasesize / nand->writesize;
	size_t bytes_written = 0;
	uint64_t block_size = page_size * pages_per_block;
	loff_t last_image_addr = 0;
	loff_t nand_addr = block_size;
	uint64_t buf_storage[(NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE) / 8] = { 0 };
	unsigned char *buf = (unsigned char *)buf_storage;
	const bootloader_header_t *header = (void *)buf;
	uint64_t old_image_nand_addr = 0;
	int old_image_size = 0;
	int image_no = 0;
	size_t write_length;
	int rc;
	int required_len;
	int required_blocks;
	int conseq_blank_blocks;
	loff_t erase_base, erase_size;

	if (!cvmx_nand_get_active_chips()) {
		puts("ERROR: No NAND Flash detected on board, can't burn "
		     "NAND bootloader image\n");
		return 1;
	}

	/* Find matching type (failsafe/normal, stage2/stage3) of image that
	 * is currently in NAND, if present.  Save location for later erasing
	 */
	while ((nand_addr = octeon_nand_image_search(dev,
						     image_no,
						     -1,
						     new_header->image_type,
						     &bytes_written, nand_addr,
						     end)) > 0) {
		rc = octeon_nand_validate_image(dev, nand_addr, end);
		if (rc) {
			printf("Found bad NAND image at offset 0x%llx\n",
			       nand_addr);
			nand_addr += page_size;
			continue;
		} else {
			printf("Found good NAND image at offset 0x%llx\n",
			       nand_addr);
		}
		last_image_addr = nand_addr;
		/* Read new header */
		rc = octeon_nand_read_dev(CONFIG_OCTEON_NAND_BOOT_DEV,
					  nand_addr, page_size,
					  end - nand_addr,
					  (void *)buf, &nand_addr);
		if (rc) {
			printf("Error reading NAND addr 0x%llx\n",
			       nand_addr);
			return 1;
		}

		/* Check a few more fields from the headers */
		if (gd->board_type != CVMX_BOARD_TYPE_GENERIC
		    && header->board_type != CVMX_BOARD_TYPE_GENERIC) {
			/* If the board type of the running image is generic,
			 * don't do any board matching.  When looking for images
			 * in NAND to overwrite, treat generic board type images
			 * as matching all board types.
			 */
			if (new_header->board_type != header->board_type) {
				puts("WARNING: A bootloader for a different "
				     "board type was found and skipped (not erased.)\n");
				/* Different board type, so skip (this is
				 * strange to find.....
				 */
				nand_addr +=
				    ((header->hlen + header->dlen + page_size - 1)
				    & ~(page_size - 1));
				continue;
			}
		}
		if ((new_header->flags & BL_HEADER_FLAG_FAILSAFE) !=
		    (new_header->flags & BL_HEADER_FLAG_FAILSAFE)) {
			/* Not a match, so skip */
			nand_addr +=
			    ((header->hlen + header->dlen + page_size -
			      1) & ~(page_size - 1));
			continue;
		}

		/* A match, so break out */
		old_image_nand_addr = last_image_addr;
		old_image_size = header->hlen + header->dlen;
		printf("Found existing bootloader image of the same type at NAND address 0x%llx\n",
		       old_image_nand_addr);
		break;
	}
	/* nand_addr is either 0 (no image found), or has the address of the
	 * image we will delete after the write of the new image.
	 */
	if (!last_image_addr)
		puts("No existing matching bootloader found in flash\n");

	/* Find a blank set of _blocks_ to put the new image in.  We want
	 * to make sure that we don't put any part of it in a block with
	 * something else, as we want to be able to erase it later.
	 */
	required_len = new_header->hlen + new_header->dlen;
	required_blocks = (required_len + block_size - 1) / block_size;

	conseq_blank_blocks = 0;
	for (nand_addr = block_size; nand_addr < end; nand_addr += block_size) {
		if (octeon_nand_is_block_blank(dev, nand_addr)) {
			debug("NAND address 0x%llx is blank\n", nand_addr);
			conseq_blank_blocks++;
			if (conseq_blank_blocks == required_blocks) {
				/* We have a large enough blank spot */
				debug("Found %d consecutive blank blocks\n",
				      conseq_blank_blocks);
				nand_addr -=
				    (conseq_blank_blocks - 1) * block_size;
				break;
			}
		} else {
			debug("NAND address 0x%llx is not blank\n", nand_addr);
			conseq_blank_blocks = 0;
		}
	}

	if (nand_addr >= end) {
		puts("ERROR: unable to find blank space for new bootloader\n");
		return 1;
	}
	printf("New bootloader image will be written at blank address 0x%llx, length 0x%x\n",
	       nand_addr, required_len);
	write_length = required_len;
	bytes_written = 0;
	/* Write the new bootloader to blank location. */
	if (nand_write_skip_bad(nand, nand_addr, &write_length, &bytes_written,
				CONFIG_OCTEON_NAND_BOOT_END - nand_addr,
				(u_char *)image_addr, 0) != 0 ||
				(bytes_written != required_len)) {
		puts("ERROR: error while writing new image to flash.\n");
		return 1;
	}
	printf("Wrote %d bytes at NAND offset 0x%llx\n", bytes_written,
	       nand_addr);
	/* Now erase the old bootloader of the same type.
	 * We know these are not bad NAND blocks since they have valid data
	 * in them.
	 */
	erase_base = old_image_nand_addr & ~(block_size - 1);
	erase_size = (old_image_size + block_size - 1) & ~(block_size - 1);

	debug("Erasing old image at 0x%llx\n", erase_base);
	if (nand_erase(nand, erase_base, erase_size)) {
		printf("nand_erase() failed, addr 0x%08llx, size 0x%llx\n",
		       erase_base, erase_size);
		return 1;
	}

	puts("Bootloader update in NAND complete.\n");
	return 0;
}
#endif		/* CONFIG_CMD_OCTEON_NAND */

#ifdef CONFIG_OCTEON_SPI_BOOT_END
/**
 * Validates the bootloader found at the specified offset in the SPI NOR
 *
 * @param sf		pointer to spi flash data structure
 * @param offset	offset of bootloader in spi NOR flash
 * @param size		size of bootloader
 *
 * @return 	0 if a valid bootloader was found at the offset
 *		1 if no valid bootloader was found
 *		-1 on error
 */
int validate_spi_bootloader(struct spi_flash *sf, uint32_t offset, size_t size)
{
	struct bootloader_header *header;
	uint8_t *buffer;
	int rc;

	buffer = calloc(size, 1);
	if (!buffer) {
		puts("Out of memory\n");
		return -1;
	}
	debug("%s(%p, 0x%x, 0x%x)\n", __func__, sf, offset, size);
	rc = spi_flash_read(sf, offset, size, buffer);
	if (rc) {
		puts("Error reading bootloader\n");
		rc = -1;
	} else {
		header = (struct bootloader_header *)buffer;
		if (validate_header(header) || validate_data(header)) {
			printf("Invalid bootloader found at offset 0x%x\n",
			       offset);
			rc = 1;
		} else {
			rc = 0;
		}
	}

	if (buffer)
		free(buffer);

	return rc;
}

/**
 * Update a SPI bootloader
 *
 * @param image_addr	Address bootloader image is located in RAM
 * @param image_size	Size of image, required for stage 1 bootloader
 * @param failsafe	True if image is failsafe stage 3 bootloader
 * @param stage1	True if image is stage 1 bootloader
 *
 * @return 0 on success
 *         1 on failure
 */
int do_bootloader_update_spi(uint32_t image_addr, size_t image_size,
			     bool failsafe, bool stage1)
{
	struct bootloader_header search_header;
	struct bootloader_map image_map[10];
	int images[4] = {-1, -1, -1, -1};	/* 2 stage 2 and 2 stage 3 */
	const struct bootloader_header *header;
	uint8_t *buffer;
	struct spi_flash *sf = NULL;
	uint32_t offset = stage1 ? 0 : CONFIG_OCTEON_SPI_STAGE2_START;
	uint32_t erase_size;
	uint32_t size;
	uint32_t start = -1, end = -1;
	int rc;
	int image_index = 0;
	int num_images = 0;
	int num_stage2 = 0;
	int i;
	bool is_stage2 = false;
	bool update_index = false;

	memset(image_map, 0, sizeof(image_map));
	if (!image_addr)
		image_addr = getenv_ulong("fileaddr", 16, 0);
	if (!image_addr)
		image_addr = getenv_ulong("loadaddr", 16, load_addr);

	sf = spi_flash_probe(0, 0, CONFIG_SF_DEFAULT_SPEED, SPI_MODE_0);
	if (!sf) {
		puts("SPI flash not found\n");
		goto error;
	}

	debug("size: %u bytes, erase block size: %u bytes\n",
	      sf->size, sf->erase_size);

	if (sf->size < CONFIG_OCTEON_SPI_BOOT_END) {
		printf("Error: Bootloader section end offset 0x%x is greater than the size of the SPI flash (0x%x)\n",
		       CONFIG_OCTEON_SPI_BOOT_END, sf->size);
		goto error;
	}

	erase_size = (image_size + sf->erase_size - 1) & ~(sf->erase_size - 1);
	buffer = CASTPTR(uint8_t, image_addr);
	if (stage1) {
		debug("Updating stage 1 bootloader\n");
		rc = spi_flash_erase(sf, 0, erase_size);
		if (rc) {
			puts("Error erasing SPI flash\n");
			goto error;
		}
		puts("Writing stage 1 SPI bootloader to offset 0.\n");
		rc = spi_flash_write(sf, 0, image_size, buffer);
		if (rc) {
			puts("Error writing stage 1 bootloader to SPI flash\n");
			goto error;
		}
		printf("Successfully wrote %zu bytes.\n", image_size);
		return 0;
	}

	header = CASTPTR(struct bootloader_header, image_addr);

	is_stage2 = header->image_type == BL_HEADER_IMAGE_STAGE2;
	printf("%s %u byte bootloader found at address 0x%x\n",
	       is_stage2 ? "Stage 2" : "Final stage", image_size, image_addr);

	/* Search for U-Boot image */
	do {
		/* debug("Looking for bootloader at offset 0x%x\n", offset);*/
		rc = spi_flash_read(sf, offset, sizeof(search_header),
				    &search_header);
		if (rc) {
			printf("Error reading offset %u of SPI flash\n", offset);
			goto error;
		}
		if (!validate_header(&search_header)) {
			debug("Found valid %u byte stage %d header at offset 0x%x\n",
			      search_header.hlen + search_header.dlen,
			      search_header.image_type == BL_HEADER_IMAGE_STAGE2 ? 2 : 3,
			      offset);
			if (update_index)
				image_index++;
			update_index = false;
			size = search_header.hlen + search_header.dlen;
			debug("Validating bootloader at 0x%x, size: %zu\n",
			      offset, size);
			if (size > (2 << 20)) {
				rc = 1;
				puts("Invalid bootloader image size exceeds 2MB\n");
			} else {
				rc = validate_spi_bootloader(sf, offset, size);
			}
			if (rc < 0) {
				debug("Error reading bootloader from SPI\n");
				goto error;
			} else if (rc == 0) {
				debug("Found valid bootloader\n");
				image_map[image_index].size =
					(size + erase_size - 1) & ~erase_size;
				image_map[image_index].free_space = false;
				image_map[image_index].type = search_header.image_type;
			} else {
				debug("Found invalid bootloader\n");
				/* Calculate free size based on invalid
				 * bootloader size.
				 */
				image_map[image_index].image_size = size;
				/* Including padding for erase block size */
				image_map[image_index].size =
					(size + sf->erase_size - 1)
							& ~(sf->erase_size - 1);
				image_map[image_index].free_space = true;
				image_map[image_index].type = BL_HEADER_IMAGE_UNKNOWN;
			}
			image_map[image_index].offset = offset;
			offset += size + sf->erase_size - 1;
			offset &= ~(sf->erase_size - 1);
			image_index++;
		} else {
			update_index = true;
			/* debug("No header at offset 0x%x\n", offset); */
			if (!image_map[image_index].size) {
				image_map[image_index].offset = offset;
				image_map[image_index].free_space = true;
			}
			image_map[image_index].size += sf->erase_size;
			offset += sf->erase_size;
		}
	} while (offset < CONFIG_OCTEON_SPI_BOOT_END && image_index < 10);

	if (update_index)
		image_index++;

	for (i = 0, num_images = 0; i < image_index && num_images < 4; i++)
		if (!image_map[i].free_space) {
			images[num_images++] = i;
			if (image_map[i].type == BL_HEADER_IMAGE_STAGE2)
				num_stage2++;
		}

#ifdef DEBUG
	debug("Image map:\n");
	for (i = 0; i < image_index; i++)
		printf("%d: offset: 0x%x, image size: %#x, size: 0x%x, type: %d, free: %s\n",
		       i, image_map[i].offset, image_map[i].image_size,
		       image_map[i].size,
		       image_map[i].type,
		       image_map[i].free_space ? "yes" : "no");
#endif

	if (is_stage2) {
#ifdef CONFIG_OCTEON_NO_FAILSAFE
		/* If there's no failsafe then we just overwrite the
		 * first bootloader by pretending it is a failsafe.
		 */
		failsafe = 1;
#endif
		if (failsafe || num_images == 0 || num_stage2 < 2) {
			i = images[0];
			if (i < 0)
				i = 0;
			if (image_map[i].type != BL_HEADER_IMAGE_STAGE2 &&
			    image_map[i].type != BL_HEADER_IMAGE_UNKNOWN &&
			    !image_map[i].free_space) {
				puts("Warning: no stage 2 bootloader found,\n"
				     "overwriting existing non-stage 2 bootloader image.\n");
			}
			if (i > 0 && image_map[i - 1].free_space)
				start = image_map[i - 1].offset;
			else
				start = image_map[i].offset;
			if ((i + 1) < image_index && image_map[i + 1].free_space)
				end = image_map[i + 1].offset + image_map[i + 1].size;
			else
				end = image_map[i].offset + image_map[i].size;
			size = end - start;
			if (image_size > size)
				puts("Warning: new failsafe stage 2 image overwrites old non-failsafe stage 2 image.\n"				     "Please update non-failsafe stage 2 bootloader next.\n");
		} else {
			if (num_stage2 > 0) {
				i = images[1];
				if (i < 0)
					i = 1;
			} else {
				i = images[0];
			}
			start = image_map[i].offset;
			end = start + image_map[i].size;
			debug("New stage 2 image map %d start: 0x%x, end: 0x%x\n",
			      i, start, end);
		}
	} else {
		debug("Non-stage 2 bootloader\n");
		i = num_stage2;
		if (!failsafe && num_images > num_stage2)
			i++;
		if (images[i] < 0)
			i = images[i - 1] + 1;
		else if (image_map[i].size < erase_size &&
			 !image_map[i + 1].free_space)
			printf("Warning: overwriting image following this image at offset 0x%x\n",
			       image_map[i + 1].offset);

		start = image_map[i].offset;
		end = start + image_map[i].size;
		debug("New final stage bootloader image map %d start: 0x%x, end: 0x%x\n",
		      i, start, end);
	}

	if (end < 0 || start < 0) {
		printf("Error detected, start or end is negative\n");
		goto error;
	}
	if (end - start < erase_size) {
		printf("Not enough space available, need %u bytes but only %u are available\n",
		       image_size, end - start);
	}

	printf("Erasing %u bytes starting at offset 0x%x, please wait...\n", erase_size, start);

	rc = spi_flash_erase(sf, start, erase_size);
	if (rc) {
		printf("Error erasing %u bytes starting at offset 0x%x\n",
		       erase_size, start);
		goto error;
	}

	printf("Writing %u bytes...\n", image_size);
	rc = spi_flash_write(sf, start, image_size, header);
	if (rc) {
		printf("Error writing %u bytes starting at offset 0x%x\n",
		       image_size, start);
		goto error;
	}

	printf("Done.\n%u bytes were written starting at offset 0x%x\n",
	       image_size, start);
	if (sf)
		free(sf);

	return 0;
error:
	if (sf)
		free(sf);
	return 1;
}
#endif

/**
 * Command for updating a bootloader image in flash.  This function
 * parses the arguments, and validates the header (if header exists.)
 * Actual flash updating is done by flash type specific functions.
 *
 * @return 0 on success
 *         1 on failure
 */
int do_bootloader_update(cmd_tbl_t * cmdtp, int flag, int argc,
			 char *const argv[])
{
	uint32_t image_addr = 0;
	uint32_t image_len = 0;
	uint32_t burn_addr = 0;
	int failsafe = 0;
	const bootloader_header_t *header;
	int force_nand = 0;
	int force_spi = 0;

	if (argc >= 2) {
		if (!strcmp(argv[1], "nand")) {
			debug("Forced NAND bootloader update\n");
			force_nand = 1;
			argc--;
			argv++;
		} else if (!strcmp(argv[1], "spi")) {
			debug("Forced SPI bootloader update\n");
			force_spi = 1;
			argc--;
			argv++;
			if (!strcmp(argv[1], "failsafe")) {
				failsafe = 1;
				argc--;
				argv++;
			}
		}
	}
	if (argc >= 2)
		image_addr = simple_strtoul(argv[1], NULL, 16);
	if (argc >= 3)
		image_len = simple_strtoul(argv[2], NULL, 16);
	if (argc >= 4) {
		if (force_spi || force_nand) {
			if (!strcmp("failsafe", argv[3]))
				failsafe = 1;
		} else {
			burn_addr = simple_strtoul(argv[3], NULL, 16);
		}
	}
	if ((argc >= 5) && (strcmp("failsafe", argv[4]) == 0))
		failsafe = 1;

	/* If we don't support failsafe images, we need to put the image at the
	 * base of flash, so we treat all images like failsafe image in this
	 * case.
	 */
#ifdef CONFIG_OCTEON_NO_FAILSAFE
	failsafe = 1;
	burn_addr = 0x1fc00000;
#endif

	if (!burn_addr)
		burn_addr = getenv_hex("burnaddr", 0);

	if (!image_addr) {
		image_addr = getenv_hex("fileaddr", load_addr);
		if (!image_addr) {
			puts("ERROR: Unable to get image address from "
			     "'fileaddr' environment variable\n");
			return 1;
		}
	}

	DBGUPD("%s: burn address: 0x%x, image address: 0x%x\n",
	      __func__, burn_addr, image_addr);

	/* Figure out what kind of flash we are going to update.  This will
	 * typically come from the bootloader header.  If the bootloader does
	 * not have a header, then it is assumed to be a legacy NOR image, and
	 * a destination NOR flash address must be supplied.  NAND images
	 * _must_ have a header.
	 */
	header = (void *)image_addr;

	if (header->magic != BOOTLOADER_HEADER_MAGIC) {
		/* No header, assume headerless NOR bootloader image */
		puts("No valid bootloader header found.  Assuming old headerless image\n"
		     "Image checks cannot be performed\n");

		if (!burn_addr) {
			burn_addr = CONFIG_SYS_NORMAL_BOOTLOADER_BASE;
			DBGUPD("Unable to get burn address from 'burnaddr' environment variable,\n");
			DBGUPD(" using default 0x%x\n", burn_addr);
		}
		/* We only need image length for the headerless case */
		if (!image_len) {
			image_len = getenv_hex("filesize", 0);
			if (!image_len) {
				puts("ERROR: Unable to get image size from "
				     "'filesize' environment variable\n");
				return 1;
			}
		}

		return do_bootloader_update_nor(image_addr, image_len,
						burn_addr, failsafe);
	}

	/* We have a header, so validate image */
	if (validate_header(header)) {
		puts("ERROR: Image header has invalid CRC.\n");
		return 1;
	}
	if (validate_data(header))	/* Errors printed */
		return 1;

	/* We now have a valid image, so determine what to do with it. */
	puts("Valid bootloader image found.\n");

	/* Check to see that it is for the board we are running on */
	if (header->board_type != cvmx_sysinfo_get()->board_type) {
		printf("Current board type: %s (%d), image board type: %s (%d)\n",
		       cvmx_board_type_to_string(cvmx_sysinfo_get()->board_type),
		       cvmx_sysinfo_get()->board_type,
		       cvmx_board_type_to_string(header->board_type),
		       header->board_type);
		if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_GENERIC
		    && header->board_type != CVMX_BOARD_TYPE_GENERIC) {
			puts("ERROR: Bootloader image is not for this board type.\n");
			return 1;
		} else {
			puts("Loading mismatched image since current "
			     "or new bootloader is generic.\n");
		}
	}

	/* SDK 1.9.0 NOR images have rev of 1.1 and unkown image_type */
	if (((header->image_type == BL_HEADER_IMAGE_NOR)
	     || (header->image_type == BL_HEADER_IMAGE_UNKNOWN
		 && header->maj_rev == 1 && header->min_rev == 1))
	    && !force_nand && !force_spi) {
		debug("Updating NOR bootloader\n");
		return do_bootloader_update_nor(image_addr, 0, burn_addr,
						failsafe);
#if defined(CONFIG_CMD_OCTEON_NAND) || defined(CONFIG_OCTEON_NAND_BOOT_END)
	} else if (!force_spi &&
		   (header->image_type == BL_HEADER_IMAGE_STAGE2 ||
		    header->image_type == BL_HEADER_IMAGE_STAGE3 ||
		    force_nand)) {
		debug("Updating NAND bootloader\n");
		return (do_bootloader_update_nand(image_addr));
#endif
#if defined(CONFIG_OCTEON_SPI_BOOT_END)
	} else if (!force_nand &&
		   (header->image_type == BL_HEADER_IMAGE_STAGE2 ||
		    header->image_type == BL_HEADER_IMAGE_STAGE3 ||
		    force_spi)) {
		debug("Updating SPI bootloader\n");
		return do_bootloader_update_spi(image_addr,
						header->dlen + header->hlen,
						failsafe, false);
#else
	} else {
		puts("ERROR: This bootloader not compiled for this medium\n");
		return 1;
#endif
	}

	return 1;
}

/**
 * Validate image header. Intended for actual header discovery.
 * Not NAND or NOR specific
 *
 * @param header     Address of expected image header.
 *
 * @return 0  on success
 *         1  on failure
 */
#define BOOTLOADER_MAX_SIZE 0x200000	/* something way too large, but it
					 * should not hopefully run ower
					 * memory end
					 */
int validate_header(const bootloader_header_t * header)
{
	if (header->magic == BOOTLOADER_HEADER_MAGIC) {
		/* check if header length field valid */
		if (header->hlen > BOOTLOADER_HEADER_MAX_SIZE) {
			puts("Corrupted header length field\n");
			return 1;
		}

		if ((header->maj_rev == 1) && (header->min_rev == 0)) {
			puts("Image header version 1.0, relocation not supported\n");
			return 1;
		}
		/* Check the CRC of the header */
		if (calculate_header_crc(header) == header->hcrc)
			return 0;
		else {
			puts("Header crc check failed\n");
			return 1;
		}
	}

	return 1;
}

/**
 * Validate image data.
 * Not NAND or NOR specific
 *
 * @param header     Address of expected image header.
 *
 * @return 0  on success
 *         1  on failure
 */
int validate_data(const bootloader_header_t * header)
{
	uint32_t image_size, crc;

	if ((image_size = get_image_size(header)) > BOOTLOADER_MAX_SIZE) {
		printf("Image has length %d - too large?\n", image_size);
		return 1;
	}

	crc = calculate_image_crc(header);
	if (crc != header->dcrc) {
		printf("Data crc failed: header value 0x%x calculated value 0x%x\n",
		       header->dcrc, crc);
		return 1;
	}
	return 0;
}

/**
 *  Given valid header returns image size (data + header); or 0
 */
uint32_t get_image_size(const bootloader_header_t * header)
{
	uint32_t isize = 0;
	if (!validate_header(header))
		/* failsafe has valid header - get the image size */
		isize = header->hlen + header->dlen;

	return isize;
}

int do_bootloader_validate(cmd_tbl_t * cmdtp, int flag, int argc,
			   char *const argv[])
{
	uint32_t image_addr = 0;
	const bootloader_header_t *header;

	if (argc >= 2)
		image_addr = simple_strtoul(argv[1], NULL, 16);

	if (!image_addr) {
		image_addr = getenv_hex("fileaddr", load_addr);
		if (!image_addr) {
			puts("ERROR: Unable to get image address from "
			     "'fileaddr' environment variable\n");
			return 1;
		}
	}

	header = (void *)image_addr;
	if (validate_header(header)) {
		puts("Image does not have valid header\n");
		return 1;
	}

	if (validate_data(header))
		return 1;

	printf("Image validated. Header size %d, data size %d\n", header->hlen,
	       header->dlen);
	printf("                 Header crc 0x%x, data crc 0x%x\n",
	       header->hcrc, header->dcrc);
	printf("                 Image link address is 0x%llx\n",
	       header->address);

	return 0;
}

U_BOOT_CMD(bootloaderupdate, 5, 0, do_bootloader_update,
	   "Update the bootloader in flash",
	   "[nand | spi] [image_address] [image_length]\n"
	   "Updates the the bootloader in flash.  Uses bootloader header if present\n"
	   "to validate image.\n"
	   "where:\n"
	   "  nand          - forces updating the NAND bootloader (optional)\n"
	   "  spi           - forces updating the SPI bootloader (optional)\n"
	   "  image_address - address image is located in RAM\n"
	   "  image_size    - size of image in hex\n"
	   "\n"
	   "If the image size and address are not specified then\n"
	   "$(fileaddr) and $(filesize) will be used.");

U_BOOT_CMD(bootloadervalidate, 2, 0, do_bootloader_validate,
	   "Validate the bootloader image",
	   "[image_address]\n"
	   "Validates the bootloader image.  Image must have bootloader header.\n"
	   "Validates header and image crc32\n");

#endif	/* CONFIG_CMD_OCTEON_BOOTLOADER_UPDATE */

int do_nmi(cmd_tbl_t * cmdtp, int flag, int argc, char *const argv[])
{
	cvmx_coremask_t coremask = CVMX_COREMASK_EMPTY;
	uint64_t cores;
	int node;

	if (argc > 1) {
		if (cvmx_coremask_str2bmp(&coremask, argv[1])) {
			puts("Error: could not parse coremask string\n");
			return -1;
		}
	} else {
		cvmx_coremask_set_self(&coremask);
	}

	if (octeon_has_feature(OCTEON_FEATURE_MULTINODE)) {
		for (node = CVMX_MAX_NODES - 1; node >= 0; node--) {
			cores = cvmx_coremask_get64_node(&coremask, node);
			cvmx_write_csr_node(node, CVMX_CIU3_NMI, cores);

		}
	} else {
		cores = cvmx_coremask_get64(&coremask);
		if (octeon_has_feature(OCTEON_FEATURE_CIU3))
			cvmx_write_csr(CVMX_CIU3_NMI, cores);
		else
			cvmx_write_csr(CVMX_CIU_NMI, cores);
	}
	return 0;
}

U_BOOT_CMD(nmi, 2, 0, do_nmi,
	   "Generate a non-maskable interrupt",
	   "Generate a non-maskable interrupt on core 0");
