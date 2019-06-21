//
// Created by wuyue on 12/26/18.
//
#include "include/stringify.h"
#include "include/types.h"
#include "include/compat.h"
#include "common/align.h"
#include "common/errno.h"
#include "common/debug.h"
#include "common/perf_counters.h"
#include "common/io_priority.h"
#include <fstream>
#include <x86intrin.h>

extern "C" {
#include "libocssd/ocssd.h"
};


#include "OCDevice.h"

#define     OCSSD_IO_READ       (0x1)
#define     OCSSD_IO_WRITE      (0x2)
#define     OCSSD_MAX_IO_SIZE   (96*1024ULL)



#undef dout_context
#define dout_context g_ceph_context
#define dout_subsys ceph_subsys_bdev
#undef dout_prefix
#define dout_prefix *_dout << "OCDevice "

#define OCSSD_DEBUG 0
#define DOUT_LEVEL 10


static std::atomic_uint read_seq = {0};
static std::atomic_uint write_seq = {0};

void OCDevice::_aio_start()
{
    dout(1) << __func__ << "..doing.." << dendl;
    aio_stop = false;
    aio_thread.init();
    dout(1) << __func__ << "..done.." << dendl;
}

void OCDevice::_aio_stop()
{
    dout(1) << __func__ << "..doing.." << dendl;
    {
        std::lock_guard< std::mutex> l(aio_submit_mtx);
        aio_stop = true;
        aio_cv.notify_all();
    }
    aio_thread.shutdown();
    dout(1) << __func__ << "..done.." << dendl;
}

int OCDevice::open( std::string path)
{
   (void)(path);
   dout(1) << __func__ << "..doing.." << dendl;
   fd = ::open(path.c_str(), O_RDWR);

   if(g_conf->bdev_ocssd_driver == "libocssd") {
        ocssd = ocssd_open(g_conf->bdev_ocssd_device.c_str());
   }

    // aio_start
    _aio_start();
    dout(1) << __func__ << "..done.. segs_num:"
        << ocssd->this_partition->nr_vblks<< "" << dendl;

    return 0;
}

void OCDevice::close() {


    dout(1) << __func__ << "..doing.." << dendl;

    // aio stop
    _aio_stop();


    if(g_conf->bdev_ocssd_driver == "libocssd") {
        // write_back pm_data
        ocssd_close(ocssd);
    }
//    else
//    {
//        ofstream fp("/tmp/ceph_core",ios::out | ios::binary);
//        fp.write((char*)(&ocssd->pm_data),sizeof(ocssd->pm_data));
//        free(ocssd);
//    }

    ::close(fd);

    dout(1) << __func__ << "..done.." << dendl;

}

int OCDevice::read_buffered(uint64_t off, uint64_t len, char *buf)
{
    bufferlist bl;
    IOContext ioc(NULL);
    read(off,len,&bl,&ioc,false);
    bl.copy(0,bl.length(),buf);
    return bl.length();
};

int OCDevice::aio_zero(uint64_t off, uint64_t len, IOContext *ioc)
{
    derr << __func__ << "NOT SUPPORTED" << dendl;
    return EIO;
}

uint64_t OCDevice::get_size() const {

    return ocssd->vblk_size * ocssd->this_partition->nr_vblks;
} ;

uint64_t OCDevice::get_block_size() const {
    return 4096;
};

int OCDevice::init_disk() {
//    if(g_conf->bdev_ocssd_driver == "libocssd") {
//        dout(1) << __func__ << ",erasing the device..."<< dendl;
//        for(size_t i = 0 ; i < ocssd->this_partition->nr_vblks ; ++i){
//            ocssd_erase(ocssd,i);
//        }
//    }
//    else
//    {
//        memset(&ocssd->pm_data,0,sizeof(ocssd->pm_data));
//        ocssd->pm_data.nr_sblks = 100;
//        for(int i = 0 ; i < 100; ++i ) {
//            ocssd->pm_data.sblk_map[i] = i;
//            ocssd->pm_data.sblk_ofst[i].fin_ofst = 0;
//        }
//        ofstream fp("/tmp/ceph_core",ios::out | ios::binary);
//        fp.write((char*)(&ocssd->pm_data),sizeof(ocssd->pm_data));
//    }
    ceph_assert(ocssd->ocssd_meta->magic == OCSSD_META_MAGIC);
    dout(1) << __func__ << ",done"  << dendl;

}

