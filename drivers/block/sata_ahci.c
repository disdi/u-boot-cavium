/*
 * Copyright (C) Freescale Semiconductor, Inc. 2006.
 * Author: Jason Jin<Jason.jin@freescale.com>
 *         Zhang Wei<wei.zhang@freescale.com>
 *
 * Copyright (C) Cavium, Inc. 2011, 2013, 2014
 * Author: Aaron Williams <Aaron.Williams@cavium.com>
 * 	Updated to follow SATA drivers
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 * with the reference on libata and ahci drvier in kernel
 *
 */

#include <common.h>
#include <pci.h>
#include <asm/processor.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <malloc.h>
#include <libata.h>
#include <ata.h>
#include <linux/ctype.h>
#include <linux/dma-direction.h>
#include <ahci.h>
#include <fis.h>
#include "ata_piix.h"

#define WAIT_MS_SPINUP	10000
#define WAIT_MS_DATAIO	5000
#define WAIT_MS_FLUSH	5000
#define WAIT_MS_LINKUP	4

#define READ_6                0x08
#define WRITE_6               0x0a
#define READ_10               0x28
#define WRITE_10              0x2a
#define READ_12               0xa8
#define WRITE_12              0xaa
#define READ_16               0x88
#define WRITE_16              0x8a


#define ATA_EH_CMD_DFL_TIMEOUT		5000

extern block_dev_desc_t sata_dev_desc[CONFIG_SYS_SATA_MAX_DEVICE];

struct ata_port *sata_port_desc[CONFIG_SYS_SATA_MAX_DEVICE];
struct ata_host *sata_host_desc[CONFIG_SYS_SATA_MAX_DEVICE];
struct ata_device *sata_device_desc[CONFIG_SYS_SATA_MAX_DEVICE];
struct ata_link *sata_pmp_link[CONFIG_SYS_SATA_MAX_DEVICE];

hd_driveid_t *ataid[AHCI_MAX_PORTS];
static unsigned int last_port_idx = 0;
static unsigned int last_device_idx = 0;
int ahci_skip_host_reset = 0;
int ahci_em_messages = 1;
#ifdef CONFIG_SATA_AHCI_PLAT
struct ahci_probe_ent *probe_ent;
#endif

struct ahci_device {
	struct ahci_probe_ent *probe;
	struct block_dev_desc *block_dev;
	hd_driveid_t *ataid;
	u8 port;
	u8 pnp;
	uint8_t pnp_used;
};

static int ahci_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val);
static int ahci_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val);
static int ahci_port_start(struct ata_port *ap);
static void ahci_port_stop(struct ata_port *ap);
static void ahci_enable_fbs(struct ata_port *ap);
static void ahci_disable_fbs(struct ata_port *ap);
static void ahci_pmp_attach(struct ata_port *ap);
static void ahci_pmp_detach(struct ata_port *ap);
static int ahci_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static int ahci_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static void ahci_postreset(struct ata_link *link, unsigned int *class);
static void ahci_dev_config(struct ata_device *dev);
static unsigned int ahci_dev_classify(struct ata_port *ap);
static void ahci_dev_config(struct ata_device *dev);
static int ahci_add_pmp_attached_dev(struct ata_link *link);


struct ata_port_operations ahci_ops = {
	.inherits		= &sata_pmp_port_ops,
	.softreset		= ahci_softreset,
	.hardreset		= ahci_hardreset,
	.pmp_softreset		= ahci_softreset,
	.dev_config		= ahci_dev_config,
	.scr_read		= ahci_scr_read,
	.scr_write		= ahci_scr_write,
	.pmp_attach		= ahci_pmp_attach,
	.port_start		= ahci_port_start,
	.port_stop		= ahci_port_stop,
	.add_pmp_attached_dev	= ahci_add_pmp_attached_dev,
};

enum board_ids {
	/* board IDs by feature in alphabetical order */
	board_ahci,
	board_ahci_ign_iferr,
	board_ahci_nosntf,
	board_ahci_yes_fbs,

	/* board IDs for specific chipsets in alphabetical order */
	board_ahci_mcp65,
	board_ahci_mcp77,
	board_ahci_mcp89,
	board_ahci_mv,
	board_ahci_sb600,
	board_ahci_sb700,	/* for SB700 and SB800 */
	board_ahci_vt8251,

	/* aliases */
	board_ahci_mcp_linux	= board_ahci_mcp65,
	board_ahci_mcp67	= board_ahci_mcp65,
	board_ahci_mcp73	= board_ahci_mcp65,
	board_ahci_mcp79	= board_ahci_mcp77,
};

static const struct ata_port_info ahci_port_info[] = {
	/* by features */
	[board_ahci] = {
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_ign_iferr] = {
		AHCI_HFLAGS	(AHCI_HFLAG_IGN_IRQ_IF_ERR),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_nosntf] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_SNTF),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_yes_fbs] = {
		AHCI_HFLAGS	(AHCI_HFLAG_YES_FBS),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	/* by chipsets */
	[board_ahci_mcp65] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA | AHCI_HFLAG_NO_PMP |
				 AHCI_HFLAG_YES_NCQ),
		.flags		= AHCI_FLAG_COMMON | ATA_FLAG_NO_DIPM,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mcp77] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA | AHCI_HFLAG_NO_PMP),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mcp89] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mv] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_NCQ | AHCI_HFLAG_NO_MSI |
				 AHCI_HFLAG_MV_PATA | AHCI_HFLAG_NO_PMP),
		.flags		= ATA_FLAG_SATA | ATA_FLAG_PIO_DMA,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
};

#ifndef CONFIG_SATA_AHCI_PLAT
static inline void ahci_writel(u32 value, volatile void *addr)
{
	writel(value, addr);
	debug("W 0x%08x <= 0x%x\n", (u32)addr, value);
}

static inline u32 ahci_readl(volatile const void *addr)
{
	uint32_t val = readl((void *)addr);
	debug("R 0x%08x => 0x%x\n", (u32)addr, val);
	return val;
}
#else
void ahci_writel(u32 value, void *addr);
u32 ahci_readl(volatile const void *addr);
#endif
#define ahci_writel_with_flush(a,b)	do { ahci_writel(a,b); ahci_readl(b); } while (0)

#ifdef CONFIG_OCTEON
#define virt_to_bus(devno, v)	pci_virt_to_mem(devno, (void *) (v))
#else
#define virt_to_bus(devno, v)	((phys_addr_t)(v))
#endif

/* The following table determines how we sequence resets.  Each entry
 * represents timeout for that try.  The first try can be soft or
 * hardreset.  All others are hardreset if available.  In most cases
 * the first reset w/ 10sec timeout should succeed.  Following entries
 * are mostly for error handling, hotplug and retarded devices.
 */
static const unsigned long ata_eh_reset_timeouts[] = {
	10000,	/* most drives spin up by 10sec */
	10000,	/* > 99% working drives spin up before 20sec */
	35000,	/* give > 30 secs of idleness for retarded devices */
	 5000,	/* and sweet one last chance */
	(unsigned long)-1, /* > 1 min has elapsed, give up */
};

static const unsigned long ata_eh_identify_timeouts[] = {
	 5000,	/* covers > 99% of successes and not too boring on failures */
	10000,  /* combined time till here is enough even for media access */
	30000,	/* for true idiots */
	(unsigned long)-1,
};

static const unsigned long ata_eh_flush_timeouts[] = {
	15000,	/* be generous with flush */
	15000,  /* ditto */
	30000,	/* and even more generous */
	(unsigned long)-1,
};

static const unsigned long ata_eh_other_timeouts[] = {
	 5000,	/* same rationale as identify timeout */
	10000,	/* ditto */
	/* but no merciful 30sec for other commands, it just isn't worth it */
	(unsigned long)-1,
};

struct ata_eh_cmd_timeout_ent {
	const u8		*commands;
	const unsigned long	*timeouts;
};

/* The following table determines timeouts to use for EH internal
 * commands.  Each table entry is a command class and matches the
 * commands the entry applies to and the timeout table to use.
 *
 * On the retry after a command timed out, the next timeout value from
 * the table is used.  If the table doesn't contain further entries,
 * the last value is used.
 *
 * ehc->cmd_timeout_idx keeps track of which timeout to use per
 * command class, so if SET_FEATURES times out on the first try, the
 * next try will use the second timeout value only for that class.
 */
#define CMDS(cmds...)	(const u8 []){ cmds, 0 }
static const struct ata_eh_cmd_timeout_ent
ata_eh_cmd_timeout_table[] = {
	{ .commands = CMDS(ATA_CMD_ID_ATA, ATA_CMD_ID_ATAPI),
	  .timeouts = ata_eh_identify_timeouts, },
	{ .commands = CMDS(ATA_CMD_READ_NATIVE_MAX, ATA_CMD_READ_NATIVE_MAX_EXT),
	  .timeouts = ata_eh_other_timeouts, },
	{ .commands = CMDS(ATA_CMD_SET_MAX, ATA_CMD_SET_MAX_EXT),
	  .timeouts = ata_eh_other_timeouts, },
	{ .commands = CMDS(ATA_CMD_SET_FEATURES),
	  .timeouts = ata_eh_other_timeouts, },
	{ .commands = CMDS(ATA_CMD_INIT_DEV_PARAMS),
	  .timeouts = ata_eh_other_timeouts, },
	{ .commands = CMDS(ATA_CMD_FLUSH, ATA_CMD_FLUSH_EXT),
	  .timeouts = ata_eh_flush_timeouts },
};
#undef CMDS
#define ATA_EH_CMD_TIMEOUT_TABLE_SIZE	ARRAY_SIZE(ata_eh_cmd_timeout_table)

static int ahci_deinit_port(struct ata_port *ap, const char **emsg);
static int ahci_port_start(struct ata_port *ap);
static void ahci_start_fis_rx(struct ata_port *ap);
static void ahci_power_up(struct ata_port *ap);

static int ata_lookup_timeout_table(u8 cmd)
{
	int i;

	for (i = 0; i < ATA_EH_CMD_TIMEOUT_TABLE_SIZE; i++) {
		const u8 *cur;

		for (cur = ata_eh_cmd_timeout_table[i].commands; *cur; cur++)
			if (*cur == cmd)
				return i;
	}

	return -1;
}

/**
 *	ata_internal_cmd_timeout - determine timeout for an internal command
 *	@dev: target device
 *	@cmd: internal command to be issued
 *
 *	Determine timeout for internal command @cmd for @dev.
 *
 *	RETURNS:
 *	Determined timeout.
 */
static unsigned long ata_internal_cmd_timeout(u8 cmd)
{
	int ent = ata_lookup_timeout_table(cmd);

	if (ent < 0)
		return ATA_EH_CMD_DFL_TIMEOUT;

	return ata_eh_cmd_timeout_table[ent].timeouts[0];
}


static void ata_fis_h2d_init(struct sata_fis_h2d *fis, u8 pmp, int is_cmd,
			     u8 command)
{
	debug("%s(0x%p, %u, %d, 0x%x)\n", __func__, fis, pmp, is_cmd, command);
	memset(fis, 0, sizeof(*fis));
	fis->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	fis->pm_port_c = pmp & 0xf;
	if (is_cmd)
		fis->pm_port_c |= 1 << 7;
	fis->device = ATA_DEVICE_OBS;
	fis->control = ATA_DEVCTL_OBS;
	fis->command = command;
}

static void ahci_sata_dump_fis(const struct sata_fis_d2h *s)
{
	int is_h2d = 0;
	const struct sata_fis_h2d *h2ds = (struct sata_fis_h2d *)s;
	printf("---------------------\n");
	printf("Status FIS dump:\n");
	printf("fis_type:		%02x ", s->fis_type);
	switch (s->fis_type) {
	case SATA_FIS_TYPE_REGISTER_H2D:
		puts("H2D\n");
		is_h2d = 1;
		break;
	case SATA_FIS_TYPE_REGISTER_D2H:
		puts("D2H\n");
		break;
	case SATA_FIS_TYPE_DMA_ACT_D2H:
		puts("ACT D2H\n");
		break;
	case SATA_FIS_TYPE_DMA_SETUP_BI:
		puts("DMA SETUP BI\n");
		break;
	case SATA_FIS_TYPE_DATA_BI:
		puts("DATA BI\n");
		break;
	case SATA_FIS_TYPE_BIST_ACT_BI:
		puts("BIST ACT BI\n");
		break;
	case SATA_FIS_TYPE_PIO_SETUP_D2H:
		puts("PIO SETUP D2H\n");
		break;
	case SATA_FIS_TYPE_SET_DEVICE_BITS_D2H:
		puts("SET DEVICE BITS D2H\n");
		break;
	default:
		puts("UNKNOWN\n");
		break;
	}
	printf("pm_port_i:		%02x\n", s->pm_port_i);
	if (is_h2d) {
		printf("command:		%02x\n", h2ds->command);
	} else {
		printf("status:			%02x", s->status);
		if (s->status & ATA_BUSY)
			puts(" BSY");
		if (s->status & ATA_DRDY)
			puts(" DTDY");
		if (s->status & ATA_DF)
			puts(" DF");
		if (s->status & ATA_DRQ)
			puts(" DRQ");
		if (s->status & ATA_ERR)
			puts(" ERR");
		puts("\n");
	}
	printf("error:			%02x\n", s->error);
	printf("lba_low:		%02x\n", s->lba_low);
	printf("lba_mid:		%02x\n", s->lba_mid);
	printf("lba_high:		%02x\n", s->lba_high);
	printf("device:			%02x\n", s->device);
	printf("lba_low_exp:		%02x\n", s->lba_low_exp);
	printf("lba_mid_exp:		%02x\n", s->lba_mid_exp);
	printf("lba_high_exp:		%02x\n", s->lba_high_exp);
	printf("res1:			%02x\n", s->res1);
	printf("sector_count:		%02x\n", s->sector_count);
	printf("sector_count_exp:	%02x\n", s->sector_count_exp);
	if (is_h2d)
		printf("control:		%02x\n", h2ds->control);
	printf("---------------------\n");
}

