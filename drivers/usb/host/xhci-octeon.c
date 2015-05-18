/*
 * Cavium Octeon USB HOST xHCI Controller
 *
 * (C) Copyright 2013
 * Texas Instruments, <www.ti.com>
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * (C) Copyright 2014
 * Cavium, Inc. <aupport@cavium.com>
 *
 * Author: Aaron Williams <awilliams@cavium.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <usb.h>
#include <asm-generic/errno.h>

#include <linux/compat.h>
#include <libfdt.h>
#include <fdtdec.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-usbdrdx-defs.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/lib_octeon.h>
#include <asm/processor.h>
#include <linux/usb/dwc3.h>
#include <asm/arch/cvmx-gpio.h>
#include <asm/arch/cvmx-gpio-defs.h>
#include <asm/gpio.h>

#include "xhci.h"

/**
 * The Octeon III XHCI implementation consists of two Designware cores so we
 * re-use the designware code from the OMAP 3 processor.
 */

#ifndef CONFIG_OCTEON_MAX_USB_PORTS
# define CONFIG_OCTEON_MAX_USB_PORTS	CONFIG_USB_MAX_CONTROLLER_COUNT
#endif

#define OCTEON_USB3_MIN_H_CLK_RATE	(150000000)
#define OCTEON_USB3_MAX_H_CLK_RATE	(300000000)
#define OCTEON_USB3_NUM_CLK_DEV_SEL	(8)


#define OCTEON_XHCI_PAGE_SIZE		0x10000		/* 64K */
#define OCTEON_XHCI_PAGE_MASK		0x0001E000	/* 64K */
#define OCTEON_XHCI_BASE_ADDR(X)	\
		MAKE_XKPHYS(0x1680000000000ull | ((X)* 0x10000000000ull))

#define OCTEON_XHCI_PAGENO		(get_num_tlb_entries() - 2)

#define OCTEON_XHCI_BASE(X)	(0xD0000000 | ((X) * OCTEON_XHCI_PAGE_SIZE))
#define OCTEON_XHCI_SIZE	(0x0000FFFF)

#define OCTEON_XHCI_HCCR_BASE(X)	OCTEON_XHCI_BASE(X)
#define OCTEON_XHCI_DWC3_BASE(X)	(OCTEON_XHCI_BASE(X) + 0xC100)

/* Global constants */
#define DWC3_EP0_BOUNCE_SIZE	512
#define DWC3_ENDPOINTS_NUM	32
#define DWC3_XHCI_RESOURCES_NUM	2

#define DWC3_EVENT_SIZE		4	/* bytes */
#define DWC3_EVENT_MAX_NUM	64	/* 2 events/endpoint */
#ifdef DWC3_EVENT_BUFFERS_SIZE
# undef DWC3_EVENT_BUFFERS_SIZE
#endif
#define DWC3_EVENT_BUFFERS_SIZE	(DWC3_EVENT_SIZE * DWC3_EVENT_MAX_NUM)
#define DWC3_EVENT_TYPE_MASK	0xfe

#define DWC3_EVENT_TYPE_DEV	0
#define DWC3_EVENT_TYPE_CARKIT	3
#define DWC3_EVENT_TYPE_I2C	4

#define DWC3_DEVICE_EVENT_DISCONNECT		0
#define DWC3_DEVICE_EVENT_RESET			1
#define DWC3_DEVICE_EVENT_CONNECT_DONE		2
#define DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE	3
#define DWC3_DEVICE_EVENT_WAKEUP		4
#define DWC3_DEVICE_EVENT_HIBER_REQ		5
#define DWC3_DEVICE_EVENT_EOPF			6
#define DWC3_DEVICE_EVENT_SOF			7
#define DWC3_DEVICE_EVENT_ERRATIC_ERROR		9
#define DWC3_DEVICE_EVENT_CMD_CMPL		10
#define DWC3_DEVICE_EVENT_OVERFLOW		11

#define DWC3_GEVNTCOUNT_MASK	0xfffc
#define DWC3_GSNPSID_MASK	0xffff0000
#define DWC3_GSNPSREV_MASK	0xffff

/* DWC3 registers memory space boundries */
#define DWC3_XHCI_REGS_START		0x0
#define DWC3_XHCI_REGS_END		0x7fff
#define DWC3_GLOBALS_REGS_START		0xc100
#define DWC3_GLOBALS_REGS_END		0xc6ff
#define DWC3_DEVICE_REGS_START		0xc700
#define DWC3_DEVICE_REGS_END		0xcbff
#define DWC3_OTG_REGS_START		0xcc00
#define DWC3_OTG_REGS_END		0xccff

/* Global Registers */
#define DWC3_GSBUSCFG0		0xc100
#define DWC3_GSBUSCFG1		0xc104
#define DWC3_GTXTHRCFG		0xc108
#define DWC3_GRXTHRCFG		0xc10c
#define DWC3_GCTL		0xc110
#define DWC3_GEVTEN		0xc114
#define DWC3_GSTS		0xc118
#define DWC3_GSNPSID		0xc120
#define DWC3_GGPIO		0xc124
#define DWC3_GUID		0xc128
#define DWC3_GUCTL		0xc12c
#define DWC3_GBUSERRADDR0	0xc130
#define DWC3_GBUSERRADDR1	0xc134
#define DWC3_GPRTBIMAP0		0xc138
#define DWC3_GPRTBIMAP1		0xc13c
#define DWC3_GHWPARAMS0		0xc140
#define DWC3_GHWPARAMS1		0xc144
#define DWC3_GHWPARAMS2		0xc148
#define DWC3_GHWPARAMS3		0xc14c
#define DWC3_GHWPARAMS4		0xc150
#define DWC3_GHWPARAMS5		0xc154
#define DWC3_GHWPARAMS6		0xc158
#define DWC3_GHWPARAMS7		0xc15c
#define DWC3_GDBGFIFOSPACE	0xc160
#define DWC3_GDBGLTSSM		0xc164
#define DWC3_GPRTBIMAP_HS0	0xc180
#define DWC3_GPRTBIMAP_HS1	0xc184
#define DWC3_GPRTBIMAP_FS0	0xc188
#define DWC3_GPRTBIMAP_FS1	0xc18c

#define DWC3_GUSB2PHYCFG(n)	(0xc200 + (n * 0x04))
#define DWC3_GUSB2I2CCTL(n)	(0xc240 + (n * 0x04))

