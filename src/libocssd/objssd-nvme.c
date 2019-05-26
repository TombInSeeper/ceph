# include "objssd-nvme.h"





/// PRIVATE
static inline void incr_virtual(const struct nvm_geo *geo, struct nvm_addr *addr) {
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

static inline void logical2virtual(
    const struct nvm_geo *geo,
    uint64_t lba_off,
    uint64_t lba_len,
    int *cnt,
    struct nvm_addr *addrs)
{
  struct nvm_addr addr;
  unsigned long long ofst;
  unsigned int wl, pg_in_wl;
  unsigned int i;


#if DEBUG_VERBOSE_PPA_CALC
  printf("Debug: calculate initial PPA\n");
#endif
  // Calculate initial PPA address
  addr.g.blk = lba_off / (geo->nchannels * geo->nluns * geo->nplanes * geo->npages * geo->page_nbytes);
  ofst       = lba_off % (geo->nchannels * geo->nluns * geo->nplanes * geo->npages * geo->page_nbytes);
#if DEBUG_VERBOSE_PPA_CALC
  printf("     : blk      %d, ofst %lld\n", addr.g.blk, ofst);
#endif

  wl   = ofst / (geo->nchannels * geo->nluns * geo->nplanes * geo->page_nbytes * 3);
  ofst = ofst % (geo->nchannels * geo->nluns * geo->nplanes * geo->page_nbytes * 3);
#if DEBUG_VERBOSE_PPA_CALC
  printf("     : wl       %d, ofst %lld\n", wl, ofst);
#endif

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
#if DEBUG_VERBOSE_PPA_CALC
  printf("     : ch       %d, ofst %lld\n", addr.g.ch, ofst);
#endif

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

  // Fill command context
  *cnt = lba_len / 4096;
  for (i = 0; i < *cnt; i++) {
    addrs[i] = addr;
    memcpy(&addrs[i],&addr ,sizeof(struct nvm_addr));
    incr_virtual(geo, &addr);
  }
}

static inline void virtual2physical(struct ocssd_t * ocssd , struct nvm_addr *addr) {
  addr->g.blk = (ocssd->pm_data.sblk_map[addr->g.blk]);
}

static void _erase_sb(struct ocssd_t * ocssd ,int logic_blk_id)
{

  struct nvm_addr addr;
  struct nvm_ret ret;
  memset(&addr,0,sizeof(addr));
  addr.g.blk = logic_blk_id;
  virtual2physical(ocssd , &addr);
  nvm_addr_erase_sb( ocssd-> dev,&addr,1,0,&ret);
  if(ret.result != 0 || ret.status != 0)
  {
    abort();
  }
  return;
}

static void _init_blk_map(struct ocssd_t * ocssd ) {
  unsigned int blk_start = 80;
  unsigned int blk_idx = 0;
  unsigned int ref_idx;
  int i, j;
  const struct nvm_geo *geo = ocssd->geo;
  struct nvm_dev * dev = ocssd->dev;

  blk_start += (int)(666 + getpid() % 50);
  ref_idx = blk_start;
  int max_count = 200;
  while (blk_idx < OCSSD_MAX_BLK_NUM) {
    bool is_bad = false;
    for (i = 0; i < geo->nchannels; i++) {
      for (j = 0; j < geo->nluns; j++) {
        if (ocssd->g_bbt[i][j]->blks[blk_start*2] != NVM_BBT_FREE
        || ocssd->g_bbt[i][j]->blks[blk_start*2+1] != NVM_BBT_FREE) {
          is_bad = true;
          break;
        }
      }
      if (is_bad) {
        break;
      }
    }
    if (is_bad == false) {
      // Find a good block
      ocssd->pm_data.sblk_map[blk_idx] = blk_start;
      blk_idx++;
      if(blk_idx >= max_count)
        break;
    }
    blk_start++;
    if (blk_start >= geo->nblocks) {
      blk_start = 0;
    }
    if (blk_start == ref_idx ) {
      break;
    }
  }
  ocssd->pm_data.sblk_map[blk_idx] = -1;
  ocssd->pm_data.nr_sblks = max_count;

}

static void _init_blk_ofst(struct ocssd_t * ocssd) {
  int i ;
  for (i = 0 ; i < ocssd->pm_data.nr_sblks ; ++i)
    ocssd->pm_data.sblk_ofst[i].fin_ofst = 0;
}



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
static UINT32 dtu_ocwc_get_hash_value(_PPA ppa) {
  UINT32 hash = 0;
  hash = (ppa.all >> 7) ^ ppa.all;
  hash = hash & _OC_WCACHE_INDEX_MASK;
  return hash;
}
static _PPA OC_OCPPA2PPA(_OC_PPA oc_ppa, int is_slc) {
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
void addr_print(struct nvm_addr *addrs, int n , char* buf){
  _PPA *ppa =  (_PPA*) malloc(sizeof(_PPA) * n);
  _OC_PPA *oc_ppa = (_OC_PPA*) malloc(sizeof(_OC_PPA) *n);
  UINT32 *hash = (UINT32 *) malloc(sizeof(UINT32) *n);

  for(int i = 0 ; i < n ;++i){
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


/// PUBLIC function
struct ocssd_t *ocssd_open( const char *path) {

  struct ocssd_t *ocssd = (struct ocssd_t *)malloc(sizeof(struct ocssd_t));
  memset(ocssd,0,sizeof(*ocssd));

  ocssd->dev = nvm_dev_open(path);
  ocssd->geo = nvm_dev_get_geo( ocssd->dev);

  /// bbt
  struct nvm_ret ret;
  {
    const struct nvm_geo *geo = ocssd->geo;
    for ( int i = 0; i < geo->nchannels; i++) {
      for (int j = 0; j < geo->nluns; j++) {
        struct nvm_addr addr;
        memset(&addr, 0, sizeof(struct nvm_addr));
        addr.g.ch = i;
        addr.g.lun = j;
        ocssd->g_bbt[i][j] = nvm_bbt_get(ocssd->dev, addr, &ret);
      }
    }
  }
  nvm_read_pm(ocssd->dev,&ocssd->pm_data,0,sizeof(ocssd->pm_data),0u,&ret);

  return ocssd;
};

int   ocssd_reset( struct ocssd_t * ocssd)
{
  int i;
  struct nvm_ret  ret ;
  struct nvm_addr addr;
  memset(&addr,0,sizeof(addr));

  struct nvm_dev *dev=ocssd->dev;
  nvm_ext_reset(dev);

  _init_blk_map(ocssd);
  _init_blk_ofst(ocssd);

  //Erase all blks
  {
    for (i = 0; i < ocssd->pm_data.nr_sblks; ++i) {
      _erase_sb(ocssd , i);
    }
  }
  nvm_write_pm(dev,&ocssd->pm_data,0,sizeof(ocssd->pm_data),0,&ret);

  return  0;
}

void  ocssd_close( struct ocssd_t* ocssd)
{

  // write_back pm_data
  struct nvm_ret ret;
  nvm_write_pm (ocssd->dev,&ocssd->pm_data,0,sizeof(ocssd->pm_data),0u,&ret);
  nvm_dev_close(ocssd->dev);

  free(ocssd);

}


struct cmd_ctx*  ocssd_prepare_ctx(
    struct ocssd_t* ocssd ,
    uint64_t lba_off ,
    uint64_t lba_len ,
    char* data ,
    struct debuginfo_t *dg )
{

  struct cmd_ctx* ctx = (struct cmd_ctx*) malloc(sizeof(struct cmd_ctx));
  memset(ctx, 0, sizeof(*ctx));
  ctx->addrs = (struct nvm_addr*) malloc (32 * sizeof(struct nvm_addr));
  logical2virtual(ocssd->geo,lba_off,lba_len,&(ctx->naddrs),ctx->addrs);
  for(int i = 0 ; i < ctx->naddrs ; ++i)
  {
    virtual2physical(ocssd , ctx->addrs + i);
  }
  if (dg) {
    int n = sprintf( dg->debug_str , "off=0x%lx,len=0x%lx,naddrs=%d,", lba_off , lba_len , ctx->naddrs);
    addr_print(ctx->addrs,ctx->naddrs,dg->debug_str + n );
  }
  ctx->data = data;
  ctx->data_len = 0; // Not use here
  return ctx;

}

void ocssd_destory_ctx(struct cmd_ctx *ctx) {
  free(ctx->addrs);
  free(ctx);
}

int ocssd_write(struct ocssd_t* ocssd , struct cmd_ctx *ctx)
{
  for(int i = 0 ; i < ctx->naddrs ; ++i)
  {
    ctx->data_len = 0x1000;
    nvm_addr_write(ocssd->dev,ctx->addrs + i , 1 , (char*)ctx->data + i * 0x1000 ,NULL,0,NULL);

  }
  return 0;
}

int ocssd_read(struct ocssd_t* ocssd , struct cmd_ctx *ctx)
{
  for(int i = 0 ; i < ctx->naddrs ; ++i)
  {
    ctx->data_len = 0x1000;
    nvm_addr_read(ocssd->dev,ctx->addrs + i , 1 , (char*)ctx->data + i * 0x1000 ,NULL,0,NULL);
  }
  return 0;
}

int ocssd_erase(struct ocssd_t* ocssd, uint32_t seg_id)
{
   _erase_sb(ocssd , seg_id);
  return 0;
}