#define ata_id_u32_t(id,n)	\
	(((u32)(id)[(n) + 1] << 16) | ((u32) (id)[(n)]))

static void dump_ataid(hd_driveid_t *ataid)
{
	u8 product[ATA_ID_PROD_LEN+1];
	u8 revision[ATA_ID_FW_REV_LEN+1];
	u8 serial[ATA_ID_SERNO_LEN + 1];
	u8 firmware[ATA_ID_FW_REV_LEN + 1];
	debug("(49)ataid->capability = 0x%x\n", ataid->capability);
	debug("(53)ataid->field_valid =0x%x\n", ataid->field_valid);
	debug("(63)ataid->dma_mword = 0x%x\n", ataid->dma_mword);
	debug("(64)ataid->eide_pio_modes = 0x%x\n", ataid->eide_pio_modes);
	debug("(75)ataid->queue_depth = 0x%x\n", ataid->queue_depth);
	debug("(80)ataid->major_rev_num = 0x%x\n", ataid->major_rev_num);
	debug("(81)ataid->minor_rev_num = 0x%x\n", ataid->minor_rev_num);
	debug("(82)ataid->command_set_1 = 0x%x\n", ataid->command_set_1);
	debug("(83)ataid->command_set_2 = 0x%x\n", ataid->command_set_2);
	debug("(84)ataid->cfsse = 0x%x\n", ataid->cfsse);
	debug("(85)ataid->cfs_enable_1 = 0x%x\n", ataid->cfs_enable_1);
	debug("(86)ataid->cfs_enable_2 = 0x%x\n", ataid->cfs_enable_2);
	debug("(87)ataid->csf_default = 0x%x\n", ataid->csf_default);
	debug("(88)ataid->dma_ultra = 0x%x\n", ataid->dma_ultra);
	debug("(93)ataid->hw_config = 0x%x\n", ataid->hw_config);
	debug("(%d)ataid->capacity = 0x%x\n", ATA_ID_LBA_SECTORS,
	      ata_id_u32_t((u16 *)ataid, ATA_ID_LBA_SECTORS));
	debug("(%d)ataid->capacity_lba48 = 0x%x\n", ATA_ID_LBA48_SECTORS,
	      ata_id_u32_t((u16 *)ataid, ATA_ID_LBA48_SECTORS));
	ata_id_c_string((u16 *)ataid, serial, ATA_ID_SERNO,
			sizeof(serial));
	debug("(%d) serial = %s\n", ATA_ID_SERNO, serial);
	ata_id_c_string((u16 *)ataid, firmware, ATA_ID_FW_REV,
			sizeof(firmware));
	debug("(%d) firmware = %s\n", ATA_ID_FW_REV, firmware);
	ata_id_c_string((u16 *)ataid, product, ATA_ID_PROD,
			sizeof(product));
	debug("(%d) product = %s\n", ATA_ID_PROD, product);
	ata_id_c_string((u16 *)ataid, revision, ATA_ID_FW_REV,
			sizeof(revision));
	debug("(%d) revision = %s\n", ATA_ID_FW_REV, revision);
}

static void ahci_dump_cmd_slot(struct ahci_ioports *pp, unsigned int tag)
{
	debug("%s: tag: %u\n", __func__, tag);
	debug("\topts: 0x%x\n", le32_to_cpu(pp->cmd_slot->opts));
	debug("\tstatus: 0x%x\n", le32_to_cpu(pp->cmd_slot->status));
	debug("\ttable address: 0x%x 0x%x\n",
	      le32_to_cpu(pp->cmd_slot->tbl_addr_hi),
	      le32_to_cpu(pp->cmd_slot->tbl_addr));
}

static void ahci_dump_sg(struct ahci_ioports *pp)
{
	struct ahci_sg *ahci_sg = pp->cmd_tbl_sg;
	debug("%s:\n", __func__);
	debug("address: 0x%x 0x%x\n", le32_to_cpu(ahci_sg->addr_hi),
	      le32_to_cpu(ahci_sg->addr));
	debug("flags/size: 0x%x\n", le32_to_cpu(ahci_sg->flags_size));
}

static void ahci_enable_ahci(void __iomem *mmio)
{
	int i;
	u32 tmp;

	debug("%s(%p)\n", __func__, mmio);
	/* turn on AHCI_EN */
	tmp = ahci_readl(mmio + HOST_CTL);
	if (tmp & HOST_AHCI_EN)
		return;

	/* Some controllers need AHCI_EN to be written multiple times.
	 * Try a few times before giving up.
	 */
	for (i = 0; i < 5; i++) {
		tmp |= HOST_AHCI_EN;
		ahci_writel(tmp, mmio + HOST_CTL);
		tmp = ahci_readl(mmio + HOST_CTL);	/* flush && sanity check */
		if (tmp & HOST_AHCI_EN)
			return;
		mdelay(10);
	}

	printf("%s: Failed to enable AHCI host\n", __func__);
}

/**
 *	ahci_save_initial_config - Save and fixup initial config values
 *	@hpriv: host private area to store config values
 *	@force_port_map: force port map to a specified value
 *	@mask_port_map: mask out particular bits from port map
 *
 *	Some registers containing configuration info might be setup by
 *	BIOS and might be cleared on reset.  This function saves the
 *	initial values of those registers into @hpriv such that they
 *	can be restored after controller reset.
 *
 *	If inconsistent, config values are fixed up by this function.
 *
 */
void ahci_save_initial_config(struct ahci_host_priv *hpriv,
			      unsigned int force_port_map,
			      unsigned int mask_port_map)
{
	void __iomem *mmio = hpriv->mmio;
	u32 cap, cap2, vers, port_map;
	int i;

	debug("%s(%p, 0x%x, 0x%x)\n", __func__, hpriv,
	      force_port_map, mask_port_map);
	/* make sure AHCI mode is enabled before accessing CAP */
	ahci_enable_ahci(mmio);

	/* Values prefixed with saved_ are written back to host after
	 * reset.  Values without are used for driver operation.
	 */
	hpriv->saved_cap = cap = ahci_readl(mmio + HOST_CAP);
	hpriv->saved_port_map = port_map = ahci_readl(mmio + HOST_PORTS_IMPL);

	/* CAP2 register is only defined for AHCI 1.2 and later */
	vers = ahci_readl(mmio + HOST_VERSION);
	if ((vers >> 16) > 1 ||
	   ((vers >> 16) == 1 && (vers & 0xFFFF) >= 0x200))
		hpriv->saved_cap2 = cap2 = ahci_readl(mmio + HOST_CAP2);
	else
		hpriv->saved_cap2 = cap2 = 0;

	debug("%s: saved cap: 0x%x, cap2: 0x%x saved port map: 0x%x\n",
	      __func__, hpriv->saved_cap, hpriv->saved_cap2,
	      hpriv->saved_port_map);

	/* some chips have errata preventing 64bit use */
	if ((cap & HOST_CAP_64) && (hpriv->flags & AHCI_HFLAG_32BIT_ONLY)) {
		printf("controller can't do 64bit DMA, forcing 32bit, cap: 0x%x, flags: 0x%x\n",
		       cap, hpriv->flags);
		cap &= ~HOST_CAP_64;
	}

	if ((cap & HOST_CAP_NCQ) && (hpriv->flags & AHCI_HFLAG_NO_NCQ)) {
		printf("controller can't do NCQ, turning off CAP_NCQ\n");
		cap &= ~HOST_CAP_NCQ;
	}

	if (!(cap & HOST_CAP_NCQ) && (hpriv->flags & AHCI_HFLAG_YES_NCQ)) {
		debug("controller can do NCQ, turning on CAP_NCQ\n");
		cap |= HOST_CAP_NCQ;
	}

	if ((cap & HOST_CAP_PMP) && (hpriv->flags & AHCI_HFLAG_NO_PMP)) {
		debug("controller can't do PMP, turning off CAP_PMP\n");
		cap &= ~HOST_CAP_PMP;
	}

	if ((cap & HOST_CAP_SNTF) && (hpriv->flags & AHCI_HFLAG_NO_SNTF)) {
		debug("controller can't do SNTF, turning off CAP_SNTF\n");
		cap &= ~HOST_CAP_SNTF;
	}

	if (!(cap & HOST_CAP_FBS) && (hpriv->flags & AHCI_HFLAG_YES_FBS)) {
		debug("controller can do FBS, turning on CAP_FBS\n");
		cap |= HOST_CAP_FBS;
	}

	if (force_port_map && port_map != force_port_map) {
		debug("%s: forcing port_map 0x%x -> 0x%x\n",
		      __func__, port_map, force_port_map);
		port_map = force_port_map;
	}

	if (mask_port_map) {
		printf("%s: masking port_map 0x%x -> 0x%x\n",
			__func__, port_map,
			port_map & mask_port_map);
		port_map &= mask_port_map;
	}

	/* cross check port_map and cap.n_ports */
	if (port_map) {
		int map_ports = 0;

		for (i = 0; i < AHCI_MAX_PORTS; i++)
			if (port_map & (1 << i))
				map_ports++;

		/* If PI has more ports than n_ports, whine, clear
		 * port_map and let it be generated from n_ports.
		 */
		if (map_ports > ahci_nr_ports(cap)) {
			printf("%s: implemented port map (0x%x) contains more ports than nr_ports (%u), using nr_ports\n",
			       __func__, port_map, ahci_nr_ports(cap));
			port_map = 0;
		}
	}

	/* fabricate port_map from cap.nr_ports */
	if (!port_map) {
		port_map = (1 << ahci_nr_ports(cap)) - 1;
		printf("forcing PORTS_IMPL to 0x%x\n", port_map);

		/* write the fixed up value to the PI register */
		hpriv->saved_port_map = port_map;
	}

	/* record values to use during operation */
	hpriv->cap = cap;
	hpriv->cap2 = cap2;
	hpriv->port_map = port_map;
	DPRINTK("EXIT\n");
}

/**
 *	ahci_restore_initial_config - Restore initial config
 *	@host: target ATA host
 *
 *	Restore initial config stored by ahci_save_initial_config().
 *
 */
static void ahci_restore_initial_config(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;

	ahci_writel(hpriv->saved_cap, mmio + HOST_CAP);
	if (hpriv->saved_cap2)
		ahci_writel(hpriv->saved_cap2, mmio + HOST_CAP2);
	ahci_writel(hpriv->saved_port_map, mmio + HOST_PORTS_IMPL);
	(void) ahci_readl(mmio + HOST_PORTS_IMPL);	/* flush */
}

static unsigned ahci_scr_offset(struct ata_port *ap, unsigned int sc_reg)
{
	static const int offset[] = {
		[SCR_STATUS]		= PORT_SCR_STAT,
		[SCR_CONTROL]		= PORT_SCR_CTL,
		[SCR_ERROR]		= PORT_SCR_ERR,
		[SCR_ACTIVE]		= PORT_SCR_ACT,
		[SCR_NOTIFICATION]	= PORT_SCR_NTF,
	};
	struct ahci_host_priv *hpriv = ap->host->private_data;

	if (sc_reg < ARRAY_SIZE(offset) &&
	    (sc_reg != SCR_NOTIFICATION || (hpriv->cap & HOST_CAP_SNTF)))
		return offset[sc_reg];
	return 0;
}

static int ahci_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val)
{
	void __iomem *port_mmio;
	int offset;

	debug("%s(%d, %d, 0x%p)\n", __func__, link->ap->port_no, sc_reg, val);
	port_mmio = ahci_port_base(link->ap);
	offset = ahci_scr_offset(link->ap, sc_reg);

	debug("  offset: %d\n", offset);
	if (offset) {
		*val = ahci_readl(port_mmio + offset);
		return 0;
	}
	return -EINVAL;
}

static int ahci_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	int offset = ahci_scr_offset(link->ap, sc_reg);

	if (offset) {
		ahci_writel(val, port_mmio + offset);
		return 0;
	}
	return -EINVAL;
}

void ahci_start_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	/* start DMA */
	tmp = ahci_readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_START;
	ahci_writel(tmp, port_mmio + PORT_CMD);
	ahci_readl(port_mmio + PORT_CMD); /* flush */
	debug("%s: EXIT\n", __func__);
}

int ahci_stop_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	tmp = ahci_readl(port_mmio + PORT_CMD);

	/* check if the HBA is idle */
	if ((tmp & (PORT_CMD_START | PORT_CMD_LIST_ON)) == 0)
		return 0;

	/* setting HBA to idle */
	tmp &= ~PORT_CMD_START;
	ahci_writel(tmp, port_mmio + PORT_CMD);

	/* wait for engine to stop. This could be as long as 500 msec */
	tmp = ata_wait_register(port_mmio + PORT_CMD,
				PORT_CMD_LIST_ON, PORT_CMD_LIST_ON, 500);
	if (tmp & PORT_CMD_LIST_ON)
		return -EIO;

	debug("%s: EXIT\n", __func__);
	return 0;
}

static void ahci_start_fis_rx(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	u32 tmp;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	/* set FIS registers */
	if (hpriv->cap & HOST_CAP_64)
		ahci_writel((pp->cmd_slot_dma >> 16) >> 16,
		       port_mmio + PORT_LST_ADDR_HI);
	ahci_writel(pp->cmd_slot_dma & 0xffffffff, port_mmio + PORT_LST_ADDR);

	if (hpriv->cap & HOST_CAP_64)
		ahci_writel((pp->rx_fis_dma >> 16) >> 16,
		       port_mmio + PORT_FIS_ADDR_HI);
	ahci_writel(pp->rx_fis_dma & 0xffffffff, port_mmio + PORT_FIS_ADDR);

	/* enable FIS reception */
	tmp = ahci_readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_FIS_RX;
	ahci_writel(tmp, port_mmio + PORT_CMD);
	debug("%s(%d port_cmd: 0x%x, cmd_slot_dma: %llx, rx_fis_dma: %llx)\n",
	       __func__, ap->port_no, tmp, (u64)pp->cmd_slot_dma,
	      (u64)pp->rx_fis_dma);

	/* flush */
	ahci_readl(port_mmio + PORT_CMD);
	debug("%s: EXIT\n", __func__);
}

