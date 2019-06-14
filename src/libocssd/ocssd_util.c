//
// Created by root on 19-6-12.
//

#include "ocssd_util.h"
#include "stdlib.h"
#include "stdio.h"

static UINT32 dtu_ocwc_get_hash_value(_PPA ppa) {
    UINT32 hash = 0;
    hash = (ppa.all >> 7) ^ ppa.all;
    hash = hash & _OC_WCACHE_INDEX_MASK;
    return hash;
}
static _PPA OC_OCPPA2PPA(_OC_PPA oc_ppa, int is_slc) {
    (void)is_slc;
    //OC_PPA oc_ppa;
    _PPA ppa;
    UINT32 pg_per_wl = 3;
    ppa.fields.ch = oc_ppa.fields.ch;
    ppa.fields.lun = oc_ppa.fields.lun / 2;
    ppa.fields.ce = oc_ppa.fields.lun % 2;
    ppa.fields.plane = oc_ppa.fields.plane;
    ppa.fields.block = oc_ppa.fields.block;
    ppa.fields.pg_in_wl = oc_ppa.fields.page % pg_per_wl;
    ppa.fields.page = oc_ppa.fields.page / pg_per_wl;
    ppa.fields.fragment = oc_ppa.fields.sector;
    return ppa;
}

/// DEBUG HELPER
void nvm_addr2str(struct nvm_addr *addrs, size_t n , char* buf){
    _PPA *ppa =  (_PPA*) malloc(sizeof(_PPA) * n);
    _OC_PPA *oc_ppa = (_OC_PPA*) malloc(sizeof(_OC_PPA) *n);
    UINT32 *hash = (UINT32 *) malloc(sizeof(UINT32) *n);

    for(unsigned int i = 0 ; i < n ;++i){
        //GET 32-BIT OC_PPA
        oc_ppa[i].fields.ch = addrs[i].g.ch;
        oc_ppa[i].fields.lun = addrs[i].g.lun;
        oc_ppa[i].fields.plane = addrs[i].g.pl;
        oc_ppa[i].fields.block = addrs[i].g.blk;
        oc_ppa[i].fields.page = addrs[i].g.pg;
        oc_ppa[i].fields.sector = addrs[i].g.sec;
        //GET LOW-LEVEL PPA
        ppa[i] = OC_OCPPA2PPA(oc_ppa[i],0);
        //GET HASH VALUE
        hash[i] = dtu_ocwc_get_hash_value( ppa[i] );
        int n = sprintf( buf ,
                         "ch:%u,ce:%u,lun:%u,blk:%u,plane:%u,page:%u,wl:%u,sec:%u,ppa:0x%x,hash:%u;",
                         ppa[i].fields.ch,
                         ppa[i].fields.ce,
                         ppa[i].fields.lun,
                         ppa[i].fields.block,
                         ppa[i].fields.plane,
                         ppa[i].fields.page,
                         ppa[i].fields.pg_in_wl,
                         ppa[i].fields.fragment,
                         ppa[i].all,
                         hash[i]
        );
        buf = buf + n;
    }
    free(hash);
    free(oc_ppa);
    free(ppa);
}

/// ADDR MAPPING
void incr_addr(const struct nvm_geo *geo, struct nvm_addr *addr) {
    uint64_t page = addr->g.pg;
    // We ensure program/read will complete a multi-plane within a lun.
    // One page is done within the first plane and then the second plane.
    // This compliance with how device auto increase address
    addr->g.sec++;
    if (addr->g.sec == geo->nsectors) {
        addr->g.sec = 0;
        addr->g.pl++;
        if (addr->g.pl == geo->nplanes) {
            addr->g.pl = 0;
            addr->g.pg++;
            if (addr->g.pg % 3 == 0) {
                addr->g.pg = page - (page % 3);
                addr->g.lun++;
                if (addr->g.lun == geo->nluns) {
                    addr->g.lun = 0;
                    addr->g.ch++;
                    if (addr->g.ch == geo->nchannels) {
                        // Return to first lun and increase page
                        addr->g.ch = 0;
                        addr->g.pg = page + 1;
                    }
                }
            }
        }
    }
    if (addr->g.pg == geo->npages) {
        // Increase block
        addr->g.pg = 0;
        addr->g.blk++;
    }
}

int lba2nvm_addr(const struct nvm_geo *geo, uint64_t lba_off, uint64_t lba_len, int *cnt, struct nvm_addr *addrs) {
    struct nvm_addr addr;
    unsigned long long ofst;
    unsigned int wl, pg_in_wl;
    //unsigned int i;

#if DEBUG_VERBOSE_PPA_CALC
    printf("Debug: calculate initial PPA\n");
#endif
    // Calculate initial PPA address
    addr.g.blk = lba_off / (geo->nchannels * geo->nluns * geo->nplanes * geo->npages * geo->page_nbytes);
    ofst       = lba_off % (geo->nchannels * geo->nluns * geo->nplanes * geo->npages * geo->page_nbytes);


    wl   = ofst / (geo->nchannels * geo->nluns * geo->nplanes * geo->page_nbytes * 3);
    ofst = ofst % (geo->nchannels * geo->nluns * geo->nplanes * geo->page_nbytes * 3);


/*
    pg_in_wl = ofst / (geo->nchannels * geo->nluns * geo->nplanes * geo->page_nbytes);
    ofst     = ofst % (geo->nchannels * geo->nluns * geo->nplanes * geo->page_nbytes);
#if DEBUG_VERBOSE_PPA_CALC
    printf("     : pg_in_wl %d, ofst %lld\n", pg_in_wl, ofst);
#endif

    addr.g.pg = wl * 3 + pg_in_wl;
*/
    addr.g.ch = ofst / (geo->nluns * geo->nplanes * geo->page_nbytes * 3);
    ofst      = ofst % (geo->nluns * geo->nplanes * geo->page_nbytes * 3);


    addr.g.lun = ofst / (geo->nplanes * geo->page_nbytes * 3);
    ofst       = ofst % (geo->nplanes * geo->page_nbytes * 3);
#if DEBUG_VERBOSE_PPA_CALC
    printf("     : lun      %d, ofst %lld\n", addr.g.lun, ofst);
#endif

    pg_in_wl = ofst / (geo->nplanes * geo->page_nbytes);
    ofst     = ofst % (geo->nplanes * geo->page_nbytes);
#if DEBUG_VERBOSE_PPA_CALC
    printf("     : pg_in_wl %d, ofst %lld\n", pg_in_wl, ofst);
#endif
    addr.g.pg = wl * 3 + pg_in_wl;


    addr.g.pl  = ofst / (geo->page_nbytes);
    ofst       = ofst % (geo->page_nbytes);
#if DEBUG_VERBOSE_PPA_CALC
    printf("     : pl       %d, ofst %lld\n", addr.g.pl, ofst);
#endif
    addr.g.sec = ofst / (geo->sector_nbytes);
#if DEBUG_VERBOSE_PPA_CALC
    printf("     : pg       %d\n", addr.g.pg);
    printf("     : sec      %d\n", addr.g.sec);
    printf("Debug: calc done\n");
#endif

    *cnt = lba_len / 4096;
    for( int j = 0 ; j < *cnt ; ++j) {
        addrs[j] = addr;
        incr_addr(geo,&addr);
    }

    return 0;
}