#define DWC3_GUSB2PHYACC(n)	(0xc280 + (n * 0x04))

#define DWC3_GUSB3PIPECTL(n)	(0xc2c0 + (n * 0x04))

#define DWC3_GTXFIFOSIZ(n)	(0xc300 + (n * 0x04))
#define DWC3_GRXFIFOSIZ(n)	(0xc380 + (n * 0x04))

#define DWC3_GEVNTADRLO(n)	(0xc400 + (n * 0x10))
#define DWC3_GEVNTADRHI(n)	(0xc404 + (n * 0x10))
#define DWC3_GEVNTSIZ(n)	(0xc408 + (n * 0x10))
#define DWC3_GEVNTCOUNT(n)	(0xc40c + (n * 0x10))

#define DWC3_GHWPARAMS8		0xc600

/* Device Registers */
#define DWC3_DCFG		0xc700
#define DWC3_DCTL		0xc704
#define DWC3_DEVTEN		0xc708
#define DWC3_DSTS		0xc70c
#define DWC3_DGCMDPAR		0xc710
#define DWC3_DGCMD		0xc714
#define DWC3_DALEPENA		0xc720
#define DWC3_DEPCMDPAR2(n)	(0xc800 + (n * 0x10))
#define DWC3_DEPCMDPAR1(n)	(0xc804 + (n * 0x10))
#define DWC3_DEPCMDPAR0(n)	(0xc808 + (n * 0x10))
#define DWC3_DEPCMD(n)		(0xc80c + (n * 0x10))

/* OTG Registers */
#define DWC3_OCFG		0xcc00
#define DWC3_OCTL		0xcc04
#define DWC3_OEVT		0xcc08
#define DWC3_OEVTEN		0xcc0C
#define DWC3_OSTS		0xcc10

/* Bit fields */

/* Global Configuration Register */
#define DWC3_GCTL_PWRDNSCALE(n)	((n) << 19)
#define DWC3_GCTL_U2RSTECN	(1 << 16)
#define DWC3_GCTL_RAMCLKSEL(x)	(((x) & DWC3_GCTL_CLK_MASK) << 6)
#define DWC3_GCTL_CLK_BUS	(0)
#define DWC3_GCTL_CLK_PIPE	(1)
#define DWC3_GCTL_CLK_PIPEHALF	(2)
#define DWC3_GCTL_CLK_MASK	(3)

#define DWC3_GCTL_PRTCAP(n)	(((n) & (3 << 12)) >> 12)
#define DWC3_GCTL_PRTCAPDIR(n)	((n) << 12)
#define DWC3_GCTL_PRTCAP_HOST	1
#define DWC3_GCTL_PRTCAP_DEVICE	2
#define DWC3_GCTL_PRTCAP_OTG	3

#define DWC3_GCTL_CORESOFTRESET		(1 << 11)
#define DWC3_GCTL_SCALEDOWN(n)		((n) << 4)
#define DWC3_GCTL_SCALEDOWN_MASK	DWC3_GCTL_SCALEDOWN(3)
#define DWC3_GCTL_DISSCRAMBLE		(1 << 3)
#define DWC3_GCTL_GBLHIBERNATIONEN	(1 << 1)
#define DWC3_GCTL_DSBLCLKGTNG		(1 << 0)

/* Global USB2 PHY Configuration Register */
#define DWC3_GUSB2PHYCFG_PHYSOFTRST	(1 << 31)
#define DWC3_GUSB2PHYCFG_SUSPHY		(1 << 6)

/* Global USB3 PIPE Control Register */
#define DWC3_GUSB3PIPECTL_PHYSOFTRST	(1 << 31)
#define DWC3_GUSB3PIPECTL_SUSPHY	(1 << 17)

/* Global TX Fifo Size Register */
#define DWC3_GTXFIFOSIZ_TXFDEF(n)	((n) & 0xffff)
#define DWC3_GTXFIFOSIZ_TXFSTADDR(n)	((n) & 0xffff0000)

/* Global HWPARAMS1 Register */
#define DWC3_GHWPARAMS1_EN_PWROPT(n)	(((n) & (3 << 24)) >> 24)
#define DWC3_GHWPARAMS1_EN_PWROPT_NO	0
#define DWC3_GHWPARAMS1_EN_PWROPT_CLK	1
#define DWC3_GHWPARAMS1_EN_PWROPT_HIB	2
#define DWC3_GHWPARAMS1_PWROPT(n)	((n) << 24)
#define DWC3_GHWPARAMS1_PWROPT_MASK	DWC3_GHWPARAMS1_PWROPT(3)

/* Global HWPARAMS4 Register */
#define DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(n)	(((n) & (0x0f << 13)) >> 13)
#define DWC3_MAX_HIBER_SCRATCHBUFS		15

/* Device Configuration Register */
#define DWC3_DCFG_LPM_CAP	(1 << 22)
#define DWC3_DCFG_DEVADDR(addr)	((addr) << 3)
#define DWC3_DCFG_DEVADDR_MASK	DWC3_DCFG_DEVADDR(0x7f)

#define DWC3_DCFG_SPEED_MASK	(7 << 0)
#define DWC3_DCFG_SUPERSPEED	(4 << 0)
#define DWC3_DCFG_HIGHSPEED	(0 << 0)
#define DWC3_DCFG_FULLSPEED2	(1 << 0)
#define DWC3_DCFG_LOWSPEED	(2 << 0)
#define DWC3_DCFG_FULLSPEED1	(3 << 0)

#define DWC3_DCFG_LPM_CAP	(1 << 22)

/* Device Control Register */
#define DWC3_DCTL_RUN_STOP	(1 << 31)
#define DWC3_DCTL_CSFTRST	(1 << 30)
#define DWC3_DCTL_LSFTRST	(1 << 29)

#define DWC3_DCTL_HIRD_THRES_MASK	(0x1f << 24)
#define DWC3_DCTL_HIRD_THRES(n)	((n) << 24)

#define DWC3_DCTL_APPL1RES	(1 << 23)