static int ahci_stop_fis_rx(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;
	int rc = 0;

	debug("%s(%p(%d))\n", __func__, ap, ap->port_no);
	/* disable FIS reception */
	tmp = ahci_readl(port_mmio + PORT_CMD);
	tmp &= ~PORT_CMD_FIS_RX;
	ahci_writel(tmp, port_mmio + PORT_CMD);

	/* wait for completion, spec says 500ms, give it 1000 */
	tmp = ata_wait_register(port_mmio + PORT_CMD, PORT_CMD_FIS_ON,
				PORT_CMD_FIS_ON, 1000);
	if (tmp & PORT_CMD_FIS_ON)
		rc = -EBUSY;

	debug("%s: EXIT %d\n", __func__, rc);
	return rc;
}

static void ahci_power_up(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 cmd;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	cmd = ahci_readl(port_mmio + PORT_CMD) & ~PORT_CMD_ICC_MASK;

	/* spin up device */
	if (hpriv->cap & HOST_CAP_SSS) {
		debug("  Spinning up drive\n");
		cmd |= PORT_CMD_SPIN_UP;
		ahci_writel(cmd, port_mmio + PORT_CMD);
	}

	/* wake up link */
	ahci_writel(cmd | PORT_CMD_ICC_ACTIVE, port_mmio + PORT_CMD);
	debug("%s: EXIT\n", __func__);
}

static void ahci_start_port(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	/* enable FIS reception */
	ahci_start_fis_rx(ap);

	/* enable DMA */
	if (!(hpriv->flags & AHCI_HFLAG_DELAY_ENGINE))
		ahci_start_engine(ap);
	debug("%s: Exit\n", __func__);
}

static int ahci_deinit_port(struct ata_port *ap, const char **emsg)
{
	int rc;
	debug("%s(%d): deinitializing port\n", __func__, ap->port_no);

	/* disable DMA */
	rc = ahci_stop_engine(ap);
	if (rc) {
		*emsg = "failed to stop engine";
		goto done;
	}

	/* disable FIS reception */
	rc = ahci_stop_fis_rx(ap);
	if (rc) {
		*emsg = "failed to stop FIS RX";
	}
done:
	debug("%s: EXIT %d\n", __func__, rc);
	return rc;
}

int ahci_reset_controller(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 tmp;

	debug("%s ENTRY\n", __func__);
	/* we must be in AHCI mode, before using anything
	 * AHCI-specific, such as HOST_RESET.
	 */
	ahci_enable_ahci(mmio);

	/* global controller reset */
	if (!ahci_skip_host_reset) {
		tmp = ahci_readl(mmio + HOST_CTL);
		if ((tmp & HOST_RESET) == 0) {
			ahci_writel(tmp | HOST_RESET, mmio + HOST_CTL);
			ahci_readl(mmio + HOST_CTL); /* flush */
		}

		/*
		 * to perform host reset, OS should set HOST_RESET
		 * and poll until this bit is read to be "0".
		 * reset must complete within 1 second, or
		 * the hardware should be considered fried.
		 */
		tmp = ata_wait_register(mmio + HOST_CTL, HOST_RESET,
					HOST_RESET, 2000);

		if (tmp & HOST_RESET) {
			printf("controller reset failed (0x%x)\n", tmp);
			return -EIO;
		}

		/* turn on AHCI mode */
		ahci_enable_ahci(mmio);

		/* Some registers might be cleared on reset.  Restore
		 * initial values.
		 */
		ahci_restore_initial_config(host);
	} else {
		debug("%s: Skipping global host reset\n", __func__);
	}

	debug("%s: EXIT 0\n", __func__);
	return 0;
}

int ahci_reset_em(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 em_ctl;

	debug("%s ENTRY\n", __func__);
	em_ctl = ahci_readl(mmio + HOST_EM_CTL);
	if ((em_ctl & EM_CTL_TM) || (em_ctl & EM_CTL_RST))
		return -EINVAL;

	ahci_writel(em_ctl | EM_CTL_RST, mmio + HOST_EM_CTL);
	debug("%s: EXIT 0\n", __func__);
	return 0;
}

/**
 * Perform a full port resets
 * @param ap port to reset
 *
 * @return 0 for success, -1 on error
 */
static int ahci_port_reset(struct ata_port *ap)
{
	void __iomem *mmio = ahci_port_base(ap);
	u32 tmp;

	debug("%s(%d)\n", __func__, ap->port_no);
	/* AHCI 1.3 spec section 10.4.2. */
	/* Clear PxCMD.ST */
	tmp = ahci_readl(mmio + PORT_CMD);
	tmp &= ~PORT_CMD_START;
	ahci_writel(tmp, mmio + PORT_CMD);
	/* Wait for PxCMD.CR to clear (up to 500ms) */
	tmp = ata_wait_register(mmio + PORT_CMD, PORT_CMD_LIST_ON, 0, 500);
	if (tmp & PORT_CMD_LIST_ON)
		printf("%s(%d): port is stuck\n", __func__, ap->port_no);

	/* Set PxSCTL.DET to 1 */
	tmp = ahci_readl(mmio + PORT_SCR_CTL);
	tmp &= ~0xf;
	tmp |= 1;
	ahci_writel(tmp, mmio + PORT_SCR_CTL);
	/* Wait at least 1ms */
	mdelay(2);
	/* Set PxSCTL.DET to 0 */
	tmp &= ~0xF;
	ahci_writel(tmp, mmio + PORT_SCR_CTL);
	/* Wait for PxSSTS.DET to be set to 1 */
	tmp = ata_wait_register(mmio + PORT_SCR_STAT, 0xD, 0, 1000);
	if ((tmp & 0xd) != 1) {
		printf("%s(%d) PxSSTS.DET is %d, not 1 or 3.\n", __func__,
		       ap->port_no, tmp & 0xf);
		return -1;
	}

	/* Write all 1s  to PxSERR */
	ahci_writel(0xFFFFFFFF, mmio + PORT_SCR_ERR);

	/* When PxSCTL.DET is set to 1, the HBA shall reset PxTFD.STS to 0x7Fh
	 * and shall reset PxSSTS.DET to 0h.  When PxSCTL.DET is set to 0h,
	 * upon receiving a COMINIT from the attached device, PxTFD.STS.BSY
	 * shall be set to '1' by the HBA.
	 */
	debug("%s: EXIT 0\n", __func__);
	return 0;
}

static void ahci_port_init(struct ata_port *ap, int port_no,
			   void __iomem *mmio, void __iomem *port_mmio)
{
	const char *emsg = NULL;
	int rc;
	u32 tmp;

	debug("%s(0x%p, %d, 0x%p, 0x%p)\n", __func__, ap, port_no,
	      mmio, port_mmio);
	/* make sure port is not active */
	rc = ahci_deinit_port(ap, &emsg);
	if (rc)
		printf("Warning: %s\n", emsg);

	/* clear SError */
	tmp = ahci_readl(port_mmio + PORT_SCR_ERR);
	debug("%s(%d): PORT_SCR_ERR 0x%x\n", __func__, port_no, tmp);
	ahci_writel(tmp, port_mmio + PORT_SCR_ERR);

	/* clear port IRQ */
	tmp = ahci_readl(port_mmio + PORT_IRQ_STAT);
	debug("%s(%d): PORT_IRQ_STAT 0x%x\n", __func__, port_no, tmp);
	if (tmp)
		ahci_writel(tmp, port_mmio + PORT_IRQ_STAT);

	/* ahci_writel(1 << port_no, mmio + HOST_IRQ_STAT); */
	debug("%s: EXIT\n", __func__);
}

void ahci_init_controller(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	int i;
	void __iomem *port_mmio;
	u32 tmp;

	debug("%s(0x%p)\n", __func__, host);
	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		ap->ops = &ahci_ops;

		port_mmio = ahci_port_base(ap);
		ahci_port_init(ap, i, mmio, port_mmio);
	}

	tmp = ahci_readl(mmio + HOST_CTL);
	debug("%s HOST_CTL 0x%x\n", __func__, tmp);
	ahci_writel(tmp | HOST_IRQ_EN, mmio + HOST_CTL);
	tmp = ahci_readl(mmio + HOST_CTL);
	debug("%s HOST_CTL 0x%x\n", __func__, tmp);
	debug("%s: EXIT\n", __func__);
}

static void ahci_dev_config(struct ata_device *dev)
{
	struct ahci_host_priv *hpriv = dev->link->ap->host->private_data;

	if (hpriv->flags & AHCI_HFLAG_SECT255) {
		dev->max_sectors = 255;
		printf("SB600 AHCI: limiting to 255 sectors per cmd\n");
	}
}

static unsigned int ahci_dev_classify(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ata_taskfile tf;
	u32 tmp;

	tmp = ahci_readl(port_mmio + PORT_SIG);
	tf.lbah		= (tmp >> 24)	& 0xff;
	tf.lbam		= (tmp >> 16)	& 0xff;
	tf.lbal		= (tmp >> 8)	& 0xff;
	tf.nsect	= (tmp)		& 0xff;

	return ata_dev_classify(&tf);
}

static void ahci_fill_cmd_slot(struct ahci_port_priv *pp, unsigned int tag,
			       u32 opts)
{
	phys_addr_t cmd_tbl_dma = pp->cmd_tbl_dma + tag * AHCI_CMD_TBL_SZ;

	debug("%s(0x%p, %d, 0x%x)\n", __func__, pp, tag, opts);
	debug("%s: opts: 0x%x, addr: 0x%llx\n", __func__, opts, cmd_tbl_dma);

	pp->cmd_slot[tag].opts = cpu_to_le32(opts);
	pp->cmd_slot[tag].status = 0;
	pp->cmd_slot[tag].tbl_addr = cpu_to_le32(cmd_tbl_dma & 0xffffffff);
	pp->cmd_slot[tag].tbl_addr_hi = cpu_to_le32((cmd_tbl_dma >> 16) >> 16);
	flush_dcache_range((long unsigned int)&pp->cmd_slot[tag],
			   (long unsigned int)(&pp->cmd_slot[tag] + sizeof(struct ahci_cmd_hdr)));

	debug("cmd table at 0x%p (0x%llx): 0x%x 0x%x 0x%x 0x%x\n",
	      pp->cmd_tbl,
	      cmd_tbl_dma,
	      le32_to_cpu(pp->cmd_slot[tag].opts),
	      le32_to_cpu(pp->cmd_slot[tag].status),
	      le32_to_cpu(pp->cmd_slot[tag].tbl_addr),
	      le32_to_cpu(pp->cmd_slot[tag].tbl_addr_hi));
}

int ahci_kick_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	u8 status = ahci_readl(port_mmio + PORT_TFDATA) & 0xFF;
	u32 tmp;
	int busy, rc;

	debug("%s: Entry, port: %d\n", __func__, ap->port_no);
	/* stop engine */
	rc = ahci_stop_engine(ap);
	if (rc) {
		debug("%s: ahci_stop_engine returned %d, restarting\n",
		      __func__, rc);
		goto out_restart;
	}

	/* need to do CLO?
	 * always do CLO if PMP is attached (AHCI-1.3 9.2)
	 */
	busy = status & (ATA_BUSY | ATA_DRQ);
	if (!busy && !sata_pmp_attached(ap)) {
		rc = 0;
		debug("%s: PMP not attached\n", __func__);
		goto out_restart;
	}

	if (!(hpriv->cap & HOST_CAP_CLO)) {
		debug("%s: command list override not supported, caps: 0x%x.\n",
		      __func__, hpriv->cap);
		rc = -EOPNOTSUPP;
		goto out_restart;
	}

	/* perform CLO */
	debug("%s: Setting command list override\n", __func__);
	tmp = ahci_readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_CLO;
	ahci_writel(tmp, port_mmio + PORT_CMD);

	rc = 0;
	tmp = ata_wait_register(port_mmio + PORT_CMD,
				PORT_CMD_CLO, PORT_CMD_CLO, 500);
	if (tmp & PORT_CMD_CLO)
		rc = -EIO;

	/* restart engine */
 out_restart:
	debug("%s: Restarting engine\n", __func__);
	ahci_start_engine(ap);
	debug("%s: Exit rc=%d\n", __func__, rc);
	return rc;
}

