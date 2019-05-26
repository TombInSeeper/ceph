#include <stdio.h>
#include <time.h>
#include "nvm_dev.h"
#include "nvm_aer.h"
#include <hal_timer.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "liblightnvm.h"
#include "nvm_debug.h"
#include "test_list.h"


static char g_dev_path[1024];
struct nvm_dev *g_dev;

//one page write
static ssize_t ppa_wr_sync(struct nvm_dev *dev, bool do_write,
                           struct nvm_addr *blks, size_t nblks,
                           uint8_t *data, size_t len)
{
	struct nvm_ret error = {0, 0};
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes;

    for (uint32_t unit = 0; unit < len / unit_size; unit++) {
        uint8_t *buf_off = &data[unit * unit_size];

        for (uint32_t i = 0; i < unit_sectors; ++i) {
            struct nvm_addr addr;
            addr.ppa = blks[unit % nblks].ppa;
            addr.g.pg = i / (geo->nsectors * geo->nplanes) + (unit / nblks);
            addr.g.pl = (i / geo->nsectors) % geo->nplanes;
            addr.g.sec = i % geo->nsectors;
			//printf("%s-%d [%3d] ", __func__, do_write, i); nvm_addr_pr_man(dev, addr);
            if (do_write == true) {
                nvm_addr_write(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, &error);
				if(error.status != 0)
				{
					//printf("[DBG]: the error status = %ld\n", error.status);
				}
				assert_int_equal(error.status, 0);
            } else {
                nvm_addr_read(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, &error);
				if(error.status != 0)
				{
					//printf("[DBG]: the error status = %ld\n", error.status);
				}
				assert_int_equal(error.status, 0);
            }
        }
    }

    return len;
}

static void count_latency(char *log_name, size_t data_len, const double *times, uint32_t len, uint32_t qos_th)
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


static ssize_t ppa_wr_sync_with_time(struct nvm_dev *dev, bool do_write,
                           struct nvm_addr *blks, size_t nblks,
                           uint8_t *data, size_t len,
                           double times[])
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes;
    uint32_t cnt = 0;
    for (uint32_t unit = 0; unit < len / unit_size; unit++) {
        uint8_t *buf_off = &data[unit * unit_size];

        for (uint32_t i = 0; i < unit_sectors; ++i) {
            struct nvm_addr addr;
            addr.ppa = blks[unit % nblks].ppa;
            addr.g.pg = i / (geo->nsectors * geo->nplanes) + (unit / nblks);
            addr.g.pl = (i / geo->nsectors) % geo->nplanes;
            addr.g.sec = i % geo->nsectors;

//			printf("%s-%d [%3d] ", __func__, do_write, i); nvm_addr_pr_man(dev, addr);
			struct hal_timer time;
            hal_timer_start(&time);
                nvm_addr_write(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, NULL);
            if (do_write == true) {
            } else {
                nvm_addr_read(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, NULL);
            }
            hal_timer_stop(&time);
            times[cnt++] = hal_timer_elapsed_usecs(&time);
        }
    }

    return len;
}

static ssize_t ppa_wr_sync_with_time_new(struct nvm_dev *dev, bool do_write,
                           struct nvm_addr *blks, size_t nblks,
                           uint8_t *data, size_t len,
                           double times[])
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes;
    uint32_t cnt = 0;
    for (uint32_t unit = 0; unit < len / unit_size; unit++) {
        uint8_t *buf_off = &data[unit * unit_size];

        for (uint32_t i = 0; i < unit_sectors; ++i) {
            struct nvm_addr addr;
            addr.ppa = blks[unit % nblks].ppa;
            addr.g.pg = i / (geo->nsectors * geo->nplanes) + (unit / nblks);
            addr.g.pl = (i / geo->nsectors) % geo->nplanes;
            addr.g.sec = i % geo->nsectors;

//			printf("%s-%d [%3d] ", __func__, do_write, i); nvm_addr_pr_man(dev, addr);
			struct hal_timer time;
            hal_timer_start(&time);
                nvm_addr_write(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL | NVM_FLASH_SLC_ENABLE , NULL);
            if (do_write == true) {
            } else {
                nvm_addr_read(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL | NVM_FLASH_SLC_ENABLE, NULL);
            }
            hal_timer_stop(&time);
            times[cnt++] = hal_timer_elapsed_usecs(&time);
        }
    }

    return len;
}

