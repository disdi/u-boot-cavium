#ifndef __OCTEON_HAL_H__
#define __OCTEON_HAL_H__

#if !defined(__U_BOOT__)
#include <cvmx.h>
#else
#include "octeon_csr.h"
#endif

/* Provide alias for __octeon_is_model_runtime__ */
#define octeon_is_model(x)     __octeon_is_model_runtime__(x)

#define OCTEON_PCI_IOSPACE_BASE     0x80011a0400000000ull

static inline int cvmx_octeon_fuse_locked (void)
{
	return cvmx_fuse_read (123);
}

#endif
