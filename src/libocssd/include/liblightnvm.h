//
// Created by jiashu on 11/9/18.
//

#ifndef OCSSD_LIBNVM_H
#define OCSSD_LIBNVM_H


#include <stdint.h>
#include <sys/types.h>

struct nvm_dev;

/**
 * Representation of device geometry
 *
 * @see nvm_dev_get_geo
 */
struct nvm_geo {
    union {

        /**
         * Spec 2.0
         */
        struct {
            size_t npugrp;        ///< # Parallel Unit Groups
            size_t npunit;        ///< # Parallel Units in PUG
            size_t nchunk;        ///< # Chunks in PU

            size_t nsectr;        ///< # Sectors per CNK
            size_t nbytes;        ///< # Bytes per SECTOR
            size_t nbytes_oob;    ///< # Bytes per SECTOR in OOB
        } l;

        struct {
            size_t nchannels;    ///< # of channels on device
            size_t nluns;        ///< # of LUNs per channel
            size_t nplanes;        ///< # of planes per LUN
            size_t nblocks;        ///< # of blocks per plane
            size_t npages;        ///< # of pages per block
            size_t nsectors;    ///< # of sectors per page

            size_t page_nbytes;    ///< # of bytes per page
            size_t sector_nbytes;    ///< # of bytes per sector
            size_t meta_nbytes;    ///< # of bytes for OOB
        };
    };

    size_t tbytes;                ///< Total # bytes in geometry
    int verid;                ///< Associated dev verid
};
/**
 * Encapsulation of generic physical nvm addressing
 *
 * Although the user need not worry about device specific address formats the
 * user has to know and respect addressing within device specific geometric
 * boundaries.
 *
 * For that purpose one can use the `struct nvm_geo` of an `struct nvm_dev` to
 * obtain device specific geometries.
 */
struct nvm_addr {
    union {
        /**
         * Address packing and geometric accessors
         */
        struct {
            uint64_t blk    : 16;   ///< Block address
            uint64_t pg : 16;   ///< Page address
            uint64_t sec    : 8;    ///< Sector address
            uint64_t pl : 8;    ///< Plane address
            uint64_t lun    : 8;    ///< LUN address
            uint64_t ch : 7;    ///< Channel address
            uint64_t rsvd   : 1;    ///< Reserved
        } g;

        struct {
            uint64_t sectr    : 32;    ///< Logical Sector in Chunk
            uint64_t chunk    : 16;    ///< Chunk in PU
            uint64_t punit    : 8;    ///< Parallel Unit (PU) in PUG
            uint64_t pugrp    : 8;    ///< Parallel Unit Group (PUG)
        } l;

        uint64_t ppa;            ///< Address as raw value

        uint64_t val;            ///< Address as raw value
    };
};

struct nvm_ret {
    uint64_t status;    ///< NVMe command status / completion bits
    uint32_t result;    ///< NVMe command error codes
};

/**
 * Representation of valid values of bad-block-table states
 */
enum nvm_bbt_state {
    NVM_BBT_FREE = 0x0,        ///< Block is free AKA good
    NVM_BBT_BAD = 0x1,        ///< Block is bad
    NVM_BBT_GBAD = 0x1 << 1,    ///< Block marked as grown bad
    NVM_BBT_DMRK = 0x1 << 2,    ///< Block marked by device side
    NVM_BBT_HMRK = 0x1 << 3    ///< Block marked by host side
};

/**
 * Plane-mode access for IO
 */
enum nvm_block_type {
    NVM_FLASH_SLC_ENABLE = 0x1 << 8   ///< STL Type
};

/**
 * Representation of bad-block-table
 *
 * The bad-block-table describes block-state of a given LUN
 *
 * @see nvm_bbt_get, nvm_bbt_set, nvm_bbt_mark, nvm_bbt_free, and nvm_bbt_pr
 */
