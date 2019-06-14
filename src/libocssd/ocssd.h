#ifndef OCSSD_H
#define OCSSD_H
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include "./include/nvm_debug.h"
#include "./src/nvm_dev.h"
#include "include/liblightnvm.h"

#define  OCSSD_META_MAGIC    (0x99924678)
#define  OCSSD_MAX_BLK_NUM   (3000)

#define  OCSSD_PARTITION_CHANNEL    1
#define  OCSSD_PARTITION_SUPERBLOCK 2


/*
 * This offset is just for isolation between
 * my meta data and fio bench's meta data
 */
#define  PM_START_OFFSET (sizeof(int) * OCSSD_MAX_BLK_NUM)



struct ocssd_vblk_t {
    int pblk_id;
    uint32_t write_pos;
};


struct ocssd_partition_t {
    int         partition_id;
    int         nr_vblks;
    struct {
        uint32_t ch_bg;
        uint32_t ch_end;
    } vblk_range;

    struct ocssd_vblk_t vblks[OCSSD_MAX_BLK_NUM];
} __attribute__((aligned(32)));


struct ocssd_meta_t{
    int magic;
    int nr_sblks;
    int partition_mode;
    int nr_partitions;
} __attribute__((aligned(32)));


struct ocssd_t {

    //open_raw
    struct nvm_dev *dev;
    const struct nvm_geo  *geo;         // == &dev->geo
    const struct nvm_bbt  **bbt;        // == dev->bbt

    //open1/open2
    struct ocssd_meta_t  *ocssd_meta;
    struct ocssd_partition_t *this_partition;
    struct nvm_geo* virtual_geo;
    int   lock_fd ;

    //memory data
    uint64_t vblk_size;

};

struct debuginfo_t {
    char *debug_str;
};





///
/// \param path :
/// \return a ocssd_t which
/// ocssd->dev,ocssd->geo,ocssd->bbt are correctly assigned while other
/// members are undefined
struct ocssd_t *_ocssd_open_raw( const char *path);


///
/// \param path
/// \return a handle of ocssd
/// ocssd->this_partion points to meta_data of my partition including
/// vblk mapping and write pos
struct ocssd_t *ocssd_open( const char *path) ;


///
/// \param ocssd
/// destroy a handle return by ocssd_open or ocssd_open_raw
void ocssd_close( struct ocssd_t* ocssd);

///
/// \param ocssd
/// \param lba_off
/// \param lba_len
/// \param data
/// \param dg
/// \return a io context to ocssd
struct cmd_ctx*  ocssd_prepare_ctx(
    struct ocssd_t* ocssd ,
    uint64_t lba_off ,
    uint64_t lba_len ,
    char* data ,
    struct debuginfo_t *dg );

void ocssd_destory_ctx(struct cmd_ctx *ctx);


int ocssd_write(struct ocssd_t* ocssd , struct cmd_ctx *ctx);

int ocssd_read(struct ocssd_t* ocssd , struct cmd_ctx *ctx);

int ocssd_erase(struct ocssd_t* , uint32_t vblk_id );

#endif
