#ifndef __OCTEON_EBB7800_SHARED_H__
#define __OCTEON_EBB7800_SHARED_H__

/*
 * Define this to use SPD data defined in header files instead of
 * using TWSI to access the SPDs in the DIMMs.  This is a workaround
 * for boards that lack working TWSI interfaces.
 */

/* 1-DIMM, 4-LMC */
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_4LMC0(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},   \
	{{0x0, 0x0}, {NULL, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_4LMC1(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},   \
	{{0x0, 0x0}, {NULL, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_4LMC2(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},   \
	{{0x0, 0x0}, {NULL, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_4LMC3(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},   \
	{{0x0, 0x0}, {NULL, NULL}}

/* 2-DIMM, 4-LMC */
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_4LMC0(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {spd_pointer, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_4LMC1(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {spd_pointer, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_4LMC2(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {spd_pointer, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_4LMC3(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {spd_pointer, NULL}}

/* 1-DIMM, 2-LMC */
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_2LMC0(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {NULL, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_2LMC1(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {NULL, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_2LMC2(spd_pointer) \
	{{0x0, 0x0}, {NULL, NULL}},\
	{{0x0, 0x0}, {NULL, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_1DIMM_2LMC3(spd_pointer) \
	{{0x0, 0x0}, {NULL, NULL}},\
	{{0x0, 0x0}, {NULL, NULL}}

/* 2-DIMM, 2-LMC */
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_2LMC0(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {spd_pointer, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_2LMC1(spd_pointer) \
	{{0x0, 0x0}, {spd_pointer, NULL}},\
	{{0x0, 0x0}, {spd_pointer, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_2LMC2(spd_pointer) \
	{{0x0, 0x0}, {NULL, NULL}},\
	{{0x0, 0x0}, {NULL, NULL}}
#define OCTEON_DRAM_SOCKET_CONFIG_2DIMM_2LMC3(spd_pointer) \
	{{0x0, 0x0}, {NULL, NULL}},\
	{{0x0, 0x0}, {NULL, NULL}}

#define WD3UN802G13LSD_SPD_VALUES \
0x92,0x11,0x0b,0x02,0x03,0x19,0x00,0x01,0x03,0x11,0x01,0x08,0x0c,0x00,0x3c,0x00,\
0x69,0x78,0x69,0x30,0x69,0x11,0x20,0x89,0x00,0x05,0x3c,0x3c,0x00,0xf0,0x83,0x01,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x01,0x20,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x01,0x61,0x00,0x12,0x30,0x00,0x00,0x00,0x00,0xb3,0x31

#define WD3UE02GX818_1333L_CT_VALUES \
0x92,0x10,0x0b,0x02,0x02,0x11,0x00,0x09,0x0b,0x52,0x01,0x08,0x0c,0x00,0x3c,0x00,\
0x69,0x78,0x69,0x30,0x69,0x11,0x20,0x89,0x70,0x03,0x3c,0x3c,0x00,0xf0,0x83,0x05,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x04,0x01,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x01,0x61,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x99,0xc0

#define TS512MLK72V8N_VALUES \
0x92,0x10,0x0b,0x02,0x03,0x19,0x00,0x09,0x0b,0x11,0x01,0x08,0x09,0x00,0xfc,0x02,\
0x69,0x78,0x69,0x28,0x69,0x11,0x10,0x79,0x00,0x05,0x3c,0x3c,0x00,0xd8,0x83,0x01,\
0x80,0x00,0xca,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x64,0x01,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x01,0x4f,0x54,0x13,0x49,0x00,0x00,0x11,0x12,0x0c,0x22

#define HMT451R7AFR8A_PB_T8_AB_VALUES \
0x92,0x12,0x0b,0x01,0x04,0x21,0x02,0x01,0x0b,0x52,0x01,0x08,0x0a,0x00,0xfc,0x00,\
0x69,0x78,0x69,0x30,0x69,0x11,0x18,0x81,0x20,0x08,0x3c,0x3c,0x00,0xf0,0x83,0x01,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x84,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x20,0x05,\
0x00,0x80,0xb3,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x80,0xad,0x01,0x13,0x34,0x3b,0x65,0x18,0x8d,0xa9,0x09

#define HMT351R7BFR8C_PB_T2_AB_VALUES \
0x92,0x10,0x0b,0x01,0x03,0x19,0x00,0x09,0x0b,0x52,0x01,0x08,0x0a,0x00,0xfc,0x00,\
0x69,0x78,0x69,0x30,0x69,0x11,0x18,0x81,0x00,0x05,0x3c,0x3c,0x00,0xf0,0x83,0x05,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x01,0x05,\
0x00,0x04,0xb3,0x11,0x00,0x00,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x80,0xad,0x01,0x11,0x03,0x0d,0x69,0x1d,0x07,0x07,0x29

#define KVR1333D3D8R9S4GHB_VALUES \
0x92,0x10,0x0b,0x01,0x03,0x19,0x00,0x09,0x0b,0x52,0x01,0x08,0x0c,0x00,0x3c,0x00,\
0x69,0x78,0x69,0x30,0x69,0x11,0x20,0x89,0x00,0x05,0x3c,0x3c,0x00,0xf0,0x83,0x01,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x01,0x05,\
0x00,0x80,0x97,0x1d,0x00,0x00,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x01,0x98,0x04,0x10,0x50,0x67,0x1a,0x90,0xd9,0xd6,0xd2

#define KVR1066D3Q8R7S8G_VALUES \
0x92,0x12,0x0b,0x01,0x03,0x19,0x00,0x19,0x0b,0x11,0x01,0x08,0x0f,0x00,0x1c,0x00,\
0x69,0x78,0x69,0x3c,0x69,0x11,0x2c,0x95,0x00,0x05,0x3c,0x3c,0x01,0x2c,0x83,0x81,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x07,0x09,\
0x00,0x86,0x32,0x10,0x00,0x00,0x50,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x01,0x98,0x04,0x13,0x27,0x38,0x02,0xf9,0x4e,0xa9,0xed

#define MDFC2GB5H0NF4_SPD_VALUES \
0x92,0x11,0x0b,0x01,0x04,0x21,0x00,0x19,0x0b,0x11,0x01,0x08,0x0a,0x00,0xfc,0x00,\
0x69,0x78,0x69,0x30,0x69,0x11,0x18,0x81,0x20,0x08,0x3c,0x3c,0x00,0xf0,0x83,0x01,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x07,0x09,\
0x00,0x80,0x97,0x1d,0x00,0x00,0x50,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0x29,0x00,0x00,0x00,0x00,0xad,0x38

#define MT18JSF1G72AZ_1G9_SPD_VALUES \
0x92,0x13,0x0b,0x02,0x04,0x21,0x00,0x09,0x0b,0x11,0x01,0x08,0x09,0x00,0xfe,0x02,\
0x69,0x78,0x69,0x28,0x69,0x11,0x10,0x79,0x20,0x08,0x3c,0x3c,0x00,0xc8,0x83,0x05,\
0x80,0x00,0xca,0x00,0x00,0x00,0x00,0x00,0x00,0x84,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x64,0x01,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x80,0x2c,0x0f,0x14,0x13,0x40,0x25,0x83,0xd5,0x56,0xbf

#define MT36JSF2G72PZ_1G9E1HE_VALUES \
0x92,0x13,0x0b,0x01,0x04,0x22,0x00,0x08,0x0b,0x11,0x01,0x08,0x09,0x00,0xfe,0x02,\
0x69,0x78,0x69,0x28,0x69,0x11,0x10,0x79,0x20,0x08,0x3c,0x3c,0x00,0xc8,0x83,0x05,\
0x80,0x00,0xca,0x00,0x00,0x00,0x00,0x00,0x00,0x84,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x44,0x09,\
0x00,0x80,0xb3,0x63,0x00,0x00,0x50,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x80,0x2c,0x0f,0x13,0x43,0xe7,0x7b,0x0f,0x50,0x02,0xda

#define MT36KSF2G72PZ_1G6E1HF_VALUES \
0x92,0x13,0x0b,0x01,0x04,0x22,0x02,0x08,0x0b,0x11,0x01,0x08,0x0a,0x00,0xfe,0x00,\
0x69,0x78,0x69,0x30,0x69,0x11,0x18,0x81,0x20,0x08,0x3c,0x3c,0x00,0xf0,0x83,0x05,\
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x84,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x11,0x44,0x09,\
0x00,0x80,0xb3,0x63,0x00,0x00,0x50,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x80,0x2c,0x01,0x14,0x04,0x40,0x20,0xd5,0x06,0xaf,0x23

/* WD3UN802G13LSD_SPD_VALUES		ID: 86 Passed 1333 1-dimm, 4-lmc */
/* WD3UE02GX818_1333L_CT_VALUES		ID: 17 Passed 1333 2-dimm, 4-lmc */
/* TS512MLK72V8N_VALUES			ID: 85 Passed 1866 1-dimm, 2-lmc */
/* HMT451R7AFR8A_PB_T8_AB_VALUES	ID: 84 Passed 1066 2-dimm, 4-lmc */
/* HMT351R7BFR8C_PB_T2_AB_VALUES	ID: 48 Failed 2-dimm, 2-lmc */
/* KVR1333D3D8R9S4GHB_VALUES		ID: 59 Passed 1066 2-dimm, 4-lmc */
/* KVR1066D3Q8R7S8G_VALUES		No ID  Passed 1066 1-dimm, 4-lmc */
/* MDFC2GB5H0NF4_SPD_VALUES		NO ID  FROM30  1333 1-dimm, 4-lmc, quad-rank */
/* MT18JSF1G72AZ_1G9_SPD_VALUES		ID: 87 */
/* MT36JSF2G72PZ_1G9E1HE_VALUES		ID: 88 */
/* MT36KSF2G72PZ_1G6E1HF_VALUES		ID: 89 */

/*-------------------------------------------------------------------*/
/* Configuration 0: gpio 1 = OFF */
/*-------------------------------------------------------------------*/
/* Select SPD from the list. */
#define OCTEON_EBB7800_CFG0_SPD_VALUES	WD3UN802G13LSD_SPD_VALUES

/* Select socket configuration.
 * _1DIMM_4LMC _2DIMM_4LMC
 * _1DIMM_2LMC _2DIMM_2LMC
 */
#define OCTEON_EBB7800_CFG0_DIMM_LMC_CONFIGURATION _1DIMM_4LMC
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Configuration 1: gpio 1 = ON */
/*-------------------------------------------------------------------*/
/* Select SPD from the list. */
#define OCTEON_EBB7800_CFG1_SPD_VALUES	TS512MLK72V8N_VALUES

/* Select socket configuration.
 * _1DIMM_4LMC _2DIMM_4LMC
 * _1DIMM_2LMC _2DIMM_2LMC
 */
#define OCTEON_EBB7800_CFG1_DIMM_LMC_CONFIGURATION _1DIMM_4LMC
/*-------------------------------------------------------------------*/


#define MERGE_AGAIN(X, Y) X ## Y
#define MERGE(X, Y) MERGE_AGAIN(X, Y)
#define OCTEON_EBB7800_CFG0_DRAM_SOCKET_CONFIGURATION0	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG0_DIMM_LMC_CONFIGURATION), 0)(octeon_ebb7800_cfg0_spd_values)
#define OCTEON_EBB7800_CFG0_DRAM_SOCKET_CONFIGURATION1	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG0_DIMM_LMC_CONFIGURATION), 1)(octeon_ebb7800_cfg0_spd_values)
#define OCTEON_EBB7800_CFG0_DRAM_SOCKET_CONFIGURATION2	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG0_DIMM_LMC_CONFIGURATION), 2)(octeon_ebb7800_cfg0_spd_values)
#define OCTEON_EBB7800_CFG0_DRAM_SOCKET_CONFIGURATION3	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG0_DIMM_LMC_CONFIGURATION), 3)(octeon_ebb7800_cfg0_spd_values)

#define OCTEON_EBB7800_CFG1_DRAM_SOCKET_CONFIGURATION0	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG1_DIMM_LMC_CONFIGURATION), 0)(octeon_ebb7800_cfg1_spd_values)
#define OCTEON_EBB7800_CFG1_DRAM_SOCKET_CONFIGURATION1	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG1_DIMM_LMC_CONFIGURATION), 1)(octeon_ebb7800_cfg1_spd_values)
#define OCTEON_EBB7800_CFG1_DRAM_SOCKET_CONFIGURATION2	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG1_DIMM_LMC_CONFIGURATION), 2)(octeon_ebb7800_cfg1_spd_values)
#define OCTEON_EBB7800_CFG1_DRAM_SOCKET_CONFIGURATION3	MERGE( MERGE( OCTEON_DRAM_SOCKET_CONFIG, OCTEON_EBB7800_CFG1_DIMM_LMC_CONFIGURATION), 3)(octeon_ebb7800_cfg1_spd_values)


#define OCTEON_EBB7800_DRAM_SOCKET_CONFIGURATION0     {{0x1050, 0x0}, {NULL, NULL}},{{0x1051, 0x0}, {NULL, NULL}}
#define OCTEON_EBB7800_DRAM_SOCKET_CONFIGURATION1     {{0x1052, 0x0}, {NULL, NULL}},{{0x1053, 0x0}, {NULL, NULL}}
#define OCTEON_EBB7800_DRAM_SOCKET_CONFIGURATION2     {{0x1054, 0x0}, {NULL, NULL}},{{0x1055, 0x0}, {NULL, NULL}}
#define OCTEON_EBB7800_DRAM_SOCKET_CONFIGURATION3     {{0x1056, 0x0}, {NULL, NULL}},{{0x1057, 0x0}, {NULL, NULL}}

#define OCTEON_EBB7800_BOARD_EEPROM_TWSI_ADDR        (0x56)



/*
 * Local copy of these parameters to allow for customization for this
 * board design.  The generic version resides in lib_octeon_shared.h.
 */

/* LMC0_MODEREG_PARAMS1 */
#define OCTEON_EBB7800_MODEREG_PARAMS1_1RANK_1SLOT      \
    { .cn78xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = 0,                    \
                  .dic_00       = 0,                    \
                  .rtt_nom_00   = rttnom_40ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = 0,                    \
                  .rtt_nom_01   = 0,                    \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = 0,                    \
                  .dic_10       = 0,                    \
                  .rtt_nom_10   = 0,                    \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = 0,                    \
                  .rtt_nom_11   = 0,                    \
        }                                               \
    }

#define OCTEON_EBB7800_MODEREG_PARAMS1_1RANK_2SLOT      \
    { .cn78xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = rttwr_120ohm,         \
                  .dic_00       = 0,                    \
                  .rtt_nom_00   = rttnom_20ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = 0,                    \
                  .rtt_nom_01   = 0,                    \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = rttwr_120ohm,         \
                  .dic_10       = 0,                    \
                  .rtt_nom_10   = rttnom_20ohm,         \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = 0,                    \
                  .rtt_nom_11   = 0                     \
        }                                               \
    }

#define OCTEON_EBB7800_MODEREG_PARAMS1_2RANK_1SLOT      \
    { .cn78xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = 0,                    \
                  .dic_00       = 0,                    \
                  .rtt_nom_00   = rttnom_40ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = 0,                    \
                  .rtt_nom_01   = 0,                    \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = 0,                    \
                  .dic_10       = 0,                    \
                  .rtt_nom_10   = 0,                    \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = 0,                    \
                  .rtt_nom_11   = 0,                    \
        }                                               \
    }

#define OCTEON_EBB7800_MODEREG_PARAMS1_2RANK_2SLOT      \
    { .cn78xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = 0,                    \
                  .dic_00       = dic_34ohm,            \
                  .rtt_nom_00   = rttnom_20ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = dic_34ohm,            \
                  .rtt_nom_01   = rttnom_60ohm,         \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = 0,                    \
                  .dic_10       = dic_34ohm,            \
                  .rtt_nom_10   = rttnom_20ohm,         \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = dic_34ohm,            \
                  .rtt_nom_11   = rttnom_60ohm,         \
        }                                               \
    }

#define OCTEON_EBB7800_MODEREG_PARAMS1_4RANK_1SLOT      \
    { .cn78xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = rttwr_60ohm,         \
                  .dic_00       = dic_34ohm,            \
                  .rtt_nom_00   = rttnom_20ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = rttwr_60ohm,         \
                  .dic_01       = dic_34ohm,            \
                  .rtt_nom_01   = rttnom_none,          \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = rttwr_60ohm,         \
                  .dic_10       = dic_34ohm,            \
                  .rtt_nom_10   = rttnom_20ohm,         \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = rttwr_60ohm,         \
                  .dic_11       = dic_34ohm,            \
                  .rtt_nom_11   = rttnom_none,          \
        }                                               \
    }


#define OCTEON_EBB7800_CN78XX_DRAM_ODT_1RANK_CONFIGURATION \
    /* DIMMS    DQX_CTL  WODT_MASK                LMCX_MODEREG_PARAMS1             reserved       RODT_CTL    RODT_MASK    reserved */ \
    /* =====   ======== ============== ==========================================  ============== ========= ============== ======== */ \
    /*   1 */ {   4,    0x00000001ULL, OCTEON_EBB7800_MODEREG_PARAMS1_1RANK_1SLOT, {.u64=0x0000},     2,     0x00000000ULL,    0  },   \
    /*   2 */ {   4,    0x00050005ULL, OCTEON_EBB7800_MODEREG_PARAMS1_1RANK_2SLOT, {.u64=0x0000},     3,     0x00010004ULL,    0  }

#define OCTEON_EBB7800_CN78XX_DRAM_ODT_2RANK_CONFIGURATION \
    /* DIMMS    DQX_CTL  WODT_MASK                LMCX_MODEREG_PARAMS1             reserved       RODT_CTL    RODT_MASK    reserved */ \
    /* =====   ======== ============== ==========================================  ============== ========= ============== ======== */ \
    /*   1 */ {   4,    0x00000101ULL, OCTEON_EBB7800_MODEREG_PARAMS1_2RANK_1SLOT, {.u64=0x0000},     3,     0x00000000ULL,    0  },   \
    /*   2 */ {   4,    0x09090606ULL, OCTEON_EBB7800_MODEREG_PARAMS1_2RANK_2SLOT, {.u64=0x0000},     3,     0x01010404ULL,    0  }

#define OCTEON_EBB7800_CN78XX_DRAM_ODT_4RANK_CONFIGURATION \
    /* DIMMS    DQX_CTL  WODT_MASK                LMCX_MODEREG_PARAMS1             reserved       RODT_CTL    RODT_MASK    reserved */ \
    /* =====   ======== ============== ==========================================  ============== ========= ============== ======== */ \
    /*   1 */ {   4,    0x01030203ULL, OCTEON_EBB7800_MODEREG_PARAMS1_4RANK_1SLOT, {.u64=0x0000},     3,     0x01010202ULL,    0  }

/* Construct a static initializer for the ddr_configuration_t variable that holds
** (almost) all of the information required for DDR initialization.
*/

/*
  The parameters below make up the custom_lmc_config data structure.
  This structure is used to customize the way that the LMC DRAM
  Controller is configured for a particular board design.

  Refer to the file lib_octeon_board_table_entry.h for a description
  of the custom board settings.  It is usually kept in the following
  location... arch/mips/include/asm/arch-octeon/

*/

#define OCTEON_EBB7800_DDR_CONFIGURATION(SPD_CONFIGURATION)             \
    /* Interface 0 */                                                   \
    {                                                                   \
        .custom_lmc_config = {                                          \
            .min_rtt_nom_idx	= 1,                                    \
            .max_rtt_nom_idx	= 5,                                    \
            .min_rodt_ctl	= 1,                                    \
            .max_rodt_ctl	= 5,                                    \
            .dqx_ctl		= 4,                                    \
            .ck_ctl		= 4,                                    \
            .cmd_ctl		= 4,                                    \
            .ctl_ctl		= 4,                                    \
            .min_cas_latency	= 0,                                    \
            .offset_en 		= 1,                                    \
            .offset_udimm	= 2,                                    \
            .offset_rdimm	= 2,                                    \
            .ddr_rtt_nom_auto	= 0,                                    \
            .ddr_rodt_ctl_auto	= 0,                                    \
            .rlevel_comp_offset_udimm	= 7,                            \
            .rlevel_comp_offset_rdimm	= 7,                            \
            .rlevel_compute 	= 0,                                    \
            .ddr2t_udimm 	= 0,                                    \
            .ddr2t_rdimm 	= 1,                                    \
            .maximum_adjacent_rlevel_delay_increment = 2,               \
            .fprch2		= 2,                                    \
            .dll_write_offset   = {0},                                  \
            .dll_read_offset    = {0},                                  \
            .parity		= 0},                                   \
            .dimm_config_table = {                                      \
             SPD_CONFIGURATION##0,                                      \
             DIMM_CONFIG_TERMINATOR},                                   \
                 .unbuffered = {                                        \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                      .registered = {                                   \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                           .odt_1rank_config = {                        \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_1RANK_CONFIGURATION},        \
                                .odt_2rank_config = {                   \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_2RANK_CONFIGURATION},        \
                                     .odt_4rank_config = {              \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_4RANK_CONFIGURATION}         \
    },                                                                  \
    /* Interface 1 */                                                   \
    {                                                                   \
        .custom_lmc_config = {                                          \
            .min_rtt_nom_idx	= 1,                                    \
            .max_rtt_nom_idx	= 5,                                    \
            .min_rodt_ctl	= 1,                                    \
            .max_rodt_ctl	= 5,                                    \
            .dqx_ctl		= 4,                                    \
            .ck_ctl		= 4,                                    \
            .cmd_ctl		= 4,                                    \
            .ctl_ctl		= 4,                                    \
            .min_cas_latency	= 0,                                    \
            .offset_en 		= 1,                                    \
            .offset_udimm	= 2,                                    \
            .offset_rdimm	= 2,                                    \
            .ddr_rtt_nom_auto	= 0,                                    \
            .ddr_rodt_ctl_auto	= 0,                                    \
            .rlevel_comp_offset_udimm	= 7,                            \
            .rlevel_comp_offset_rdimm	= 7,                            \
            .rlevel_compute 	= 0,                                    \
            .ddr2t_udimm 	= 0,                                    \
            .ddr2t_rdimm 	= 1,                                    \
            .maximum_adjacent_rlevel_delay_increment = 2,               \
            .fprch2		= 2,                                    \
            .dll_write_offset   = {0},                                  \
            .dll_read_offset    = {0},                                  \
            .parity		= 0},                                   \
            .dimm_config_table = {                                      \
             SPD_CONFIGURATION##1,                                      \
             DIMM_CONFIG_TERMINATOR},                                   \
                 .unbuffered = {                                        \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                      .registered = {                                   \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                           .odt_1rank_config = {                        \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_1RANK_CONFIGURATION},        \
                                .odt_2rank_config = {                   \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_2RANK_CONFIGURATION},        \
                                     .odt_4rank_config = {              \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_4RANK_CONFIGURATION}         \
    },                                                                  \
    /* Interface 2 */                                                   \
    {                                                                   \
        .custom_lmc_config = {                                          \
            .min_rtt_nom_idx	= 1,                                    \
            .max_rtt_nom_idx	= 5,                                    \
            .min_rodt_ctl	= 1,                                    \
            .max_rodt_ctl	= 5,                                    \
            .dqx_ctl		= 4,                                    \
            .ck_ctl		= 4,                                    \
            .cmd_ctl		= 4,                                    \
            .ctl_ctl		= 4,                                    \
            .min_cas_latency	= 0,                                    \
            .offset_en 		= 1,                                    \
            .offset_udimm	= 2,                                    \
            .offset_rdimm	= 2,                                    \
            .ddr_rtt_nom_auto	= 0,                                    \
            .ddr_rodt_ctl_auto	= 0,                                    \
            .rlevel_comp_offset_udimm	= 7,                            \
            .rlevel_comp_offset_rdimm	= 7,                            \
            .rlevel_compute 	= 0,                                    \
            .ddr2t_udimm 	= 0,                                    \
            .ddr2t_rdimm 	= 1,                                    \
            .maximum_adjacent_rlevel_delay_increment = 2,               \
            .fprch2		= 2,                                    \
            .dll_write_offset   = {0},                                  \
            .dll_read_offset    = {0},                                  \
            .parity		= 0},                                   \
            .dimm_config_table = {                                      \
             SPD_CONFIGURATION##2,                                      \
             DIMM_CONFIG_TERMINATOR},                                   \
                 .unbuffered = {                                        \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                      .registered = {                                   \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                           .odt_1rank_config = {                        \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_1RANK_CONFIGURATION},        \
                                .odt_2rank_config = {                   \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_2RANK_CONFIGURATION},        \
                                     .odt_4rank_config = {              \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_4RANK_CONFIGURATION}         \
    },                                                                  \
    /* Interface 3 */                                                   \
    {                                                                   \
        .custom_lmc_config = {                                          \
            .min_rtt_nom_idx	= 1,                                    \
            .max_rtt_nom_idx	= 5,                                    \
            .min_rodt_ctl	= 1,                                    \
            .max_rodt_ctl	= 5,                                    \
            .dqx_ctl		= 4,                                    \
            .ck_ctl		= 4,                                    \
            .cmd_ctl		= 4,                                    \
            .ctl_ctl		= 4,                                    \
            .min_cas_latency	= 0,                                    \
            .offset_en 		= 1,                                    \
            .offset_udimm	= 2,                                    \
            .offset_rdimm	= 2,                                    \
            .ddr_rtt_nom_auto	= 0,                                    \
            .ddr_rodt_ctl_auto	= 0,                                    \
            .rlevel_comp_offset_udimm	= 7,                            \
            .rlevel_comp_offset_rdimm	= 7,                            \
            .rlevel_compute 	= 0,                                    \
            .ddr2t_udimm 	= 0,                                    \
            .ddr2t_rdimm 	= 1,                                    \
            .maximum_adjacent_rlevel_delay_increment = 2,               \
            .fprch2		= 2,                                    \
            .dll_write_offset   = {0},                                  \
            .dll_read_offset    = {0},                                  \
            .parity		= 0},                                   \
            .dimm_config_table = {                                      \
             SPD_CONFIGURATION##3,                                      \
             DIMM_CONFIG_TERMINATOR},                                   \
                 .unbuffered = {                                        \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                      .registered = {                                   \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                           .odt_1rank_config = {                        \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_1RANK_CONFIGURATION},        \
                                .odt_2rank_config = {                   \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_2RANK_CONFIGURATION},        \
                                     .odt_4rank_config = {              \
            OCTEON_EBB7800_CN78XX_DRAM_ODT_4RANK_CONFIGURATION}         \
    }

#endif   /* __OCTEON_EBB7800_SHARED_H__ */