static int ahci_exec_polled_cmd(struct ata_port *ap, int pmp,
				struct ata_taskfile *tf, int is_cmd, u16 flags,
				unsigned long timeout_msec)
{
	const u32 cmd_fis_len = 5; /* five dwords */
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u8 *fis = pp->cmd_tbl;
	u32 tmp;

	debug("%s(%d pmp: 0x%x, is_cmd: %d, flags: 0x%x, timeout: %lu) cmd: 0x%x\n", __func__,
	      ap->port_no, pmp, is_cmd, flags, timeout_msec, tf->command);

	debug("  PxTFD: 0x%x\n", ahci_readl(port_mmio + PORT_TFDATA));
	debug("  PxSCR_ERR: 0x%x, PxSCR_ACT: 0x%x, PxIS: 0x%x, PxCMD: 0x%x, PxSSTS: 0x%x, PxSCTL: 0x%x, PxCI: 0x%x, PxSNTF: 0x%x, PxFBS: 0x%x\n",
	      ahci_readl(port_mmio + PORT_SCR_ERR),
	      ahci_readl(port_mmio + PORT_SCR_ACT),
	      ahci_readl(port_mmio + PORT_IRQ_STAT),
	      ahci_readl(port_mmio + PORT_CMD),
	      ahci_readl(port_mmio + PORT_SCR_STAT),
	      ahci_readl(port_mmio + PORT_SCR_CTL),
	      ahci_readl(port_mmio + PORT_CMD_ISSUE),
	      ahci_readl(port_mmio + PORT_SCR_NTF),
	      ahci_readl(port_mmio + PORT_FBS));
	/* prep the command */
	ata_tf_to_fis(tf, pmp, is_cmd, fis);

	ahci_fill_cmd_slot(pp, 0, cmd_fis_len | flags | (pmp << 12));
	flush_dcache_range((ulong)fis, (ulong)fis + cmd_fis_len * 4);

	/* issue & wait */
	ahci_writel(1, port_mmio + PORT_CMD_ISSUE);

	if (!timeout_msec) {
		timeout_msec = ata_internal_cmd_timeout(tf->command);
		debug("%s: command timeout: %lu ms\n", __func__, timeout_msec);
	}

	if (timeout_msec) {
		tmp = ata_wait_register(port_mmio + PORT_CMD_ISSUE,
					1, 1, timeout_msec);
		if (tmp & 1) {
			u32 pscr_reg = 0;
			int rc;
			debug("%s: timed out\n", __func__);
			debug("%s: PxTFD: 0x%x, PxCI: 0x%x\n",
			      __func__, ahci_readl(port_mmio + PORT_TFDATA),
			      tmp);
			debug("  PxSCR_ERR: 0x%x, PxSCR_ACT: 0x%x, PxIS: 0x%x, PxCMD: 0x%x, PxSSTS: 0x%x, PxSCTL: 0x%x, PxCI: 0x%x, PxSNTF: 0x%x, PxFBS: 0x%x\n",
			      ahci_readl(port_mmio + PORT_SCR_ERR),
			      ahci_readl(port_mmio + PORT_SCR_ACT),
			      ahci_readl(port_mmio + PORT_IRQ_STAT),
			      ahci_readl(port_mmio + PORT_CMD),
			      ahci_readl(port_mmio + PORT_SCR_STAT),
			      ahci_readl(port_mmio + PORT_SCR_CTL),
			      ahci_readl(port_mmio + PORT_CMD_ISSUE),
			      ahci_readl(port_mmio + PORT_SCR_NTF),
			      ahci_readl(port_mmio + PORT_FBS));
			if (tf->command != ATA_CMD_PMP_READ) {
				rc = sata_pmp_scr_read(&ap->link, 0, &pscr_reg);
				if (rc) {
					debug("%s: Could not read PMP SCR register 0\n",
					      __func__);
				}
				debug("%s: pscr[0]: 0x%x\n", __func__, pscr_reg);
				rc = sata_pmp_scr_read(&ap->link, 1, &pscr_reg);
				if (rc) {
					debug("%s: Could not read PMP SCR register 1\n",
					      __func__);
				}
				debug("%s: pscr[1]: 0x%x\n", __func__, pscr_reg);
			}
			ahci_kick_engine(ap);
			debug("%s: EXIT -EBUSY\n", __func__);
			return -EBUSY;
		}
	} else
		ahci_readl(port_mmio + PORT_CMD_ISSUE);	/* flush */

	debug("%s: EXIT 0\n", __func__);
	return 0;
}

int ahci_do_softreset(struct ata_link *link, unsigned int *class,
		      int pmp, unsigned long deadline,
		      int (*check_ready)(struct ata_link *link))
{
	struct ata_port *ap = link->ap;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	const char *reason = NULL;
	unsigned long msecs = deadline;
	struct ata_taskfile tf;
	int rc;

	debug("%s(%d pmp: %d)\n", __func__, ap->port_no, pmp);

	/* prepare for SRST (AHCI-1.1 10.4.1) */
	rc = ahci_kick_engine(ap);
	if (rc && rc != -EOPNOTSUPP)
		printf("ahci softreset failed to reset engine on port %d (errno=%d)\n",
		       rc, ap->port_no);

	ata_tf_init(link->device, &tf);

	/* issue the first D2H Register FIS */
	tf.ctl |= ATA_SRST;
	debug("%s: Issuing D2H Register FIS, tf.ctl: 0x%x\n", __func__, tf.ctl);
	if (ahci_exec_polled_cmd(ap, pmp, &tf, 0,
				 AHCI_CMD_RESET | AHCI_CMD_CLR_BUSY, msecs)) {
		rc = -EIO;
		reason = "1st FIS failed";
		debug("%s: First FIS failed\n", __func__);
		goto fail;
	}

	/* spec says at least 5us, but be generous and sleep for 1ms */
	mdelay(1);

	/* issue the second D2H Register FIS */
	tf.ctl &= ~ATA_SRST;
	debug("%s: Issuing D2H Register FIS, tf.ctl: 0x%x\n", __func__, tf.ctl);
	ahci_exec_polled_cmd(ap, pmp, &tf, 0, 0, 0);

	/* wait for link to become ready */
	rc = ata_wait_after_reset(link, deadline, check_ready);
	if (rc == -EBUSY && hpriv->flags & AHCI_HFLAG_SRST_TOUT_IS_OFFLINE) {
		/*
		 * Workaround for cases where link online status can't
		 * be trusted.  Treat device readiness timeout as link
		 * offline.
		 */
		printf("ahci device on port %d not ready, treating as offline\n",
		       ap->port_no);
		*class = ATA_DEV_NONE;
	} else if (rc) {
		/* link occupied, -ENODEV too is an error */
		reason = "device not ready";
		goto fail;
	} else
		*class = ahci_dev_classify(ap);

	debug("EXIT, class=%u\n", *class);
	return 0;

 fail:
	printf("SATA softreset failed (%s)\n", reason);
	return rc;
}

int ahci_check_ready(struct ata_link *link)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	u8 status = ahci_readl(port_mmio + PORT_TFDATA) & 0xFF;

	return ata_check_ready(status);
}

static int ahci_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	int pmp = sata_srst_pmp(link);

	return ahci_do_softreset(link, class, pmp, deadline, ahci_check_ready);
}

static int ahci_bad_pmp_check_ready(struct ata_link *link)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	u8 status = ahci_readl(port_mmio + PORT_TFDATA) & 0xFF;
	u32 irq_status = ahci_readl(port_mmio + PORT_IRQ_STAT);

	/*
	 * There is no need to check TFDATA if BAD PMP is found due to HW bug,
	 * which can save timeout delay.
	 */
	if (irq_status & PORT_IRQ_BAD_PMP)
		return -EIO;

	return ata_check_ready(status);
}

int ahci_pmp_retry_softreset(struct ata_link *link, unsigned int *class,
			     unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	int pmp = sata_srst_pmp(link);
	int rc;
	u32 irq_sts;

	debug("%s(%d)\n", __func__, link->ap->port_no);

	rc = ahci_do_softreset(link, class, pmp, deadline,
			       ahci_bad_pmp_check_ready);

	/*
	 * Soft reset fails with IPMS set when PMP is enabled but
	 * SATA HDD/ODD is connected to SATA port, do soft reset
	 * again to port 0.
	 */
	if (rc == -EIO) {
		irq_sts = ahci_readl(port_mmio + PORT_IRQ_STAT);
		debug("%s: irq_sts: 0x%x\n", __func__, irq_sts);
		if (irq_sts & PORT_IRQ_BAD_PMP) {
			printf("AHCI softreset applying PMP SRST workaround "
			       "and retrying port %d\n", ap->port_no);
			rc = ahci_do_softreset(link, class, 0, deadline,
					       ahci_check_ready);
		}
	}

	debug("%s: EXIT %d\n", __func__, rc);
	return rc;
}

static int ahci_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	const unsigned long timing[] = { 100, 2000, 5000};
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	u8 *d2h_fis = pp->rx_fis + RX_FIS_D2H_REG;
	struct ata_taskfile tf;
	bool online;
	int rc;

	debug("%s(%d): ENTER\n", __func__, ap->port_no);

	ahci_stop_engine(ap);

	/* clear D2H reception area to properly wait for D2H FIS */
	ata_tf_init(link->device, &tf);
	tf.command = 0x80;
	ata_tf_to_fis(&tf, 0, 0, d2h_fis);

	rc = sata_link_hardreset(link, timing, deadline, &online,
				 ahci_check_ready);
	ahci_start_engine(ap);

	if (online)
		*class = ahci_dev_classify(ap);

	debug("%s: EXIT, rc=%d, class=%u\n", __func__, rc, *class);
	return rc;
}

static void ahci_postreset(struct ata_link *link, unsigned int *class)
{
	struct ata_port *ap = link->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 new_tmp, tmp;

	debug("%s(%d)\n", __func__, ap->port_no);
	ata_std_postreset(link, class);

	/* Make sure port's ATAPI bit is set appropriately */
	new_tmp = tmp = ahci_readl(port_mmio + PORT_CMD);
	if (*class == ATA_DEV_ATAPI)
		new_tmp |= PORT_CMD_ATAPI;
	else
		new_tmp &= ~PORT_CMD_ATAPI;
	debug("%s(port_cmd: 0x%x, old: 0x%x)\n", __func__, new_tmp, tmp);
	if (new_tmp != tmp) {
		ahci_writel(new_tmp, port_mmio + PORT_CMD);
		ahci_readl(port_mmio + PORT_CMD); /* flush */
	}
	debug("%s: EXIT\n", __func__);
}

#define MAX_DATA_BYTE_COUNT  (4*1024*1024)

static int ahci_fill_sg(struct ata_port *ap, unsigned char *buf, int buf_len)
{
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_sg *ahci_sg = pp->cmd_tbl + AHCI_CMD_TBL_HDR_SZ;
	u32 sg_count;
	int i;
	phys_addr_t paddr = virt_to_bus(ap->host->pdev, buf);

	debug("%s(%d, 0x%p, %d)\n", __func__, ap->port_no, buf, buf_len);
	debug("pp: 0x%p\n", pp);
	debug("\tsg address: 0x%p (0x%llx)\n", ahci_sg, pp->cmd_tbl_dma + AHCI_CMD_TBL_HDR_SZ);;
	sg_count = ((buf_len - 1) / MAX_DATA_BYTE_COUNT) + 1;
	if (sg_count > AHCI_MAX_SG) {
		printf("Error: Too much data!\n");
		return -1;
	}

	for (i = 0; i < sg_count; i++) {
		debug("%s: sg[%d].paddr: 0x%llx\n", __func__, i, paddr);
		ahci_sg->addr = cpu_to_le32(paddr & 0xffffffff);
		ahci_sg->addr_hi = cpu_to_le32((paddr >> 16) >> 16);
		ahci_sg->reserved = 0;
		ahci_sg->flags_size = cpu_to_le32(0x3fffff &
					  (buf_len < MAX_DATA_BYTE_COUNT
					   ? (buf_len - 1)
					   : (MAX_DATA_BYTE_COUNT - 1)));
		debug("ahci_sg[%d](0x%p) addr=0x%08x, addr_hi=0x%08x, flags/size=0x%08x\n",
		      i, ahci_sg,
		      le32_to_cpu(ahci_sg->addr),
		      le32_to_cpu(ahci_sg->addr_hi),
		      le32_to_cpu(ahci_sg->flags_size));
#ifdef DEBUG
		print_buffer((ulong)buf, buf, 1,
			     le32_to_cpu(ahci_sg->flags_size) + 1, 0);
#endif
		ahci_sg++;
		buf_len -= MAX_DATA_BYTE_COUNT;
		paddr += MAX_DATA_BYTE_COUNT;
	}

	return sg_count;
}

static void ahci_enable_fbs(struct ata_port *ap)
{
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 fbs;
	int rc;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	if (!pp->fbs_supported) {
		debug("FBS not supported\n");
		return;
	}

	fbs = ahci_readl(port_mmio + PORT_FBS);
	if (fbs & PORT_FBS_EN) {
		pp->fbs_enabled = true;
		pp->fbs_last_dev = -1; /* initialization */
		return;
	}

	debug("  stoping engine\n");
	rc = ahci_stop_engine(ap);
	if (rc) {
		debug("%s: ahci_stop_engine returned %d\n", __func__, rc);
		return;
	}

	ahci_writel(fbs | PORT_FBS_EN, port_mmio + PORT_FBS);
	fbs = ahci_readl(port_mmio + PORT_FBS);
	if (fbs & PORT_FBS_EN) {
		printf("FBS is enabled for SATA port %d\n", ap->port_no);
		pp->fbs_enabled = true;
		pp->fbs_last_dev = -1; /* initialization */
	} else
		printf("Failed to enable FBS for SATA port %d\n", ap->port_no);

	debug("%s: starting engine\n", __func__);
	ahci_start_engine(ap);
	debug("%s: EXIT\n", __func__);
}

static void ahci_disable_fbs(struct ata_port *ap)
{
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 fbs;
	int rc;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	if (!pp->fbs_supported) {
		debug("  fbs not supported!\n");
		return;
	}

	fbs = ahci_readl(port_mmio + PORT_FBS);
	if ((fbs & PORT_FBS_EN) == 0) {
		pp->fbs_enabled = false;
		debug("  fbs already disabled\n");
		return;
	}

	rc = ahci_stop_engine(ap);
	if (rc)
		return;

	ahci_writel(fbs & ~PORT_FBS_EN, port_mmio + PORT_FBS);
	fbs = ahci_readl(port_mmio + PORT_FBS);
	if (fbs & PORT_FBS_EN)
		printf("Failed to disable FBS for SATA port %d\n", ap->port_no);
	else {
		printf("FBS is disabled for SATA port %d\n", ap->port_no);
		pp->fbs_enabled = false;
	}

	ahci_start_engine(ap);
	debug("%s EXIT\n", __func__);
}

