//
// Created by jiashu on 11/26/18.
//

#ifndef OCSSD_BAIDU_NVM_CMD_H
#define OCSSD_BAIDU_NVM_CMD_H

#include <stdint.h>

/**
 * Opaque handle for NVM devices
 *
 * @struct nvm_dev
 */
struct nvm_dev;
struct nvm_addr;
struct nvm_ret;

/**
 * Execute an Open-Channel 1.2 identify / Open-Channel 2.0 geometry command
 *
 * NOTE: Caller is responsible for de-allocating the returned structure
 *
 * @return On success, pointer identify structure is returned. On error, NULL is
 * returned and `errno` set to indicate the error and ret filled with
 * lower-level result codes
 */
struct nvm_spec_idfy *nvm_cmd_idfy(struct nvm_dev *dev, struct nvm_ret *ret);

/**
 * Execute an Open-Channel 2.0 report chunk command
 *
 * NOTE: Caller is responsible for de-allocating the returned structure
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * report for
 * @param opts Reporting options, see `enum nvm_spec_chunk_state`
 *
 * @return On success, pointer report chunk structure is returned. On error,
 * NULL is returned and `errno` set to indicate the error and ret filled with
 * lower-level result codes
 */
struct nvm_spec_rprt *nvm_cmd_rprt(struct nvm_dev *dev, struct nvm_addr *addr,
                                   int opt, struct nvm_ret *ret);

/**
 * Find an arbitrary set of 'naddrs' chunk-addresses on the given 'dev', in the
 * given chunk state 'cs' and store them in the provided 'addrs' array
 *
 * @returns 0 on success, -1 on error and errno set to indicate the error.
 */
int nvm_cmd_rprt_arbs(struct nvm_dev *dev, int cs, int naddrs, struct nvm_addr addrs[]);

/**
 * Execute an Open-Channel 1.2 get bad-block-table command
 *
 * @return On success, pointer to bad block table is returned. On error, NULL is
 * returned and `errno` set to indicate the error and ret filled with
 * lower-level result codes
 */
struct nvm_spec_bbt *nvm_cmd_gbbt(struct nvm_dev *dev, struct nvm_addr addr, struct nvm_ret *ret);

/**
 * Execute an Open-Channel 1.2 set bad block table command
 *
 * @returns On success, 0 is returned. On error, -1 is returned and errno set to
 * indicate the error.
 */
int nvm_cmd_sbbt(struct nvm_dev *dev, struct nvm_addr *addrs, int naddrs,
                 uint16_t flags, struct nvm_ret *ret);

/**
 * Execute an Open-Channel 1.2 erase / Open-Channel 2.0 reset command
 *
 * @return On success, 0 is returned. On error, -1 is returned and `errno` set
 * to indicate the error and ret filled with lower-level result codes
 */
int nvm_cmd_erase(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs,
                  uint16_t flags, struct nvm_ret *ret);

/**
 * Execute an Open-Channel 1.2 / 2.0 vector-write command
 *
 * @return On success, 0 is returned. On error, -1 is returned and `errno` set
 * to indicate the error and ret filled with lower-level result codes
 */
int nvm_cmd_write(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs,
                  void *data, void *meta, uint16_t flags, struct nvm_ret *ret);

/**
 * Execute an Open-Channel 1.2 / 2.0 vector-read command
 *
 * @return On success, 0 is returned. On error, -1 is returned and `errno` set
 * to indicate the error and ret filled with lower-level result codes
 */
int nvm_cmd_read(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs,
                 void *data, void *meta, uint16_t flags, struct nvm_ret *ret);

/**
 * Execute an Open-Channel 2.0 vector-copy command
 *
 * @return On success, 0 is returned. On error, -1 is returned and `errno` set
 * to indicate the error and ret filled with lower-level result codes
 */
int nvm_cmd_copy(struct nvm_dev *dev, struct nvm_addr src[],
                 struct nvm_addr dst[], int naddrs, uint16_t flags,
                 struct nvm_ret *ret);

#endif //OCSSD_BAIDU_NVM_CMD_H