int OCDevice::read(uint64_t off, uint64_t len, bufferlist *pbl, IOContext *ioc , bool buffered) {

    //ceph_assert( len <= (OCSSD_MAX_IO_SIZE));
    ceph_assert( len % 4096 == 0);
    ceph_assert( off % 4096 == 0);


    dout(10) << __func__ << "..doing.." << dendl;
    uint64_t nr_align = len / (OCSSD_MAX_IO_SIZE);
    uint64_t nr_unalign = len % (OCSSD_MAX_IO_SIZE);
    uint64_t _off = off , _len = len;

    ocssd_aio_t new_aio;
    //ioc->ocssd_pending_aios.push_back(std::move(new_aio));


    ocssd_aio_t &aio = new_aio;
    aio.lba_off = off;
    aio.lba_len = len;
    aio.priv = ioc;
    aio.io_type = OCSSD_IO_READ;
    aio.io_depth = (uint8_t)(nr_align + (nr_unalign > 0));

    uint64_t _clen =  _len;
    bufferptr  p = buffer::create_aligned((uint32_t)_clen , 4096);

    char buf[1024];
    struct debuginfo_t dg , *pdg = NULL;
    dg.debug_str = buf;

    if(OCSSD_DEBUG)
        pdg = &dg;

    //sync read
    if(g_conf->bdev_ocssd_driver == "libocssd")
    {
        aio.ocssd_ctx = ocssd_prepare_ctx(ocssd,aio.lba_off,aio.lba_len,p.c_str(), pdg ) ;
        aio.dg_str = dg.debug_str;
        dout(DOUT_LEVEL) << __func__ << "," << read_seq++ << "," <<  aio.dg_str << dendl;
        ocssd_read(ocssd , (struct cmd_ctx*)aio.ocssd_ctx);
        ocssd_destory_ctx ((struct cmd_ctx*)aio.ocssd_ctx);
    }
//    else
//    {
//        dout(DOUT_LEVEL) << __func__ << read_seq++ << ",[READ]" << std::hex <<
//                         "[0x" << aio.lba_off << "~" << aio.lba_len << "]" <<  std::dec << dendl;
//        ::pread(fd,p.c_str(),aio.lba_len,aio.lba_off);
//    }
    pbl->append(std::move(p));
    //dout(10) << __func__ << "..done.." << dendl;
    return 0;
}

int OCDevice::aio_write(uint64_t off, bufferlist &bl, IOContext *ioc, bool buffered){

    (void)buffered;
    (void)(ioc);


    dout(10) << __func__ << "..doing.." << dendl;

    auto len = bl.length();
    ceph_assert( len % 4096 == 0);
    ceph_assert( off % 4096 == 0);
    //ceph_assert( len <= (OCSSD_MAX_IO_SIZE));

    uint64_t nr_align = len / (OCSSD_MAX_IO_SIZE);
    uint64_t nr_unalign = len % (OCSSD_MAX_IO_SIZE);
    uint64_t i;
    uint64_t _off = off , _len = len;

    ocssd_aio_t new_aio;
    ioc->ocssd_pending_aios.push_back(std::move(new_aio));
    ioc->num_pending++;
    ocssd_aio_t *aio = &(ioc->ocssd_pending_aios.back());

    aio->lba_off = off;
    aio->lba_len = len;
    aio->priv = ioc;
    aio->bl.claim_append(bl);
    aio->bl.rebuild_page_aligned();
    aio->io_type = OCSSD_IO_WRITE;
    aio->io_depth = (uint8_t)(nr_align + (nr_unalign > 0));


    char buf [1024];
    struct debuginfo_t dg ,*pdg = NULL;
    dg.debug_str = buf;
    if(OCSSD_DEBUG)
        pdg = &dg;

    if(g_conf->bdev_ocssd_driver == "libocssd") {
        aio->ocssd_ctx = ocssd_prepare_ctx(ocssd,aio->lba_off,aio->lba_len,
                                           aio->bl.c_str(),
                                           pdg);
        aio->dg_str = buf;
    }
    return 0;
}

void OCDevice::aio_submit(IOContext *ioc)
{

    dout(10) << __func__ << "..doing.." << dendl;

    if(ioc->num_pending.load() == 0)
        return;

    ioc->ocssd_running_aios.swap(ioc->ocssd_pending_aios);
    assert(ioc->ocssd_pending_aios.empty());

    int pending = ioc->num_pending.load();
    ioc->num_pending -= pending;
    ioc->num_running += pending;

    uint8_t io_type = ioc->ocssd_running_aios.front().io_type;
    ssize_t r = 0;


    switch (io_type){
        case OCSSD_IO_WRITE:
        {
            while(ioc->ocssd_submit_seq != submitted_seq.load()){
                dout(20) << __func__ << " blocking here as seq is not updated yet " << dendl;
                _mm_pause();
               }
            dout(DOUT_LEVEL) << __func__ << "..submitting seq=" << ioc->ocssd_submit_seq << dendl;
            std::lock_guard<mutex> l(aio_submit_mtx);
            aio_queue.push_back(ioc);
            ++submitted_seq;
            aio_cv.notify_all();
            break;
        }
        default:
        {
            derr << "unknown io-type" << dendl;
            ceph_abort();
            break;
        }
    }
    dout(10) << __func__ << "..done.." << dendl;

}