/* These apply for core versions 1.87a and earlier */
#define DWC3_DCTL_TRGTULST_MASK		(0x0f << 17)
#define DWC3_DCTL_TRGTULST(n)		((n) << 17)
#define DWC3_DCTL_TRGTULST_U2		(DWC3_DCTL_TRGTULST(2))
#define DWC3_DCTL_TRGTULST_U3		(DWC3_DCTL_TRGTULST(3))
#define DWC3_DCTL_TRGTULST_SS_DIS	(DWC3_DCTL_TRGTULST(4))
#define DWC3_DCTL_TRGTULST_RX_DET	(DWC3_DCTL_TRGTULST(5))
#define DWC3_DCTL_TRGTULST_SS_INACT	(DWC3_DCTL_TRGTULST(6))

/* These apply for core versions 1.94a and later */
#define DWC3_DCTL_KEEP_CONNECT	(1 << 19)
#define DWC3_DCTL_L1_HIBER_EN	(1 << 18)
#define DWC3_DCTL_CRS		(1 << 17)
#define DWC3_DCTL_CSS		(1 << 16)

#define DWC3_DCTL_INITU2ENA	(1 << 12)
#define DWC3_DCTL_ACCEPTU2ENA	(1 << 11)
#define DWC3_DCTL_INITU1ENA	(1 << 10)
#define DWC3_DCTL_ACCEPTU1ENA	(1 << 9)
#define DWC3_DCTL_TSTCTRL_MASK	(0xf << 1)

#define DWC3_DCTL_ULSTCHNGREQ_MASK	(0x0f << 5)
#define DWC3_DCTL_ULSTCHNGREQ(n) (((n) << 5) & DWC3_DCTL_ULSTCHNGREQ_MASK)

#define DWC3_DCTL_ULSTCHNG_NO_ACTION	(DWC3_DCTL_ULSTCHNGREQ(0))
#define DWC3_DCTL_ULSTCHNG_SS_DISABLED	(DWC3_DCTL_ULSTCHNGREQ(4))
#define DWC3_DCTL_ULSTCHNG_RX_DETECT	(DWC3_DCTL_ULSTCHNGREQ(5))
#define DWC3_DCTL_ULSTCHNG_SS_INACTIVE	(DWC3_DCTL_ULSTCHNGREQ(6))
#define DWC3_DCTL_ULSTCHNG_RECOVERY	(DWC3_DCTL_ULSTCHNGREQ(8))
#define DWC3_DCTL_ULSTCHNG_COMPLIANCE	(DWC3_DCTL_ULSTCHNGREQ(10))
#define DWC3_DCTL_ULSTCHNG_LOOPBACK	(DWC3_DCTL_ULSTCHNGREQ(11))

/* Device Event Enable Register */
#define DWC3_DEVTEN_VNDRDEVTSTRCVEDEN	(1 << 12)
#define DWC3_DEVTEN_EVNTOVERFLOWEN	(1 << 11)
#define DWC3_DEVTEN_CMDCMPLTEN		(1 << 10)
#define DWC3_DEVTEN_ERRTICERREN		(1 << 9)
#define DWC3_DEVTEN_SOFEN		(1 << 7)
#define DWC3_DEVTEN_EOPFEN		(1 << 6)
#define DWC3_DEVTEN_HIBERNATIONREQEVTEN	(1 << 5)
#define DWC3_DEVTEN_WKUPEVTEN		(1 << 4)
#define DWC3_DEVTEN_ULSTCNGEN		(1 << 3)
#define DWC3_DEVTEN_CONNECTDONEEN	(1 << 2)
#define DWC3_DEVTEN_USBRSTEN		(1 << 1)
#define DWC3_DEVTEN_DISCONNEVTEN	(1 << 0)

/* Device Status Register */
#define DWC3_DSTS_DCNRD			(1 << 29)

/* This applies for core versions 1.87a and earlier */
#define DWC3_DSTS_PWRUPREQ		(1 << 24)

/* These apply for core versions 1.94a and later */
#define DWC3_DSTS_RSS			(1 << 25)
#define DWC3_DSTS_SSS			(1 << 24)

#define DWC3_DSTS_COREIDLE		(1 << 23)
#define DWC3_DSTS_DEVCTRLHLT		(1 << 22)

#define DWC3_DSTS_USBLNKST_MASK		(0x0f << 18)
#define DWC3_DSTS_USBLNKST(n)		(((n) & DWC3_DSTS_USBLNKST_MASK) >> 18)

#define DWC3_DSTS_RXFIFOEMPTY		(1 << 17)

#define DWC3_DSTS_SOFFN_MASK		(0x3fff << 3)
#define DWC3_DSTS_SOFFN(n)		(((n) & DWC3_DSTS_SOFFN_MASK) >> 3)

#define DWC3_DSTS_CONNECTSPD		(7 << 0)

#define DWC3_DSTS_SUPERSPEED		(4 << 0)
#define DWC3_DSTS_HIGHSPEED		(0 << 0)
#define DWC3_DSTS_FULLSPEED2		(1 << 0)
#define DWC3_DSTS_LOWSPEED		(2 << 0)
#define DWC3_DSTS_FULLSPEED1		(3 << 0)

/* Device Generic Command Register */
#define DWC3_DGCMD_SET_LMP		0x01
#define DWC3_DGCMD_SET_PERIODIC_PAR	0x02
#define DWC3_DGCMD_XMIT_FUNCTION	0x03

/* These apply for core versions 1.94a and later */
#define DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO	0x04
#define DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI	0x05

#define DWC3_DGCMD_SELECTED_FIFO_FLUSH	0x09
#define DWC3_DGCMD_ALL_FIFO_FLUSH	0x0a
#define DWC3_DGCMD_SET_ENDPOINT_NRDY	0x0c
#define DWC3_DGCMD_RUN_SOC_BUS_LOOPBACK	0x10

#define DWC3_DGCMD_STATUS(n)		(((n) >> 15) & 1)
#define DWC3_DGCMD_CMDACT		(1 << 10)
#define DWC3_DGCMD_CMDIOC		(1 << 8)

/* Device Generic Command Parameter Register */
#define DWC3_DGCMDPAR_FORCE_LINKPM_ACCEPT	(1 << 0)
#define DWC3_DGCMDPAR_FIFO_NUM(n)		((n) << 0)
#define DWC3_DGCMDPAR_RX_FIFO			(0 << 5)
#define DWC3_DGCMDPAR_TX_FIFO			(1 << 5)
#define DWC3_DGCMDPAR_LOOPBACK_DIS		(0 << 0)
#define DWC3_DGCMDPAR_LOOPBACK_ENA		(1 << 0)

