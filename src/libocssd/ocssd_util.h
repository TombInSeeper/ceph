//
// Created by root on 19-6-12.
//

#ifndef OCSSD_UTIL_H
#define OCSSD_UTIL_H

#include "include/liblightnvm.h"

#define _OC_WCACHE_TABLE_DEPTH_SHIFT (12)
#define _OC_WCACHE_TABLE_DEPTH (1 << _OC_WCACHE_TABLE_DEPTH_SHIFT)
#define _OC_WCACHE_INDEX_MASK (_OC_WCACHE_TABLE_DEPTH - 1)
typedef unsigned int UINT32;
typedef union {
    UINT32 all;
    struct {
        UINT32 fragment : 2;
        UINT32 plane : 1;
        UINT32 pg_in_wl : 2;
        UINT32 ch : 3;
        UINT32 ce : 3;
        UINT32 lun : 1;
        UINT32 page : 8;
        UINT32 block : 11;
    } fields;
} _PPA;
typedef union {
    UINT32 all;
    struct {
        UINT32 sector : //fragment
                2;
        UINT32 page : //page + pg_in_wl
                10;
        UINT32 block : //block
                11;
        UINT32 plane : //plane
                1;
        UINT32 lun : //lun + ce
                4;
        UINT32 ch : //ch
                3;
    } fields;
} _OC_PPA;


///
/// \param geo
/// \param addr
void incr_addr (const struct nvm_geo* geo , struct nvm_addr* addr );


///
/// \param geo
/// \param lba_off
/// \param lba_len
/// \param cnt
/// \param addr
/// \return 0 if success; fail -1
int lba2nvm_addr(const struct nvm_geo* geo,
                  uint64_t lba_off,
                  uint64_t lba_len,
                  int *cnt,
                  struct nvm_addr* addrs);

/// DEBUG HELPER
void nvm_addr2str(struct nvm_addr *addrs, size_t n , char* buf);


#endif //OCSSD_UTIL_H