void test_erase_page(void **state)
{
	(void)(state);
	struct nvm_ret error = {0, 0};
	//sequential write
	//struct nvm_dev *dev = *state;
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	uint32_t nblks = geo->nluns;

	const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes * 3;
    //const size_t size = geo->npages * geo->page_nbytes;    // * one plane size total:geo->nplanes;
	//const size_t size = geo->page_nbytes;
    struct nvm_addr blks[nblks];
	//char buffForWrite[1024] = {1,2,3,4};
	//char buffForRead[1024] = {0};

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
    nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

	blks[0].ppa   = 0;
	blks[0].g.ch  = 0;
	blks[0].g.lun = 0;
	blks[0].g.blk = 83;

	assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));

	//directly read without program the block
	 nvm_addr_read(g_dev, &blks[0], 1, (uint8_t *) bufs->read, NULL, NVM_FLAG_PMODE_DUAL, &error);
	
	assert_int_equal(0x290, error.status);
	nvm_buf_set_free(bufs);

}

void test_io_read_with_uecc(void **state)
{
	printf("%s", __func__);
	(void)(state);
	struct nvm_ret error = {0, 0};
	//sequential write
	//struct nvm_dev *dev = *state;
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	uint32_t nblks = geo->nluns;

	const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes * 3;
    //const size_t size = geo->npages * geo->page_nbytes;    // * one plane size total:geo->nplanes;
	//const size_t size = geo->page_nbytes;
    struct nvm_addr blks[nblks];
	//char buffForWrite[1024] = {1,2,3,4};
	//char buffForRead[1024] = {0};

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
    nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

	assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));

	assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, true, &blks[0], 1, (uint8_t *)bufs->write, bufs->nbytes));

	assert_int_equal(0, nvm_ext_inject_error(g_dev,  blks[0], INJECT_Read_UNCORRECTABLE));
	
	nvm_addr_read(g_dev, &blks[0], 1, (uint8_t *) bufs->read, NULL, NVM_FLAG_PMODE_DUAL, &error);
	assert_int_equal(0x281, error.status);
	nvm_buf_set_free(bufs);
}


void test_io_aysnc_read_with_uecc(void **state)
{
	(void)(state);
	//sequential write
	//struct nvm_dev *dev = *state;
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	uint32_t nblks = geo->nluns;

	const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes * 3;
    //const size_t size = geo->npages * geo->page_nbytes;    // * one plane size total:geo->nplanes;
	//const size_t size = geo->page_nbytes;
    struct nvm_addr blks[nblks];
	//char buffForWrite[1024] = {1,2,3,4};
	//char buffForRead[1024] = {0};

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_BAD, nblks, blks));
    nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);


	//assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));
	
	//assert_int_equal(bufs->nbytes,
	//						ppa_wr_sync(g_dev, true, &blks[0], 1, (uint8_t *)bufs->write, bufs->nbytes));
	
	//assert_int_equal(0, nvm_ext_inject_error(g_dev,  blks[0], INJECT_Read_UNCORRECTABLE));

	struct cmd_ctx  nvm_cmd_ctx;
	nvm_cmd_ctx.data = bufs->read;
	nvm_cmd_ctx.addrs = &blks[0];
	nvm_cmd_ctx.naddrs = 1;

	ssize_t err = nvm_addr_async_read(g_dev, &nvm_cmd_ctx, 0, 0);
	assert_int_equal(0, err);

	struct result_ctx ctx_out;
	assert_int_equal(0, nvm_get_async_cmd_event(g_dev, &ctx_out));\
	//printf("the max count is %d\n", ctx_out.max_count);
	assert_int_equal(1, ctx_out.max_count);
	
	//printf("the result is %d\n", ctx_out.ctx[0]->result);
	//assert_int_equal(0x281, ctx_out.ctx[0]->status);
	//assert_int_equal(0xffffffc3, ctx_out.ctx[0]->result);
	assert_in_range((ctx_out.ctx[0]->result), 0xf0000000, 0xffffffff);
	nvm_buf_set_free(bufs);
}

