//  The MIT License (MIT)
//  Copyright by starblaze (c)
//  Author: suntong - Create at 2018-10-23-15:28:52
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software")
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
/*  code  */
#include <stdio.h>
#include <time.h>
#include "nvm_dev.h"
#include "nvm_aer.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "liblightnvm.h"
#include "../include/nvm_debug.h"
#include "test_list.h"


static char g_dev_path[1024];
struct nvm_dev *g_dev;

#define TEST_BBT_BLKS   (20)
#define BUFSIZE 128

static int parse_slot_no(char *buf_p) {
    char *cmd = "lspci | grep -i non | cut -d ' ' -f 1";

    char buf[BUFSIZE];
    FILE *fp;

    if ((fp = popen(cmd, "r")) == NULL) {
        LOG_INFO("Error opening pipe!\n");
        return -1;
    }

    while (fgets(buf, BUFSIZE, fp) != NULL) {
        // Do whatever you want here...
        LOG_INFO("OUTPUT: %s", buf);
		memcpy(buf_p, buf, strlen(buf)-1);
    }

    if(pclose(fp))  {
        LOG_INFO("Command not found or exited with error status\n");
        return -1;
    }
    return 0;
}

void system_print( int status)
{
    if (-1 == status)
    {
        printf("system error!");
    }
    else
    {
        printf("exit status value = [0x%x]\n", status);

        if (WIFEXITED(status))
        {
            if (0 == WEXITSTATUS(status))
            {
                printf("run shell script successfully.\n");
            }
            else
            {
                printf("run shell script fail, script exit code: %d\n", WEXITSTATUS(status));
            }
        }
        else
        {
            printf("exit status = [%d]\n", WEXITSTATUS(status));
        }
	}
}

void power_loss_proc()
{
	char slot_no[1024];
	char command[1024];
	//first close the device
	LOG_INFO("close the device\n");
	nvm_dev_close(g_dev);

	LOG_INFO("start to power down\n");
	system("rmmod nvme");
	sleep(2);
	memset(slot_no, 0x00, 1024);
	memset(command, 0x00, 1024);
	parse_slot_no(slot_no);
	snprintf(command, sizeof(command), "echo 1 > /sys/bus/pci/devices/0000:%s/remove", slot_no);
	system(command);
	//system_print(remove_status);
	sleep(2);
	system("python  pci_power_off.py");
	sleep(2);
	system("python  pci_power_on.py");
	sleep(5);
	system("echo 1 > /sys/bus/pci/rescan");
	sleep(2);
	system("modprobe nvme");
	sleep(2);
	system("nvme list");
	LOG_INFO("end power down\n");
	//open second time
	struct nvm_dev *dev2 = nvm_dev_open(g_dev_path);
    assert_non_null(dev2);
	g_dev = dev2;
}

//write to a block's page according to the lengh(auto fill)
static ssize_t ppa_wr_sync(struct nvm_dev *dev, bool do_write,
                           struct nvm_addr *blks, size_t nblks,
                           uint8_t *data, size_t len)
{
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
			//printf("the unit is %d\n", len/unit_size);
			//printf("the unitsector is %d\n", unit_sectors);
			//printf("the blks index is %d\n", unit % nblks);
			//printf("the addr.ppa = %x\n",  addr.ppa);
			//printf("the addr.g.pg = %x\n",  addr.g.pg);
			//printf("the addr.g.pl = %x\n",  addr.g.pl);
			//printf("the addr.g.sec = %x\n",  addr.g.sec);
			
			//printf("%s-%d [%3d] ", __func__, do_write, i); nvm_addr_pr_man(dev, addr);
            if (do_write == true) {
                nvm_addr_write(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, NULL);
            } else {
                nvm_addr_read(dev, &addr, 1, &buf_off[i * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, NULL);
            }
        }
    }

    return len;
}

