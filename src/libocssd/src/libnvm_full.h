/*
 * User space I/O library for Open-Channel SSDs
 *
 * Copyright (C) 2015-2017 Javier Gonzáles <javier@cnexlabs.com>
 * Copyright (C) 2015-2017 Matias Bjørling <matias@cnexlabs.com>
 * Copyright (C) 2015-2017 Simon A. F. Lund <slund@cnexlabs.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @file liblightnvm.h
 */
#ifndef __LIBLIGHTNVM_H
#define __LIBLIGHTNVM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include "../include/liblightnvm.h"

#define NVM_NADDR_MAX 64

#define NVM_DEV_NAME_LEN 32
#define NVM_DEV_PATH_LEN (NVM_DEV_NAME_LEN + 5)

#define NVM_FLAG_SCRBL 0x200    ///< Scrambler ON/OFF: Context sensitive

/**
 * Specification version identifier
 */
enum nvm_spec_verid {
    NVM_SPEC_VERID_12 = 0x1,
    NVM_SPEC_VERID_20 = 0x2,
};

/**
 * Enumeration of cmd back-ends used by liblightnvm
 */
enum nvm_be_id {
    NVM_BE_ANY = 0x0,        ///< ANY backend
    NVM_BE_IOCTL = 0x1,        ///< IOCTL backend
    NVM_BE_SYSFS = 0x1 << 1,    ///< IOCTL + sysfs backend
    NVM_BE_LBA = 0x1 << 2,    ///< IOCTL + sysfs + LBA backend
    NVM_BE_SPDK = 0x1 << 3,    ///< SPDK backend
    NVM_BE_BAIDU = 0x1 << 4,    ///< SPDK backend
};
#define NVM_BE_ALL (NVM_BE_IOCTL | NVM_BE_SYSFS | NVM_BE_LBA | NVM_BE_SPDK)

/**
 * Plane-mode access for IO
 */
enum nvm_pmode {
    NVM_FLAG_PMODE_SNGL = 0x0,        ///< Single-plane
    NVM_FLAG_PMODE_DUAL = 0x1,        ///< Dual-plane
    NVM_FLAG_PMODE_QUAD = 0x1 << 1    ///< Quad-plane
};
#define NVM_FLAG_DEFAULT (NVM_FLAG_PMODE_SNGL | NVM_FLAG_SCRBL);

/**
 * Flags for device quirks
 */
enum nvm_quirks {
    NVM_QUIRK_PMODE_ERASE_RUNROLL = 0x1,
    NVM_QUIRK_NSID_BY_NAMECONV = 0x1 << 1,
    NVM_QUIRK_OOB_READ_1ST4BYTES_NULL = 0x1 << 2,
    NVM_QUIRK_OOB_2LRG = 0x1 << 3,
};

/**
 * Opaque handle for NVM devices
 *
 * @struct nvm_dev
 */
struct nvm_dev;

/**
 * Virtual block abstraction
 *
 * Facilitates a libc-like read/write and a system-like `pread`/`pwrite`
 * interface to perform I/O on a virtual block spanning multiple blocks of
 * physical NVM.
 *
 * Consult the `nvm_vblk_alloc`, `nvm_vblk_alloc_line` for the different spans
 *
 * @see nvm_vblk_alloc
 * @see nvm_vblk_alloc_line
 *
 * @struct nvm_vblk
 */
struct nvm_vblk;

/**
 * Enumeration of pseudo meta mode
 */
enum nvm_meta_mode {
    NVM_META_MODE_NONE = 0x0,
    NVM_META_MODE_ALPHA = 0x1,
    NVM_META_MODE_CONST = 0x1 << 1
};

/**
 * Enumeration of device bounds
 */
enum nvm_bounds {
    NVM_BOUNDS_CHANNEL = 0x1,
    NVM_BOUNDS_LUN = 0x1 << 1,
    NVM_BOUNDS_PLANE = 0x1 << 2,
    NVM_BOUNDS_BLOCK = 0x1 << 3,
    NVM_BOUNDS_PAGE = 0x1 << 4,
    NVM_BOUNDS_SECTOR = 0x1 << 5,

    NVM_BOUNDS_PUGRP = 0x1 << 6,
    NVM_BOUNDS_PUNIT = 0x1 << 7,
    NVM_BOUNDS_CHUNK = 0x1 << 8,
    NVM_BOUNDS_SECTR = 0x1 << 9
};

