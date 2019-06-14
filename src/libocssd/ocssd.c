
#include <liblightnvm.h>
#include "ocssd.h"
#include "ocssd_util.h"

static inline void virtual2physical(struct ocssd_t * ocssd ,
        struct nvm_addr *addr, size_t n) {
    //addr->g.blk = (ocssd->pm_data.sblk_map[addr->g.blk]);
    for (unsigned int i = 0; i < n; ++i) {
        addr->g.blk = ocssd->this_partition->vblks[addr->g.blk].pblk_id;
        addr->g.ch += ocssd->this_partition->vblk_range.ch_bg;
    }
}

static void _erase_vblk(struct ocssd_t * ocssd ,int logic_blk_id)
{

  struct nvm_addr addr;
  memset(&addr,0,sizeof(addr));
  addr.g.blk = ocssd->this_partition->vblks[logic_blk_id].pblk_id;
  {
      uint32_t bg_ch = ocssd->this_partition->vblk_range.ch_bg;
      uint32_t end_ch = ocssd->this_partition->vblk_range.ch_end;
      for ( unsigned i = bg_ch ; i < end_ch ; ++i)
      {
          addr.g.ch = i;
          for (unsigned j = 0 ; j < ocssd->geo->nluns ; ++j )
          {
              addr.g.lun = j;
              struct nvm_addr addrs[ocssd->geo->nplanes];
              for(unsigned k = 0 ; k < ocssd->geo->nplanes ; ++k)
              {
                  addrs[k] = addr;
                  addrs[k].g.pl = k;
              }
              nvm_addr_erase(ocssd->dev,addrs,ocssd->geo->nplanes,0,NULL);
          }
      }
  }
}

struct ocssd_t *_ocssd_open_raw( const char *path) {

    struct ocssd_t *ocssd = (struct ocssd_t *)malloc(sizeof(struct ocssd_t));
    memset(ocssd,0,sizeof(*ocssd));

    ocssd->dev = nvm_dev_open(path);
    if(!ocssd->dev)
    {
        free(ocssd);
        ocssd = NULL;
        goto end;
    }
    ocssd->geo = nvm_dev_get_geo( ocssd->dev);
    ocssd->bbt = (const struct nvm_bbt**)ocssd->dev->bbts;

    /// bbt
    struct nvm_ret ret;
    {
        const struct nvm_geo *geo = ocssd->geo;
        for ( unsigned i = 0; i < geo->nchannels; i++) {
            for ( unsigned j = 0; j < geo->nluns; j++) {
                struct nvm_addr addr;
                memset(&addr, 0, sizeof(struct nvm_addr));
                addr.g.ch = i;
                addr.g.lun = j;
                ocssd->bbt[i * geo->nluns + j] = nvm_bbt_get(ocssd->dev, addr, &ret);
            }
        }
    }


    end:
    return ocssd;
};

struct ocssd_t *ocssd_open2(const char* path , int partition_id)
{
    struct ocssd_t * ocssd = _ocssd_open_raw(path);
    if(!ocssd)
        goto end;
    if(partition_id <= 0 ) {
        LOG_ERROR("part_id = %d\n" , partition_id);
        goto free_ocssd;
    }

    char fname[32];
    sprintf(fname, "/tmp/ocssd%d_lock" ,partition_id);

    ocssd->lock_fd = open(fname, O_WRONLY | O_CREAT);
    if(flock(ocssd->lock_fd,LOCK_EX | LOCK_NB)) {
        LOG_ERROR("%s is locked by others.\n",fname);
        goto close_fd;
    }

    if(1){
        //alloc for meta
        ocssd->ocssd_meta =(struct ocssd_meta_t*)malloc(sizeof(struct ocssd_meta_t));
        ocssd->this_partition = (struct ocssd_partition_t*)malloc(sizeof(struct ocssd_partition_t));

        // read ocssd meta
        nvm_read_pm(ocssd->dev,ocssd->ocssd_meta, 0,sizeof(*ocssd->ocssd_meta),
                PM_START_OFFSET, NULL);

        // read this partition meta
        uint64_t partition_offset = PM_START_OFFSET + sizeof(struct ocssd_meta_t)
                + (partition_id - 1) *sizeof(struct ocssd_partition_t);

        nvm_read_pm(ocssd->dev, ocssd->this_partition, 0 ,sizeof(struct ocssd_partition_t)
            ,partition_offset, NULL);


        //virtual geo for address mapping
        ocssd->virtual_geo = (struct nvm_geo*)malloc(sizeof(struct nvm_geo));
        memmove(ocssd->virtual_geo , ocssd->geo , sizeof(struct nvm_geo));

        ocssd->virtual_geo->nchannels =ocssd->this_partition->vblk_range.ch_end
                    - ocssd->this_partition->vblk_range.ch_bg;

        ocssd->vblk_size = ocssd->virtual_geo->nchannels *
                           ocssd->virtual_geo->nluns *
                           ocssd->virtual_geo->nplanes *
                           ocssd->virtual_geo->npages *
                           ocssd->virtual_geo->page_nbytes;


        return ocssd;
    }


close_fd:
    close(ocssd->lock_fd);
free_ocssd:
    free(ocssd);
end:
    return NULL;

}