void test_pm_power_loss(void **state)
{
    //fist write the pm
	//struct nvm_dev *dev = *state;
	(void)(state);

	char *bufForWrite = malloc(2*1024*1024*sizeof(char));
	char *bufForRead  = malloc(2*1024*1024*sizeof(char));
	assert_non_null(bufForWrite);
	assert_non_null(bufForRead);
	memset(bufForWrite, 0x00, 2*1024*1024);
	memset(bufForRead, 0x00, 2*1024*1024);
	/*generate random data*/
	for(uint64_t i= 0; i< 2*1024*1024; i++)
	{
		char randomData = rand()%128;
		//printf("the random data is %d\n",(int)randomData);
		bufForWrite[i] = randomData;
	}

	LOG_INFO("[INFO]: write the test data to cmb\n");
	nvm_write_pm(g_dev, bufForWrite, 0,  2*1024*1024, 0, NULL);


#if 1
	power_loss_proc();
#endif

	/**/
	LOG_INFO("[INFO]: read the test data from cmb\n");
	nvm_read_pm(g_dev, bufForRead, 0, 2*1024*1024, 0,  NULL);
	LOG_INFO("[INFO]: compare the read and write buf\n");
	for(uint64_t i= 0; i< 2*1024*1024;i++)
	{
		if(memcmp(&bufForWrite[i], &bufForRead[i],1)==0)
		{
			//do nothing
		}
		else
		{
			LOG_INFO("the buf[%ld] For Write is %d, otherwise the buf[%ld] for read is %d\n", i,*(&bufForWrite[i]),i,*(&bufForRead[i]));
			LOG_WARN("the buf[%ld] For Write is %d, otherwise the buf[%ld] for read is %d\n", i,*(&bufForWrite[i]),i,*(&bufForRead[i]));
			LOG_INFO("[DBG]: the buf[%ld] is not the same\n",i);
			LOG_WARN("[DBG]: the buf[%ld] is not the same\n",i);
			free(bufForWrite);
			free(bufForRead);
			assert_true(false);
			return;
		}
	}
	assert_true(true);
	LOG_INFO("[INFO]: succeed!!! read and write buf are the same\n");
	free(bufForWrite);
	free(bufForRead);
	//nvm_dev_close(g_dev);
	//second get the pm	
}

void test_pe_power_loss(void **state)
{
	//struct nvm_dev *dev = *state;
	(void)(state);
    uint16_t *buf = aligned_alloc(4096, 4096);
	//write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	//const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;

	const uint32_t unit_sectors = (uint32_t)(geo->nplanes * geo->nsectors);
	const uint32_t unit_size = unit_sectors * (uint32_t)geo->sector_nbytes* 3; //write 96k

	//uint32_t nblks = (uint32_t)(geo->nchannels * geo->nluns);
	//struct nvm_addr blks[nblks];
	struct nvm_addr erase_blk;

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, 1, &erase_blk));
	nvm_addr_prn(&erase_blk, 1, g_dev);

	//struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, nblks * size, 0);
	//int block_page = geo-> npages;
	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
	assert_non_null(bufs);
	nvm_buf_set_fill(bufs);

	//normal case
	for(int i = 0; i< 1; i++)
	{
		//reset the device
		int rlt0 = nvm_ext_reset(g_dev);
		assert_int_equal(rlt0, 0);

		int rlt1 = nvm_ext_get_pe(g_dev, buf, 4096);
		if (rlt1 != 0)
		{
			free(buf);
		}
		assert_int_equal(rlt1, 0);
		uint16_t before_erase_cnt = buf[erase_blk.g.blk];

		LOG_INFO("the cnt before erase is %d\n", before_erase_cnt);
		nvm_buf_set_fill(bufs);
		//assert_int_equal(0, nvm_addr_erase_sb(dev, blks, nblks, NVM_FLAG_PMODE_DUAL, NULL));
		assert_int_equal(0, nvm_addr_erase_sb(g_dev, &erase_blk, 1, NVM_FLAG_PMODE_DUAL, NULL));
		//sleep(5);
		assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, true, &erase_blk, 1, (uint8_t *)bufs->write, bufs->nbytes));

		
		assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, false, &erase_blk, 1, (uint8_t *)bufs->read, bufs->nbytes));

		assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
		

		uint16_t after_erase_cnt = 0;
		memset(buf, 0x00, 4096);
		int rlt2 = nvm_ext_get_pe(g_dev, buf, 4096);
		if (rlt2 != 0)
		{
			free(buf);
		}
		assert_int_equal(rlt2, 0);
		after_erase_cnt = buf[erase_blk.g.blk];
		//assert_in_range(before_erase_cnt, 1, 5000);
		assert_int_equal(after_erase_cnt, before_erase_cnt + 1);
		//assert_in_range(after_erase_cnt, 1, 5000);
		LOG_INFO("the cnt after erase is %d\n", after_erase_cnt);

	//power down
#if 1
		power_loss_proc();
#endif

		memset(buf, 0x00, 4096);
		int rlt3 = nvm_ext_get_pe(g_dev, buf, 4096);
		if (rlt3 != 0)
		{
			free(buf);
		}
		assert_int_equal(rlt3, 0);
		after_erase_cnt = buf[erase_blk.g.blk];
		assert_int_equal(after_erase_cnt, before_erase_cnt + 1);
	}

	//abnormal case

    nvm_buf_set_free(bufs);
    free(buf);    
}