/**
 * Obtain string representation of the given plane-mode
 *
 * @param pmode The plane-mode to obtain string representation of
 *
 * @returns On success, string representation of the given plane-mode. On error,
 * "UNKN".
 */
const char *nvm_pmode_str(int pmode);

/**
 * @returns the "major" version of the library
 */
int nvm_ver_major(void);

/**
 * @returns the "minor" version of the library
 */
int nvm_ver_minor(void);

/**
 * @returns the "patch" version of the library
 */
int nvm_ver_patch(void);

/**
 * Prints version information about the library
 */
void nvm_ver_pr(void);

/**
 * Prints a humanly readable description of given boundary mask
 */
void nvm_bounds_pr(int mask);

/**
 * Prints a humanly readable representation the given `struct nvm_ret`
 *
 * @param ret Pointer to the `struct nvm_ret` to print
 */
void nvm_ret_pr(const struct nvm_ret *ret);

/**
 * Updates the bad-block-table on given device using the provided bbt
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param bbt The bbt to write to device
 * @param ret Pointer to structure in which to store lower-level status and
 *            result
 * @returns On success, 0 is returned. On error, -1 is returned, `errno` set to
 * indicate the error and ret filled with lower-level result codes
 */
int nvm_bbt_set(struct nvm_dev *dev, const struct nvm_bbt *bbt,
                struct nvm_ret *ret);

/**
 * Persist the bad-block-table at `addr` on device and deallocate managed memory
 * for the given bad-block-table describing the LUN at `addr`.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr Address of the LUN to flush bad-block-table for
 * @param ret Pointer to structure in which to store lower-level status and
 *            result
 * @returns On success, 0 is returned. On error, -1 is returned, `errno` set to
 * indicate the error and ret filled with lower-level result codes
 */
int nvm_bbt_flush(struct nvm_dev *dev, struct nvm_addr addr,
                  struct nvm_ret *ret);

/**
 * Persist all bad-block-tables associated with the given `dev`
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param ret Pointer to structure in which to store lower-level status and
 *            result
 * @returns On success, 0 is returned. On error, -1 is returned, `errno` set to
 * indicate the error and ret filled with lower-level result codes
 */
int nvm_bbt_flush_all(struct nvm_dev *dev, struct nvm_ret *ret);

/**
 * Prints a humanly readable representation of the given bad-block-table
 *
 * @param bbt The bad-block-table to print
 */
void nvm_bbt_pr(const struct nvm_bbt *bbt);

/**
 * Prints a humanly readable representation of the given bad-block-table state
 */
void nvm_bbt_state_pr(int state);

/**
 * Find an arbitrary set of 'naddrs' block-addresses on the given 'dev', in the
 * given block state 'bs' and store them in the provided 'addrs' array
 *
 * @returns On success, 0 is returned. On error, -1 is returned and errno set to
 * indicate the error.
 */
int nvm_bbt_gbbt_arbs(struct nvm_dev *dev, int bs, int naddrs, struct nvm_addr addrs[]);

/**
 * Prints human readable representation of the given geometry
 */
void nvm_geo_pr(const struct nvm_geo *geo);

/**
 * Creates a handle to given device path
 *
 * @param dev_path Path of the device to open e.g. "/dev/nvme0n1"
 *
 * @returns A handle to the device
 */
struct nvm_dev *nvm_dev_open(const char *dev_path);

/**
 * Creates a handle to given device path
 *
 * @param dev_path Path of the device to open e.g. "/dev/nvme0n1"
 * @param flags Flags for opening device in different modes
 *
 * @returns A handle to the device
 */
struct nvm_dev *nvm_dev_openf(const char *dev_path, int flags);

/**
 * Prints misc. device attribute associated with the given handle
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
void nvm_dev_attr_pr(const struct nvm_dev *dev);

/**
 * Prints all information about the device associated with the given handle
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
void nvm_dev_pr(const struct nvm_dev *dev);

/**
 * Returns the file-descriptor associated with the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, file descriptor is returned
 */
int nvm_dev_get_fd(const struct nvm_dev *dev);

/**
 * Returns the name associated with the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, string is returned. On error, NULL is returned.
 */
const char *nvm_dev_get_name(const struct nvm_dev *dev);

/**
 * Returns the path associated with the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, string is returned. On error, NULL is returned.
 */
const char *nvm_dev_get_path(const struct nvm_dev *dev);

