//
// Created by root on 19-6-13.
//


#include "ocssd.h"
#include "pthread.h"

typedef struct {

    double speed;
    double lat_erase;
    double iops;
    double lat_avg;
    double lat_50th;
    double lat_99th;
    double lat_99_9th;
} result;

typedef struct {
    int id ;
    const char * dev_path;
    uint64_t  total_size;
    uint32_t  io_uint;
    result * r;


}thread_data_t;

thread_data_t thread_datas[] = {
        { 0 , "/dev/nvme0n1p1", 384*1024*1024, 64 * 1024,NULL},
        { 1 , "/dev/nvme0n1p2", 384*1024*1024, 64 * 1024,NULL},
        { 2 , "/dev/nvme0n1p3", 384*1024*1024, 64 * 1024,NULL},
        { 3 , "/dev/nvme0n1p4", 384*1024*1024, 64 * 1024,NULL},
        };


thread_data_t thread_datas_read[] = {
        { 0 , "/dev/nvme0n1p1", 384*1024*1024, 4 * 1024,NULL},
        { 1 , "/dev/nvme0n1p2", 384*1024*1024, 4 * 1024,NULL},
        { 2 , "/dev/nvme0n1p3", 384*1024*1024, 4 * 1024,NULL},
        { 3 , "/dev/nvme0n1p4", 384*1024*1024, 4 * 1024,NULL},
};
void* thread_worker(void *pdata){

    thread_data_t * tdata = (thread_data_t*)pdata;
    struct ocssd_t* ocssd = ocssd_open(tdata->dev_path);
    if(!ocssd){
        LOG_ERROR("ocssd device open failed!");
        return NULL;
    }

//    cpu_set_t cpus;
//    CPU_ZERO(&cpus);
//    CPU_SET(tdata->id,&cpus);
//    pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpus);

    sleep(1);


    double *lat = (double *) malloc (sizeof(double) *100* 1000);
    uint32_t eio = 0;
    char * buf = (char*)malloc(tdata->io_uint);
    char * rbuf = (char*)malloc(tdata->io_uint);

    memset(buf,0x7a,tdata->io_uint);

    size_t vblk_size = ocssd->virtual_geo->nchannels *
            ocssd->virtual_geo->nluns *
            ocssd->virtual_geo->nplanes *
            ocssd->virtual_geo->npages *
            ocssd->virtual_geo->page_nbytes;

    LOG_DEBUG("vblk_size=%lu MiB",vblk_size/ (1024* 1024UL));
    for( unsigned  i = 0 ; i < tdata->total_size / vblk_size ; ++i){
        struct timespec t ,t2;
        clock_gettime(CLOCK_MONOTONIC_RAW , &t);
        ocssd_erase(ocssd,i);
        clock_gettime(CLOCK_MONOTONIC_RAW , &t2);
        double duration = (double) (t2.tv_sec - t.tv_sec) +
                          (double)(t2.tv_nsec - t.tv_nsec) / 1e9  ;
        lat[eio++] = duration * 1e6;


    }
    double avg = 0.0;
    for(uint32_t i= 0 ; i < eio ; ++i) {
        avg += lat[i];
    }
    avg /= eio;
    printf("erase avg lat = %lf us\n",avg);


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



    //tdata->r = (result*)malloc(sizeof(result));
    tdata->r->lat_erase = avg;
    tdata->r->speed =(tdata->total_size / (1024.0 * 1024UL))/ duration;

    //
    printf("write bench :total_size=%lu MiB,io_uint=%u KiB, speed=%lf MiB/s\n" ,
            tdata->total_size / (1024 * 1024),
            tdata->io_uint / 1024,
            (tdata->r->speed =(tdata->total_size / (1024.0 * 1024UL))/ duration) );
    //
    free(buf);
    free(rbuf);
    free(lat);
    ocssd_close(ocssd);

    return  NULL;
}

static int cmp_double(const void*d1, const void*d2){

    return (*(double*)d1 > *(double*)d2);

}