void test_single_blk_pe_power_loss(void **state)
{
	//struct nvm_dev *dev = *state;
	(void)(state);
	//write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);
	//const size_t size = geo->npages * geo->page_nbytes * geo->nplanes;

	const uint32_t unit_sectors = (uint32_t)(geo->nplanes * geo->nsectors);
	const uint32_t unit_size = unit_sectors * (uint32_t)geo->sector_nbytes* 3; //write 96k

	//uint32_t nblks = (uint32_t)(geo->nchannels * geo->nluns);
	//struct nvm_addr blks[nblks];
	struct nvm_addr erase_blk;

	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, 1, &erase_blk));
	nvm_addr_prn(&erase_blk, 1, g_dev);

	//struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, nblks * size, 0);
	//int block_page = geo-> npages;
	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
	assert_non_null(bufs);
	nvm_buf_set_fill(bufs);

	//normal case
	for(int i = 0; i< 1; i++)
	{
		//reset the device
		int rlt0 = nvm_ext_reset(g_dev);
		assert_int_equal(rlt0, 0);

		uint32_t pe_cnt1 = 0;
		int ret1 = nvm_ext_get_blk_pe(g_dev, erase_blk, &pe_cnt1);
		assert_int_not_equal(ret1, -1);
		uint16_t before_erase_cnt = pe_cnt1;

		LOG_INFO("the cnt before erase is %d\n", before_erase_cnt);
		nvm_buf_set_fill(bufs);
		//assert_int_equal(0, nvm_addr_erase_sb(dev, blks, nblks, NVM_FLAG_PMODE_DUAL, NULL));

		assert_int_equal(0, nvm_addr_erase(g_dev, &erase_blk, 1, NVM_FLAG_PMODE_DUAL, NULL));
		//sleep(5);
		assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, true, &erase_blk, 1, (uint8_t *)bufs->write, bufs->nbytes));

		
		assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, false, &erase_blk, 1, (uint8_t *)bufs->read, bufs->nbytes));

		assert_memory_equal(bufs->write, bufs->read, bufs->nbytes);
		

		uint16_t after_erase_cnt = 0;
		uint32_t pe_cnt2 = 0;
		int ret2 = nvm_ext_get_blk_pe(g_dev, erase_blk, &pe_cnt2);
		after_erase_cnt  = pe_cnt2;
		assert_int_not_equal(ret2, -1);
		//assert_in_range(before_erase_cnt, 1, 5000);
		assert_int_equal(after_erase_cnt, before_erase_cnt + 1);
		//assert_in_range(after_erase_cnt, 1, 5000);
		LOG_INFO("the cnt after erase is %d\n", after_erase_cnt);

	//power down
#if 1
		power_loss_proc();
#endif
		uint32_t pe_cnt3 = 0;
		int ret3 = nvm_ext_get_blk_pe(g_dev, erase_blk, &pe_cnt3);
		assert_int_not_equal(ret3, -1);
		after_erase_cnt = pe_cnt3;
		assert_int_equal(after_erase_cnt, before_erase_cnt + 1);
	}

	//abnormal case

    nvm_buf_set_free(bufs);
}

static void verify_bbt(const struct nvm_bbt *bbt, const struct nvm_geo *geo)
{
    uint32_t nbad = 0, ngbad = 0, ndmrk = 0, nhmrk = 0;
    assert_int_equal(bbt->nblks, geo->nplanes * geo->nblocks);

    for (uint64_t i = 0; i < bbt->nblks; ++i) {
        switch (bbt->blks[i]) {
            case NVM_BBT_FREE:
                break;
            case NVM_BBT_BAD:
                ++nbad;
                break;
            case NVM_BBT_GBAD:
                ++ngbad;
                break;
            case NVM_BBT_DMRK:
                ++ndmrk;
                break;
            case NVM_BBT_HMRK:
                ++nhmrk;
                break;
            default:
                print_error("Invalid blk-state");
                break;
        }
    }

    assert_int_equal(bbt->nbad, nbad);
    assert_int_equal(bbt->ngbad, ngbad);
    assert_int_equal(bbt->ndmrk, ndmrk);
    assert_int_equal(bbt->nhmrk, nhmrk);
}

