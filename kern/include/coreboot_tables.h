/*
 * This file is part of the libpayload project.
 *
 * Copyright (C) 2008 Advanced Micro Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _COREBOOT_TABLES_H
#define _COREBOOT_TABLES_H

#include <arch/types.h>

/* Maximum number of memory range definitions. */
#define SYSINFO_MAX_MEM_RANGES 32
/* Allow a maximum of 8 GPIOs */
#define SYSINFO_MAX_GPIOS 8

struct cb_serial;

struct sysinfo_t {
	unsigned int cpu_khz;
	struct cb_serial *serial;
	unsigned short ser_ioport;
	unsigned long ser_base; // for mmapped serial

	int n_memranges;

	struct memrange {
		unsigned long long base;
		unsigned long long size;
		unsigned int type;
	} memrange[SYSINFO_MAX_MEM_RANGES];

	struct cb_cmos_option_table *option_table;
	uint32_t cmos_range_start;
	uint32_t cmos_range_end;
	uint32_t cmos_checksum_location;
#ifdef CONFIG_CHROMEOS
	uint32_t vbnv_start;
	uint32_t vbnv_size;
#endif

	char *version;
	char *extra_version;
	char *build;
	char *compile_time;
	char *compile_by;
	char *compile_host;
	char *compile_domain;
	char *compiler;
	char *linker;
	char *assembler;

	char *cb_version;

	struct cb_framebuffer *framebuffer;

#ifdef CONFIG_CHROMEOS
	int num_gpios;
	struct cb_gpio gpios[SYSINFO_MAX_GPIOS];
#endif

	unsigned long *mbtable; /** Pointer to the multiboot table */

	struct cb_header *header;
	struct cb_mainboard *mainboard;

	/* these are chromeos specific and may or may not be valid. */
	void	*vboot_handoff;
	uint32_t	vboot_handoff_size;
	void	*vdat_addr;
	uint32_t	vdat_size;

#ifdef CONFIG_X86
	int x86_rom_var_mtrr_index;
#endif

	void	*tstamp_table;
	void	*cbmem_cons;
	void	*mrc_cache;
	void	*acpi_gnvs;
};

extern struct sysinfo_t lib_sysinfo;

struct cbuint64 {
	uint32_t lo;
	uint32_t hi;
};

struct cb_header {
	uint8_t signature[4];
	uint32_t header_bytes;
	uint32_t header_checksum;
	uint32_t table_bytes;
	uint32_t table_checksum;
	uint32_t table_entries;
};

struct cb_record {
	uint32_t tag;
	uint32_t size;
};

#define CB_TAG_UNUSED     0x0000
#define CB_TAG_MEMORY     0x0001

struct cb_memory_range {
	struct cbuint64 start;
	struct cbuint64 size;
	uint32_t type;
};

#define CB_MEM_RAM          1
#define CB_MEM_RESERVED     2
#define CB_MEM_ACPI         3
#define CB_MEM_NVS          4
#define CB_MEM_UNUSABLE     5
#define CB_MEM_VENDOR_RSVD  6
#define CB_MEM_TABLE       16

struct cb_memory {
	uint32_t tag;
	uint32_t size;
	struct cb_memory_range map[0];
};

#define CB_TAG_HWRPB      0x0002

struct cb_hwrpb {
	uint32_t tag;
	uint32_t size;
	uint64_t hwrpb;
};

#define CB_TAG_MAINBOARD  0x0003

struct cb_mainboard {
	uint32_t tag;
	uint32_t size;
	uint8_t vendor_idx;
	uint8_t part_number_idx;
	uint8_t strings[0];
};

#define CB_TAG_VERSION        0x0004
#define CB_TAG_EXTRA_VERSION  0x0005
#define CB_TAG_BUILD          0x0006
#define CB_TAG_COMPILE_TIME   0x0007
#define CB_TAG_COMPILE_BY     0x0008
#define CB_TAG_COMPILE_HOST   0x0009
#define CB_TAG_COMPILE_DOMAIN 0x000a
#define CB_TAG_COMPILER       0x000b
#define CB_TAG_LINKER         0x000c
#define CB_TAG_ASSEMBLER      0x000d

struct cb_string {
	uint32_t tag;
	uint32_t size;
	uint8_t string[0];
};

#define CB_TAG_SERIAL         0x000f

struct cb_serial {
	uint32_t tag;
	uint32_t size;
#define CB_SERIAL_TYPE_IO_MAPPED     1
#define CB_SERIAL_TYPE_MEMORY_MAPPED 2
	uint32_t type;
	uint32_t baseaddr;
	uint32_t baud;
};

#define CB_TAG_CONSOLE       0x00010

struct cb_console {
	uint32_t tag;
	uint32_t size;
	uint16_t type;
};

#define CB_TAG_CONSOLE_SERIAL8250 0
#define CB_TAG_CONSOLE_VGA        1 // OBSOLETE
#define CB_TAG_CONSOLE_BTEXT      2 // OBSOLETE
#define CB_TAG_CONSOLE_LOGBUF     3 // OBSOLETE
#define CB_TAG_CONSOLE_SROM       4 // OBSOLETE
#define CB_TAG_CONSOLE_EHCI       5