/* Device Endpoint Command Register */
#define DWC3_DEPCMD_PARAM_SHIFT		16
#define DWC3_DEPCMD_PARAM(x)		((x) << DWC3_DEPCMD_PARAM_SHIFT)
#define DWC3_DEPCMD_GET_RSC_IDX(x)     (((x) >> DWC3_DEPCMD_PARAM_SHIFT) & 0x7f)
#define DWC3_DEPCMD_STATUS(x)		(((x) >> 15) & 1)
#define DWC3_DEPCMD_HIPRI_FORCERM	(1 << 11)
#define DWC3_DEPCMD_CMDACT		(1 << 10)
#define DWC3_DEPCMD_CMDIOC		(1 << 8)

#define DWC3_DEPCMD_DEPSTARTCFG		(0x09 << 0)
#define DWC3_DEPCMD_ENDTRANSFER		(0x08 << 0)
#define DWC3_DEPCMD_UPDATETRANSFER	(0x07 << 0)
#define DWC3_DEPCMD_STARTTRANSFER	(0x06 << 0)
#define DWC3_DEPCMD_CLEARSTALL		(0x05 << 0)
#define DWC3_DEPCMD_SETSTALL		(0x04 << 0)
/* This applies for core versions 1.90a and earlier */
#define DWC3_DEPCMD_GETSEQNUMBER	(0x03 << 0)
/* This applies for core versions 1.94a and later */
#define DWC3_DEPCMD_GETEPSTATE		(0x03 << 0)
#define DWC3_DEPCMD_SETTRANSFRESOURCE	(0x02 << 0)
#define DWC3_DEPCMD_SETEPCONFIG		(0x01 << 0)

/* The EP number goes 0..31 so ep0 is always out and ep1 is always in */
#define DWC3_DALEPENA_EP(n)		(1 << n)

#define DWC3_DEPCMD_TYPE_CONTROL	0
#define DWC3_DEPCMD_TYPE_ISOC		1
#define DWC3_DEPCMD_TYPE_BULK		2
#define DWC3_DEPCMD_TYPE_INTR		3

/* Declare global data pointer */
DECLARE_GLOBAL_DATA_PTR;

struct octeon_xhci {
	int intf;		/** Interface number */
	int node;
	int clock_initialized;
	int power_gpio;
	int power_active_low;
	int power_sense_gpio;
	int power_sense_active_low;
	int fdt_offset;
	struct dwc3 *dwc3;
	struct xhci_hcrr *hcd;
	struct xhci_hcor *hcor;
	struct xhci_hccr *hccr;
};

static const uint8_t clk_div[OCTEON_USB3_NUM_CLK_DEV_SEL] = {
	1, 2, 4, 6, 8, 16, 24, 32
};

static struct octeon_xhci octeon_xhci[CONFIG_OCTEON_MAX_USB_PORTS * CVMX_MAX_NODES];

static void octeon3_usb_phy_reset(int intf);

/**
 * Read from an XHCI register.  Note that the interface number is encoded in
 * bit 16 in the pointer and must be moved to bits 40+.  The node number
 * is encoded in bits 17-19.  Addresses in the range of 0xD0000000 -
 * 0xD00fffff are remapped to the appropriate 64-bit address, otherwise the
 * address is treated as a virtual address and is used as-is.
 *
 * @param addr32	"pointer" to register to read from
 *
 * @return value of register
 *
 * NOTE: Address bit 16 in addr32 is the interface number to use.  The
 * node number is encoded in bits 17-19 of the addr32 field.
 */
u32 xhci_readl(volatile uint32_t *addr32)
{
	u64 addr64;
	u32 val;

	if ((ulong)addr32 >= OCTEON_XHCI_BASE(0) &&
	    (ulong)addr32 < OCTEON_XHCI_BASE(16)) {
		int intf = ((ulong)addr32 >> 16) & 0x1;
		u64 node = ((ulong)addr32 >> 17) & 0x7;
		addr64 = OCTEON_XHCI_BASE_ADDR(intf) |
			     ((ulong)addr32 & 0xffff) |
			     (node << CVMX_NODE_IO_SHIFT);
		val = le32_to_cpu(cvmx_read64_uint32(addr64));
#ifdef DEBUG_IO
		debug("%s(%p (0x%llx)) => 0x%x\n", __func__, addr32,
		      addr64, val);
#endif
	} else {
		val = le32_to_cpu(*addr32);
#ifdef DEBUG_IO
		debug("%s(0x%p) => 0x%x\n", __func__, addr32, val);
#endif
	}

	return val;
}

/**
 * Writes to an XHCI register.  Note that the interface number is encoded in
 * bit 16 in the pointer and must be moved to bits 40+.  The node number
 * is encoded in bits 17-19.  Addresses in the range of 0xD0000000 -
 * 0xD00fffff are remapped to the appropriate 64-bit address, otherwise the
 * address is treated as a virtual address and is used as-is.
 *
 * @param addr32	"pointer" to register to write to
 * @param val		value to write
 *
 * NOTE: Address bit 16 in addr32 is the interface number to use.  The
 * node number is encoded in bits 17-19 of the addr32 field.
 */
void xhci_writel(volatile u32 *addr32, u32 val)
{
	u64 addr64;

	if ((ulong)addr32 >= OCTEON_XHCI_BASE(0) &&
	    (ulong)addr32 < OCTEON_XHCI_BASE(16)) {
		int intf = ((ulong)addr32 >> 16) & 0x1;
		u64 node = ((ulong)addr32 >> 17) & 0x7;
		addr64 = OCTEON_XHCI_BASE_ADDR(intf) |
			 ((ulong)addr32 & 0xffff) |
			 (node << CVMX_NODE_IO_SHIFT);
	} else {
		addr64 = (long)addr32;
	}
#ifdef DEBUG_IO
	debug("%s(%p (0x%llx), 0x%x)\n", __func__, addr32, addr64, val);
#endif
	cvmx_write64_uint32(addr64, cpu_to_le32(val));
	cvmx_read64_uint32(addr64);
}

static inline u32 dwc3_readl(void __iomem *base, u32 offset)
{
	/*
	 * We requested the mem region starting from the Globals address
	 * space, see dwc3_probe in core.c.
	 * However, the offsets are given starting from xHCI address space.
	 */
	return xhci_readl(base + (offset - DWC3_GLOBALS_REGS_START));
}