static void ahci_pmp_attach(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 cmd;
	int i;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	cmd = ahci_readl(port_mmio + PORT_CMD);
	cmd |= PORT_CMD_PMP;
	ahci_writel(cmd, port_mmio + PORT_CMD);
	debug(" enabling FBS\n");
	ahci_enable_fbs(ap);
	debug("%s: ap: %p, pmp_link: %p, nr_pmp_links: %d\n",
	      __func__, ap, ap->pmp_link, ap->nr_pmp_links);
	debug(" sizes: port: %d, link: %d, dev: %d\n", sizeof(struct ata_port),
	      sizeof(struct ata_link), sizeof(struct ata_device));
	for (i = 0; i < ap->nr_pmp_links; i++) {
		int j;
		debug("  link: %d, link address: %p, pmp: %d\n",
		      i, &(ap->pmp_link[i]), ap->pmp_link[i].pmp);
		for (j = 0; j < ATA_MAX_DEVICES; j++) {
			debug("    device: %d (%p), link: %p, devno: %d\n",
			      j, &(ap->pmp_link[i].device[j]),
			      ap->pmp_link[i].device[j].link,
			      ap->pmp_link[i].device[j].devno);
		}
	}
	for (i = 0; i < CONFIG_SYS_SATA_MAX_DEVICE; i++)
		if (!sata_pmp_link[i]) {
			debug("%s: Setting sata pmp link %d to %p\n",
			      __func__, i, ap->pmp_link);
			sata_pmp_link[i] = ap->pmp_link;
			break;
		}
	debug("%s(%d) EXIT\n", __func__, ap->port_no);
}

static void ahci_pmp_detach(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 cmd;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	ahci_disable_fbs(ap);

	cmd = readl(port_mmio + PORT_CMD);
	cmd &= ~PORT_CMD_PMP;
	ahci_writel(cmd, port_mmio + PORT_CMD);
	debug("%s: EXIT\n", __func__);
}


int ahci_port_resume(struct ata_port *ap)
{
	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);

	ahci_power_up(ap);
	ahci_start_port(ap);

	if (sata_pmp_attached(ap)) {
		debug("%s: Attaching pmp\n", __func__);
		ahci_pmp_attach(ap);
	} else {
		debug("%s: Detaching pmp\n", __func__);
		ahci_pmp_detach(ap);
	}
	debug("%s EXIT\n", __func__);
	return 0;
}

static int ahci_port_start(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp;
	void *mem;
	phys_addr_t mem_dma;
	size_t dma_sz, rx_fis_sz;
	int rc;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);

	if (ap->private_data)
		pp = (struct ahci_port_priv *)ap->private_data;
	else
		pp = calloc(sizeof(*pp), 1);

	if (!pp)
		return -ENOMEM;

	/* check FBS capability */
	if ((hpriv->cap & HOST_CAP_FBS) && sata_pmp_supported(ap)) {
		void __iomem *port_mmio = ahci_port_base(ap);
		u32 cmd = readl(port_mmio + PORT_CMD);
		debug("Port %d capable of FBS and PMP\n", ap->port_no);
		if (cmd & PORT_CMD_FBSCP) {
			pp->fbs_supported = true;
			debug("port %d can do FBS\n", ap->port_no);
		} else if (hpriv->flags & AHCI_HFLAG_YES_FBS) {
			debug("port %d can do FBS, forcing FBSCP\n",
			      ap->port_no);
			pp->fbs_supported = true;
		} else
			printf("port %d is not capable of FBS\n", ap->port_no);
	} else {
		debug("Host does not support FBS (cap: 0x%x, sata pmp supported: %d)\n",
		      hpriv->cap & HOST_CAP_FBS, sata_pmp_supported(ap));
	}

	if (pp->fbs_supported) {
		dma_sz = AHCI_PORT_PRIV_FBS_DMA_SZ;
		rx_fis_sz = AHCI_RX_FIS_SZ * 16;
	} else {
		dma_sz = AHCI_PORT_PRIV_DMA_SZ;
		rx_fis_sz = AHCI_RX_FIS_SZ;
	}
	debug("FBS%s supported, dma size: 0x%x, fis size: 0x%x\n",
	      pp->fbs_supported ? "" : " NOT", dma_sz, rx_fis_sz);

	mem = memalign(2048, dma_sz);
	if (!mem)
		return -ENOMEM;
	memset(mem, 0, dma_sz);
	mem_dma = virt_to_bus(hpriv->pdev, mem);

	/*
	 * First item in chunk of DMA memory: 32-slot command table,
	 * 32 bytes each in size
	 */
	pp->cmd_slot = mem;
	pp->cmd_slot_dma = mem_dma;

	mem += AHCI_CMD_SLOT_SZ;
	mem_dma += AHCI_CMD_SLOT_SZ;

	/*
	 * Second item: Received-FIS area
	 */
	pp->rx_fis = mem;
	pp->rx_fis_dma = mem_dma;

	mem += rx_fis_sz;
	mem_dma += rx_fis_sz;

	/*
	 * Third item: data area for storing a single command
	 * and its scatter-gather table
	 */
	debug("pp: 0x%p, pp->cmd_tbl: %p, pp->cmd_tbl_dma: 0x%llx\n",
	      pp, mem, mem_dma);
	pp->cmd_tbl = mem;
	pp->cmd_tbl_dma = mem_dma;

	/*
	 * Save off initial list of interrupts to be enabled.
	 * This could be changed later
	 */
	pp->intr_mask = DEF_PORT_IRQ;

	ap->private_data = pp;

	/* engage engines, captain */
	rc = ahci_port_resume(ap);
	debug("%s: EXIT %d\n", __func__, rc);
	return rc;
}

static void ahci_port_stop(struct ata_port *ap)
{
	const char *emsg = NULL;
	int rc;
	struct ahci_port_priv *pp = ap->private_data;

	debug("%s(0x%p(%d))\n", __func__, ap, ap->port_no);
	/* de-initialize port */
	rc = ahci_deinit_port(ap, &emsg);
	if (rc)
		printf("%s (%d)\n", emsg, rc);

	if (!pp)
		return;

	/* Free up the port data structures */
	free(pp->cmd_slot);
	pp->cmd_slot = NULL;
	pp->cmd_slot_dma = 0;
	pp->rx_fis = NULL;
	pp->rx_fis_dma = 0;
	pp->cmd_tbl = NULL;
	pp->cmd_tbl_dma = 0;
	ap->private_data = NULL;
	free(pp);
	debug("%s: EXIT\n", __func__);
}

void ahci_print_info(struct ata_host *host, const char *scc_s)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 vers, cap, cap2, impl, speed;
	const char *speed_s;

	vers = ahci_readl(mmio + HOST_VERSION);
	cap = hpriv->cap;
	cap2 = hpriv->cap2;
	impl = hpriv->port_map;

	speed = (cap >> 20) & 0xf;
	if (speed == 1)
		speed_s = "1.5";
	else if (speed == 2)
		speed_s = "3";
	else if (speed == 3)
		speed_s = "6";
	else
		speed_s = "?";

	printf("AHCI %02x%02x.%02x%02x "
	       "%u slots %u ports %s Gbps 0x%x impl %s mode\n"	,
	       (vers >> 24) & 0xff,
	       (vers >> 16) & 0xff,
	       (vers >> 8) & 0xff,
	       vers & 0xff,

	       ((cap >> 8) & 0x1f) + 1,
	       (cap & 0x1f) + 1,
	       speed_s,
	       impl,
	       scc_s);

	printf("flags: "
	       "%s%s%s%s%s%s%s"
	       "%s%s%s%s%s%s%s"
	       "%s%s%s%s%s%s\n",

		cap & HOST_CAP_64 ? "64bit " : "",
		cap & HOST_CAP_NCQ ? "ncq " : "",
		cap & HOST_CAP_SNTF ? "sntf " : "",
		cap & HOST_CAP_MPS ? "ilck " : "",
		cap & HOST_CAP_SSS ? "stag " : "",
		cap & HOST_CAP_ALPM ? "pm " : "",
		cap & HOST_CAP_LED ? "led " : "",
		cap & HOST_CAP_CLO ? "clo " : "",
		cap & HOST_CAP_ONLY ? "only " : "",
		cap & HOST_CAP_PMP ? "pmp " : "",
		cap & HOST_CAP_FBS ? "fbs " : "",
		cap & HOST_CAP_PIO_MULTI ? "pio " : "",
		cap & HOST_CAP_SSC ? "slum " : "",
		cap & HOST_CAP_PART ? "part " : "",
		cap & HOST_CAP_CCC ? "ccc " : "",
		cap & HOST_CAP_EMS ? "ems " : "",
		cap & HOST_CAP_SXS ? "sxs " : "",
		cap2 & HOST_CAP2_APST ? "apst " : "",
		cap2 & HOST_CAP2_NVMHCI ? "nvmp " : "",
		cap2 & HOST_CAP2_BOH ? "boh " : ""
		);
}

void ahci_set_em_messages(struct ahci_host_priv *hpriv,
			  struct ata_port_info *pi)
{
	u8 messages;
	void __iomem *mmio = hpriv->mmio;
	u32 em_loc = ahci_readl(mmio + HOST_EM_LOC);
	u32 em_ctl = ahci_readl(mmio + HOST_EM_CTL);

	if (!ahci_em_messages || !(hpriv->cap & HOST_CAP_EMS))
		return;

	messages = (em_ctl & EM_CTRL_MSG_TYPE) >> 16;

	if (messages) {
		hpriv->em_loc = ((em_loc >> 16) * 4);
		hpriv->em_buf_sz = ((em_loc & 0xff) * 4);
		hpriv->em_msg_type = messages;
		if (!(em_ctl & EM_CTL_ALHD))
			pi->flags |= ATA_FLAG_SW_ACTIVITY;
	}
}

static int ahci_host_init(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
#ifndef CONFIG_SATA_AHCI_PLAT
	pci_dev_t pdev = host->pdev;
	u16 tmp16;
	unsigned short vendor;
#endif
	volatile u8 *mmio = (volatile u8 *)hpriv->mmio;
	u32 tmp, cmd;
	int i, j;

	debug("%s(0x%p)\n", __func__, host);
#ifndef CONFIG_SATA_AHCI_PLAT
	debug("  dev=%d\n", pdev);
#endif

#if 0
	cap_save = ahci_readl(mmio + HOST_CAP);
	debug("%s: Initial host cap: 0x%x\n", __func__, cap_save);
	cap_save &= (HOST_CAP_MPS | HOST_CAP_PMP);	/* Disable PMP for now */
	cap_save |= HOST_CAP_SSS;
#endif
	/* global controller reset */
	tmp = ahci_readl(mmio + HOST_CTL);
	debug("%s: HOST_CTL: 0x%x\n", __func__, tmp);
#if 0
	for (i = 0; i < 5; i++) {
		if (!(tmp & HOST_AHCI_EN))
			tmp |= HOST_AHCI_EN;
		else
			break;
		ahci_writel(tmp, mmio + HOST_CTL);
		tmp = ahci_readl(mmio + HOST_CTL);
	}
#endif
	if ((tmp & HOST_RESET) == 0)
		ahci_writel_with_flush(tmp | HOST_RESET, mmio + HOST_CTL);

	/* reset must complete within 1 second, or
	 * the hardware should be considered fried.
	 */
	i = 1000;
	do {
		mdelay(1);
		tmp = ahci_readl(mmio + HOST_CTL);
		if (!i--) {
			debug("controller reset failed (0x%x)\n", tmp);
			return -1;
		}
	} while (tmp & HOST_RESET);
	debug("controller reset after %d ms.\n", 1000 - i);

	ahci_writel_with_flush(HOST_AHCI_EN, mmio + HOST_CTL);
/*	ahci_writel(cap_save, mmio + HOST_CAP);*/
/*	ahci_writel_with_flush(0xf, mmio + HOST_PORTS_IMPL);*/

#ifndef CONFIG_SATA_AHCI_PLAT
	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);

	if (vendor == PCI_VENDOR_ID_INTEL) {
		u16 tmp16;
		pci_read_config_word(pdev, 0x92, &tmp16);
		tmp16 |= 0xf;
		pci_write_config_word(pdev, 0x92, tmp16);
	}