#define CB_TAG_FORWARD       0x00011

struct cb_forward {
	uint32_t tag;
	uint32_t size;
	uint64_t forward;
};

#define CB_TAG_FRAMEBUFFER      0x0012
struct cb_framebuffer {
	uint32_t tag;
	uint32_t size;

	uint64_t physical_address;
	uint32_t x_resolution;
	uint32_t y_resolution;
	uint32_t bytes_per_line;
	uint8_t bits_per_pixel;
        uint8_t red_mask_pos;
	uint8_t red_mask_size;
	uint8_t green_mask_pos;
	uint8_t green_mask_size;
	uint8_t blue_mask_pos;
	uint8_t blue_mask_size;
	uint8_t reserved_mask_pos;
	uint8_t reserved_mask_size;
};

#define CB_TAG_GPIO 0x0013
#define CB_GPIO_ACTIVE_LOW 0
#define CB_GPIO_ACTIVE_HIGH 1
#define CB_GPIO_MAX_NAME_LENGTH 16
struct cb_gpio {
	uint32_t port;
	uint32_t polarity;
	uint32_t value;
	uint8_t name[CB_GPIO_MAX_NAME_LENGTH];
};

struct cb_gpios {
	uint32_t tag;
	uint32_t size;

	uint32_t count;
	struct cb_gpio gpios[0];
};

#define CB_TAG_VDAT		0x0015
#define CB_TAG_VBNV		0x0019
#define CB_TAG_VBOOT_HANDOFF	0x0020
#define CB_TAG_DMA		0x0022
struct cb_range {
	uint32_t tag;
	uint32_t size;
	uint64_t range_start;
	uint32_t range_size;
};

#define CB_TAG_TIMESTAMPS	0x0016
#define CB_TAG_CBMEM_CONSOLE	0x0017
#define CB_TAG_MRC_CACHE	0x0018
#define CB_TAG_ACPI_GNVS	0x0024
struct cb_cbmem_tab {
	uint32_t tag;
	uint32_t size;
	uint64_t cbmem_tab;
};

#define CB_TAG_X86_ROM_MTRR	0x0021
struct cb_x86_rom_mtrr {
	uint32_t tag;
	uint32_t size;
	/* The variable range MTRR index covering the ROM. If one wants to
	 * enable caching the ROM, the variable MTRR needs to be set to
	 * write-protect. To disable the caching after enabling set the
	 * type to uncacheable. */
	uint32_t index;
};


#define CB_TAG_CMOS_OPTION_TABLE 0x00c8
struct cb_cmos_option_table {
	uint32_t tag;
	uint32_t size;
	uint32_t header_length;
};

#define CB_TAG_OPTION         0x00c9
#define CB_CMOS_MAX_NAME_LENGTH    32
struct cb_cmos_entries {
	uint32_t tag;
	uint32_t size;
	uint32_t bit;
	uint32_t length;
	uint32_t config;
	uint32_t config_id;
	uint8_t name[CB_CMOS_MAX_NAME_LENGTH];
};


#define CB_TAG_OPTION_ENUM    0x00ca
#define CB_CMOS_MAX_TEXT_LENGTH 32
struct cb_cmos_enums {
	uint32_t tag;
	uint32_t size;
	uint32_t config_id;
	uint32_t value;
	uint8_t text[CB_CMOS_MAX_TEXT_LENGTH];
};

#define CB_TAG_OPTION_DEFAULTS 0x00cb
#define CB_CMOS_IMAGE_BUFFER_SIZE 128
struct cb_cmos_defaults {
	uint32_t tag;
	uint32_t size;
	uint32_t name_length;
	uint8_t name[CB_CMOS_MAX_NAME_LENGTH];
	uint8_t default_set[CB_CMOS_IMAGE_BUFFER_SIZE];
};

#define CB_TAG_OPTION_CHECKSUM 0x00cc
#define CB_CHECKSUM_NONE	0
#define CB_CHECKSUM_PCBIOS	1
struct	cb_cmos_checksum {
	uint32_t tag;
	uint32_t size;
	uint32_t range_start;
	uint32_t range_end;
	uint32_t location;
	uint32_t type;
};

/* Helpful inlines */

static inline uint64_t cb_unpack64(struct cbuint64 val)
{
	return (((uint64_t) val.hi) << 32) | val.lo;
}

static inline uint16_t cb_checksum(const void *ptr, unsigned len)
{
	return ipchecksum((uint8_t *)ptr, len);
}

static inline const char *cb_mb_vendor_string(const struct cb_mainboard *cbm)
{
	return (char *)(cbm->strings + cbm->vendor_idx);
}

static inline const char *cb_mb_part_string(const struct cb_mainboard *cbm)
{
	return (char *)(cbm->strings + cbm->part_number_idx);
}

/* Helpful macros */

#define MEM_RANGE_COUNT(_rec) \
	(((_rec)->size - sizeof(*(_rec))) / sizeof((_rec)->map[0]))

#define MEM_RANGE_PTR(_rec, _idx) \
	(void *)(((uint8_t *) (_rec)) + sizeof(*(_rec)) \
		+ (sizeof((_rec)->map[0]) * (_idx)))

int get_coreboot_info(struct sysinfo_t *info);
#endif
