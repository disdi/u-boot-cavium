#ifndef _OCTEON_BIENDIAN_H
#define _OCTEON_BIENDIAN_H

#include <elf.h>

struct elf_accessors {
	uint16_t (*w16)(uint16_t);
	uint32_t (*w32)(uint32_t);
	uint64_t (*w64)(uint64_t);
};

struct elf_accessors *get_elf_accessors(unsigned long addr);

static inline int is_little_endian_elf(unsigned long addr)
{
	return ((Elf32_Ehdr *)addr)->e_ident[EI_DATA] == ELFDATA2LSB;
}

#endif