void* thread_worker_read(void *pdata){


    thread_data_t * tdata = (thread_data_t*)pdata;
    struct ocssd_t* ocssd = ocssd_open(tdata->dev_path);
    if(!ocssd){
        LOG_ERROR("ocssd device open failed!");
        return NULL;
    }
//    cpu_set_t cpus;
//    CPU_ZERO(&cpus);
//    CPU_SET(tdata->id,&cpus);
//    pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpus);

    sleep(1);

    char *buf = (char*)malloc(tdata->io_uint);

    double *lat = (double *) malloc (sizeof(double) *100* 1000);
    uint64_t io = 0;

    struct timespec t ,t2;
    clock_gettime(CLOCK_MONOTONIC_RAW , &t);
    for(uint64_t off = 0 ; off < tdata->total_size ; off+= tdata->io_uint){
        //Write
        struct timespec t ,t2;

        uint64_t roff = ( rand() % tdata->total_size ) & (~(tdata->io_uint-1));
        struct cmd_ctx * ctx = ocssd_prepare_ctx(ocssd,roff,tdata->io_uint,buf,NULL);
        clock_gettime(CLOCK_MONOTONIC_RAW , &t);
        ocssd_read(ocssd,ctx);
        clock_gettime(CLOCK_MONOTONIC_RAW , &t2);
        double duration = (double) (t2.tv_sec - t.tv_sec) +
                          (double)(t2.tv_nsec - t.tv_nsec) / 1e9 ;

        lat[io++] = duration * 1e6;
        ocssd_destory_ctx(ctx);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW , &t2);



    double duration = (double) (t2.tv_sec - t.tv_sec) +
                      (double)(t2.tv_nsec - t.tv_nsec) / 1e9 ;


    printf("read bench :total_size=%lu MiB,io_uint=%u KiB, total_io=%lu, iops=%lf K\n" ,
           tdata->total_size / (1024 * 1024),
           tdata->io_uint / 1024,
           io,
           io / duration / 1000 );


    qsort(lat,io,sizeof(double),cmp_double);

    printf("min lat = %lf us\n",lat[(uint32_t )(0)]);
    printf("max lat = %lf us\n",lat[(uint32_t )(io - 1)]);

    double avg = 0.0;
    for(int i= 0 ; i < io ; ++i) {
        avg += lat[i];
    }
    avg /= io;

    printf("avg lat = %lf us\n",avg);
    printf("50th lat = %lf us\n",lat[(uint32_t )(0.5*io)]);
    printf("99th lat = %lf us\n",lat[(uint32_t )(0.99*io)]);
    printf("99.9th lat = %lf us\n",lat[(uint32_t )(0.999*io)]);


    //tdata->r = (result*) malloc(sizeof(result));
    tdata->r->lat_50th = lat[(uint32_t )(0.5*io)];
    tdata->r->lat_99th = lat[(uint32_t )(0.99*io)];
    tdata->r->lat_99_9th = lat[(uint32_t )(0.999*io)];
    tdata->r->lat_avg = avg;
    tdata->r->iops = io / duration / 1000;

    //
    free(buf);
    free(lat);
    ocssd_close(ocssd);

    return  NULL;
}




void bench( int concurrent , int t) {

    pthread_t threads[concurrent];
    //
    for( int i = 0 ; i < concurrent ; ++i) {
        if( t == 'r')
            pthread_create(&threads[i],NULL,thread_worker_read, &thread_datas_read[i]);
        else if ( t == 'w'){
            pthread_create(&threads[i],NULL,thread_worker, &thread_datas[i]);
        }
    }
    //
    for(int i = 0 ; i < concurrent; ++i) {
        pthread_join(threads[i],NULL);
    }
}



int main ( int argc , char *argv[])
{
    if(argc < 2) {
        printf("ocssd_test <num_threads> <w/r>\n");
        exit(1);
    }
    const int concurrent = atoi(argv[1]);
    int t = argv[2][0];

    for(int i = 0 ; i < concurrent ; ++i){
        thread_datas_read[i].r = (result*)malloc(sizeof(result));
        thread_datas[i].r = (result*)malloc(sizeof(result));
    }


    bench(concurrent,'w');

    if( t == 'r'){
        bench(concurrent, 'r');
    }
    if(1) {

        result r ;
        memset(&r, 0 ,sizeof(r));
        for(int i = 0 ; i < concurrent ;++i) {
            r.speed += thread_datas[i].r->speed;
            r.iops  += thread_datas_read[i].r->iops;
            r.lat_erase += thread_datas[i].r->lat_erase / concurrent;
            r.lat_avg += thread_datas_read[i].r->lat_avg/ concurrent;
            r.lat_50th += thread_datas_read[i].r->lat_50th/ concurrent;
            r.lat_99th += thread_datas_read[i].r->lat_99th/ concurrent;
            r.lat_99_9th += thread_datas_read[i].r->lat_99_9th/ concurrent;
        }
        printf("\n<------result summary------->");
        printf("write_speed = %lf MiB/s, " ,r.speed);
        printf("[vblk=%dblks]lat_erase = %lf us, " ,32 / concurrent , r.lat_erase);
        printf("read_iops = %lf K, " ,r.iops);
        printf("lat_50th = %lf us, " ,r.lat_50th);
        printf("lat_99th = %lf us, " ,r.lat_99th);
        printf("lat_99_9th = %lf us, " ,r.lat_99_9th);
        printf("\n");
    }
    for(int i = 0 ; i < concurrent; ++i) {
        free(thread_datas_read[i].r);
        free(thread_datas[i].r);
    }
    return 0;
}