void test_erase_to_slc_mode_old(void **state)
{
	//get a good block
	//better to use a api to get a block
	(void)(state);
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;
	uint32_t nblks = (uint32_t) (geo->nchannels * geo->nluns);
	nblks = 1;
    struct nvm_addr blks[nblks];

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
    nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, nblks * size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

	uint32_t num = (uint32_t) (bufs->nbytes / geo->sector_nbytes);
    double time_write_slc[num];
    double time_write_normal[num];

	for(uint32_t i = 0; i< nblks; i++)
	{
		for(uint32_t plane = 0; plane < geo->nplanes; plane++)
		{
			for (uint32_t p = 0; p < geo->npages; p++ )
			{
				for(uint32_t j = 0; j < geo->nsectors; j++)
				{
					blks[i].g.pg = p;
					blks[i].g.pl = plane;
					blks[i].g.sec = j;
					nvm_addr_erase(g_dev, &blks[i], 1, NVM_FLAG_PMODE_DUAL|NVM_FLASH_SLC_ENABLE, NULL);
				}
			}
		}
	}
	

	assert_int_equal(bufs->nbytes,
                 ppa_wr_sync_with_time(g_dev, true, blks, nblks, (uint8_t *) bufs->write, bufs->nbytes, time_write_slc));
	count_latency("erase to slc mode and get write time", bufs->nbytes, time_write_slc, num, 100);
	assert_int_equal(bufs->nbytes,
    	             ppa_wr_sync(g_dev, false, blks, nblks, (uint8_t *) bufs->read, bufs->nbytes));

	assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);

	//second time
	assert_non_null(bufs);
    nvm_buf_set_fill(bufs);
	for(uint32_t i = 0; i< nblks; i++)
	{
		for(uint32_t plane = 0; plane < geo->nplanes; plane++)
		{
			for (uint32_t p = 0; p < geo->npages; p++ )
			{
				for(uint32_t j = 0; j < geo->nsectors; j++)
				{
					blks[i].g.pg = p;
					blks[i].g.pl = plane;
					blks[i].g.sec = j;
					nvm_addr_erase(g_dev, &blks[i], 1, NVM_FLAG_PMODE_DUAL, NULL);
				}
			}
		}
	}

	assert_int_equal(bufs->nbytes,
                 ppa_wr_sync_with_time(g_dev, true, blks, nblks, (uint8_t *) bufs->write, bufs->nbytes, time_write_normal));
	count_latency("erase to normal mode and get write time", bufs->nbytes, time_write_normal, num, 100);
	assert_int_equal(bufs->nbytes,
    	             ppa_wr_sync(g_dev, false, blks, nblks, (uint8_t *) bufs->read, bufs->nbytes));

	assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
	
	double total_time_slc;
	double total_time_normal;
	for(uint32_t i = 0; i< num; i++)
	{
		total_time_slc = total_time_slc + time_write_slc[i];
		total_time_normal = total_time_normal + time_write_normal[i];
	}
	
	assert_in_range(total_time_slc, 0, total_time_normal/2);

	nvm_buf_set_free(bufs);

}