/**
 * Returns the NVME namespace identifier of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 *
 * @return On success, NVME namespace identifier is returned.
 */
int nvm_dev_get_nsid(const struct nvm_dev *dev);

/**
 * Returns the verid of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, verid is returned
 */
int nvm_dev_get_verid(const struct nvm_dev *dev);

/**
 * Returns the media-controller capabilities mask of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, capabilities mask is returned
 */
uint32_t nvm_dev_get_mccap(const struct nvm_dev *dev);

/**
 * Returns the default plane_mode of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, pmode flag is returned
 */
int nvm_dev_get_pmode(const struct nvm_dev *dev);

/**
 * Set the default plane-mode for the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param pmode Default plane-mode
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_dev_set_pmode(struct nvm_dev *dev, int pmode);

/**
 * Returns the mask of quirks for the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, quirk mask is returned
 */
int nvm_dev_get_quirks(const struct nvm_dev *dev);

/**
 * Set the default plane-mode for the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param quirks Mask of `enum nvm_quirks`
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_dev_set_quirks(struct nvm_dev *dev, int quirks);

/**
 * Returns the ppa-format of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, ppa-format is returned
 *
 */
const struct nvm_spec_ppaf_nand *nvm_dev_get_ppaf(const struct nvm_dev *dev);

/**
 * Returns the ppa-format mask of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, ppa-format mask is returned
 *
 */
const struct nvm_spec_ppaf_nand_mask *nvm_dev_get_ppaf_mask(const struct nvm_dev *dev);

/**
 * Returns the LBA format of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, LBA format is returned
 */
const struct nvm_spec_lbaf *nvm_dev_get_lbaf(const struct nvm_dev *dev);

/**
 * Returns the 'meta-mode' of the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @return On success, meta-mode is returned
 */
int nvm_dev_get_meta_mode(const struct nvm_dev *dev);

/**
 * Set the default 'meta-mode' of the given device
 *
 * The meta-mode is a setting used by the nvm_vblk interface to write
 * pseudo-meta data to the out-of-bound area.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param meta_mode One of: NVM_META_MODE_[NONE|ALPHA|CONST]
 *
 * @returns On success, 0 is returned. On error, -1 is returned and errno set to
 * indicate the error.
 */
int nvm_dev_set_meta_mode(struct nvm_dev *dev, int meta_mode);

/**
 * Returns the maximum number of addresses to use when sending erases to device.
 * That is, when invoking nvm_addr_erase.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
int nvm_dev_get_erase_naddrs_max(const struct nvm_dev *dev);

/**
 * Returns whether caching is enabled for bad-block-tables on the device.
 *
 * @note
 * 0 = cache disabled
 * 1 = cache enabled
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
int nvm_dev_get_bbts_cached(const struct nvm_dev *dev);

/**
 * Returns the backend identifier associated with the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
int nvm_dev_get_be_id(const struct nvm_dev *dev);

/**
 * Set the maximum number of addresses to use for reads, that is, when invoking
 * nvm_addr_read
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
int nvm_dev_get_read_naddrs_max(const struct nvm_dev *dev);

/**
 * Set the maximum number of addresses to use for writes, that is, when invoking
 * nvm_addr_write
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
int nvm_dev_get_write_naddrs_max(const struct nvm_dev *dev);

/**
 * Set the maximum number of addresses to use for erases, that is, when invoking
 * nvm_addr_erase
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param naddrs The maximum
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_dev_set_erase_naddrs_max(struct nvm_dev *dev, int naddrs);

/**
 * Sets whether retrieval and changes to bad-block-tables should be cached.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param bbts_cached 1 = cache enabled, 0 = cache disabled
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_dev_set_bbts_cached(struct nvm_dev *dev, int bbts_cached);

/**
 * Set the maximum number of addresses to use for erases, that is, when invoking
 * nvm_addr_erase.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param naddrs The maximum
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_dev_set_read_naddrs_max(struct nvm_dev *dev, int naddrs);

/**
 * Set the maximum number of addresses to use for erases, that is, when invoking
 * nvm_addr_erase.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param naddrs The maximum
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_dev_set_write_naddrs_max(struct nvm_dev *dev, int naddrs);

/**
 * Returns the geometry of the given device
 *
 * @note
 * See struct nvm_geo for the specifics of the returned geometry
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 *
 * @returns The geometry (struct nvm_geo) of given device handle
 */