static inline void dwc3_writel(void __iomem *base, u32 offset, u32 value)
{
	/*
	 * We requested the mem region starting from the Globals address
	 * space, see dwc3_probe in core.c.
	 * However, the offsets are given starting from xHCI address space.
	 */
	xhci_writel(base + (offset - DWC3_GLOBALS_REGS_START), value);
}

static void dwc3_set_mode(struct dwc3 *dwc3_reg, u32 mode)
{
	u32 reg;

	reg = dwc3_readl(dwc3_reg, DWC3_GCTL);
	reg &= ~(DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG));
	reg |= DWC3_GCTL_PRTCAPDIR(mode);
	dwc3_writel(dwc3_reg, DWC3_GCTL, reg);
}

/**
 * dwc3_core_soft_reset - Issues core soft reset and PHY reset
 * @dwc3_reg: pointer to our context structure
 */
static void dwc3_core_soft_reset(struct dwc3 *dwc3_reg, int intf)
{
	u32		reg;

	/* Before Resetting PHY, put Core in Reset */
	reg = dwc3_readl(dwc3_reg, DWC3_GCTL);
	reg |= DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc3_reg, DWC3_GCTL, reg);

	/* Assert USB3 PHY reset */
	reg = dwc3_readl(dwc3_reg, DWC3_GUSB3PIPECTL(0));
	reg |= DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc3_reg, DWC3_GUSB3PIPECTL(0), reg);

	/* Assert USB2 PHY reset */
	reg = dwc3_readl(dwc3_reg, DWC3_GUSB2PHYCFG(0));
	reg |= DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc3_reg, DWC3_GUSB2PHYCFG(0), reg);

	octeon3_usb_phy_reset(intf);
	mdelay(100);

	/* Clear USB3 PHY reset */
	reg = dwc3_readl(dwc3_reg, DWC3_GUSB3PIPECTL(0));
	reg &= ~DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc3_reg, DWC3_GUSB3PIPECTL(0), reg);

	/* Clear USB2 PHY reset */
	reg = dwc3_readl(dwc3_reg, DWC3_GUSB2PHYCFG(0));
	reg &= ~DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc3_reg, DWC3_GUSB2PHYCFG(0), reg);

	mdelay(100);

	/* After PHYs are stable we can take Core out of reset state */
	reg = dwc3_readl(dwc3_reg, DWC3_GCTL);
	reg &= ~DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc3_reg, DWC3_GCTL, reg);
}

static int dwc3_core_init(struct dwc3 *dwc3_reg, int intf)
{
	u32 reg;
	u32 revision;
	unsigned int dwc3_hwparams1;

	revision = xhci_readl(&dwc3_reg->g_snpsid);
	/* This should read as U3 followed by revision number */
	if ((revision & DWC3_GSNPSID_MASK) != 0x55330000) {
		puts("this is not a DesignWare USB3 DRD Core\n");
		return -1;
	}

	dwc3_core_soft_reset(dwc3_reg, intf);

	dwc3_hwparams1 = xhci_readl(&dwc3_reg->g_hwparams1);

	reg = xhci_readl(&dwc3_reg->g_ctl);
	reg &= ~DWC3_GCTL_SCALEDOWN_MASK;
	reg &= ~DWC3_GCTL_DISSCRAMBLE;
	switch (DWC3_GHWPARAMS1_EN_PWROPT(dwc3_hwparams1)) {
	case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		reg &= ~DWC3_GCTL_DSBLCLKGTNG;
		break;
	default:
		debug("No power optimization available\n");
	}

	/*
	 * WORKAROUND: DWC3 revisions <1.90a have a bug
	 * where the device can fail to connect at SuperSpeed
	 * and falls back to high-speed mode which causes
	 * the device to enter a Connect/Disconnect loop
	 */
	if ((revision & DWC3_REVISION_MASK) < 0x190a)
		reg |= DWC3_GCTL_U2RSTECN;

	xhci_writel(&dwc3_reg->g_ctl, reg);

	return 0;
}

static int octeon3_usb_gpio_compatible(int gpio_offset)
{
	return !(fdt_node_check_compatible(gd->fdt_blob, gpio_offset,
					   "cavium,octeon-3860-gpio") &&
		 fdt_node_check_compatible(gd->fdt_blob, gpio_offset,
					   "cavium,octeon-7890-gpio"));
}
/**
 * Configure the power from the device tree.
 *
 * @param ctx	XHCI context
 *
 * @return 0 for success, -1 for error.
 *
 * NOTE: This should only be called when reset is asserted.
 */