void test_erase_to_slc_mode_new(void **state)
{
	//get a good block
	//better to use a api to get a block
	(void)(state);
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;
	uint32_t nblks = (uint32_t) (geo->nchannels * geo->nluns);
	nblks = 1;
    struct nvm_addr blks[nblks];

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
    nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, nblks * size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

	uint32_t num = (uint32_t) (bufs->nbytes / geo->sector_nbytes);
    double time_write_slc[num];
	double time_read_slc[num];
    double time_write_normal[num];

	for(uint32_t i = 0; i< nblks; i++)
	{
		for(uint32_t plane = 0; plane < geo->nplanes; plane++)
		{
			for (uint32_t p = 0; p < geo->npages; p++ )
			{
				for(uint32_t j = 0; j < geo->nsectors; j++)
				{
					blks[i].g.pg = p;
					blks[i].g.pl = plane;
					blks[i].g.sec = j;
					nvm_addr_erase(g_dev, &blks[i], 1, NVM_FLAG_PMODE_DUAL|NVM_FLASH_SLC_ENABLE, NULL);
				}
			}
		}
	}
	

	assert_int_equal(bufs->nbytes,
                 ppa_wr_sync_with_time_new(g_dev, true, blks, nblks, (uint8_t *) bufs->write, bufs->nbytes, time_write_slc));
	count_latency("erase to slc mode and get write time", bufs->nbytes, time_write_slc, num, 100);
	assert_int_equal(bufs->nbytes,
    	            ppa_wr_sync_with_time_new(g_dev, false, blks, nblks, (uint8_t *) bufs->read, bufs->nbytes, time_read_slc));

	assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);

	//second time
	assert_non_null(bufs);
    nvm_buf_set_fill(bufs);
	for(uint32_t i = 0; i< nblks; i++)
	{
		for(uint32_t plane = 0; plane < geo->nplanes; plane++)
		{
			for (uint32_t p = 0; p < geo->npages; p++ )
			{
				for(uint32_t j = 0; j < geo->nsectors; j++)
				{
					blks[i].g.pg = p;
					blks[i].g.pl = plane;
					blks[i].g.sec = j;
					nvm_addr_erase(g_dev, &blks[i], 1, NVM_FLAG_PMODE_DUAL, NULL);
				}
			}
		}
	}

	assert_int_equal(bufs->nbytes,
                 ppa_wr_sync_with_time(g_dev, true, blks, nblks, (uint8_t *) bufs->write, bufs->nbytes, time_write_normal));
	count_latency("erase to normal mode and get write time", bufs->nbytes, time_write_normal, num, 100);
	assert_int_equal(bufs->nbytes,
    	             ppa_wr_sync(g_dev, false, blks, nblks, (uint8_t *) bufs->read, bufs->nbytes));

	assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
	
	double total_time_slc;
	double total_time_normal;
	for(uint32_t i = 0; i< num; i++)
	{
		total_time_slc = total_time_slc + time_write_slc[i];
		total_time_normal = total_time_normal + time_write_normal[i];
	}
	
	assert_in_range(total_time_slc, 0, total_time_normal/2);

	nvm_buf_set_free(bufs);

}

void test_get_lun_blks_pe(void **state)
{
		//struct nvm_dev *dev = *state;
	(void)(state);
	//write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	//const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;

	//uint32_t nblks = (uint32_t)(geo->nchannels * geo->nluns);
	//struct nvm_addr blks[nblks];
	struct nvm_addr erase_blk;

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, 1, &erase_blk));
	nvm_addr_prn(&erase_blk, 1, g_dev);

	uint16_t *buf = aligned_alloc(4096, 4096);

	nvm_ext_get_per_lun_blks_pe(g_dev, erase_blk, buf, 4096);
	assert_int_equal(geo->nblocks, buf[0]);
	free(buf);
}

void test_error_injection(void **state)
{
	printf("%s", __func__);
	(void)(state);
	/*
	INJECT_ERASE=0,
    INJECT_Program,
    INJECT_NAND_OVER_WRITE,
    INJECT_Read_UNCORRECTABLE,
    INJECT_Read_EMPTY_PAGE,
	*/

	//sequential write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	uint32_t nblks = geo->nluns;

	//write 96k
	const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes * 3;
    struct nvm_addr blks[nblks];

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
    nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

	struct nvm_ret error_program = {0, 0};
	assert_int_equal(0, nvm_addr_erase(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));

	assert_int_equal(0, nvm_ext_inject_error(g_dev,  blks[0], INJECT_Program));

	assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, true, &blks[0], 1, (uint8_t *)bufs->write, bufs->nbytes));

	/*write data to the page*/
	/*get log page*/
	nvm_addr_read(g_dev, &blks[0], 1, (uint8_t *) bufs->read, NULL, NVM_FLAG_PMODE_DUAL, &error_program);
	assert_int_equal(0x290, error_program.status);

	struct nvm_ret error_empty_page = {0, 0};
	assert_int_equal(0, nvm_addr_erase(g_dev, &blks[1], 1, NVM_FLAG_PMODE_DUAL, NULL));

	assert_int_equal(0, nvm_ext_inject_error(g_dev,  blks[1], INJECT_Read_EMPTY_PAGE));

	assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, true, &blks[1], 1, (uint8_t *)bufs->write, bufs->nbytes));

	/*write data to the page*/
	/*get log page*/
	nvm_addr_read(g_dev, &blks[1], 1, (uint8_t *) bufs->read, NULL, NVM_FLAG_PMODE_DUAL, &error_empty_page);
	assert_int_equal(0x290, error_empty_page.status);
	nvm_buf_set_free(bufs);
}