struct nvm_bbt {
    struct nvm_dev *dev;    ///< Device on which the bbt resides
    struct nvm_addr addr;   ///< Address of the LUN described by the bbt
    uint64_t nblks;     ///< Total # of blocks in lun
    uint32_t nbad;      ///< # of manufacturer marked bad blocks
    uint32_t ngbad;     ///< # of grown bad blocks
    uint32_t ndmrk;     ///< # of device reserved/marked blocks
    uint32_t nhmrk;     ///< # of of host reserved/marked blocks
    uint8_t blks[];     ///< Array of block status for each block in LUN
};

enum nvm_error_inject {
    INJECT_ERASE = 0, INJECT_Program, INJECT_NAND_OVER_WRITE, INJECT_Read_UNCORRECTABLE, INJECT_Read_EMPTY_PAGE,
};

struct nvm_log_page {
    uint32_t nppas;
    uint32_t rsvd;
    uint64_t ppas[127];
};

struct cmd_ctx {
    int naddrs;
    struct nvm_addr *addrs;
    void *data;
    void *meta;
    void *private_data;
    uint8_t opcode;
    uint8_t flags;
    uint16_t control;
    uint16_t nppas;
    uint16_t rsvd;
    uint64_t metadata;
    uint64_t addr;
    uint64_t ppa_list;
    uint32_t metadata_len;
    uint32_t data_len;
    uint64_t status;
    uint32_t result;
    uint32_t rsvd3[3];
};

struct result_ctx {
    uint64_t t_ns;
    int max_count;
    struct cmd_ctx *ctx[20];
};

/**
 * Creates a handle to given device path
 *
 * @param dev_path Path of the device to open e.g. "/dev/nvme0n1"
 *
 * @returns A handle to the device
 */
struct nvm_dev *nvm_dev_open(const char *dev_path);

/**
 * Destroys device-handle
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 */
void nvm_dev_close(struct nvm_dev *dev);

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
 * Erase super block at given addresses
 *
 * @note
 * The addresses given to this function are interpreted as super block addresses, in
 * contrast to `nvm_addr_erase` for which the address is interpreted as a block address.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs Array of memory address
 * @param naddrs Length of array of memory addresses
 * @param flags Access mode
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t
nvm_addr_erase_sb(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, uint16_t flags, struct nvm_ret *ret);

/**
 * Handle complete command use async mode
 *
 * @note
 * handle complete command and return result to user
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param rctx query result context
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 */
ssize_t nvm_get_async_cmd_event(struct nvm_dev *dev, struct result_ctx *rctx);

/**
 * Read content of nvm at addresses into buf use async mode
 *
 * @note
 * The addresses given to this function are interpreted as sector addresses, in
 * contrast to `nvm_addr_mark` and `nvm_addr_erase` for which the address is
 * interpreted as a block address.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param ctx cmd context
 * @param flags Access mode
 * @param no_meta Whether read with metadata
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly
 */
ssize_t nvm_addr_async_read(struct nvm_dev *dev, struct cmd_ctx *ctx, uint16_t flags, int no_meta);

/**
 * Write content of buf to nvm at address(es) with RAID5 protection use async mode
 *
 * @note
 * This is much like nvm_addr_protected_write, except for async mode.
 *
 * @param head_idx Open block(head) index, two heads (0, 1) are supported. Set this
 *         field to 2 if you don't want RAID5 protection.
 * @param fake_write Write data to RAID engine only. No data is written to media
 * @param no_meta Whether write with metadata
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly
 */
ssize_t
nvm_addr_async_protected_write(struct nvm_dev *dev, struct cmd_ctx *ctx, uint16_t flags, int head_idx, int fake_write,
                               int no_meta);

