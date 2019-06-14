//
// Created by root on 19-6-12.
//

#include <liblightnvm.h>
#include "ocssd.h"
#include "getopt.h"


static void _init_blk_map(struct ocssd_t * ocssd ,
        int *blk_map,
        int max_count) {
    unsigned int blk_start = 80;
    unsigned int blk_idx = 0;
    unsigned int ref_idx;
    unsigned int i, j;
    const struct nvm_geo *geo = ocssd->geo;


    blk_start += (int)(678 + getpid() % 50);
    ref_idx = blk_start;
    while (blk_idx < OCSSD_MAX_BLK_NUM) {
        bool is_bad = false;
        for (i = 0; i < geo->nchannels; i++) {
            for (j = 0; j < geo->nluns; j++) {
                if (ocssd->bbt[i * geo->nluns + j]->blks[blk_start * 2] != NVM_BBT_FREE
                    || ocssd->bbt[i * geo->nluns + j]->blks[blk_start * 2 + 1] != NVM_BBT_FREE) {
                    is_bad = true;
                    LOG_DEBUG("Skip bad block:%d",blk_start);
                    break;
                }
            }
            if (is_bad) {
                break;
            }
        }
        if (is_bad == false) {
            // Find a good block
            blk_map[blk_idx] = blk_start;
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
    blk_map[blk_idx] = -1;
}


struct init_opt_t {
    const char* dev_path;
    int mode;
    int nr_sblks;
    int nr_partitons;
};

const char *partition_mode[] = {
        "Unknown",
        "Channel",
        "SuperBlock",
        ""
};

void _dump_meta (struct ocssd_meta_t* meta){

    struct ocssd_partition_t *partitions = (struct ocssd_partition_t *)(meta+1);
    printf("partition result:\n");
    printf("[meta]:nr_partions=%d,nr_sblks=%d,partition_mode=%s\n"
            ,meta->nr_partitions
            ,meta->nr_sblks
            ,partition_mode[meta->partition_mode]);

    for (int i = 0 ; i < meta->nr_partitions ;++i) {
        struct ocssd_partition_t *p = &partitions[i];
        printf("[part%d]:nr_vblks=%d,ch_range:[%u~%u)\n",
               p->partition_id , p->nr_vblks,
               p->vblk_range.ch_bg , p->vblk_range.ch_end);
        printf("[blk_map_info]\n");
        for(int j = 0 ; j < p->nr_vblks ; ++j){
            printf("%d:%d,",j,p->vblks[j].pblk_id);
        }
        printf("\n");
    }
}

int init_ocssd( struct init_opt_t *opt)
{
    struct ocssd_t* ocssd = _ocssd_open_raw(opt->dev_path);
    if(!ocssd) {
        LOG_ERROR("ocssd:%s opening failed!\n" , opt->dev_path);
        exit(1);
    }

    //reset raw device
    nvm_ext_reset(ocssd->dev);
    LOG_DEBUG("nvm_ext_reset...over.");

    //Get global blk map and erase the entire device
    int global_blk_map[OCSSD_MAX_BLK_NUM];
    _init_blk_map(ocssd,global_blk_map,opt->nr_sblks);
    LOG_DEBUG("generate block map...over.");

    for(int i = 0 ; i < opt->nr_sblks ;++i){
        struct nvm_addr addr;
        memset(&addr,0,sizeof(addr));
        addr.g.blk = global_blk_map[i];
        nvm_addr_erase_sb(ocssd->dev,&addr,1,0,NULL);
    }
    LOG_DEBUG("erase all super blocks...over.");


    //alloc memory for meta data
    size_t  meta_size = sizeof(struct ocssd_meta_t)
                        + sizeof(struct ocssd_partition_t) * opt->nr_partitons;
    struct ocssd_meta_t * meta = (struct ocssd_meta_t*)malloc(sizeof(struct ocssd_meta_t)
            + sizeof(struct ocssd_partition_t) * opt->nr_partitons);
    struct ocssd_partition_t *partitions = (struct ocssd_partition_t*)(meta + 1);

    //meta
    {
        meta->magic = OCSSD_META_MAGIC;
        meta->nr_partitions = opt->nr_partitons;
        meta->partition_mode = opt->mode;
        meta->nr_sblks = opt->nr_sblks;
    }
    LOG_DEBUG("fill meta information...over.");

    //partition info
    for( int i = 0 ; i < opt->nr_partitons ; ++i) {
        partitions[i].partition_id = i + 1;
        if (opt->mode == OCSSD_PARTITION_CHANNEL) {
            partitions[i].vblk_range.ch_bg = i * (ocssd->geo->nchannels / opt->nr_partitons);
            partitions[i].vblk_range.ch_end = (i+1) * (ocssd->geo->nchannels / opt->nr_partitons);
            partitions[i].nr_vblks = opt->nr_sblks;
            for(int j = 0 ; j < partitions[i].nr_vblks ;++j){
                partitions[i].vblks[j].write_pos = 0;
                partitions[i].vblks[j].pblk_id = global_blk_map[j];
            }
        } else if (opt->mode == OCSSD_PARTITION_SUPERBLOCK) {
            partitions[i].vblk_range.ch_bg = 0;
            partitions[i].vblk_range.ch_end = ocssd->geo->nchannels;
            partitions[i].nr_vblks = opt->nr_sblks / opt->nr_partitons;
            for(int j = 0 ; j < partitions[i].nr_vblks ;++j){
                partitions[i].vblks[j].write_pos = 0;
                partitions[i].vblks[j].pblk_id =
                        global_blk_map[ i * (opt->nr_sblks / opt->nr_partitons) + j];
            }
        }
        else {
            LOG_ERROR(" Unknown Patitin Mode:%d..",opt->mode);
            return 1;
        }
    }
    LOG_DEBUG("fill partition information...over.");

    //write to pm
    nvm_write_pm(ocssd->dev , meta ,0 , meta_size , PM_START_OFFSET , NULL);
    LOG_DEBUG("write back to dev->pm...over.");

    //Dump result
    _dump_meta(meta);

    free(meta);
    ocssd_close(ocssd);
    LOG_DEBUG("close device...over.");

    return 0;
}

void help()
{
    printf("[**help**]\n");
    printf("./ocssd_cli -d <dev_path> [option]\n");
    printf("required: -p <nr_partitions>,can be 1~8. \n");
    printf("required: -m <patition_mode>,can be 1(ch) or 2(sb).\n");
    printf("required: -n <nr_sblks>,can be 10~1000.\n");
    exit(0);
}

int main(int argc , char *argv[])
{
    const char* opt_str = "hd:p:m:n:";
    int op;

    struct init_opt_t opts;

    opts.dev_path = NULL;
    opts.nr_sblks = 100;
    opts.nr_partitons = -1;
    opts.mode = -1;

    if(argc < 2)
        help();

    while( (op = getopt(argc,argv,opt_str)) != -1) {
       if(op == 'd') {
           opts.dev_path = optarg;
       }
       else if( op == 'p') {
           opts.nr_partitons = atoi(optarg);
       }
       else if( op == 'm') {
           opts.mode = atoi(optarg);
       }
       else if(op == 'n'){
           opts.nr_sblks = atoi(optarg);
       }
       else if(op == 'h'){
           help();
       }
       else {
           help();
       }
    }


    LOG_DEBUG("start initialize..");
    int r = init_ocssd(&opts);
    if( r == 0) {
        printf("Partition Successfully!\n");
    }
    else
    {
        return 1;
    }
}



