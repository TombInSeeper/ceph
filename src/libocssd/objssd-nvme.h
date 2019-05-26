#ifndef __OBJECT_SSD_H__
#define __OBJECT_SSD_H__
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
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>

#include "include/liblightnvm.h"


//typedef struct req_tag{
//    __u32 req_orig;
//}req_tag;
//
///**
// * parameters of reading or writing
// */
//typedef struct _io_u{
//    __u32 obj_id;          /**< id of the object to operate */
//    __u64 obj_off;         /**< the start offset to operate */
//    void *data;            /**< the position to store reading data or write data */
//    __u64 data_size;       /**< data size to transfer */
//    req_tag io_req_tag;    /**< the tag which is passed from top */
//}io_u;
//
///**
// *the struct to store information of one object
// */
//typedef struct {
//    unsigned int created : 1;    /**< the object is created or not */
//    unsigned int obj_id;         /**< id of this object */
//    unsigned int obj_size;       /**< size of this object */
//    __u64 obj_offset;            /**< the writable offset of this object */
//    unsigned int *block_bitmap;
//} nvme_obj;
//
//int obj_read(struct nvm_dev *dev, io_u *io);
//int obj_write(struct nvm_dev *dev, io_u *io);
//int obj_create(struct nvm_dev *dev, unsigned int *obj_id, unsigned int *obj_4k_size);
//int obj_delete(struct nvm_dev *dev, unsigned int obj_id);
//struct nvm_dev* dev_open(const char * dev_path);
//int dev_close(struct nvm_dev *dev);
//
//
////----------------
//int mark_created(unsigned int obj_id);

#define     OCSSD_MAX_BLK_NUM   (1500U)

struct ocssd_t {
    struct nvm_dev *dev;
    struct nvm_geo *geo;
    const  struct nvm_bbt  *g_bbt[8][32];
    struct ocssd_sb_summary_t{
        int nr_sblks;
        int sblk_map[OCSSD_MAX_BLK_NUM];
        struct ocssd_offset_t {
            unsigned int fin_ofst;
        } sblk_ofst[OCSSD_MAX_BLK_NUM];
    } pm_data;

    uint64_t per_seg_size;
};

struct debuginfo_t {
    char *debug_str;
};


/// DEBUG HELPER

void addr_print(struct nvm_addr *addrs, int n , char* buf);

/// function


struct ocssd_t *ocssd_open( const char *path) ;

int ocssd_reset( struct ocssd_t * ocssd);

void ocssd_close( struct ocssd_t* ocssd);


struct cmd_ctx*  ocssd_prepare_ctx(
    struct ocssd_t* ocssd ,
    uint64_t lba_off ,
    uint64_t lba_len ,
    char* data ,
    struct debuginfo_t *dg );

void ocssd_destory_ctx(struct cmd_ctx *ctx);


int ocssd_write(struct ocssd_t* ocssd , struct cmd_ctx *ctx);
int ocssd_read(struct ocssd_t* ocssd , struct cmd_ctx *ctx);

int ocssd_erase(struct ocssd_t* , uint32_t seg_id );




#endif