void OCDevice::aio_thread_work()
{
    auto pred = [&](){
        return aio_stop || !aio_queue.empty();
    };
    std::list<IOContext*> _io_queue;
    std::unique_lock<std::mutex> l(aio_submit_mtx);
    while(true)
    {
        aio_cv.wait(l,pred);
        if(aio_stop)
        {
            return;
        }
        {
            _io_queue.swap(aio_queue);
            l.unlock();
        }
        dout(10) << __func__ << " The IOContetx queue depth: " << _io_queue.size() << dendl;
        while ( !_io_queue.empty())
        {

            IOContext * ioc = _io_queue.front();
            for(auto it = ioc->ocssd_running_aios.begin()  ;
                it != ioc->ocssd_running_aios.end();
                ++it)
            {

                ocssd_aio_t *aio = &(*it);
                if((g_conf->bdev_ocssd_driver == "libocssd"))
                {
                    //dout(0) << __func__ << std::hex <<  " TRULY:LBA_off=" << aio->lba_off << " LBA_len=" << aio->lba_len << std::dec << " done.." << dendl;
                    dout(DOUT_LEVEL) << __func__ << "," << "[WRITE]," << write_seq++ << "," << aio->dg_str << dendl;
                    ocssd_write(ocssd, (struct cmd_ctx*)aio->ocssd_ctx);
                    ocssd_destory_ctx((struct cmd_ctx*)aio->ocssd_ctx);
                }
//                else
//                {
//                    dout(DOUT_LEVEL) << __func__ << ",[WIRTE]" << write_seq++   << std::hex <<
//                     ",[0x" << aio->lba_off << "~" << aio->lba_len << "]" <<  std::dec << dendl;
//                    ::pwrite(fd,aio->bl.c_str(),aio->lba_len,aio->lba_off);
//                }
//
//                dout(10) << __func__ << std::hex <<  " off=" << aio->lba_off << " len=" << aio->lba_len << std::dec << " done.." << dendl;
//
//                auto id = aio->lba_off / OCSSD_SEG_SIZE;
//                auto iofst = aio->lba_off % OCSSD_SEG_SIZE + aio->lba_len;
//                if(likely (iofst <= OCSSD_SEG_SIZE) )
//                {
//                    ocssd->pm_data.sblk_ofst[id].fin_ofst += aio->lba_len;
//                }
//                else
//                {
//                    ocssd->pm_data.sblk_ofst[id].fin_ofst = OCSSD_SEG_SIZE;
//                    ocssd->pm_data.sblk_ofst[id + 1].fin_ofst = iofst - OCSSD_SEG_SIZE;
//                }
            }

            if(likely(ioc->priv != nullptr))
            {
                ioc->num_running = 0;
                aio_callback(this->aio_callback_priv,ioc->priv);
            }
            _io_queue.pop_front();
        }
        l.lock();
    }
}

int OCDevice::get_written_extents(interval_set<uint64_t> &p)
{
    for ( int i = 0 ; i < ocssd->this_partition->nr_vblks ; ++i) {
      uint32_t ofst = ocssd->this_partition->vblks[i].write_pos;
      if(ofst) {
          p.insert( (uint32_t)i * ocssd->vblk_size ,
                  ofst);
      }
    }
    return 0;
}

int OCDevice::queue_discard(interval_set<uint64_t> &p)
{

  const uint32_t  OCSSD_SEG_SIZE = ocssd->vblk_size;

  for(auto it = p.begin() ; it != p.end() ; ++it) {
       ceph_assert(it.get_start() % OCSSD_SEG_SIZE == 0);
       ceph_assert(it.get_len() % OCSSD_SEG_SIZE == 0);
       uint64_t bg_id = it.get_start() / OCSSD_SEG_SIZE;
       uint64_t end_id = (it.get_start()+it.get_len()) / OCSSD_SEG_SIZE;
       uint64_t i;
       for (i= bg_id ; i < end_id ;++i) {
	  dout(1) << __func__ << ",erase vblk=" << i << dendl;
          ocssd_erase( ocssd , i);
       }
  }

  return 0;
}

uint32_t OCDevice::get_segment_size() {
    return (uint32_t )ocssd->vblk_size;
}