const struct nvm_geo *nvm_dev_get_geo(const struct nvm_dev *dev);

/**
 * Returns the minimum write size, in number of sectors, for the given device.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 *
 * @returns The spec. 2.0 defined minimum write size, in sectors. An equivalent
 * value for spec. 1.2
 */
int nvm_dev_get_ws_min(const struct nvm_dev *dev);

/**
 * Returns the optimal write size, in number of sectors, for the given device.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 *
 * @returns The spec. 2.0 defined optimal write size, in sectors, an equivalent
 * value for spec. 1.2
 */
int nvm_dev_get_ws_opt(const struct nvm_dev *dev);

/**
 * Returns the minimal write cache units for the given device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 *
 * @returns The spec. 2.0 defined minimal write cache units, in sectors.
 */
int nvm_dev_get_mw_cunits(const struct nvm_dev *dev);

/**
 * Allocate a buffer of the given size with the given alignment
 *
 * @note
 * Free the buffer using nvm_buf_free
 *
 * @param alignment The alignment in bytes
 * @param nbytes The size of the buffer in bytes
 *
 * @returns A pointer to the allocated memory. On error: NULL is returned and
 * `errno` set appropriatly
 */
void *nvm_buf_alloca(size_t alignment, size_t nbytes);

/**
 * Fills `buf` with chars A-Z
 *
 * @param buf Pointer to the buffer to fill
 * @param nbytes Amount of bytes to fill in buf
 */
void nvm_buf_fill(char *buf, size_t nbytes);

/**
 * Prints `buf` to stdout
 *
 * @param buf Pointer to the buffer to print
 * @param nbytes Amount of bytes of buf to print
 */
void nvm_buf_pr(char *buf, size_t nbytes);

/**
 * Returns the number of bytes where expected is different from actual
 */
size_t nvm_buf_diff(char *expected, char *actual, size_t nbytes);

/**
 * Prints the number and value of bytes where expected is different from actual
 */
void nvm_buf_diff_pr(char *expected, char *actual, size_t nbytes);

/**
 * Write content of buffer into file
 *
 * @param buf Pointer to the buffer
 * @param nbytes Size of buf
 * @param path Destination where buffer will be dumped to
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_buf_to_file(char *buf, size_t nbytes, const char *path);

/**
 * Read content of file into buffer
 *
 * @param buf Pointer to the buffer
 * @param nbytes Size of buf
 * @param path Source to read from
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_buf_from_file(char *buf, size_t nbytes, const char *path);

struct nvm_buf_set {
    char *write;
    char *write_meta;

    char *read;
    char *read_meta;

    size_t nbytes;
    size_t nbytes_meta;
};

struct nvm_buf_set *nvm_buf_set_alloc(struct nvm_dev *dev, size_t nbytes,
                                      size_t nbytes_meta);

void nvm_buf_set_fill(struct nvm_buf_set *bufs);

void nvm_buf_set_free(struct nvm_buf_set *bufs);

/**
 * Checks whether the given address exceeds bounds of the geometry of the given
 * device
 *
 * @param addr The addr to check
 * @param dev The device of which to check geometric bounds against
 * @returns A mask of exceeded boundaries
 */
int nvm_addr_check(struct nvm_addr addr, const struct nvm_dev *dev);

/**
 * Compute log page offset (lpo) for the chunk-information log page
 *
 * That is, the chunk-descriptor address for the given address in the log page
 *
 * @returns the log page offset (lpo) for the given addr
 */
uint64_t nvm_addr_gen2lpo(struct nvm_dev *dev, struct nvm_addr addr);

/**
 * Inverse function of `nvm_addr_gen2lpo`
 *
 * @returns the page offset (lpo) for the given addr
 */
struct nvm_addr nvm_addr_lpo2gen(struct nvm_dev *dev, uint64_t lpo);

/**
 * Converts a given physical address generic-format to device-format
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr The physical address on generic-format to convert
 * @return Physical address on device-format
 */
uint64_t nvm_addr_gen2dev(struct nvm_dev *dev, struct nvm_addr addr);

/**
 * Converts a given physical address on device-format to generic-format
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr The physical address on device-format to convert
 * @return Physical address on generic-format
 */
struct nvm_addr nvm_addr_dev2gen(struct nvm_dev *dev, uint64_t addr);

/**
 * Converts a given physical address on device-format to lba-format
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr The physical address on device-format to convert
 * @return Physical address on lba-format
 */