static int octeon3_usb_config_power(struct octeon_xhci *ctx)
{
	union cvmx_usbdrdx_uctl_host_cfg uctl_host_cfg;
	union cvmx_gpio_usbh_ctl gpio_usbh_ctl;
	const uint32_t *pgpio_handle;
	int gpio_offset;
	int phandle;
	int len;
	int gpio;
	int offset = ctx->fdt_offset;
	int index = ctx->intf & 1;
	int node = ctx->node;

	ctx->power_sense_gpio = -1;
	pgpio_handle = fdt_getprop(gd->fdt_blob, offset, "sense", &len);
	if (pgpio_handle) {
		if (len == 12)
			ctx->power_sense_active_low =
					(fdt32_to_cpu(pgpio_handle[2]) & 1);
		else if (len == 8)
			ctx->power_sense_active_low = 0;
		else {
			printf("%s: USB%d: Invalid power sense GPIO device tree entry\n",
			       __func__, ctx->intf);
			return -1;
		}
		phandle = pgpio_handle[0];
		gpio = ctx->power_gpio = fdt32_to_cpu(pgpio_handle[1]);
		gpio_offset = fdt_node_offset_by_phandle(gd->fdt_blob, phandle);
		if (!octeon3_usb_gpio_compatible(gpio_offset)) {
			printf("%s: Error, only native GPIO pins supported\n",
			       __func__);
			return -1;
		}

		debug("%s: USB%d power sense GPIO: %d, active %s\n",
		      __func__, ctx->intf, gpio,
		      ctx->power_sense_active_low ? "low" : "high");

		/* Configure the GPIO mux so that the sense over current
		 * pins are selected.
		 */
		gpio_usbh_ctl.u64 = cvmx_read_csr_node(node, CVMX_GPIO_USBH_CTL);
		if (index == 0)
			gpio_usbh_ctl.cn70xx.sel = gpio;
		else
			gpio_usbh_ctl.cn70xx.sel1 = gpio;
		cvmx_write_csr_node(node, CVMX_GPIO_USBH_CTL, gpio_usbh_ctl.u64);
		gpio_direction_input((node << 8) | gpio);
		cvmx_gpio_invert_input(node, gpio, ctx->power_sense_active_low);
	} else {
		debug("%s: No power sense control for USB%d\n",
		      __func__, ctx->intf);
		/* Configure the GPIO mux so that the sense over current
		 * pins are selected.
		 */
		gpio_usbh_ctl.u64 = cvmx_read_csr_node(node, CVMX_GPIO_USBH_CTL);
		if (index == 0)
			gpio_usbh_ctl.cn70xx.sel = 0x1f;
		else
			gpio_usbh_ctl.cn70xx.sel1 = 0x1f;
		cvmx_write_csr_node(node, CVMX_GPIO_USBH_CTL, gpio_usbh_ctl.u64);
	}

	ctx->power_gpio = -1;
	pgpio_handle = fdt_getprop(gd->fdt_blob, offset, "power", &len);
	if (pgpio_handle) {
		if (len == 12)
			ctx->power_active_low =
					fdt32_to_cpu(pgpio_handle[2]) & 1;
		else if (len == 8)
			ctx->power_active_low = 0;
		else {
			printf("%s: USB%d: Invalid power GPIO device tree entry\n",
			       __func__, ctx->intf);
			return -1;
		}
		phandle = pgpio_handle[0];
		gpio = ctx->power_gpio = fdt32_to_cpu(pgpio_handle[1]);
		gpio_offset = fdt_node_offset_by_phandle(gd->fdt_blob, phandle);
		if (!octeon3_usb_gpio_compatible(gpio_offset)) {
			printf("%s: Error, only native GPIO pins supported\n",
			       __func__);
			return -1;
		}

		debug("%s: USB%d power GPIO: %d, active %s\n", __func__,
		      ctx->intf, gpio,
		      ctx->power_active_low ? "low" : "high");

		/* Configure the GPIO mux so that the USB XHCI controls
		 * control the GPIO port.
		 */
		gpio_direction_output(gpio | node << 8, !ctx->power_active_low);
		cvmx_gpio_cfg_sel(node, gpio, (index == 0) ? 0x14 : 0x19);
		__cvmx_gpio_cfg(node, gpio, 1);

		/* Enable XHCI power control and set if active high or
		 * low
		 */
		uctl_host_cfg.u64 =
			cvmx_read_csr_node(node,
					   CVMX_USBDRDX_UCTL_HOST_CFG(index));
		uctl_host_cfg.s.ppc_en = 1;
		uctl_host_cfg.s.ppc_active_high_en = !ctx->power_active_low;
		debug("%s: USB%d: wrote 0x%llx to uctl_host_cfg\n",
		      __func__, index, uctl_host_cfg.u64);
		cvmx_write_csr_node(node, CVMX_USBDRDX_UCTL_HOST_CFG(index),
				    uctl_host_cfg.u64);
		uctl_host_cfg.u64 = cvmx_read_csr_node(node,
						       CVMX_USBDRDX_UCTL_HOST_CFG(index));
		debug("%s: USB%d: read back 0x%llx from uctl_host_cfg\n",
		      __func__, index, uctl_host_cfg.u64);
		debug("%s: USB%d: uctl_ctl: 0x%llx\n", __func__, index,
		      cvmx_read_csr_node(node, CVMX_USBDRDX_UCTL_CTL(index)));
		/* Wait for power to stabilize */
		mdelay(55);
	} else {
		debug("No power control for USB%d\n", ctx->intf);
		/* Enable XHCI power control and set if active high or
		 * low
		 */
		uctl_host_cfg.u64 =
			cvmx_read_csr_node(node,
					   CVMX_USBDRDX_UCTL_HOST_CFG(index));
		uctl_host_cfg.s.ppc_en = 0;
		uctl_host_cfg.s.ppc_active_high_en = 0;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_HOST_CFG(index),
				    uctl_host_cfg.u64);
		cvmx_read_csr_node(node, CVMX_USBDRDX_UCTL_HOST_CFG(index));
	}

	return 0;
}

