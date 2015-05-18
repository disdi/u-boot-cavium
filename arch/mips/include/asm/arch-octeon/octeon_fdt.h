/***********************license start************************************
 * Copyright (c) 2011 Cavium, Inc. (support@cavium.com).  All rights
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
#ifndef __OCTEON_FDT_H__
#define __OCTEON_FDT_H__
/**
 *
 * $Id$
 *
 */

/**
 * Trims nodes from the flat device tree.
 *
 * @param fdt - pointer to working FDT, usually in gd->fdt_blob
 * @param fdt_key - key to preserve.  All non-matching keys are removed
 * @param trim_name - name of property to look for.  If NULL use
 * 		      'cavium,qlm-trim'
 * @param rename - set to TRUE to rename interfaces.
 * @param callback - function to call on matched nodes.
 * @param cbarg - passed to callback.
 *
 * The key should look something like device #, type where device # is a
 * number from 0-9 and type is a string describing the type.  For QLM
 * operations this would typically contain the QLM number followed by
 * the type in the device tree, like "0,xaui", "0,sgmii", etc.  This function
 * will trim all items in the device tree which match the device number but
 * have a type which does not match.  For example, if a QLM has a xaui module
 * installed on QLM 0 and "0,xaui" is passed as a key, then all FDT nodes that
 * have "0,xaui" will be preserved but all others, i.e. "0,sgmii" will be
 * removed.
 *
 * Note that the trim_name must also match.  If trim_name is NULL then it
 * looks for the property "cavium,qlm-trim".
 *
 * Also, when the trim_name is "cavium,qlm-trim" or NULL that the interfaces
 * will also be renamed based on their register values.
 *
 * For example, if a PIP interface is named "interface@W" and has the property
 * reg = <0> then the interface will be renamed after this function to
 * interface@0.
 *
 * @return 0 for success.
 */
int octeon_fdt_patch_rename(void *fdt, const char *fdt_key,
			    const char *trim_name, bool rename,
			    void (*callback)(void *fdt, int offset, void *arg),
			    void *cbarg);

/**
 * Trims nodes from the flat device tree.
 *
 * @param fdt - pointer to working FDT, usually in gd->fdt_blob
 * @param fdt_key - key to preserve.  All non-matching keys are removed
 * @param trim_name - name of property to look for.  If NULL use
 * 		      'cavium,qlm-trim'
 *
 * The key should look something like device #, type where device # is a
 * number from 0-9 and type is a string describing the type.  For QLM
 * operations this would typically contain the QLM number followed by
 * the type in the device tree, like "0,xaui", "0,sgmii", etc.  This function
 * will trim all items in the device tree which match the device number but
 * have a type which does not match.  For example, if a QLM has a xaui module
 * installed on QLM 0 and "0,xaui" is passed as a key, then all FDT nodes that
 * have "0,xaui" will be preserved but all others, i.e. "0,sgmii" will be
 * removed.
 *
 * Note that the trim_name must also match.  If trim_name is NULL then it
 * looks for the property "cavium,qlm-trim".
 *
 * Also, when the trim_name is "cavium,qlm-trim" or NULL that the interfaces
 * will also be renamed based on their register values.
 *
 * For example, if a PIP interface is named "interface@W" and has the property
 * reg = <0> then the interface will be renamed after this function to
 * interface@0.
 *
 * @return 0 for success.
 */
int octeon_fdt_patch(void *fdt, const char *fdt_key, const char *trim_name);

int board_fixup_fdt(void);
void octeon_fixup_fdt(void);

/**
 * This is a helper function to find the offset of a PHY device given
 * an Ethernet device.
 *
 * @param[in] eth - Ethernet device to search for PHY offset
 *
 * @returns offset of phy info in device tree or -1 if not found
 */
int octeon_fdt_find_phy(const struct eth_device *eth);

/**
 * This helper function returns if a node contains the specified vendor name.
 *
 * @param[in]	fdt		pointer to device tree blob
 * @param	nodeoffset	offset of the tree node
 * @param[in]	vendor		name of vendor to check
 *
 * returns:
 *	0, if the node has a compatible vendor string property
 *	1, if the node does not contain the vendor string property
 *	-FDT_ERR_NOTFOUND, if the given node has no 'compatible' property
 *	-FDT_ERR_BADOFFSET, if nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int octeon_fdt_compat_vendor(const void *fdt, int nodeoffset,
			     const char *vendor);

/**
 * Given a node in the device tree get the OCTEON OCX node number
 *
 * @param fdt		pointer to flat device tree
 * @param nodeoffset	node offset to get OCX node for
 *
 * @return the Octeon OCX node number
 */
int octeon_fdt_get_soc_node(const void *fdt, int nodeoffset);

#endif /* __OCTEON_FDT_H__ */