uint64_t nvm_addr_dev2lba(struct nvm_dev *dev, uint64_t addr);

/**
 * Converts a given physical address on device-format to lba-offset-format
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr The physical address on device-format to convert
 * @return Physical address on lba-offset-format
 */
uint64_t nvm_addr_dev2off(struct nvm_dev *dev, uint64_t addr);

/**
 * Converts a given physical address on generic-format to byte offset
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr The physical address on generic-format to convert
 * @return Logical address as byte offset
 */
uint64_t nvm_addr_gen2off(struct nvm_dev *dev, struct nvm_addr addr);

/**
 * Converts a given byte offset to physical address on generic-format
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param off Logical address as byte offset to convert
 * @return Physical address on generic-format
 */
struct nvm_addr nvm_addr_off2gen(struct nvm_dev *dev, uint64_t off);

/**
 * Converts a given physical address on generic-format to LBA offset
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr The physical address on generic-format to convert
 * @return Logical address as LBA offset
 */
uint64_t nvm_addr_gen2lba(struct nvm_dev *dev, struct nvm_addr addr);

/**
 * Converts a given LBA offset to physical address on generic-format
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param off Logical address as LBA offset to convert
 * @return Physical address on generic-format
 */
struct nvm_addr nvm_addr_lba2gen(struct nvm_dev *dev, uint64_t off);

/**
 * Prints a hexidecimal representation of the given address value
 */
void nvm_addr_pr(const struct nvm_addr addr);

/**
 * Prints a humanly readable representation of the given list of addresses
 * according to the geometry of the given device (spec 1.2 or 2.0)
 */
void nvm_addr_prn(const struct nvm_addr *addr, unsigned int naddrs, const struct nvm_dev *dev);

void nvm_addr_pr_man(const struct nvm_dev *dev, const struct nvm_addr addr);

/**
 * Allocate a virtual block, spanning a given set of physical blocks
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs Set of block-addresses forming the virtual block
 * @param naddrs The number of addresses in the address-set
 *
 * @returns On success, an opaque pointer to the initialized virtual block is
 * returned. On error, NULL and `errno` set to indicate the error.
 */
struct nvm_vblk *nvm_vblk_alloc(struct nvm_dev *dev, struct nvm_addr addrs[],
                                int naddrs);

/**
 * Allocate a virtual block (spanning planes, channels, and LUNs)
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param ch_bgn Beginning of the channel span, as inclusive index
 * @param ch_end End of the channel span, as inclusive index
 * @param lun_bgn Beginning of the LUN span, as inclusive index
 * @param lun_end End of the LUN span, as inclusive index
 * @param blk Block index
 *
 * @returns On success, an opaque pointer to the initialized virtual block is
 * returned.  On error, NULL and `errno` set to indicate the error.
 */
struct nvm_vblk *nvm_vblk_alloc_line(struct nvm_dev *dev, int ch_bgn,
                                     int ch_end, int lun_bgn, int lun_end,
                                     int blk);

/**
 * Destroy a virtual block
 *
 * @param vblk The virtual block to destroy
 */
void nvm_vblk_free(struct nvm_vblk *vblk);

/**
 * Erase a virtual block
 *
 * @note
 * Erasing a vblk will reset internal position pointers
 *
 * @param vblk The virtual block to erase
 * @returns On success, the number of bytes erased is returned. On error, -1 is
 * returned and `errno` set to indicate the error.
 */
ssize_t nvm_vblk_erase(struct nvm_vblk *vblk);

/**
 * Write to a virtual block
 *
 * @note
 * buf must be aligned to device geometry, see struct nvm_geo and nvm_buf_alloc
 * count must be a multiple of min-size, see struct nvm_geo
 * do not mix use of nvm_vblk_pwrite with nvm_vblk_write on the same virtual
 * block
 *
 * @param vblk The virtual block to write to
 * @param buf Write content starting at buf
 * @param count The number of bytes to write
 * @returns On success, the number of bytes written is returned and vblk
 * internal position is updated. On error, -1 is returned and `errno` set to
 * indicate the error.
 */
ssize_t nvm_vblk_write(struct nvm_vblk *vblk, const void *buf, size_t count);