static int octeon3_usb_clocks_start(void)
{
	uint64_t reg, size;
	union cvmx_usbdrdx_uctl_ctl uctl_ctl;
	uint64_t div;
	uint64_t h_clk_rate;
	int mpll_mul;
	int node = 0;
	int index = 0;
	int offset = -1;
	uint32_t clock_rate = 100000000;
	const char *ss_clock_type;
	const char *hs_clock_type;
	int ref_clk_sel = 2;
	struct octeon_xhci *ctx;

	debug("%s\n", __func__);
	do {
		offset = fdt_node_offset_by_compatible(gd->fdt_blob, offset,
						"cavium,octeon-7130-usb-uctl");
		if (offset < 0)
			break;

		debug("%s: Found dt entry at offset 0x%x\n", __func__, offset);
		reg = fdtdec_get_addr_size(gd->fdt_blob, offset, "reg", &size);
		debug("  register address: 0x%llx\n", reg);
		reg = fdt_translate_address((void *)(gd->fdt_blob), offset,
					    (const uint32_t *)&reg);
		debug("  translated register address: 0x%llx\n", reg);
		switch (reg) {
		case FDT_ADDR_T_NONE:
			printf("%s: Error: \"reg\" field missing from uctl USB device tree entry\n",
			       __func__);
			return -1;
		case 0x1180068000000ull:
			index = 0;
			break;
		case 0x1180069000000ull:
			if (OCTEON_IS_MODEL(OCTEON_CN78XX) ||
			    OCTEON_IS_MODEL(OCTEON_CN76XX)) {
				printf("%s: Error: Only interface 0 is present on 78xx\n",
					__func__);
				return -1;
			}
			index = 1;
			break;
#ifdef CONFIG_OCTEON_OCX
		case 0x1181068000000ull:
			index = 0;
			node = 1;
			break;
		case 0x1182068000000ull:
			index = 0;
			node = 2;
			break;
		case 0x1183068000000ull:
			index = 0;
			node = 3;
			break;
#endif
		default:
			printf("%s: Unknown register address 0x%llx for node %d\n",
			       __func__, reg, node);
			return -1;
		}

		ctx = &octeon_xhci[index + node * 2];
		debug("%s: Initializing USB%d\n", __func__, index + node * 2);
		if (ctx->clock_initialized) {
			debug("%s: USB interface %d already initialized\n",
			      __func__, index);
			continue;
		}

		ctx->fdt_offset = offset;
		ctx->node = node;

		clock_rate = fdtdec_get_int(gd->fdt_blob, offset,
					    "refclk-frequency",
					    100000000);
		debug("  clock rate: %d\n", clock_rate);
		ss_clock_type = fdt_getprop(gd->fdt_blob, offset,
					    "refclk-type-ss", NULL);

		if (!ss_clock_type) {
			printf("No UCTL \"refclk-type-ss\"\n");
			return -1;
		}
		hs_clock_type = fdt_getprop(gd->fdt_blob, offset,
					    "refclk-type-hs", NULL);
		if (!hs_clock_type) {
			printf("No UCTL \"refclk-type-hs\"\n");
			return -1;
		}

		debug("ss clock type: %s\n", ss_clock_type);
		debug("hs clock type: %s\n", hs_clock_type);

		if (strcmp("dlmc_ref_clk0", ss_clock_type) == 0) {
			if (strcmp(hs_clock_type, "dlmc_ref_clk0") == 0)
				ref_clk_sel = 0;
			else if (strcmp(hs_clock_type, "pll_ref_clk") == 0)
				ref_clk_sel = 2;
			else
				printf("Invalid USB HS clock type %s, using pll_ref_clk instead\n",
				       hs_clock_type);
		} else if (strcmp(ss_clock_type, "dlmc_ref_clk1") == 0) {
			if (strcmp(hs_clock_type, "dlmc_ref_clk1") == 0)
				ref_clk_sel = 1;
			else if (strcmp(hs_clock_type, "pll_ref_clk") == 0)
				ref_clk_sel = 3;
			else
				printf("Invalid USB HS clock type %s, using pll_ref_clk instead\n",
				       hs_clock_type);
		} else {
			printf("Invalid SS clock type %s, using dlmc_ref_clk0 instead\n",
			       ss_clock_type);
		}

		if ((ref_clk_sel == 0 || ref_clk_sel == 1) &&
		    (clock_rate != 100000000))
			printf("invalid UCTL clock rate of %u, using 100000000 instead\n",
			       clock_rate);

		/*
		 * Step 1: Wait for voltages stable.  That surely happened
		 * before starting the kernel.
		 * Ensure the reference clock is up and stable ??
		 */

		/* Step 2: Wait for IOI reset to deassert  ?? */

		/* Step 3: program over current indication if desired, later */

		/* Step 3: program the port power control feature if desired,
		 * later
		 */
		/* Step 4: Assert all resets */
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.uphy_rst = 1;
		uctl_ctl.s.uahc_rst = 1;
		uctl_ctl.s.uctl_rst = 1;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);

		/* Step 5a: Reset the clock dividers */
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.h_clkdiv_rst = 1;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);

		/* 5b */
		/* Step 5b: Select controller clock frequency */
		for (div = 0; div < ARRAY_SIZE(clk_div); div++) {
			h_clk_rate = octeon_get_io_clock_rate() / clk_div[div];
			if (h_clk_rate <= OCTEON_USB3_MAX_H_CLK_RATE &&
			    h_clk_rate >= OCTEON_USB3_MIN_H_CLK_RATE)
				break;
		}

		debug("%s: hclock div: %llu\n", __func__, div);
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.h_clkdiv_sel = div;
		uctl_ctl.s.h_clk_en = 1;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		if ((div != uctl_ctl.s.h_clkdiv_sel) || (!uctl_ctl.s.h_clk_en)) {
			puts("ERROR: usb controller clock init\n");
			continue;
		}

		/* Step 5c: Deassert the controller clock divider reset */
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.h_clkdiv_rst = 0;
		cvmx_write_csr_node(node, CVMX_USBDRDX_UCTL_CTL(index),
				    uctl_ctl.u64);

		/* Step ??*/
		cvmx_wait(2000);

		/* Step 6a-6d & 7: Reference clock configuration */

		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.ssc_en =0;
		uctl_ctl.s.ref_ssp_en = 1;
		uctl_ctl.s.ref_clk_sel = ref_clk_sel;
		uctl_ctl.s.ref_clk_fsel = 0x07;
		uctl_ctl.s.ref_clk_div2 = 0;

		switch (clock_rate) {
		default:
			printf("Invalid UCTL ref_clk %u, using 100000000 instead\n",
			       clock_rate);
			/* Fall through */
		case 100000000:
			mpll_mul = 0x19;
			break;
		case 50000000:
			mpll_mul = 0x32;
			break;
		case 125000000:
			mpll_mul = 0x28;
			break;
		}
		uctl_ctl.s.mpll_multiplier = mpll_mul;
		uctl_ctl.s.ss_power_en = 1;
		uctl_ctl.s.hs_power_en = 1;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);

		/* Step 9 */
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.uctl_rst = 0;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);

		debug("%s: USB%d: uctl_ctl: 0x%llx\n", __func__, index,
		      cvmx_read_csr_node(node, CVMX_USBDRDX_UCTL_CTL(index)));
		debug("%s: USB%d: uctl_host: 0x%llx\n", __func__, index,
		      cvmx_read_csr_node(node,
					 CVMX_USBDRDX_UCTL_HOST_CFG(index)));

		if (octeon3_usb_config_power(ctx)) {
			printf("Error configuring power for USB%d\n", index);
			continue;
		}

		uctl_ctl.s.uahc_rst = 0;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);
		mdelay(50);

		/* Step 8*/
		cvmx_wait(10);
		debug("%s: USB%d: uctl_ctl: 0x%llx\n", __func__, index,
		      cvmx_read_csr_node(node, CVMX_USBDRDX_UCTL_CTL(index)));
		debug("%s: USB%d: uctl_host: 0x%llx\n", __func__, index,
		      cvmx_read_csr_node(node,
					 CVMX_USBDRDX_UCTL_HOST_CFG(index)));
		/* Step 10*/
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.csclk_en = 1;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);

		/*Step 11*/
		uctl_ctl.u64 = cvmx_read_csr_node(node,
						  CVMX_USBDRDX_UCTL_CTL(index));
		uctl_ctl.s.drd_mode = 0;
		cvmx_write_csr_node(node,
				    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);

		ctx->clock_initialized++;
	} while (offset >= 0);

	return 0;
}