/**
 * Initialize the RAID5 engine for this super block
 *
 * @note
 * Must send this cmd before write to a new super block
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_parity_init(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, struct nvm_ret *ret);
/**
 * Indicate the RAID5 engine to write parity data to given address
 *
 * @note
 * Must send all data write cmds in a page stripe before send this cmd
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address, only 1 entry in this cmd. The ppa must be in
 *      the same page stripe and pointed to the first sector in parity lun.
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_parity_out(struct nvm_dev *dev, struct nvm_addr *addrs, int naddrs, struct nvm_ret *ret);

/**
 * Indicate the RAID5 engine to write parity data to given address use async mode
 *
 * @note
 * Must send all data write cmds in a page stripe before send this cmd
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param ctx cmd context
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly
 */
ssize_t nvm_addr_async_parity_init(struct nvm_dev *dev, struct cmd_ctx *ctx);

ssize_t nvm_addr_async_parity_out(struct nvm_dev *dev, struct cmd_ctx *ctx);

/**
 * Read persistent memory data
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param buf Buffer to store result of read into, must be aligned to device
 *            granularity min read and size equal to `naddrs *
 *            geo.sector_nbytes`
 * @param flags Access mode
 * @param length data length
 * @param offset data offset
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_read_pm(struct nvm_dev *dev, void *buf, uint16_t flags, unsigned int length, unsigned int offset,
                    struct nvm_ret *ret);

/**
 * Write data to persistent memory
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param buf Buffer to store result of read into, must be aligned to device
 *            granularity min read and size equal to `naddrs *
 *            geo.sector_nbytes`
 * @param flags Access mode
 * @param length data length
 * @param offset data offset
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_write_pm(struct nvm_dev *dev, void *buf, uint16_t flags, unsigned int length, unsigned int offset,
                     struct nvm_ret *ret);

/**
 * Allocate a copy of the given bad-block-table
 *
 * @param bbt Pointer to the bad-block-table to copy
 * @returns On success, a pointer to a write-able copy of the given bbt is
 * returned. On error, NULL is returned and errno set to indicate the error
 */
struct nvm_bbt *nvm_bbt_alloc_cp(const struct nvm_bbt *bbt);

/**
 * Destroys a given bad-block-table
 *
 * @param bbt The bad-block-table to destroy
 */
void nvm_bbt_free(struct nvm_bbt *bbt);

/**
 * Retrieves a bad block table from device
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addr Address of the LUN to retrieve bad-block-table for
 * @param ret Pointer to structure in which to store lower-level status and
 *            result
 * @returns On success, a pointer to the bad-block-table is returned. On error,
 * NULL is returned, `errno` set to indicate the error and ret filled with
 * lower-level result codes
 */
const struct nvm_bbt *nvm_bbt_get(struct nvm_dev *dev, struct nvm_addr addr, struct nvm_ret *ret);

/**
 * Mark addresses good, bad, or host-bad.
 *
 * @note
 * The addresses given to this function are interpreted as block addresses, in
 * contrast to `nvm_addr_write`, and `nvm_addr_read` which interpret addresses
 * and sector addresses.
 *
 * @see `enum nvm_bbt_state`
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs Array of memory address
 * @param naddrs Length of memory address array
 * @param flags 0x0 = GOOD, 0x1 = BAD, 0x2 = GROWN_BAD, as well as access mode
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns On success, 0 is returned. On error, -1 is returned, `errno` set to
 * indicate the error and ret filled with lower-level result codes
 */
int nvm_bbt_mark(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, uint16_t flags, struct nvm_ret *ret);