void test_bbt_power_loss(void **state)
{
	(void)(state);
	//struct nvm_dev *dev = *state;
    struct nvm_ret ret = {0, 0};
    const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);

    nvm_dev_set_bbts_cached(g_dev, true);

    /** 1. backup one bbt */
    struct nvm_addr addr = {.ppa=0};
    addr.g.ch = 0;
    addr.g.lun = 0;
    struct nvm_bbt *bbt_back = nvm_bbt_alloc_cp(nvm_bbt_get(g_dev, addr, &ret));
    assert_non_null(bbt_back);
    verify_bbt(bbt_back, geo);

    /** 2. alter the state of the bbt */
    struct nvm_bbt *bbt_exp = nvm_bbt_alloc_cp(nvm_bbt_get(g_dev, addr, &ret));

    int num = 0;
    for (uint64_t i = 0; i < bbt_back->nblks; i++) {
        if (bbt_exp->blks[i] == NVM_BBT_FREE) {
            bbt_exp->blks[i] = NVM_BBT_BAD;
            if (num++ > TEST_BBT_BLKS)
                break;
        }
    }
    assert_int_equal(0, nvm_bbt_set(g_dev, bbt_exp, &ret));

	//power down
#if 1
	power_loss_proc();
#endif

    /** 3. get the new state of the bbt */
    const struct nvm_bbt *bbt_alter = nvm_bbt_get(g_dev, addr, &ret);
    assert_non_null(bbt_alter);
    verify_bbt(bbt_alter, geo);


    /** 4. verify the state of these two bbts */
    size_t diff = nvm_buf_diff((char *) bbt_exp->blks, (char *) bbt_alter->blks, bbt_exp->nblks);

    /** 5. recover the state of ocssd's bbt */
    assert_int_equal(0, nvm_bbt_set(g_dev, bbt_back, &ret));

    assert_int_equal(0, diff);
    nvm_bbt_free(bbt_exp);
}


void test_down_to_nand_after_64k_write_with_powerdown(void **state)
{
	//struct nvm_dev *dev = *state;
	(void)(state);

	//write
	const struct nvm_geo *geo = nvm_dev_get_geo(g_dev);

	const uint32_t unit_sectors = (uint32_t)(geo->nplanes * geo->nsectors);
	const uint32_t unit_size = unit_sectors * (uint32_t)geo->sector_nbytes * 2;

	struct nvm_addr write_blk;
	assert_int_equal(0, nvm_bbt_gbbt_arbs(g_dev, NVM_BBT_FREE, 1, &write_blk));
	nvm_addr_prn(&write_blk, 1, g_dev);

	//one page 32k, write 64k
	struct nvm_buf_set *bufs = nvm_buf_set_alloc(g_dev, unit_size, 0);
	assert_non_null(bufs);

	//normal case
	for(int i = 0; i< 1; i++)
	{
		//reset the device
		int rlt0 = nvm_ext_reset(g_dev);
		assert_int_equal(rlt0, 0);

		nvm_buf_set_fill(bufs);
		//assert_int_equal(0, nvm_addr_erase_sb(dev, blks, nblks, NVM_FLAG_PMODE_DUAL, NULL));
		assert_int_equal(0, nvm_addr_erase_sb(g_dev, &write_blk, 1, NVM_FLAG_PMODE_DUAL, NULL));
		//sleep(5);
		assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, true, &write_blk, 1, (uint8_t *)bufs->write, bufs->nbytes));

		//power down
#if 1
		power_loss_proc();
#endif		

		assert_int_equal(bufs->nbytes,
						 ppa_wr_sync(g_dev, false, &write_blk, 1, (uint8_t *)bufs->read, bufs->nbytes));

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

int unit_power_loss(char *dev_path)
{
	strcpy(g_dev_path, dev_path);
	//generate radom seed
	srand((unsigned int)time(NULL));

#if 0
	const struct CMUnitTest tests[] = {
		 cmocka_unit_test(test_pm_power_loss),
		 cmocka_unit_test(test_pe_power_loss),
		 cmocka_unit_test(test_cross_luns_write),
		//cmocka_unit_test(test_ebc_power_loss),
		//cmocka_unit_test(test_illegal_ppa_write),
	};
	return cmocka_run_group_tests(tests, setup, teardown);
#endif


	register_test("pm_power_loss", test_pm_power_loss, setup, teardown, dev_path);
	register_test("single_blk_pe_power_loss", test_single_blk_pe_power_loss, setup, teardown, dev_path);
	//register_test("pe_power_loss", test_pe_power_loss, setup, teardown, dev_path);
	register_test("bbt_power_loss", test_bbt_power_loss, setup, teardown, dev_path);
	//register_test("abnormal_write_but_to_nand_after_powerdown", test_down_to_nand_after_64k_write_with_powerdown, setup, teardown, dev_path);

	return 0;
}
