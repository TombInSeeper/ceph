#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include "liblightnvm.h"
#include <hal_timer.h>
#include <memory.h>
#include <nvm_dev.h>
#include <nvm_async.h>
#include <log_csv.h>
#include <errno.h>
#include <nvm_debug.h>
#include <nvm_vblk.h>
#include "test_list.h"

static ssize_t ppa_wr_sync(struct nvm_dev *dev, bool do_write,
                           struct nvm_addr *blks, size_t nblks,
                           uint8_t *data, size_t len,
                           double times[])
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes;
    struct hal_timer time;

    uint32_t cnt = 0;
    for (uint32_t unit = 0; unit < len / unit_size; unit++) {
        uint8_t *buf_off = &data[unit * unit_size];

        for (uint32_t i = 0; i < unit_sectors; ++i) {
            struct nvm_addr addr = nvm_addr_next(dev, blks[unit % nblks], i + unit/nblks*unit_sectors);
//			printf("%s-%d [%3d] ", __func__, do_write, i); nvm_addr_pr_man(dev, addr);
            hal_timer_start(&time);
            if (do_write == true) {
                nvm_addr_write(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, NULL);
            } else {
                nvm_addr_read(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, NULL);
            }
            hal_timer_stop(&time);
            times[cnt++] = hal_timer_elapsed_usecs(&time);
        }
    }

    return len;
}

void count_latency(char *log_name, size_t data_len, const double *times, uint32_t len, uint32_t qos_th)
{
    double total_us = 0;
    double max_time = 0;
    double min_time = 0xffffffffff;
    double qos = 0;

    for (uint32_t i = 0; i < len; ++i) {
        double tmp = times[i];
        total_us += tmp;
        if (tmp > max_time)
            max_time = tmp;
        if (tmp < min_time)
            min_time = tmp;
        if (tmp > qos_th)
            qos++;
    }
    double avg_time = total_us / len;
    printf("%s: avg latency:"PR_GREEN"%9.3f us"PR_CLEAN", ", log_name, avg_time);
    printf("min:"PR_GREEN"%9.3f us"PR_CLEAN", ", min_time);
    printf("max:"PR_GREEN"%9.3f us"PR_CLEAN", ", max_time);
    printf("speed:"PR_GREEN"%9.3f MB/s"PR_CLEAN", ", data_len / total_us);
    printf("Qos:"PR_GREEN"%6.3f%%"PR_CLEAN"(%.0f/%d)\n", 100 - qos * 100 / len, qos, len);
}

void test_ppa_latency_qd1(void **state)
{
    struct nvm_dev *dev = *state;
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;
    uint32_t nblks = (uint32_t) (geo->nchannels * geo->nluns);
    struct nvm_addr blks[nblks];

    assert_int_equal(0, nvm_bbt_gbbt_arbs(dev, NVM_BBT_FREE, nblks, blks));
    nvm_addr_prn(blks, nblks, dev);

    struct nvm_buf_set *bufs = nvm_buf_set_alloc(dev, nblks * size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

    assert_int_equal(0, nvm_addr_erase(dev, blks, nblks, NVM_FLAG_PMODE_DUAL, NULL));

    uint32_t num = (uint32_t) (bufs->nbytes / geo->sector_nbytes);
    double time_write[num];
    assert_int_equal(bufs->nbytes,
                     ppa_wr_sync(dev, true, blks, nblks, (uint8_t *) bufs->write, bufs->nbytes, time_write));
    count_latency("4K_write_latency_qd1", bufs->nbytes, time_write, num, 100);
    log_csv("4K_latency_qd1", "Time (us)", "write", time_write, num);

    sleep(5);

    double time_read[num];
    assert_int_equal(bufs->nbytes,
                     ppa_wr_sync(dev, false, blks, nblks, (uint8_t *) bufs->read, bufs->nbytes, time_read));
    count_latency("4K_read__latency_qd1", bufs->nbytes, time_read, num, 200);
    log_csv("4K_latency_qd1", "Time (us)", "read", time_read, num);

    assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
    nvm_buf_set_free(bufs);
}

static ssize_t ppa_wr_async(struct nvm_dev *dev, bool do_write,
                            struct nvm_addr *blks, size_t nblks,
                            uint8_t *data, size_t len,
                            double *times, uint32_t depth)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes;
    struct hal_timer time;
#if 0
    int err;
    struct nvm_async_ctx *aio = nvm_async_init(dev->fd, depth);
    hal_timer_start(&time);
    for (uint32_t unit = 0; unit < len / unit_size; unit++) {
        uint8_t *buf_off = &data[unit * unit_size];

        for (uint32_t i = 0; i < unit_sectors; ++i) {
            struct nvm_addr addr;
            addr.ppa = blks[unit % nblks].ppa;
            addr.g.pg = i / (geo->nsectors * geo->nplanes) + (unit / nblks);
            addr.g.pl = (i / geo->nsectors) % geo->nplanes;
            addr.g.sec = i % geo->nsectors;

//			printf("%s-%d [%3d] ", __func__, do_write, i); nvm_addr_pr_man(dev, addr);
            uint64_t offset = nvm_addr_gen2dev(dev, addr) * 512;
            while (1) {
                if (do_write == true) {
                    err = nvm_async_pwrite(aio, &buf_off[i * geo->sector_nbytes], geo->sector_nbytes, offset, NULL);
                } else {
                    err = nvm_async_pread(aio, &buf_off[i * geo->sector_nbytes], geo->sector_nbytes, offset, NULL);
                }
                if (err < 0) {
                    if (errno == EAGAIN) {
                        while (1) {
                            int poke = nvm_async_poke(aio, 0);
                            if (poke > 0) {
                                LOG_INFO("async poke event=%d", poke);
                                break;
                            }
                        }
                        continue;
                    } else
                        return -1;
                }
                break;
            }
        }
    }
    nvm_async_wait(aio);
    hal_timer_stop(&time);
    *times = hal_timer_elapsed_usecs(&time);

    nvm_async_term(aio);
#else
    static struct cmd_ctx ctx[256];
    bool ctx_state[256] = {0};
    uint32_t ctx_id = 0;
    uint32_t ctx_cnt = 0;

    hal_timer_start(&time);
    for (uint32_t unit = 0; unit < len / unit_size; unit++) {
        uint8_t *buf_off = &data[unit * unit_size];

        for (uint32_t i = 0; i < unit_sectors; ++i) {
            struct nvm_addr addr;
            addr.ppa = blks[unit % nblks].ppa;
            addr.g.pg = i / (geo->nsectors * geo->nplanes) + (unit / nblks);
            addr.g.pl = (i / geo->nsectors) % geo->nplanes;
            addr.g.sec = i % geo->nsectors;

            for (uint32_t j = 0; j < depth; ++j) {
                if(ctx_state[j] == false) {
                    ctx_id = j;
                    ctx_state[j] = true;
                    break;
                }
            }

            ctx[ctx_id].data = &buf_off[i * geo->sector_nbytes];
            ctx[ctx_id].addrs = &addr;
            ctx[ctx_id].naddrs = 1;

            if (do_write == true) {
                nvm_addr_async_protected_write(dev, &ctx[ctx_id], 0, 0, 0, 0);
            } else {
                nvm_addr_async_read(dev, &ctx[ctx_id], 0, 0);
            }
            ctx_cnt ++;
            LOG_INFO("ctx_id=%d, ctx_cnt=%d", ctx_id, ctx_cnt);
            if( ctx_cnt == depth ) {
                struct result_ctx ctx_rlt;
                nvm_get_async_cmd_event(dev, &ctx_rlt);
                for (int ctx_i = 0; ctx_i < ctx_rlt.max_count; ++ctx_i) {
                    for (uint32_t j = 0; j < depth; ++j) {
                        if(ctx_rlt.ctx[ctx_i] == &ctx[j]) {
                            LOG_INFO("free[%d] index=%d", ctx_i, j);
                            ctx_state[j] = false;
                        }
                    }
                }
                ctx_cnt -= ctx_rlt.max_count;
                LOG_INFO("end ctx_cnt=%d", ctx_cnt);
            }
        }
    }
    while (1) {
        struct result_ctx ctx_rlt;
        nvm_get_async_cmd_event(dev, &ctx_rlt);
        LOG_INFO("ctx_rlt.max_count=%d", ctx_rlt.max_count);
        if(nvm_async_get_outstanding(dev->aio) == 0)
            break;
    }
    hal_timer_stop(&time);
    *times = hal_timer_elapsed_usecs(&time);

#endif
    return len;
}