/**
 * Write to a virtual block at a given offset
 *
 * @note
 * buf must be aligned to device geometry, see struct nvm_geo and nvm_buf_alloc
 * count must be a multiple of min-size, see struct nvm_geo
 * offset must be a multiple of min-size, see struct nvm_geo
 * do not mix use of nvm_vblk_pwrite with nvm_vblk_write on the same virtual
 * block
 *
 * @param vblk The virtual block to write to
 * @param buf Write content starting at buf
 * @param count The number of bytes to write
 * @param offset Start writing offset bytes within virtual block
 * @returns On success, the number of bytes written is returned. On error, -1 is
 * returned and `errno` set to indicate the error.
 */
ssize_t nvm_vblk_pwrite(struct nvm_vblk *vblk, const void *buf, size_t count,
                        size_t offset);

/**
 * Pad the virtual block with synthetic data
 *
 * @note
 * Assumes that you have used nvm_vblk_write and now want to fill the remaining
 * virtual block in order to meet block write-before-read constraints
 *
 * @param vblk The virtual block to pad
 * @returns On success, the number of bytes padded is returned and internal
 * position is updated. On error, -1 is returned and `errno` set to indicate the
 * error.
 */
ssize_t nvm_vblk_pad(struct nvm_vblk *vblk);

/**
 * Read from a virtual block
 */
ssize_t nvm_vblk_read(struct nvm_vblk *vblk, void *buf, size_t count);

/**
 * Read from a virtual block at given offset
 */
ssize_t nvm_vblk_pread(struct nvm_vblk *vblk, void *buf, size_t count,
                       size_t offset);

/**
 * Copy the virtual block 'src' to the virtual block 'dst'
 *
 * @returns On success, the number of bytes copied is returned. On error, -1 is
 * returned and `errno` set to indicate the error.
 */
ssize_t nvm_vblk_copy(struct nvm_vblk *src, struct nvm_vblk *dst, int flags);

/**
 * Retrieve the device associated with the given virtual block
 *
 * @param vblk The entity to retrieve information from
 */
struct nvm_dev *nvm_vblk_get_dev(struct nvm_vblk *vblk);

/**
 * Retrieve the set of addresses defining the virtual block
 *
 * @param vblk The entity to retrieve information from
 */
struct nvm_addr *nvm_vblk_get_addrs(struct nvm_vblk *vblk);

/**
 * Retrieve the number of addresses in the address set of the virtual block
 *
 * @param vblk The entity to retrieve information from
 */
int nvm_vblk_get_naddrs(struct nvm_vblk *vblk);

/**
 * Retrieve the size, in bytes, of a given virtual block
 *
 * @param vblk The entity to retrieve information from
 */
size_t nvm_vblk_get_nbytes(struct nvm_vblk *vblk);

/**
 * Retrieve the read cursor position for the given virtual block
 *
 * @param vblk The entity to retrieve information from
 */
size_t nvm_vblk_get_pos_read(struct nvm_vblk *vblk);

/**
 * Retrieve the write cursor position for the given virtual block
 *
 * @param vblk The entity to retrieve information from
 */
size_t nvm_vblk_get_pos_write(struct nvm_vblk *vblk);

/**
 * Set the read cursor position for the given virtual block
 *
 * @param vblk The vblk to change 
 * @param pos The new read cursor
 *
 * @returns On success, 0 is returned. On error, -1 is returned and `errno` set
 * to indicate the error
 */
int nvm_vblk_set_pos_read(struct nvm_vblk *vblk, size_t pos);

/**
 * Set the write cursor position for the given virtual block
 *
 * @param vblk The vblk to change
 * @param pos The new write cursor
 *
 * @returns On success, 0 is returned. On error, -1 is returned and `errno` set
 * to indicate the error
 */
int nvm_vblk_set_pos_write(struct nvm_vblk *vblk, size_t pos);

/**
 * Print the virtual block in a humanly readable form
 *
 * @param vblk The entity to print information about
 */
void nvm_vblk_pr(struct nvm_vblk *vblk);

bool nvm_vblk_line_valid(struct nvm_dev *dev, struct nvm_vblk *vblk);

int nvm_vblk_create(struct nvm_dev *dev, struct nvm_vblk *vblk[], uint32_t vblk_len);

struct nvm_addr nvm_addr_next(struct nvm_dev *dev, struct nvm_addr current_addr, uint32_t next);

int nvm_async_set_config(struct nvm_dev* dev, uint32_t max, uint32_t min);

#ifdef __cplusplus
}
#endif

#endif /* __LIBLIGHTNVM.H */
