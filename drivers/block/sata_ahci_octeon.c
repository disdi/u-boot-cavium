/*
 * Copyright (C) Cavium, Inc. 2014
 * Author: Aaron Williams <Aaron.Williams@cavium.com>
 * 	Updated to follow SATA drivers
 *
 * SATA glue for Cavium Octeon III SOCs
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */
#include <common.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/arch/cvmx-access.h>
#include <asm/arch/cvmx-sata-defs.h>
#include <libata.h>
#include <ata.h>
#include <linux/ctype.h>
#include <ahci.h>

#define AHCI_FAKE_ADDR		0xC8000000
#define CVMX_SATA_UAHC_GBL_BASE	CVMX_SATA_UAHC_GBL_CAP

extern block_dev_desc_t sata_dev_desc[CONFIG_SYS_SATA_MAX_DEVICE];

extern struct ata_port *sata_port_desc[CONFIG_SYS_SATA_MAX_DEVICE];
extern struct ata_host *sata_host_desc[CONFIG_SYS_SATA_MAX_DEVICE];
extern struct ata_device *sata_device_desc[CONFIG_SYS_SATA_MAX_DEVICE];
extern hd_driveid_t *ataid[AHCI_MAX_PORTS];
extern struct ata_port_operations ahci_ops;
extern struct ahci_probe_ent *probe_ent;


static unsigned int last_host_idx = 0;
static unsigned int last_port_idx = 0;
static unsigned int last_device_idx = 0;

extern void ahci_save_initial_config(struct ahci_host_priv *hpriv,
				     unsigned int force_port_map,
				     unsigned int mask_port_map);

void ahci_writel(u32 value, volatile void *addr)
{
	u64 addr64 = ((u32)addr - AHCI_FAKE_ADDR) + CVMX_SATA_UAHC_GBL_BASE;
	debug("%s(0x%x, 0x%p (0x%llx))\n", __func__, value, addr, addr64);
	cvmx_write64_uint32(addr64, value);
}

u32 ahci_readl(volatile const void *addr)
{
	u64 addr64 = ((u32)addr - AHCI_FAKE_ADDR) + CVMX_SATA_UAHC_GBL_BASE;
	debug("%s(0x%p (0x%llx))\n", __func__, addr, addr64);
	return cvmx_read64_uint32(addr64);
}

void ata_readl(u32 addr) __attribute__((alias("ahci_readl")));
void ata_writel(u32 val, u32 addr) __attribute__((alias("ahci_writel")));

int ahci_octeon_config(void)
{
	cvmx_sata_uctl_shim_cfg_t shim_cfg;

	/* Set up endian mode */
	shim_cfg.u64 = cvmx_read_csr(CVMX_SATA_UCTL_SHIM_CFG);
#ifdef __BIG_ENDIAN
	shim_cfg.s.dma_endian_mode = 1;
	shim_cfg.s.csr_endian_mode = 1;
#else
	shim_cfg.s.dma_endian_mode = 0;
	shim_cfg.s.csr_endian_mode = 0;
#endif
	shim_cfg.s.dma_read_cmd = 1; /* No allocate L2C */
	cvmx_write_csr(CVMX_SATA_UCTL_SHIM_CFG, shim_cfg.u64);

	return 0;
}

int ahci_init_platform(void)
{
	int rc = -1;
	struct ata_port *port = NULL;
	struct ata_link *link = NULL;
	struct ahci_host_priv *hpriv;
	struct ata_host *host = NULL;
	static struct ata_port_info pi;
	const struct ata_port_info *ppi[] = { &pi, NULL };
	u32 mmio;
	int port_no, dev_no;
	int start_port_idx = last_port_idx;
	int start_host_idx = last_host_idx;
	int start_device_idx = last_device_idx;
	struct ahci_platform_data *pdata;
	int n_ports;

	debug("%s() ENTER\n", __func__);

	ahci_octeon_config();

	memset(&pi, 0, sizeof(pi));

	pi.flags = AHCI_FLAG_COMMON;
	pi.pio_mask = ATA_PIO4;
	pi.udma_mask = ATA_UDMA7;
	pi.port_ops = &ahci_ops;

	hpriv = calloc(sizeof(*hpriv), 1);
	if (!hpriv)
		return -ENOMEM;

	hpriv->mmio = AHCI_FAKE_ADDR;
	hpriv->flags = 0;

	ahci_save_initial_config(hpriv, 0, 0);
	if (hpriv->cap & HOST_CAP_NCQ)
		pi.flags |= ATA_FLAG_NCQ;

	if (hpriv->cap & HOST_CAP_PMP)
		pi.flags |= ATA_FLAG_PMP;

	ahci_set_em_messages(hpriv, &pi);

	n_ports = max(ahci_nr_ports(hpriv->cap), fls(hpriv->port_map));

	host = ata_host_alloc_pinfo(ppi, n_ports);
	if (!host)
		return -ENOMEM;

	host->private_data = hpriv;

	ahci_init_controller(host);

	ahci_print_info(host, "SATA");

	sata_host_desc[last_host_idx++] = host;
	if (last_port_idx + host->n_ports > CONFIG_SYS_SATA_MAX_DEVICE) {
		printf("Too many devices found, increase CONFIG_SYS_SATA_MAX_DEVICE (currently %d)\n",
		       CONFIG_SYS_SATA_MAX_DEVICE);
		goto err_out;
	}
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
		sata_device_desc[last_device_idx++] = &link->device[0];
		debug("SATA port %d has %d PMP links\n",
		      port->port_no, port->nr_pmp_links);
		for (dev_no = 0; dev_no < port->nr_pmp_links; dev_no++)
			sata_device_desc[last_device_idx++] =
				&port->pmp_link[dev_no].device[0];
	}

	return 0;

err_out:
	sata_host_desc[start_host_idx] = NULL;
	last_host_idx = start_host_idx;
	for (port_no = start_port_idx; port_no < last_port_idx; port_no++)
		sata_port_desc[port_no] = NULL;
	last_port_idx = start_port_idx;
	for (dev_no = start_device_idx; dev_no < last_device_idx; dev_no++)
		sata_device_desc[dev_no] = NULL;
	last_device_idx = start_device_idx;
	if (host)
		free(host);
	if (hpriv)
		free(hpriv);

	return rc;
}

void ahci_shutdown_platform(struct ata_host *host)
{
	int i;

	for (i = 0; i < host->n_ports; i++) {
		if (host->ports[i])
			free(host->ports[i]);
	}
	memset(host, 0, sizeof(*host));
	if (host->private_data)
		free(host->private_data);
	free(host);
}