/**
 * Erase nvm at given addresses
 *
 * @note
 * The addresses given to this function are interpreted as block addresses, in
 * contrast to `nvm_addr_mark`, `nvm_addr_write`, and `nvm_addr_read` for which
 * the address is interpreted as a sector address.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs Array of memory address
 * @param naddrs Length of array of memory addresses
 * @param flags Access mode
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_erase(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, uint16_t flags, struct nvm_ret *ret);

/**
 * Write content of buf to nvm at address(es)
 *
 * @note
 * The addresses given to this function are interpreted as sector addresses, in
 * contrast to nvm_addr_mark and nvm_addr_erase for which the address is
 * interpreted as a block address.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs Array of memory address
 * @param naddrs Length of array of memory addresses
 * @param buf The buffer which content to write, must be aligned to device
 *            geometry of minimal write granularity and size equal to
 *            `naddrs * geo.nbytes`
 * @param meta Buffer containing metadata, must be of size equal to device
 *             `naddrs * geo.meta_nbytes`
 * @param flags Access mode
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_write(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, const void *buf, const void *meta,
                       uint16_t flags, struct nvm_ret *ret);

/**
 * Read content of nvm at addresses into buf
 *
 * @note
 * The addresses given to this function are interpreted as sector addresses, in
 * contrast to `nvm_addr_mark` and `nvm_addr_erase` for which the address is
 * interpreted as a block address.
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses
 * @param buf Buffer to store result of read into, must be aligned to device
 *            granularity min read and size equal to `naddrs *
 *            geo.sector_nbytes`
 * @param meta Buffer to store content of metadata, must be of size equal to
 *             device `naddrs * geo.meta_nbytes`
 * @param flags Access mode
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_read(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, void *buf, void *meta, uint16_t flags,
                      struct nvm_ret *ret);

/**
 * Allocate a buffer aligned to match the given geometry
 *
 * @note
 * nbytes must be greater than zero and a multiple of minimal granularity
 * @note
 * Free the buffer using nvm_buf_free
 *
 * @param geo The geometry to get alignment information from
 * @param nbytes The size of the allocated buffer in bytes
 *
 * @returns A pointer to the allocated memory. On error: NULL is returned and
 * `errno` set appropriatly
 */
void *nvm_buf_alloc(const struct nvm_geo *geo, size_t nbytes);

/**
 * Free the given buffer, calling regular "free" on the buffer might fail
 *
 * @param buf Pointer to the buffer to fill
 * @param nbytes Amount of bytes to fill in buf
 */
void nvm_buf_free(void *buf);

/**
 * GET erase failed log page
 *
 * @note
 * query nvm program error event
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param log query log page context
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_get_mef_log(struct nvm_dev *dev, struct nvm_log_page *log);

/**
 * Query write failed async error event, and get write failed log page
 *
 * @note
 * query nvm program error event, then get write failed log page
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param log query log page context
 * @returns 0 on success. On error: returns -1
 */
ssize_t nvm_get_async_error_event(struct nvm_dev *dev, struct nvm_log_page *log);

/**
 * Inject read error
 *
 * @note
 * This is error inject cmd use to trigger read data error
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_inject_read_failed(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, struct nvm_ret *ret);

/**
 * Inject program error
 *
 * @note
 * This is error inject cmd use to trigger program data data to nand failed
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_inject_write_failed(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, struct nvm_ret *ret);

/**
 * Inject erase error
 *
 * @note
 * This is error inject cmd use to trigger erase super block failed
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_inject_erase_failed(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, struct nvm_ret *ret);

int nvm_ext_format(struct nvm_dev *dev);
int nvm_ext_error_bit_count(struct nvm_dev *dev, struct nvm_addr address, uint32_t *ebc);
int nvm_ext_inject_error(struct nvm_dev *dev, struct nvm_addr address, enum nvm_error_inject error_type);
int nvm_ext_reset(struct nvm_dev *dev);
int nvm_ext_get_pe(struct nvm_dev *dev, uint16_t *buf_pe, uint32_t len);
int nvm_ext_get_per_lun_blks_pe(struct nvm_dev *dev, struct nvm_addr addr, uint16_t *buf_pe, uint32_t len);
int nvm_ext_get_blk_pe(struct nvm_dev *dev, struct nvm_addr address, uint32_t *count);
int nvm_ext_aer_clean(struct nvm_dev *dev);
int nvm_ext_aer_set(struct nvm_dev *dev, struct nvm_ret *ret);




#endif //OCSSD_BAIDU_LIBNVM_H