void test_cross_luns_write(void **state)
{
	(void)(state);
	//sequential write
	//struct nvm_dev *dev = *state;
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	uint32_t nblks = geo->nluns;

	const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes * 3;
    //const size_t size = geo->npages * geo->page_nbytes;    // * one plane size total:geo->nplanes;
	//const size_t size = geo->page_nbytes;
    struct nvm_addr blks[nblks];

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
    nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
    assert_non_null(bufs);
	for(int i = 0; i< 100; i++)
	{

    	nvm_buf_set_fill(bufs);

		blks[0].g.ch  = 0 % geo->nchannels;
		blks[0].g.lun = rand() % geo->nluns;

		assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));

		assert_int_equal(bufs->nbytes,
    	             ppa_wr_sync(g_dev, true, &blks[0], 1, (uint8_t *) bufs->write, bufs->nbytes));
		//sleep(5);
		assert_int_equal(bufs->nbytes,
    	             ppa_wr_sync(g_dev, false, &blks[0], 1, (uint8_t *) bufs->read, bufs->nbytes));

		assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
	}
	nvm_buf_set_free(bufs);

}

void test_ebc_power_loss(void **state)
{
	//struct nvm_dev *dev = *state;
	(void)(state);
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	struct nvm_addr sblk;
	uint32_t nblks = (uint32_t) (geo->nchannels * geo->nluns);
	struct nvm_addr write_blks[nblks];
	ssize_t err = 0;
	uint32_t bit_err_count = 0;

	sblk.ppa = 0;
	sblk.g.blk = 83;

	//erase the block
	//int block_num = rand()%(geo->nblocks);
	int block_num = 83;
	err = nvm_addr_erase_sb(g_dev, &sblk, 1, NVM_FLAG_PMODE_DUAL, NULL);
	assert_int_equal(err, 0);

	//printf("erase the sb completed\n");

	//write to a superblcoks
	for(uint32_t i = 0; i< 1; i++)
	{
		//write to the blk's all the page
		//assert_int_equal(0, nvm_addr_erase_sb(g_dev, &write_blks[i], 1, NVM_FLAG_PMODE_DUAL, NULL));
		//for( uint32_t j = 0; j < geo->npages; j++)
		for( uint32_t j = 0; j < 1; j++)
		//for( uint32_t j = 0; j < 1; j++)
		{
			int rlt0 = nvm_ext_reset(g_dev);
			assert_int_equal(rlt0, 0);
			write_blks[i].ppa  = 0;
			write_blks[i].g.blk = block_num;
			write_blks[i].g.pg  = j;

			assert_int_equal(0, nvm_addr_erase_sb(g_dev, &write_blks[i], 1, NVM_FLAG_PMODE_DUAL, NULL));

			//const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    		//const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes* 3; //write 96k;

			const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;
			//struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
			struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, size, 0);
    		assert_non_null(bufs);
    		nvm_buf_set_fill(bufs);

			assert_int_equal(bufs->nbytes,
                     ppa_wr_sync(g_dev, true, &write_blks[i], 1, (uint8_t *) bufs->write, bufs->nbytes));
			//printf("[DBG]:write completed\n");
			assert_int_equal(bufs->nbytes,
                     ppa_wr_sync(g_dev, false, &write_blks[i], 1, (uint8_t *) bufs->read, bufs->nbytes));
			assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
			nvm_buf_set_free(bufs);
		}


		for(uint32_t pg = 0; pg < geo->npages; pg++ )
		{
			for(uint32_t sec= 0; sec < geo->nsectors; sec++)
			{
				for(uint32_t pl = 0; pl < geo->nplanes; pl++)
				{
					uint32_t count = 0;
					struct nvm_addr err_bit_cout_addr;
					err_bit_cout_addr.ppa = 0;
					err_bit_cout_addr.g.ch  = 0;
					err_bit_cout_addr.g.lun = 0;
					err_bit_cout_addr.g.blk = block_num;
					err_bit_cout_addr.g.pg  = pg;
					err_bit_cout_addr.g.pl	= pl;
					err_bit_cout_addr.g.sec  = sec;
					//get the ebc
					int rlt = nvm_ext_error_bit_count(g_dev,err_bit_cout_addr, &count);
					//sleep(1);
					//printf("[DBG]:the count is %d\n",count);
					assert_int_equal(rlt, 0);
					bit_err_count = bit_err_count + count;
				}
			}
		}
	}

	printf("[DBG]: the bit err count is %d\n", bit_err_count);

}