static void octeon3_usb_clocks_stop(struct octeon_xhci *ctx)
{
	if (ctx->clock_initialized)
		ctx->clock_initialized--;
}

void octeon3_usb_set_endian_mode(int node, int index)
{
	union cvmx_usbdrdx_uctl_shim_cfg shim_cfg;

	debug("%s(%d)\n", __func__, index);
	shim_cfg.u64 = cvmx_read_csr_node(node,
					  CVMX_USBDRDX_UCTL_SHIM_CFG(index));
#ifdef __BIG_ENDIAN
	shim_cfg.s.dma_endian_mode = 1;
	shim_cfg.s.csr_endian_mode = 1;
#else
	shim_cfg.s.dma_endian_mode = 0;
	shim_cfg.s.csr_endian_mode = 0;
#endif
	cvmx_write_csr_node(node,
			    CVMX_USBDRDX_UCTL_SHIM_CFG(index), shim_cfg.u64);
}

static void octeon3_usb_phy_reset(int intf)
{
	union cvmx_usbdrdx_uctl_ctl uctl_ctl;
	int node = octeon_xhci[intf].node;
	int index = intf & 1;

	debug("%s(%d)\n", __func__, index);
	uctl_ctl.u64 = cvmx_read_csr_node(node,
					  CVMX_USBDRDX_UCTL_CTL(index));
	uctl_ctl.s.uphy_rst = 0;
	cvmx_write_csr_node(node,
			    CVMX_USBDRDX_UCTL_CTL(index), uctl_ctl.u64);
}

inline int __board_usb_init(int index, enum usb_init_type init)
{
	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
	__attribute__((weak, alias("__board_usb_init")));

int xhci_hcd_init(int index, struct xhci_hccr **hccr, struct xhci_hcor **hcor)
{
	struct octeon_xhci *ctx = &octeon_xhci[index];
	int ret = 0;

	debug("%s(%d, %p, %p)\n", __func__, index, hccr, hcor);
#ifndef CONFIG_OCTEON_OCX
	if (OCTEON_IS_MODEL(OCTEON_CN78XX) && index > 0) {
		printf("%s: Error: max controller count must be 1 for 78xx\n",
		       __func__);
		return -1;
	}
#else
	if (octeon_has_feature(OCTEON_FEATURE_MULTINODE)) {
		if (!(gd->arch.node_mask & (1 << index))) {
			debug("Octeon OCX node %d not present.\n", index);
			return -1;
		}
		index *= 2;
	}
#endif
	ctx->hcd = (struct xhci_hcrr *)(OCTEON_XHCI_HCCR_BASE(index));
	ctx->intf = index;
	ctx->dwc3 = (struct dwc3 *)OCTEON_XHCI_DWC3_BASE(index);
	ctx->node = 0;

	if (fdt_node_offset_by_compatible(gd->fdt_blob, 0,
					  "cavium,octeon-7130-usb-uctl") < 0) {
		puts("USB not available.\n");
		return -1;
	}
	ret = board_usb_init(index, USB_INIT_HOST);

	if (ret != 0) {
		puts("Failed to initialize board for USB\n");
		return ret;
	}

	ret = octeon3_usb_clocks_start();
	debug("USB %d clocks initialized\n", index);
	if (ret < 0) {
		puts("Failed to initialize xhci\n");
		return ret;
	}

	octeon3_usb_set_endian_mode(ctx->node, index & 1);

	ret = dwc3_core_init(ctx->dwc3, index);
	if (ret) {
		printf("%s(%d) DWC3 core init failed\n", __func__, index);
		return ret;
	}

	/* We are hard-coding DWC3 core to Host Mode */
	dwc3_set_mode(ctx->dwc3, DWC3_GCTL_PRTCAP_HOST);

	*hccr = (struct xhci_hccr *)(OCTEON_XHCI_BASE(index));
	ctx->hccr =*hccr;
	debug("  hccr: 0x%p, hccr->cr_capbase: %p\n", *hccr,
	      &(*hccr)->cr_capbase);
	*hcor = (struct xhci_hcor *)((uint32_t) *hccr
				+ HC_LENGTH(xhci_readl(&(*hccr)->cr_capbase)));
	ctx->hcor = *hcor;
	debug("octeon-xhci: init hccr %p and hcor %p hc_length %d\n",
	      *hccr, *hcor,
	      (uint32_t)HC_LENGTH(xhci_readl(&(*hccr)->cr_capbase)));

	return ret;
}

/* Put XHCI interface into test mode */
int octeon_xhci_test_usb2(struct xhci_hcor *hcor, int dev, int index)
{
	uint32_t cmd, reg;
	int mode;

	mode = (index >> 8) & 0xf;

	/* Program the Test Mode bits in the PORTPMSC_20X register */
	reg = le32_to_cpu(cvmx_read64_uint32(CVMX_USBDRDX_UAHC_PORTPMSC_20X(dev, 0)));
	debug("portpmsc_20x: 0x%x\n ", reg);

	reg |= (mode << 28);
	cvmx_write64_uint32(CVMX_USBDRDX_UAHC_PORTPMSC_20X(dev, 0),
			    cpu_to_le32(reg));

	reg = le32_to_cpu(cvmx_read64_uint32(CVMX_USBDRDX_UAHC_PORTPMSC_20X(dev, 0)));
	debug("portpmsc_20x: 0x%x\n", reg);

	/* For force enable, turn Run/Stop back on */
	if (mode == 5) {
		cmd = xhci_readl(&hcor->or_usbcmd);
		cmd |= CMD_RUN;
		xhci_writel(&hcor->or_usbcmd, cmd);
	}

	return 0;
}

void xhci_hcd_stop(int index)
{
	struct octeon_xhci *ctx = &octeon_xhci[index];

	debug("%s(%d)\n", __func__, index);
#ifdef CONFIG_OCTEON_OCX
	if (octeon_has_feature(OCTEON_FEATURE_MULTINODE)) {
		if (!(gd->arch.node_mask & (1 << index))) {
			debug("Octeon OCX node %d not present.\n", index);
			return;
		}
	}
#endif
	octeon3_usb_clocks_stop(ctx);

	/* Kill power to USB port */
	if (ctx->power_gpio >= 0) {
		gpio_direction_output(ctx->power_gpio, ctx->power_active_low);
		/* see 7.1.6 tSrcRecover */
		mdelay(500);
	}

	memset(ctx, 0, sizeof(*ctx));
}