struct ocssd_t *ocssd_open( const char *path_with_partition_suffix) {

  //device name is like /dev/nvme0n1p1
  const char *path = path_with_partition_suffix;
  size_t dev_pathlen = strlen(path) - 2;

  int i = 0;
  char _path[64] = {0};

  if( path[dev_pathlen] == 'p') {
      //Partion info is here
      i = path[dev_pathlen + 1] - '0';
      memmove(_path , path , dev_pathlen);
  }
  else {
      LOG_ERROR("dev_path:%s is not with suffix \"p1,or p2...\"\n",path);
      return NULL;
  }
  return ocssd_open2(_path , i);

};

void ocssd_close ( struct ocssd_t* ocssd)
{
  if(ocssd->this_partition)
  {
      uint64_t partition_offset = PM_START_OFFSET + sizeof(struct ocssd_meta_t)
                                  + (ocssd->this_partition ->partition_id - 1) *sizeof(struct ocssd_partition_t);
      nvm_write_pm( ocssd->dev ,ocssd->this_partition,
              0 , sizeof(struct ocssd_partition_t),partition_offset,
              NULL);
  }

  free(ocssd->this_partition);
  free(ocssd->ocssd_meta);
  free(ocssd->virtual_geo);
  if(ocssd->lock_fd)
    close(ocssd->lock_fd);


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

  //assert(lba_len <= 96 * 1024); //96K

  size_t naddrs = lba_len / 4096;


  struct cmd_ctx* ctx = (struct cmd_ctx*) malloc(sizeof(struct cmd_ctx));
  memset(ctx, 0, sizeof(*ctx));
  ctx->addrs = (struct nvm_addr*) malloc (naddrs * sizeof(struct nvm_addr));
  lba2nvm_addr(ocssd->virtual_geo,lba_off,lba_len,&(ctx->naddrs),ctx->addrs);

  if (dg) {
      struct nvm_addr tmp_addrs[ctx->naddrs];
      for(int i = 0 ; i < ctx->naddrs ; ++i){
          tmp_addrs[i] = ctx->addrs[i];
      }
      virtual2physical(ocssd,tmp_addrs,ctx->naddrs);
      int n = sprintf( dg->debug_str , "off=0x%lx,len=0x%lx,naddrs=%d,", lba_off , lba_len , ctx->naddrs);
      nvm_addr2str(tmp_addrs,ctx->naddrs,dg->debug_str + n );
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

  //divide
  for(int i = 0 ; i < ctx->naddrs ; ++i) {
    ctx->data_len = 0x1000;

    struct nvm_addr virtual_addr = ctx->addrs[i];
    //Update write_pos
    uint32_t vblk_idx = virtual_addr.g.blk;

    virtual2physical(ocssd,ctx->addrs+i ,1);

    //Write to device
    nvm_addr_write(ocssd->dev,ctx->addrs + i , 1 , (char*)ctx->data + i * 0x1000 ,NULL,0,NULL);

    ocssd->this_partition->vblks[vblk_idx].write_pos += ctx->data_len;
  }
  return 0;
}

int ocssd_read(struct ocssd_t* ocssd , struct cmd_ctx *ctx)
{
  for(int i = 0 ; i < ctx->naddrs ; ++i) {
    ctx->data_len = 0x1000;
    virtual2physical(ocssd,ctx->addrs + i ,1);
    nvm_addr_read(ocssd->dev,ctx->addrs + i , 1 , (char*)ctx->data + i * 0x1000 ,NULL,0,NULL);
  }
  return 0;
}

int ocssd_erase(struct ocssd_t* ocssd, uint32_t seg_id)
{
    //Lazy way
    if(true) {
        ocssd->this_partition->vblks[seg_id].write_pos = 0;
        LOG_DEBUG("really erase vblk%u",seg_id);
        _erase_vblk(ocssd,seg_id);
    }

    return 0;
}