/* skip page write*/
void test_illegal_ppa_write(void **state)
{
	(void)(state);
	//struct nvm_dev *dev = *state;
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);

	const uint32_t unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    const uint32_t unit_size = unit_sectors * (uint32_t) geo->sector_nbytes* 3; //write 96k;

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
    assert_non_null(bufs);
    nvm_buf_set_fill(bufs);

	struct nvm_addr addrForWriteAddr;
	nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, 1, &addrForWriteAddr);

	/*erase the block*/
	assert_int_equal(0, nvm_addr_erase_sb(g_dev, &addrForWriteAddr, 1, NVM_FLAG_PMODE_DUAL, NULL));

	assert_int_equal(0, nvm_addr_write(g_dev, &addrForWriteAddr, 1, bufs->write, NULL, NVM_FLAG_PMODE_DUAL, NULL));

	//normal operation
	assert_int_equal(0, nvm_addr_read(g_dev, &addrForWriteAddr, 1,  bufs->read,  NULL, NVM_FLAG_PMODE_DUAL, NULL));
	assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);

	//abnormal operation (skip page)
	for(int i= 0; i< 100; i++)
	{
		nvm_buf_set_fill(bufs);
		addrForWriteAddr.g.pg = rand()%(geo->npages);
		assert_int_equal(0, nvm_addr_erase_sb(g_dev, &addrForWriteAddr, 1, NVM_FLAG_PMODE_DUAL, NULL));
		//printf("the addres ppa = %ld, blk = %d, page = %d\n", addrForWriteAddr.ppa,  addrForWriteAddr.g.blk, addrForWriteAddr.g.pg);

		assert_int_equal(0, nvm_addr_write(g_dev, &addrForWriteAddr, 1, bufs->write, NULL, NVM_FLAG_PMODE_DUAL, NULL));
		assert_int_equal(0, nvm_addr_read(g_dev, &addrForWriteAddr, 1,  bufs->read,  NULL, NVM_FLAG_PMODE_DUAL, NULL));
		assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
	}
	nvm_buf_set_free(bufs);

}


static int setup(void **state)
{
    //struct nvm_dev *dev = nvm_dev_open(g_dev_path);
    g_dev = nvm_dev_open(g_dev_path);
    assert_non_null(g_dev);
    if (!g_dev)
        return -1;

    *state = g_dev;
    return 0;
}

static int teardown(void **state)
{
    struct nvm_dev *dev = *state;
    nvm_dev_close(dev);
    return 0;
}

int unit_io_status(char *dev_path)
{
	strcpy(g_dev_path, dev_path);
	//generate radom seed
	srand((unsigned int)time(NULL));

	register_test("io_get_lun_blks_pe", test_get_lun_blks_pe, setup,teardown, dev_path);
	register_test("io_error_injection", test_error_injection, setup, teardown, dev_path);
	register_test("io_read_erase_page", test_erase_page, setup, teardown, dev_path);
	register_test("io_read_uecc", test_io_read_with_uecc, setup, teardown, dev_path);
	register_test("io_read_aysnc_uecc", test_io_aysnc_read_with_uecc, setup, teardown, dev_path);
	//register_test("io_erase_to_slc_mode", test_erase_to_slc_mode_old, setup, teardown, dev_path);  //temporarly comment out this case
	//register_test("io_erase_to_slc_mode_new", test_erase_to_slc_mode_new, setup, teardown, dev_path);
	register_test("io_cross_luns_write", test_cross_luns_write, setup, teardown, dev_path);
	//register_test("ebc_get", test_ebc_power_loss, setup, teardown, dev_path);
	//register_test("illegal_ppa_write", test_illegal_ppa_write, setup, teardown, dev_path);

	return 0;
}