#endif

	hpriv->cap = ahci_readl(mmio + HOST_CAP);
	hpriv->port_map = ahci_readl(mmio + HOST_PORTS_IMPL);
	host->n_ports = (hpriv->cap & 0x1f) + 1;

	debug("cap 0x%x  port_map 0x%x  n_ports %d\n",
	      hpriv->cap, hpriv->port_map, host->n_ports);

	for (i = 0; i < host->n_ports; i++) {
		volatile u8 *port_mmio;

		port_mmio = __ahci_port_base(host, i);

		/* Make sure port is not active */
		tmp = ahci_readl(port_mmio + PORT_CMD);
		if (tmp & (PORT_CMD_LIST_ON | PORT_CMD_FIS_ON |
			   PORT_CMD_FIS_RX | PORT_CMD_START)) {
			tmp &= ~(PORT_CMD_LIST_ON | PORT_CMD_FIS_ON |
				 PORT_CMD_FIS_RX | PORT_CMD_START);
			ahci_writel_with_flush(tmp, port_mmio + PORT_CMD);
			/* spec says 500 msecs for each bit,
			 * so this is slightly incorrect.
			 */
			mdelay(2000);
		}

		debug("Spinning up device on SATA port %d...\n", i);
		/* Add the spinup command to whatever mode bits may
		 * already be on in the command register.
		 */
		cmd = ahci_readl(port_mmio + PORT_CMD);
		cmd |= PORT_CMD_FIS_RX;
		cmd |= PORT_CMD_SPIN_UP;
		ahci_writel_with_flush(cmd, port_mmio + PORT_CMD);

		/* Bring up SATA link.
		 * SATA link bringup time is usually less than 1 ms; only very
		 * rarely has it taken between 1-2 ms.  Never seen it above 2 ms.
		 */
		j = 0;
		while (j < WAIT_MS_LINKUP) {
			tmp = ahci_readl(port_mmio + PORT_SCR_STAT);
			if ((tmp & 0xf) == 0x3)
				break;
			mdelay(1);
			j++;
		}
		debug("Target spinup took %d ms.\n", j);
		if (j == WAIT_MS_LINKUP) {
			printf("SATA link %d timeout.\n", i);
			continue;
		} else {
			debug("SATA link %d OK, %d ms.\n", i, j);
		}

		/* Clear error status */
		tmp = ahci_readl(port_mmio + PORT_SCR_ERR);
		if (tmp)
			ahci_writel(tmp, port_mmio + PORT_SCR_ERR);


		/* ack any pending irq events port this port */
		tmp = ahci_readl(port_mmio + PORT_IRQ_STAT);
		debug("PORT_IRQ_STAT 0x%x\n", tmp);
		if (tmp)
			ahci_writel(tmp, port_mmio + PORT_IRQ_STAT);

		ahci_writel(1 << i, mmio + HOST_IRQ_STAT);

		/* set irq mask (enables interrupts) */
		ahci_writel(0, port_mmio + PORT_IRQ_MASK);

		/* register linkup ports */
#ifdef DEBUG
		tmp = ahci_readl(port_mmio + PORT_SCR_STAT);
		debug("SATA port %d status: 0x%x\n", i, tmp);
#endif
	}

	tmp = ahci_readl(mmio + HOST_CTL);
	debug("HOST_CTL 0x%x\n", tmp);
	ahci_writel(tmp | HOST_IRQ_EN, mmio + HOST_CTL);
	tmp = ahci_readl(mmio + HOST_CTL);
	debug("HOST_CTL 0x%x\n", tmp);

#ifndef CONFIG_SATA_AHCI_PLAT
	pci_read_config_word(pdev, PCI_COMMAND, &tmp16);
	tmp |= PCI_COMMAND_MASTER;
	pci_write_config_word(pdev, PCI_COMMAND, tmp16);
#endif

	debug("%s: EXIT 0\n", __func__);
	return 0;
}


#ifndef CONFIG_SATA_AHCI_PLAT
static int ahci_init_one(pci_dev_t pdev)
{
	u16 vendor;
	int rc = -1;
	struct ata_host *host;
	struct ata_port *port;
	struct ata_link *link;
	struct ahci_host_priv *hpriv;
	static struct ata_port_info pi;
	const struct ata_port_info *ppi[] = { &pi, NULL };
	u32 mmio;
	int port_no, dev_no;
	int start_port_idx = last_port_idx;
	int host_idx = -1;
	int start_device_idx = last_device_idx;
	int n_ports;

	debug("%s(0x%x)\n", __func__, pdev);

	memset(&pi, 0, sizeof(pi));
	pi.flags = AHCI_FLAG_COMMON;
	pi.pio_mask = ATA_PIO4;
	pi.udma_mask = ATA_UDMA6;
	pi.port_ops = &ahci_ops;

	mmio = (u32)pci_map_bar(pdev, AHCI_PCI_BAR, PCI_REGION_MEM);
	debug("ahci mmio=0x%08x\n", mmio);
	/* Take from kernel:
	 * JMicron-specific fixup:
	 * make sure we're in AHCI mode
	 */
	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
	if (vendor == 0x197b) {
		u32 conf1, conf5;
		u16 id;

		pci_write_config_byte(pdev, 0x41, 0xa1);
		conf1 = pci_read_config_dword(pdev, 0x40, &conf1);
		conf5 = pci_read_config_dword(pdev, 0x80, &conf5);
		conf1 &= ~0x00CFF302;	/* Clear bit 1, 8, 9, 12-19, 22, 23 */
		conf5 &= ~(1 << 24);	/* Clear bit 24 */
		pci_read_config_word(pdev, PCI_DEVICE_ID, &id);
		switch (id) {
		case 0x2360:	/* Single SATA port */
		case 0x2362:	/* Dual SATA port */
			/* The controller should be in single function ahci mode */
			conf1 |= 0x002A100;	/* Set 8, 13, 15, 17 */
			break;
		case 0x2361:
		case 0x2363:
			/* Redirect IDE second PATA port to the right spot */
			conf1 |= 0x00C2A1B3;	/* Set 0, 1, 4, 5, 7, 8, 13, 15, 17, 22, 23 */
			break;
		}
		pci_write_config_dword(pdev, 0x40, conf1);
		pci_write_config_dword(pdev, 0x80, conf5);
	}

	hpriv = calloc(sizeof(*hpriv), 1);
	if (!hpriv)
		return -ENOMEM;
	hpriv->mmio = (void *)mmio;
	hpriv->flags = 0;	/* Don't bother with special boards for now */

	ahci_save_initial_config(hpriv, 0, 0);

	if (hpriv->cap & HOST_CAP_NCQ) {
		pi.flags |= ATA_FLAG_NCQ;
		/*
		 * Auto-activate optimization is supposed to be
		 * supported on all AHCI controllers indicating NCQ
		 * capability, but it seems to be broken on some
		 * chipsets including NVIDIAs.
		 */
		if (!(hpriv->flags & AHCI_HFLAG_NO_FPDMA_AA))
			pi.flags |= ATA_FLAG_FPDMA_AA;
	}
	if (hpriv->cap & HOST_CAP_PMP)
		pi.flags |= ATA_FLAG_PMP;

	ahci_set_em_messages(hpriv, &pi);

	n_ports = max(ahci_nr_ports(hpriv->cap), fls(hpriv->port_map));

	host = ata_host_alloc_pinfo(ppi, n_ports);
	if (!host)
		return -ENOMEM;
	host->private_data = hpriv;

	debug("%s: host->ports[0]: 0x%p\n", __func__, host->ports[0]);
	debug("%s 1: port->nr_pmp_links: %d\n", __func__,
	      host->ports[0]->nr_pmp_links);
	ahci_init_controller(host);
	debug("%s 2: port->nr_pmp_links: %d\n", __func__,
	      host->ports[0]->nr_pmp_links);
	ahci_print_info(host, "SATA");
	debug("%s 3: port->nr_pmp_links: %d\n", __func__,
	      host->ports[0]->nr_pmp_links);

	for (host_idx = 0; host_idx < CONFIG_SYS_SATA_MAX_DEVICE; host_idx++) {
		if (sata_host_desc[host_idx] == NULL) {
			if (host_idx + host->n_ports >= CONFIG_SYS_SATA_MAX_DEVICE) {
				printf("Too many SATA devices found, increase CONFIG_SYS_SATA_MAX_DEVICE (currently %d)\n",
				       CONFIG_SYS_SATA_MAX_DEVICE);
				goto err_out;
			}
			host->index = host_idx;
			sata_host_desc[host_idx] = host;
			break;
		}
	}
	debug("%s: last port index: %d, number of ports for host %d: %d\n",
	      __func__, last_port_idx, host_idx, host->n_ports);
	if (pi.flags & ATA_FLAG_EM)
		ahci_reset_em(host);


	for (port_no = 0; port_no < host->n_ports; port_no++) {
		port = host->ports[port_no];
		if (port->flags & ATA_FLAG_EM)
			port->em_message_type = hpriv->em_msg_type;

		sata_port_desc[last_port_idx++] = port;
		link = &port->link;
		debug("%s: port %d, last_device_idx: %d, nr_pmp_links: %d\n",
		      __func__, port_no, last_device_idx, port->nr_pmp_links);
		if (last_device_idx + 1 + port->nr_pmp_links >
		    CONFIG_SYS_SATA_MAX_DEVICE) {
			printf("Too many SATA devices found on port, increase CONFIG_SYS_SATA_MAX_DEVICE (currently %d)\n",
			       CONFIG_SYS_SATA_MAX_DEVICE);
			goto err_out;

		}
		ahci_stop_engine(link->ap);
		sata_device_desc[last_device_idx++] = &link->device[0];
		debug("SATA port %d has %d PMP links\n",
		      port->port_no, port->nr_pmp_links);
		for (dev_no = 0; dev_no < port->nr_pmp_links; dev_no++) {
			if (last_device_idx >= CONFIG_SYS_SATA_MAX_DEVICE) {
				puts("SATA port multiplier ports exceed CONFIG_SYS_SATA_MAX_DEVICE\n");
				break;
			}
			sata_device_desc[last_device_idx++] =
				&port->pmp_link[dev_no].device[0];
			debug("%s: Adding port %d pmp %d as device %d\n",
			      __func__, port_no, dev_no, last_device_idx - 1);
		}
	}

	rc = ata_host_register(host);
	DPRINTK("EXIT %d\n", rc);
	if (!rc)
		return 0;

err_out:
	if (host_idx >= 0)
		sata_host_desc[host_idx] = NULL;
	for (port_no = start_port_idx; port_no < last_port_idx; port_no++) {
		if (sata_port_desc[port_no]) {
			free(sata_port_desc[port_no]);
			sata_port_desc[port_no] = NULL;
		}
	}
	last_port_idx = start_port_idx;
	for (dev_no = start_device_idx; dev_no < last_device_idx; dev_no++) {
		sata_device_desc[dev_no] = NULL;
	}
	last_device_idx = start_device_idx;
	free(host);
	free(hpriv);

	debug("%s: EXIT %d\n", __func__, rc);
	return rc;
}
#endif

#ifdef CONFIG_AHCI_SETFEATURES_XFER
static int ahci_set_feature(struct ata_port *ap)
{
	volatile struct ahci_port_priv *pp = ap->private_data;
	volatile u8 *port_mmio = ahci_port_base(ap);
	struct ata_taskfile tf;
	int pmp;
	int port = ap->port_no;
	int rc;

	/*set feature */
	debug("%s(%d)\n", __func__, port);
	pmp = sata_srst_pmp(&ap->link);
	ata_tf_init(ap->link.device, &tf);
	tf.feature = SETFEATURES_XFER;
	tf.command = ATA_CMD_SETF;
	tf.flags = ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE | ATA_TFLAG_POLLING;
	tf.protocol = ATA_PROT_NODATA;

	switch (ap->udma_mask) {
	case ATA_UDMA7:
		tf.nsect = XFER_UDMA_7;
		debug("%s(%d), UDMA7\n", __func__, port);
		break;
	case ATA_UDMA6:
		tf.nsect = XFER_UDMA_6;
		debug("%s(%d), UDMA6\n", __func__, port);
		break;
	case ATA_UDMA5:
		tf.nsect = XFER_UDMA_5;
		debug("%s(%d), UDMA5\n", __func__, port);
		break;
	case ATA_UDMA4:
		tf.nsect = XFER_UDMA_4;
		debug("%s(%d), UDMA4\n", __func__, port);
		break;
	case ATA_UDMA3:
		tf.nsect = XFER_UDMA_3;
		debug("%s(%d), UDMA3\n", __func__, port);
		break;
	default:
		printf("%s: Unknown UDMA mask 0x%x for port %d\n",
		       __func__, ap->udma_mask, port);
	}

	debug("%s: exec polled command\n", __func__);
	rc = ahci_exec_polled_cmd(ap, pmp, &tf, 1, 0, 0);
	if (rc) {
		printf("%s port %d setfeature failed\n", __func__, port);
		printf("set feature error (port %d)!\n", port);
		debug("port interrupt status: 0x%x, cmd issue: 0x%x\n",
		      ahci_readl(port_mmio + PORT_IRQ_STAT),
		      ahci_readl(port_mmio + PORT_CMD_ISSUE));
		debug("phy error: 0x%x\n",
		      ahci_readl(port_mmio + PORT_SCR_ERR));
		debug("udma mask: 0x%x\n", ap->udma_mask);
		ahci_sata_dump_fis((struct sata_fis_d2h *)pp->cmd_tbl);
		debug("%s: EXIT -1\n", __func__);
		return -1;
	}
	debug("%s: EXIT 0\n", __func__);
	return 0;
}
#endif

static int ahci_exec_internal(struct ata_device *dev, struct ata_taskfile *tf,
			      const u8 *cdb, int dma_dir,
			      void *buf, unsigned int buflen,
			      unsigned long timeout)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	volatile u8 *port_mmio = ahci_port_base(ap);
	unsigned int n_elem = 0;
	int is_atapi = !!(tf->protocol & ATA_PROT_FLAG_ATAPI);
	u32 port_status;
	void *cmd_tbl;
	u8 *rx_fis = pp->rx_fis;
	u32 opts;
	const u32 cmd_fis_len = 5;
	u32 tmp;

	debug("%s(0x%p, 0x%p, 0x%p, %d, 0x%p, 0x%x, %lu)\n",
	      __func__, dev, tf, cdb, dma_dir, buf, buflen, timeout);

#if 1
	port_status = ahci_readl(port_mmio + PORT_SCR_STAT);

	if ((port_status & 0xf) != 0x3) {
		debug("No link on port %d, status: 0x%x\n", ap->port_no,
		      port_status);
		return -1;
	}