void test_ppa_iops_qd256(void **state)
{
    struct nvm_dev *dev = *state;
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;
    uint32_t nblks = (uint32_t) (geo->nchannels * geo->nluns);

    uint32_t total = 100;
    struct nvm_vblk *vblk[total];
    nvm_vblk_create(dev, vblk, total);

    struct nvm_buf_set *bufs = nvm_buf_set_alloc(dev, nblks * size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

//    for (uint32_t i = 0; i < len; ++i)
    {
        uint32_t i = rand()%total;
        nvm_addr_pr_man(dev, vblk[i]->blks[0]);
        assert_int_equal(0, nvm_vblk_erase(vblk[i]));

        uint32_t num = (uint32_t) (bufs->nbytes / geo->sector_nbytes);
        double time_write;
        assert_int_equal(bufs->nbytes,
                         ppa_wr_async(dev, true, vblk[i]->blks, vblk[i]->nblks,
                                      (uint8_t *) bufs->write, bufs->nbytes, &time_write, 256));
        printf("4K_write_iops_qd%d: time:%9.3f ms, count:%d, IOPS:"PR_GREEN"%9.3fK"PR_CLEAN"\n",
               256, time_write / 1000, num, num * 1000 / time_write);

        sleep(5);

        double time_read;
        assert_int_equal(bufs->nbytes,
                         ppa_wr_async(dev, false, vblk[i]->blks, vblk[i]->nblks,
                                      (uint8_t *) bufs->read, bufs->nbytes, &time_read, 256));
        printf("4K_read__iops_qd%d: time:%9.3f ms, count:%d, IOPS:"PR_GREEN"%9.3fK"PR_CLEAN"\n",
               256, time_read / 1000, num, num * 1000 / time_read);

        assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
    }

    nvm_buf_set_free(bufs);
    for (uint32_t j = 0; j < total; ++j) {
        nvm_vblk_free(vblk[j]);
    }
}

static char g_dev_path[1024];

static int setup(void **state)
{
    struct nvm_dev *dev = nvm_dev_open(g_dev_path);
    assert_non_null(dev);
    if (!dev)
        return -1;

    *state = dev;
    return 0;
}

static int teardown(void **state)
{
    struct nvm_dev *dev = *state;
    nvm_dev_close(dev);
    return 0;
}

int unit_ppa(char *dev_path)
{
    srand((unsigned int) time(NULL));
    strcpy(g_dev_path, dev_path);

    register_test("io_basic_latency_qd1", test_ppa_latency_qd1, setup, teardown, dev_path);
    register_test("io_basic_iops_qd256", test_ppa_iops_qd256, setup, teardown, dev_path);
 
    return 0;   
}
