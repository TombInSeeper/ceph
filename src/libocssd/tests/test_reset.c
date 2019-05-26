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
#include "libnvm_full.h"
#include "../include/nvm_debug.h"
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

#if 0
static int nvm_cmd_gbbt_arbs_at_same_lun(struct nvm_dev *dev, int bs, int naddrs, struct nvm_addr *addrs)
{
	const struct nvm_geo *geo = nvm_dev_get_geo(dev);
	size_t tluns = geo->nchannels * geo->nluns;
	int arb = rand();

	if ((naddrs < 0) || naddrs > (int)tluns) {
		return -1;
	}

	for (size_t idx = 0; idx < (size_t)naddrs; ++idx) {
		struct nvm_spec_bbt *bbt = NULL;
		struct nvm_addr addr = { .val = 0 };
		size_t cur = (idx + arb) % naddrs;
		size_t blk_idx;

		addr.g.ch = cur % geo->nchannels;

		//at same lun, define lun0
		addr.g.lun = 0;

//		bbt = nvm_cmd_gbbt(dev, addr, NULL);		// Grab BBT
//		if (!bbt)
//			return -1;

		for (blk_idx = 0; blk_idx < geo->nblocks; ++blk_idx) {
			size_t blk_cur = (blk_idx + arb) % geo->nblocks;
			int state = 0;

			// Accumulate plane state
			for (size_t i = blk_cur * geo->nplanes; i < blk_cur * geo->nplanes + geo->nplanes; ++i)
				state |= bbt->blk[i];

			// Check state
			if (bs != state)
				continue;

			addrs[idx].val = addr.val;
			addrs[idx].g.blk = blk_cur;
			break;
		}
		if (blk_idx == geo->nblocks)
			return -1;				// No block!
	}

	return 0;
}
#endif

void test_normal_reset(void **state)
{
	//struct nvm_dev *dev = *state;
	(void)(state);
	//write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);

	const uint32_t unit_sectors = (uint32_t)(geo->nplanes * geo->nsectors);
	const uint32_t unit_size = unit_sectors * (uint32_t)geo->sector_nbytes; //first write 32k

	uint32_t nblks = (uint32_t)(geo->nchannels * geo->nluns);
	struct nvm_addr blks[nblks];

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
	nvm_addr_prn(blks, nblks, g_dev);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
	assert_non_null(bufs);

	for(int i = 0; i< 4; i++)
	{
		nvm_buf_set_fill(bufs);
	
		assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));
		assert_int_equal(bufs->nbytes,
						ppa_wr_sync(g_dev, true, &blks[0], 1, (uint8_t *)bufs->write, bufs->nbytes));

		assert_int_equal(bufs->nbytes,
						ppa_wr_sync(g_dev, false, &blks[0], 1, (uint8_t *)bufs->read, bufs->nbytes));
		assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);

		//reset the controller
		int rlt0 = nvm_ext_reset(g_dev);
		assert_int_equal(rlt0, 0);
	}
	

	//judge the 32k whether down to the nand flash
	struct nvm_buf_set *bufs_read = nvm_buf_set_alloc(g_dev, unit_size, 0);
	assert_non_null(bufs_read);
	nvm_buf_set_fill(bufs_read);
	assert_int_equal(bufs_read->nbytes,
						 ppa_wr_sync(g_dev, false, &blks[0], 1, (uint8_t *)bufs_read->read, bufs_read->nbytes));
	
	assert_memory_equal(bufs->write, bufs_read->read, bufs_read->nbytes);

	//set free the buffer
	nvm_buf_set_free(bufs);
	nvm_buf_set_free(bufs_read);

	//normal case write 96k
	struct nvm_buf_set *bufs2 = nvm_buf_set_alloc(g_dev, unit_size*3, 0);
	assert_non_null(bufs2);
	nvm_buf_set_fill(bufs2);

	assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[1], 1, NVM_FLAG_PMODE_DUAL, NULL));


	assert_int_equal(bufs2->nbytes,
						 ppa_wr_sync(g_dev, true, &blks[1], 1, (uint8_t *)bufs2->write, bufs2->nbytes));

		
	assert_int_equal(bufs2->nbytes,
						 ppa_wr_sync(g_dev, false, &blks[1], 1, (uint8_t *)bufs2->read, bufs2->nbytes));

	assert_memory_equal(bufs2->write, bufs2->read, bufs2->nbytes);

	nvm_buf_set_free(bufs2);
}