#endif
	cmd_tbl = pp->cmd_tbl;

	ata_tf_to_fis(tf, link->pmp, 1, cmd_tbl);
	if (is_atapi) {
		if (!cdb) {
			printf("Error: ATAPI requires cdb be set!\n");
			return -1;
		}
		debug("Clearing and setting command table\n");
		memset(cmd_tbl + AHCI_CMD_TBL_CDB, 0, 32);
		memcpy(cmd_tbl + AHCI_CMD_TBL_CDB, cdb, ATAPI_CDB_LEN);
	}

	/* some SATA bridges need us to indicate data xfer direction */
	if (tf->protocol == ATAPI_PROT_DMA && (dev->flags & ATA_DFLAG_DMADIR) &&
	    dma_dir == DMA_FROM_DEVICE)
		tf->feature |= ATAPI_DMADIR;

	if (dma_dir != DMA_NONE) {
		n_elem = ahci_fill_sg(ap, buf, buflen);
		if (dma_dir == DMA_TO_DEVICE || dma_dir == DMA_BIDIRECTIONAL)
			flush_dcache_range((ulong)buf,
					   (ulong)((uint8_t *)buf + buflen));
	}

	opts = cmd_fis_len | (link->pmp << 12) | (n_elem << 16);

	if (tf->flags & ATA_TFLAG_WRITE)
		opts |= AHCI_CMD_WRITE;
	if (is_atapi)
		opts |= AHCI_CMD_ATAPI | AHCI_CMD_PREFETCH;

	ahci_fill_cmd_slot(pp, 0, opts);

	if (pp->fbs_enabled && pp->fbs_last_dev != link->pmp) {
		u32 fbs;
		fbs = ahci_readl(port_mmio + PORT_FBS);
		debug("%s: FBS enabled (0x%x) and last dev not PMP\n",
		      __func__, fbs);
		fbs &= ~(PORT_FBS_DEV_MASK | PORT_FBS_DEC);
		fbs |= link->pmp << PORT_FBS_DEV_OFFSET;
		ahci_writel(fbs, port_mmio + PORT_FBS);
		pp->fbs_last_dev = link->pmp;
	}
#ifdef DEBUG
	print_buffer((ulong)pp->cmd_tbl, pp->cmd_tbl, 1, 4 * cmd_fis_len, 0);
#endif
	ahci_writel(1, port_mmio + PORT_CMD_ISSUE);

	tmp = ata_wait_register((u8 *)port_mmio + PORT_CMD_ISSUE, 1, 1, timeout);
	if (tmp & 1) {
		ahci_kick_engine(ap);
		printf("SATA port %u timed out issuing command\n", ap->port_no);
		return -EBUSY;
	}
	if (dma_dir == DMA_FROM_DEVICE || dma_dir == DMA_BIDIRECTIONAL)
		invalidate_dcache_range((ulong)buf, (ulong)buf + buflen);
	if (tf->protocol == ATA_PROT_PIO && dma_dir == DMA_FROM_DEVICE) {
		ata_tf_from_fis(rx_fis + RX_FIS_PIO_SETUP, tf);
		tf->command = (rx_fis + RX_FIS_PIO_SETUP)[15];
	} else {
		ata_tf_from_fis(rx_fis + RX_FIS_D2H_REG, tf);
	}

	debug("%s: EXIT 0\n", __func__);
	return 0;
}

#ifdef CONFIG_SATA_PMP
int ata_exec_internal(struct ata_device *dev, struct ata_taskfile *tf,
		      const u8 *cdb, int dma_dir,
		      void *buf, unsigned int buflen,
		      unsigned long timeout)
{
	return ahci_exec_internal(dev, tf, cdb, dma_dir, buf,
				  buflen, timeout);
}
#endif

/*
 * SATA read/write command operation.
 */
static int ahci_sata_rw_cmd(struct ata_device *dev,
			    lbaint_t start, lbaint_t blkcnt,
			    u8 *buffer, int is_write, int is_lba48)
{
	struct ata_taskfile tf;
	u32 flags = 0;
	int rc;

	debug("%s(0x%p, 0x%llx, 0x%llx, 0x%p, %d, %d)\n", __func__, dev, start,
	      blkcnt, buffer, is_write, is_lba48);
	/* For 10-byte and 16-byte SCSI R/W commands, transfer
	 * length 0 means transfer 0 block of data.
	 * However, for ATA R/W commands, sector count 0 means
	 * 256 or 65536 sectors, not 0 sectors as in SCSI.
	 *
	 * WARNING: one or two older ATA drives treat 0 as 0...
	 */
	if (!blkcnt)
		return 0;

	ata_tf_init(dev, &tf);
	if (is_write)
		flags |= ATA_TFLAG_WRITE;
	debug("Building rw taskfile\n");
	if (ata_build_rw_tf(&tf, dev, start, blkcnt, flags, 0)) {
		printf("Error building rw taskfile\n");
		return -1;
	}
	rc = ahci_exec_internal(dev, &tf, NULL,
				is_write ? DMA_TO_DEVICE : DMA_FROM_DEVICE,
				buffer, blkcnt * ATA_SECT_SIZE,
				5000 + 10 * blkcnt);
	debug("%s: EXIT %d\n", __func__, rc);
	return rc;
}

lbaint_t ahci_sata_rw(int dev, lbaint_t blknr, lbaint_t blkcnt,
		      void *buffer, int is_write)
{
	lbaint_t start, blks, max_blks;
	u8 *addr;
	int is_lba48;
	struct ata_device *ahci_dev = sata_device_desc[dev];
	u32 num_blks;

	start = blknr;
	blks = blkcnt;
	addr = (u8 *)buffer;

	if (dev >= CONFIG_SYS_SATA_MAX_DEVICE || !ahci_dev || !ahci_dev->link) {
		printf("SATA device %d not initialized\n", dev);
		return 0;
	}
	is_lba48 = ahci_dev->flags & ATA_DFLAG_LBA48;

	if (is_lba48)
		max_blks = ATA_MAX_SECTORS_LBA48;
	else
		max_blks = ATA_MAX_SECTORS;
	do {
		num_blks = min(max_blks, blks);

		if (ahci_sata_rw_cmd(ahci_dev, start, num_blks, addr,
				     is_write, is_lba48))
			return blkcnt - blks;

		start += num_blks;
		blks -= num_blks;
		addr += ATA_SECT_SIZE * num_blks;
	} while (blks != 0);

	debug("%s: %s %llx blocks\n", __func__, is_write ? "wrote" : "read",
	      (u64)blkcnt);
	return blkcnt;
}

static lbaint_t ahci_sata_read(int dev, lbaint_t blknr, lbaint_t blkcnt,
			       void *buffer)
{
	return ahci_sata_rw(dev, blknr, blkcnt, buffer, 0);
}

/**
 * Redefinition of ahci_sata_read to make linker happy for hard-coded sata_read
 * function.
 */
lbaint_t sata_read(int dev, lbaint_t blknr, lbaint_t blkcnt, void *buffer)
{
	return ahci_sata_read(dev, blknr, blkcnt, buffer);
}

lbaint_t sata_read(int dev, lbaint_t blknr, lbaint_t blkcnt, void *buffer)
	__attribute__((weak));

#if 0
static lbaint_t ahci_sata_write(int dev, lbaint_t blknr, lbaint_t blkcnt,
				const void *buffer)
{
	return ahci_sata_rw(dev, blknr, blkcnt, (void *)buffer, 1);
}
#endif
/**
 * Redefinition of ahci_sata_write to make linker happy for hard-coded
 * sata_write function.
 */
lbaint_t sata_write(int dev, lbaint_t blknr, lbaint_t blkcnt,
		    const void *buffer)
{
	return 0;
}

lbaint_t sata_write(int dev, lbaint_t blknr, lbaint_t blkcnt,
		    const void *buffer)
	__attribute__((weak));

static int ahci_identify_device(struct ata_device *dev, unsigned int *p_class,
				unsigned int flags, u16 *id)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_taskfile tf;
	unsigned int class = *p_class;
	int rc;

	debug("%s(%u, 0x%p)\n", __func__, ap->port_no, id);

	memset(&tf, 0, sizeof(tf));
	ata_tf_init(dev, &tf);

	switch(class) {
	case ATA_DEV_SEMB:
		class =  ATA_DEV_ATA;
	case ATA_DEV_ATA:
		tf.command = ATA_CMD_ID_ATA;
		break;
	case ATA_DEV_ATAPI:
		tf.command = ATA_CMD_ID_ATAPI;
		break;
	default:
		printf("%s: Unsupported class 0x%x\n", __func__, class);
		return -1;
	}

	tf.protocol = ATA_PROT_PIO;

	/* Some devices choke if TF registers contain garbage.  Make
	 * sure those are properly initialized.
	 */
	tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;

	/* Device presence detection is unreliable on some
	 * controllers.  Always poll IDENTIFY if available.
	 */
	tf.flags |= ATA_TFLAG_POLLING;


	rc = ahci_exec_internal(dev, &tf, NULL, DMA_FROM_DEVICE, id,
				sizeof(id[0]) * ATA_ID_WORDS, 5000);

	swap_buf_le16((u16 *)id, ATA_ID_WORDS);
#ifdef DEBUG
	debug("SATA port %d id (0x%p):\n", ap->port_no, id);
	print_buffer(0, id, 2,
		     sizeof(struct hd_driveid) / 2, 0);
#endif
	debug("%s: EXIT %d\n", __func__, rc);
	return rc;
}

int init_sata(int dev)
{
#ifndef CONFIG_SATA_AHCI_PLAT
	pci_dev_t pcidev;
#endif
	int rc = 0;
	static int initted = 0;
	int host_num = 0, port_num = 0;

	debug("%s(%d) entry\n", __func__, dev);
	sata_dev_desc[dev].type = DEV_TYPE_UNKNOWN;
	if (initted) {
		debug("%s(%d) already initialized\n", __func__, dev);
		if (dev < CONFIG_SYS_SATA_MAX_DEVICE &&
		    sata_device_desc[dev] != NULL)
			return 0;
		else
			return -1;
	}

#ifndef CONFIG_SATA_AHCI_PLAT
	do {
		pcidev = pci_find_class(1 /* storage */,
					6 /* SATA */,
					1 /* AHCI */,
					host_num++);

		if (pcidev == -1)
			break;

		debug("%s: Calling ahci_init_one(0x%x)\n", __func__, pcidev);
		rc = ahci_init_one(pcidev);
		if (rc) {
			debug("ahci_init_one(0x%x) returned %d\n", pcidev, rc);
			return rc;
		}
	} while (pcidev >= 0 && !rc);
#else
	rc = ahci_init_platform();
	if (rc) {
		debug("ahci_init_plat() returned %d\n", rc);
		return rc;
	}
#endif

	/* Now that we've initialized all of the hosts it's time to register
	 * them.  In Linux this triggers an asynchronous port probe but
	 * we  have to do it synchronously.
	 */

	debug("%s: last_port_idx: %d\n", __func__, last_port_idx);
	for (port_num = 0; port_num < last_port_idx; port_num++) {
		struct ata_port *ap = sata_port_desc[port_num];
		if (!ap) {
			debug("Port id %d does not exist, skipping\n",
			      port_num);
			continue;
		}

		/* set SATA cable type if still unset */
		if (ap->cbl == ATA_CBL_NONE && (ap->flags & ATA_FLAG_SATA))
			ap->cbl = ATA_CBL_SATA;

		/* init sata_spd_limit to the current value */
		sata_link_init_spd(&(ap->link));
		if (ap->slave_link)
			sata_link_init_spd(ap->slave_link);

		debug("Starting port %d\n", ap->port_no);
		if (ahci_port_start(ap)) {
			printf("Can not start port %d\n", ap->port_no);
			continue;
		}
#ifdef CONFIG_AHCI_SETFEATURES_XFER
		ahci_set_feature(ap);
#endif
	}

	initted = 1;
	debug("%s(%d) finished successfully\n", __func__, dev);

	return 0;
}

static int sata_reset(struct ata_link *link)
{
	return 0;
}

static int sata_recover(struct ata_port *ap, ata_prereset_fn_t prereset,
			ata_reset_fn_t softreset, ata_reset_fn_t hardreset,
			ata_postreset_fn_t postreset,
			struct ata_link **r_failed_link)
{
	struct ata_link *link;
	struct ata_device *dev;
	int rc;
	unsigned long flags, deadline;
	ulong start;

	debug("%s: ENTER port %d\n", __func__, ap->port_no);

	rc = 0;

	/* reset */
	ata_for_each_link(link, ap, EDGE) {
		rc = sata_reset(link);
		if (rc) {
			printf("Link reset failed on port %d, pmp %d\n",
			       ap->port_no, link->pmp);
			goto out;
		}
	}

	start = get_timer(0);
	do {
		ata_for_each_link(link, ap, EDGE) {
			ata_for_each_dev(dev, link, ALL) {

			}
		}
	} while (get_timer(start) < deadline);

	ata_for_each_link(link, ap, PMP_FIRST) {
		/* revalidate existing devices and attach new ones */
		rc = ata_eh_revalidate_and_attach(link, &dev);
		if (rc)
			break;

		/* if PMP got attached, return */
		if (link->device->class == ATA_DEV_PMP)
			return 0;

	}
out:
	return rc;
}

static int ahci_add_pmp_attached_dev(struct ata_link *link)
{
	int i;
	debug("%s(%p)\n", __func__, link);
	for (i = 0; i < CONFIG_SYS_SATA_MAX_DEVICE; i++)
		if (sata_pmp_link[i] == NULL) {
			debug("%s: Adding link %p to sata_pmp_link[%d]\n",
			      __func__, link, i);
			sata_pmp_link[i] = link;
			return 0;
		}

	return -1;
}

static int sata_pmp_connect(struct ata_port *ap)
{
	struct ata_port_operations *ops = ap->ops;
	struct ata_link *pmp_link = &ap->link;
	struct ata_device *pmp_dev = pmp_link->device;
	struct ata_link *link;
	struct ata_device *dev;
	int rc = 0;

	DPRINTK("ENTRY port: %d\n", ap->port_no);

	/* PMP attached? */
	if (!sata_pmp_attached(ap))  {
		DPRINTK("Recovering non-PMP\n");
		rc = sata_recover(ap, ops->prereset, ops->softreset,
				 ops->hardreset, ops->postreset, NULL);
		if (rc)  {
			DPRINTK("sata_recovery failed, disabling all devices\n");
			ata_for_each_dev(dev, &ap->link, ALL)
				ata_dev_disable(dev);
			return rc;
		}

		if (pmp_dev->class != ATA_DEV_PMP)
			return 0;

		/* new PMP online */
		DPRINTK("New PMP online\n");
		/* fall through */

	/* recover pmp */
	}
	return rc;
}

