/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _RSC_TABLE_H_
#define _RSC_TABLE_H_

#define SIZE_1M 0x100000
#define SIZE_1K 1024
#define DRAM_BASE 0x80000000
#define DRAM_SIZE 64 * SIZE_1M    // 256 for duo 256m
#define ZEPHYR_SIZE 768 * SIZE_1K // 2 * SIZE_1M for duo 256m
#define START_ADDR DRAM_BASE + DRAM_SIZE - ZEPHYR_SIZE // 0x8fe00000 for 256m

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t u8;

/**
 * struct resource_table - firmware resource table header
 * @ver: version number
 * @num: number of resource entries
 * @reserved: reserved (must be zero)
 * @offset: array of offsets pointing at the various resource entries
 *
 * The header of the resource table, as expressed by this structure,
 * contains a version number (should we need to change this format in the
 * future), the number of available resource entries, and their offsets
 * in the table.
 */
struct resource_table {
  u32 ver;
  u32 num;
  u32 reserved[2];
  u32 offset[0];
} __packed;

/**
 * struct fw_rsc_hdr - firmware resource entry header
 * @type: resource type
 * @data: resource data
 *
 * Every resource entry begins with a 'struct fw_rsc_hdr' header providing
 * its @type. The content of the entry itself will immediately follow
 * this header, and it should be parsed according to the resource type.
 */
struct fw_rsc_hdr {
  u32 type;
  u8 data[0];
} __packed;

/**
 * enum fw_resource_type - types of resource entries
 *
 * @RSC_CARVEOUT:   request for allocation of a physically contiguous
 *		    memory region.
 * @RSC_DEVMEM:     request to iommu_map a memory-based peripheral.
 * @RSC_TRACE:	    announces the availability of a trace buffer into which
 *		    the remote processor will be writing logs.
 * @RSC_VDEV:       declare support for a virtio device, and serve as its
 *		    virtio header.
 * @RSC_LAST:       just keep this one at the end
 * @RSC_VENDOR_START:	start of the vendor specific resource types range
 * @RSC_VENDOR_END:	end of the vendor specific resource types range
 *
 * Please note that these values are used as indices to the rproc_handle_rsc
 * lookup table, so please keep them sane. Moreover, @RSC_LAST is used to
 * check the validity of an index before the lookup table is accessed, so
 * please update it as needed.
 */
enum fw_resource_type {
  RSC_CARVEOUT = 0,
  RSC_DEVMEM = 1,
  RSC_TRACE = 2,
  RSC_VDEV = 3,
  RSC_LAST = 4,
  RSC_VENDOR_START = 128,
  RSC_VENDOR_END = 512,
};

#define FW_RSC_ADDR_ANY (-1)

/**
 * struct fw_rsc_carveout - physically contiguous memory request
 * @da: device address
 * @pa: physical address
 * @len: length (in bytes)
 * @flags: iommu protection flags
 * @reserved: reserved (must be zero)
 * @name: human-readable name of the requested memory region
 *
 * This resource entry requests the host to allocate a physically contiguous
 * memory region.
 *
 * These request entries should precede other firmware resource entries,
 * as other entries might request placing other data objects inside
 * these memory regions (e.g. data/code segments, trace resource entries, ...).
 *
 * Allocating memory this way helps utilizing the reserved physical memory
 * (e.g. CMA) more efficiently, and also minimizes the number of TLB entries
 * needed to map it (in case @rproc is using an IOMMU). Reducing the TLB
 * pressure is important; it may have a substantial impact on performance.
 *
 * If the firmware is compiled with static addresses, then @da should specify
 * the expected device address of this memory region. If @da is set to
 * FW_RSC_ADDR_ANY, then the host will dynamically allocate it, and then
 * overwrite @da with the dynamically allocated address.
 *
 * We will always use @da to negotiate the device addresses, even if it
 * isn't using an iommu. In that case, though, it will obviously contain
 * physical addresses.
 *
 * Some remote processors needs to know the allocated physical address
 * even if they do use an iommu. This is needed, e.g., if they control
 * hardware accelerators which access the physical memory directly (this
 * is the case with OMAP4 for instance). In that case, the host will
 * overwrite @pa with the dynamically allocated physical address.
 * Generally we don't want to expose physical addresses if we don't have to
 * (remote processors are generally _not_ trusted), so we might want to
 * change this to happen _only_ when explicitly required by the hardware.
 *
 * @flags is used to provide IOMMU protection flags, and @name should
 * (optionally) contain a human readable name of this carveout region
 * (mainly for debugging purposes).
 */
struct fw_rsc_carveout {
  u32 da;
  u32 pa;
  u32 len;
  u32 flags;
  u32 reserved;
  u8 name[32];
} __packed;

/* ------------------------- */
/* Resource table definition */
/* ------------------------- */

#define NO_RESOURCE_ENTRIES 1

struct remote_resource_table {
  struct resource_table resource_table;
  u32 offset[NO_RESOURCE_ENTRIES];
  struct fw_rsc_hdr carve_out;
  struct fw_rsc_carveout carve_out_data;
} __packed;

__attribute__((
    section(".resource_table"))) struct remote_resource_table resources = {
    .resource_table =
        {
            .ver = 1,
            .num = NO_RESOURCE_ENTRIES,
            .reserved = {0, 0},
        },
    .offset = {offsetof(struct remote_resource_table, carve_out)},
    .carve_out = {.type = RSC_CARVEOUT},
    .carve_out_data = {.da = START_ADDR,
                       .pa = START_ADDR,
                       .len = ZEPHYR_SIZE,
                       .flags = 0x0,
                       .reserved = 0,
                       .name = "zephyr-fw"},
};

#ifdef __cplusplus
}
#endif

#endif