void test_reset_with_4block(void **state)
{
	//struct nvm_dev *dev = *state;
	(void)(state);
	//write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);

	const uint32_t unit_sectors = (uint32_t)(geo->nplanes * geo->nsectors);
	const uint32_t unit_size = unit_sectors * (uint32_t)geo->sector_nbytes; //first write 32k

	uint32_t nblks = (uint32_t)(geo->nchannels * geo->nluns);
	//nblks = 5;
	/*write 32k * 4 block*/
	struct nvm_addr blks[nblks];

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
	assert_non_null(bufs);

	/*have the risk for choose the same block*/
	for(uint32_t i=0; i< 4; i++)
	{
		assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
		nvm_buf_set_fill(bufs);
	
		/*all the nblks */
		//assert_int_equal(0, nvm_cmd_gbbt_arbs_at_same_lun(g_dev, NVM_BBT_FREE, 1, &blks[i]));
		nvm_addr_prn(&blks[0], 1, g_dev);
		assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));


		assert_int_equal(bufs->nbytes,
							 ppa_wr_sync(g_dev, true, &blks[0], 1, (uint8_t *)bufs->write, bufs->nbytes));

		assert_int_equal(bufs->nbytes,
							 ppa_wr_sync(g_dev, false, &blks[0], 1, (uint8_t *)bufs->read, bufs->nbytes));

		assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
	}	

	//above have write 32k * 4 at different block, so continue to write a block my hit a fw assert
	//and if reset the controller, and my not hit a assert
	//test here
	int rlt0 = nvm_ext_reset(g_dev);
	assert_int_equal(rlt0, 0);


	/*all the nblks */
//#if 0
	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, nblks, blks));
	struct nvm_buf_set *bufs_normal = nvm_buf_set_alloc(g_dev, unit_size * 3, 0);
	nvm_buf_set_fill(bufs_normal);
	assert_non_null(bufs_normal);

	assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[4], 1, NVM_FLAG_PMODE_DUAL, NULL));

	assert_int_equal(bufs_normal->nbytes,
			ppa_wr_sync(g_dev, true, &blks[4], 1, (uint8_t *)bufs_normal->write, bufs_normal->nbytes));

	assert_int_equal(bufs_normal->nbytes,
			ppa_wr_sync(g_dev, false, &blks[4], 1, (uint8_t *)bufs_normal->read, bufs_normal->nbytes));

	assert_memory_equal(bufs_normal->write, bufs_normal->read, bufs_normal->nbytes);
//#endif
	
	nvm_buf_set_free(bufs_normal);
	nvm_buf_set_free(bufs);
}

void test_down_to_nand_after_reset(void **state)
{
	//struct nvm_dev *dev = *state;
	(void)(state);
	//write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);

	const uint32_t unit_sectors = (uint32_t)(geo->nplanes * geo->nsectors);
	const uint32_t unit_size = unit_sectors * (uint32_t)geo->sector_nbytes; //first write 32k

	uint32_t nblks = (uint32_t)(geo->nchannels * geo->nluns);
	nblks = 5;
	/*write 32k * 4 block*/
	struct nvm_addr blks[nblks];

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
	assert_non_null(bufs);
	nvm_buf_set_fill(bufs);

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, 1, blks));
	nvm_addr_prn(blks, 1, g_dev);
	assert_int_equal(0, nvm_addr_erase_sb(g_dev, &blks[0], 1, NVM_FLAG_PMODE_DUAL, NULL));
	assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, true, &blks[0], 1, (uint8_t *)bufs->write, bufs->nbytes));

	int rlt0 = nvm_ext_reset(g_dev);
	assert_int_equal(rlt0, 0);

	assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, false, &blks[0], 1, (uint8_t *)bufs->read, bufs->nbytes));
	assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
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


int unit_io_reset(char *dev_path)
{
	strcpy(g_dev_path, dev_path);
	//generate radom seed
	srand((unsigned int)time(NULL));
	//write same blcok with 
	register_test("io_normal_reset", test_normal_reset, setup, teardown, dev_path);
	register_test("io_reset_with_4block", test_reset_with_4block, setup, teardown, dev_path);
	register_test("io_down_to_nand_after_reset", test_down_to_nand_after_reset, setup, teardown, dev_path);

	return 0;
}
