/***********************license start************************************
 * Copyright (c) 2011 Cavium, Inc. (support@cavium.com). All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of
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
#ifndef __OCTEON_NETWORK_H__
#define __OCTEON_NETWORK_H__

/** Enable the OCTEON Ethernet driver */
#define CONFIG_OCTEON_ETH

/**
 * This is required to support multiple network interfaces and is required
 * for all OCTEON ethernet support.
 */
#define CONFIG_NET_MULTI

/** Enables MII support */
#define CONFIG_MII

/** Generates and validates the UDP checksum in U-Boot */
#define CONFIG_UDP_CHECKSUM

/** Support time offset feature */
#define CONFIG_BOOTP_TIMEOFFSET

/** Get default gateway from DHCP */
#define CONFIG_BOOTP_GATEWAY

/** Get DNS server from DHCP */
#define CONFIG_BOOTP_DNS

/** Get secondary DHCP server from DHCP */
#define CONFIG_BOOTP_DNS2

/** Obtain our hostname from DHCP */
#define CONFIG_BOOTP_HOSTNAME

/** Get pathname of the client's root disk */
#define CONFIG_BOOTP_BOOTPATH

/** Get size of boot file */
#define CONFIG_BOOTP_BOOTFILESIZE

/** Get our subnet mask from DHCP */
#define CONFIG_BOOTP_SUBNETMASK

/** Send our hostname to the DHCP server */
#define CONFIG_BOOTP_SEND_HOSTNAME

/** Get the TFTP server IP address from DHCP server */
#define CONFIG_BOOTP_TFTP_SERVERIP

/** Enable network console support */
#define CONFIG_NETCONSOLE

/** Enable MDIO support */
#define CONFIG_OCTEON_MDIO

/** Enable U-Boot PHY Library support */
#define CONFIG_PHYLIB

#endif /* __OCTEON_NETWORK_H__ */