int scan_sata(int dev)
{
	int rc;
	u16 *id;
	unsigned char serial[ATA_ID_SERNO_LEN + 1];
	unsigned char firmware[ATA_ID_FW_REV_LEN + 1];
	unsigned char product[ATA_ID_PROD_LEN + 1];
	u32 class = ATA_DEV_ATA;
	struct ata_device *ahci_dev;
	struct ata_port *ap;
	void __iomem *port_mmio;
	u32 port_cmd;
	int pmp;

	debug("%s(%d)\n", __func__, dev);

	debug("last_port_idx: %d\n", last_port_idx);
	if (!last_port_idx)
		return 1;
	if (dev > last_port_idx || !sata_device_desc[dev]) {
		printf("SATA#%d is not present\n", dev);
		return 1;
	}
	ahci_dev = sata_device_desc[dev];
	pmp = sata_srst_pmp(ahci_dev->link);
	debug("  pmp: 0x%x\n", pmp);
	ap = ahci_dev->link->ap;
	ahci_stop_engine(ap);
	ahci_port_resume(ap);
#if 0
	ahci_power_up(ap);
	mdelay(1000);
	ahci_start_engine(ap);
	mdelay(1000);
#endif
	port_mmio = ahci_port_base(ap);
	debug("%s: port %d mmio: 0x%p\n", __func__, dev, port_mmio);
	if (!((port_cmd = ahci_readl(port_mmio + PORT_CMD)) & PORT_CMD_CPS)) {
		debug("Port %d disabled, port cmd: 0x%x\n", ap->port_no,
		      port_cmd);
		return 1;
	}

	printf("SATA#%d\n", dev);
	debug("%s: pmp: 0x%x\n", __func__, pmp);
	rc = ahci_port_reset(ap);
	rc = ahci_softreset(ahci_dev->link, &class, 5000);
	if (!rc) {
		debug("%s: Setting device %p class to %d\n", __func__, ahci_dev, class);
		ahci_dev->class = class;
		if (class == ATA_DEV_PMP) {
			debug("%s: Attaching PMP\n", __func__);
			debug("  dev: %d, port: %d\n", dev, ap->port_no);
 			rc = sata_pmp_attach(ahci_dev);
			if (rc) {
				printf("%s: dev: %d, Error attaching PMP\n",
				       __func__, dev);
			}
			return rc;
		}
		debug("%s: class: %d\n", __func__, class);
	} else {
		debug("%s: class: %d, rc: %d\n", __func__, class, rc);
		return rc;
	}
	id = ahci_dev->id;
	rc = ahci_identify_device(ahci_dev, &class, 0, id);
	if (rc) {
		debug("%s(%d) identify device returned %d\n", __func__, dev, rc);
		return rc;
	}

	ahci_dev->n_sectors = ata_id_n_sectors(id);
	ahci_dev->n_native_sectors = ata_id_n_sectors(id);

	if (ata_id_has_lba(id)) {
		ahci_dev->flags |= ATA_DFLAG_LBA;
#ifdef CONFIG_LBA48
		if (ata_id_has_lba48(id)) {
			ahci_dev->flags |= ATA_DFLAG_LBA48;
			ahci_dev->max_sectors = ATA_MAX_SECTORS_LBA48;
			sata_dev_desc[dev].lba48 = 1;
			if (ahci_dev->n_sectors >= (1UL << 28) &&
			    ata_id_has_flush_ext(id))
				ahci_dev->flags |= ATA_DFLAG_FLUSH_EXT;
		} else
#endif
			ahci_dev->max_sectors = ATA_MAX_SECTORS;
	}

	/* Serial number */
	ata_id_c_string(id, serial, ATA_ID_SERNO, sizeof(serial));
	strncpy(sata_dev_desc[dev].product, (char *)serial,
		sizeof(sata_dev_desc[dev].product));

	/* Firmware version */
	ata_id_c_string(id, firmware, ATA_ID_FW_REV, sizeof(firmware));
	strncpy(sata_dev_desc[dev].revision, (char *)firmware,
		sizeof(sata_dev_desc[dev].revision));

	/* Product model */
	ata_id_c_string(id, product, ATA_ID_PROD, sizeof(product));
	strncpy(sata_dev_desc[dev].vendor, (char *)product,
		sizeof(sata_dev_desc[dev].vendor));
	/* Total sectors */
	sata_dev_desc[dev].lba = ata_id_n_sectors(id);
	/* Assume hard drive */
	sata_dev_desc[dev].type = DEV_TYPE_HARDDISK;
	sata_dev_desc[dev].blksz = ATA_BLOCKSIZE;
	sata_dev_desc[dev].lun = 0;
	sata_dev_desc[dev].priv = ahci_dev;
#ifdef DEBUG
	ata_dump_id(id);
#endif


#ifdef CONFIG_AHCI_SETFEATURES_XFER
	ahci_set_feature(ahci_dev->link->ap);
#endif

	debug("%s: EXIT 0\n", __func__);
	return 0;
}

int scan_sata_pmp(int link_nr, int dev, int *num_devices)
{
	int rc;
	u16 *id = NULL;	/* Make compiler happy */
	unsigned char serial[ATA_ID_SERNO_LEN + 1];
	unsigned char firmware[ATA_ID_FW_REV_LEN + 1];
	unsigned char product[ATA_ID_PROD_LEN + 1];
	u32 class = ATA_DEV_ATA;
	struct ata_device *ahci_dev;
	struct ata_port *ap;
	struct ata_link *link;
	struct ata_device *pmp_dev;
	void __iomem *port_mmio;
	u32 port_cmd;
	int pmp;
	int nr_pmp_links;
	int pmp_port;

	debug("%s(%d, %d, %p)\n", __func__, link_nr, dev, num_devices);

	debug("last_port_idx: %d\n", last_port_idx);
#if 0
	if (dev > last_port_idx || !sata_device_desc[dev]) {
		printf("SATA#%d is not present\n", dev);
		return 1;
	}
#endif
	link = sata_pmp_link[link_nr];
	if (!link) {
		debug("%s: no link\n", __func__);
		return 1;
	}
	if (sata_port_desc[dev] == NULL) {
		debug("%s: No dev\n", __func__);
		return 1;
	}

	ahci_dev = link->device;
	pmp = sata_srst_pmp(link);
	ap = link->ap;
	nr_pmp_links = ap->nr_pmp_links;
	debug("  pmp: 0x%x, port: %d, num pmp links: %d\n",
	      pmp, ap->port_no, nr_pmp_links);
	debug("%s: link: %p, dev: %p, ap: %p, ap->link: %p, dev->link: %p, ap->pmp_link: %p\n",
	      __func__, link, ahci_dev, ap, &(ap->link), ahci_dev->link,
	      ap->pmp_link);
	debug("%s: nr_pmp_links: %d, ap: %p\n", __func__, ap->nr_pmp_links, ap);
	ahci_stop_engine(ap);
	ahci_port_resume(ap);

#if 0
	ahci_power_up(ap);
	mdelay(1000);
	ahci_start_engine(ap);
	mdelay(1000);
#endif
	port_mmio = ahci_port_base(ap);
	debug("%s: port %d mmio: 0x%p\n", __func__, ap->port_no, port_mmio);
	if (!((port_cmd = ahci_readl(port_mmio + PORT_CMD)) & PORT_CMD_CPS)) {
		debug("Port %d disabled, port cmd: 0x%x\n", ap->port_no,
		      port_cmd);
		return 1;
	}

	rc = ahci_softreset(&ap->link, &class, 5000);
	debug("%s: ahci_softreset rc: %d, class: %d\n", __func__, rc, class);
	printf("SATA#%d\n", ap->port_no);
	debug("%s: pmp: 0x%x, class: %d\n", __func__, pmp,
	      link->device[0].class);
	if (!nr_pmp_links) {
		debug("%s: port %d has no PMP ports\n", __func__, ap->port_no);
		return 0;
	}
#if 0
	rc = ahci_port_reset(ap);
#endif
	for (pmp_port = 0; pmp_port < nr_pmp_links; pmp_port++) {
		debug("%s: dev: %d, Resetting pmp port %d\n", __func__,
		      dev, pmp_port);
		rc = ahci_softreset(&ap->pmp_link[pmp_port], &class, 5000);
		if (!rc) {
			if (class == ATA_DEV_PMP) {
				printf("%s(%d) ERROR: Should not see PMP!\n",
			               __func__, link_nr);
				return -1;
			}
			debug("%s: class: %d\n", __func__, class);
		} else {
			debug("%s: pmp link %d returned %d\n", __func__,
			      pmp_port, rc);
			continue;
		}
		pmp_dev = ap->pmp_link[pmp_port].device;
		id = pmp_dev->id;
		rc = ahci_identify_device(pmp_dev, &class, 0, id);
		if (rc) {
			debug("%s: pmp link %d identify device returned %d\n",
			      __func__, pmp_port, rc);
			continue;
		}
		pmp_dev->class = class;
		pmp_dev->n_sectors = ata_id_n_sectors(id);
		pmp_dev->n_native_sectors = ata_id_n_sectors(id);
		if (ata_id_has_lba(id)) {
			pmp_dev->flags |= ATA_DFLAG_LBA;
#ifdef CONFIG_LBA48
			if (ata_id_has_lba48(id)) {
				pmp_dev->flags |= ATA_DFLAG_LBA48;
				pmp_dev->max_sectors = ATA_MAX_SECTORS_LBA48;

			}
#endif
		}
	}

	if (ata_id_has_lba(id)) {
		ahci_dev->flags |= ATA_DFLAG_LBA;
#ifdef CONFIG_LBA48
		if (ata_id_has_lba48(id)) {
			ahci_dev->flags |= ATA_DFLAG_LBA48;
			ahci_dev->max_sectors = ATA_MAX_SECTORS_LBA48;
			sata_dev_desc[dev].lba48 = 1;
			if (ahci_dev->n_sectors >= (1UL << 28) &&
			    ata_id_has_flush_ext(id))
				ahci_dev->flags |= ATA_DFLAG_FLUSH_EXT;
		} else
#endif
			ahci_dev->max_sectors = ATA_MAX_SECTORS;
	}

	/* Serial number */
	ata_id_c_string(id, serial, ATA_ID_SERNO, sizeof(serial));
	strncpy(sata_dev_desc[link_nr].product, (char *)serial,
		sizeof(sata_dev_desc[dev].product));

	/* Firmware version */
	ata_id_c_string(id, firmware, ATA_ID_FW_REV, sizeof(firmware));
	strncpy(sata_dev_desc[dev].revision, (char *)firmware,
		sizeof(sata_dev_desc[dev].revision));

	/* Product model */
	ata_id_c_string(id, product, ATA_ID_PROD, sizeof(product));
	strncpy(sata_dev_desc[dev].vendor, (char *)product,
		sizeof(sata_dev_desc[dev].vendor));
	/* Total sectors */
	sata_dev_desc[dev].lba = ata_id_n_sectors(id);
	/* Assume hard drive */
	sata_dev_desc[dev].type = DEV_TYPE_HARDDISK;
	sata_dev_desc[dev].blksz = ATA_BLOCKSIZE;
	sata_dev_desc[dev].lun = 0;
	sata_dev_desc[dev].priv = ahci_dev;
#ifdef DEBUG
	ata_dump_id(id);
#endif


#ifdef CONFIG_AHCI_SETFEATURES_XFER
	ahci_set_feature(ahci_dev->link->ap);
#endif
	debug("%s: EXIT 0\n", __func__);

	return 0;
}

int scan_sata_devices(void)
{
	int rc = 0;
	int dev = 0;
	int pmp_dev;
	int num_devices = 0;

	debug("%s()\n", __func__);

	for (dev = 0; dev < CONFIG_SYS_SATA_MAX_DEVICE; dev++) {
		debug("%s: scanning dev: %d\n", __func__, dev);
		rc = scan_sata(dev);
		debug("%s: dev: %d scan returned %d\n", __func__, dev, rc);
	}

	for (dev = 0; dev < CONFIG_SYS_SATA_MAX_DEVICE; dev++) {
		if (sata_dev_desc[dev].lba <= 0)
			break;
	}
	if (dev == CONFIG_SYS_SATA_MAX_DEVICE) {
		printf("Out of SATA devices, cannot scan for PMP devices.\n"
		       "Consider increasing CONFIG_SYS_SATA_MAX_DEVICE from %d\n",
		       CONFIG_SYS_SATA_MAX_DEVICE);
	} else {
		for (pmp_dev = 0; pmp_dev < CONFIG_SYS_SATA_MAX_DEVICE;
		     pmp_dev++) {
			if (!sata_pmp_link[pmp_dev]) {
				debug("Skipping pmp dev %d\n", pmp_dev);
				continue;
			}
			debug("%s: scanning dev: %d, pmp_dev: %d\n",
			      __func__, dev, pmp_dev);
			num_devices = 0;
			rc = scan_sata_pmp(pmp_dev, dev, &num_devices);
			debug("%s: pmp_dev: %d scan returned %d, %d devices\n",
			      __func__, pmp_dev, rc, num_devices);
			if (!rc && num_devices) {
				debug("%s: Adding %d new PMP attached devices\n",
				      __func__, num_devices);
				dev += num_devices;
			}
		}
	}

	for (dev = 0; dev < CONFIG_SYS_SATA_MAX_DEVICE; dev++) {
		if (sata_dev_desc[dev].lba > 0) {
			debug("%s: Initializing device %d\n", __func__, dev);
			init_part(&sata_dev_desc[dev]);
		}
	}

	debug("%s: EXIT 0\n", __func__);
	return 0;
}
