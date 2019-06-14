//
// Created by root on 19-6-13.
//


#include "ocssd.h"
#include "pthread.h"

typedef struct {
    const char * dev_path;
    uint64_t  total_size;
    uint32_t  io_uint;
}thread_data_t;

thread_data_t thread_datas[] = {
        { "/dev/nvme0n1p1", 384*1024*1024, 4 * 1024 * 1024},
        { "/dev/nvme0n1p2", 384*1024*1024, 4 * 1024 * 1024},
        };

void* thread_worker(void *pdata){

    thread_data_t * tdata = (thread_data_t*)pdata;
    struct ocssd_t* ocssd = ocssd_open(tdata->dev_path);
    if(!ocssd){
        LOG_ERROR("ocssd device open failed!");
        return NULL;
    }

    char * buf = (char*)malloc(tdata->io_uint);
    char * rbuf = (char*)malloc(tdata->io_uint);

    memset(buf,0xff,tdata->io_uint);

    size_t vblk_size = ocssd->virtual_geo->nchannels *
            ocssd->virtual_geo->nluns *
            ocssd->virtual_geo->nplanes *
            ocssd->virtual_geo->npages *
            ocssd->virtual_geo->page_nbytes;

    LOG_DEBUG("vblk_size=%lu MiB",vblk_size/ (1024* 1024UL));
    for( unsigned  i = 0 ; i < tdata->total_size / vblk_size ; ++i){
         ocssd_erase(ocssd,i);
    }

    struct timespec t ,t2;
    clock_gettime(CLOCK_MONOTONIC_RAW , &t);
    for(uint64_t off = 0 ; off < tdata->total_size ; off+= tdata->io_uint){
        //Write
        struct cmd_ctx * ctx = ocssd_prepare_ctx(ocssd,off,tdata->io_uint,buf,NULL);
        ocssd_write(ocssd,ctx);
        ocssd_destory_ctx(ctx);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW , &t2);



    double duration = (double) (t2.tv_sec - t.tv_sec) +
            (double)(t2.tv_nsec - t.tv_nsec) / 1e9 ;

    //
    printf("write bench :total_size=%lu MiB,io_uint=%u KiB, speed=%lf MiB/s\n" ,
            tdata->total_size / (1024 * 1024),
            tdata->io_uint / 1024,
            (tdata->total_size / (1024.0 * 1024UL))/ duration );
    //
    free(buf);
    free(rbuf);

    ocssd_close(ocssd);

    return  NULL;
}


int main ( int argc , char *argv[])
{
    const int concurrent = 2;

    pthread_t threads[concurrent];
    //
    for( int i = 0 ; i < concurrent ; ++i) {
        pthread_create(&threads[i],NULL,thread_worker, &thread_datas[i]);
    }
    //
    for(int i = 0 ; i < concurrent; ++i) {
        pthread_join(threads[i],NULL);
    }
    return 0;
}


