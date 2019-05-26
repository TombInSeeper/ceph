/*************************************************************
 *
 * Copyright (C) 2017-2018 by Beijing Starblaze Technology Co., Ltd.
 *
 * NOTICE:  All information contained herein is, and remains the
 * property of Beijing Starblaze Technology Co., Ltd and its
 * suppliers, if any.  The intellectual and technical concepts
 * contained herein are proprietary to Beijing Starblaze
 * Technology Co., Ltd and are protected by patent or trade
 * secret or copyright law.
 *
 * Dissemination of this information or reproduction of this
 * material is strictly forbidden unless prior written permission
 * is obtained from Beijing Starblaze Technology Co., Ltd. Source
 * code from this file can only be used on Starblaze ASIC products.
 *
 * Distribution of source code or binaries derived from this file
 * is not permitted except as specifically allowed By Non_Disclosure
 * Agreement between your company and Beijing Starblaze Technology
 * Co., Ltd. All copies of this source code modified or unmodified
 * must retain this entire copyright notice and comment as is.
 *
 * You should have received a copy of the Non-Disclosure Agreement
 * along with this program; if not, Please contact your manager
 * immediately.
 *
 *************************************************************/

// Initialize the disk for fio to use

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <liblightnvm.h>
#include <nvm_debug.h>
#include <nvm_aer.h>

#include <omp.h>
#include <nvm_cli.h>

static void blk_bbt_mark(struct nvm_dev *dev, struct nvm_addr addr, enum nvm_bbt_state state)
{
    addr.g.pl = 0;
    nvm_bbt_mark(dev, &addr, 1, state, NULL);
    addr.g.pl = 1;
    nvm_bbt_mark(dev, &addr, 1, state, NULL);
}

static void blk_erase_line(struct nvm_dev *dev, uint32_t line)
{
    struct nvm_addr addr = {.ppa=0};
    struct nvm_ret ret;
    addr.g.ch = 0;
    addr.g.lun = 0;
    addr.g.blk = line;
    addr.g.pl = 0;

    nvm_addr_erase_sb(dev, &addr, 1, 0, &ret);
    if (ret.status != 0 || ret.result != 0) {
        struct nvm_log_page log;
        nvm_get_mef_log(dev, &log);
        for (uint32_t i = 0; i < log.nppas; ++i) {
            struct nvm_addr bad = nvm_addr_dev2gen(dev, log.ppas[i]);
            LOG_DEBUG("mark bbt ch=%d lun=%d blk=%d", bad.g.ch, bad.g.lun, bad.g.blk);
            blk_bbt_mark(dev, bad, NVM_BBT_BAD);
        }
    }
}

static void blk_erase(struct nvm_dev *dev, uint32_t channel, uint32_t lun)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);

    for (uint32_t blk = 10; blk < geo->nblocks; blk++) {
        LOG_DEBUG("[%2d] ch=%d lun=%d blk=%d", omp_get_thread_num(), channel, lun, blk);

        struct nvm_addr addr = {.ppa=0};
        struct nvm_ret ret;
        addr.g.ch = channel;
        addr.g.lun = lun;
        addr.g.blk = blk;
        addr.g.pl = 0;

        const struct nvm_bbt *bbt = nvm_bbt_get(dev, addr, NULL);
        if (bbt->blks[blk*2] != NVM_BBT_FREE || bbt->blks[blk*2+1] != NVM_BBT_FREE) {
            LOG_DEBUG("ignore ch=%d lun=%d blk=%d bbt=%d-%d",
                      channel, lun, blk, bbt->blks[blk*2], bbt->blks[blk*2+1]);
            continue;
        }

        nvm_addr_erase(dev, &addr, 1, 0, &ret);
        if (ret.status != 0 || ret.result != 0) {
            LOG_DEBUG("mark bbt ch=%d lun=%d blk=%d", channel, lun, blk);
            blk_bbt_mark(dev, addr, NVM_BBT_BAD);
        }
    }
}

static void *thread_aer_program(void *arg)
{
    struct nvm_dev *dev = arg;
    struct nvm_log_page log;

    while (1) {
        if(0 != nvm_get_async_error_event(dev, &log))
            break;
        show_baidu_log(&log);
        nvm_bbt_mark(dev, (struct nvm_addr*)log.ppas, log.nppas, NVM_BBT_BAD, NULL);
        struct nvm_addr addr;
        addr.ppa = log.ppas[0];
        LOG_DEBUG("mark bbt ch=%d lun=%d blk=%d", addr.g.ch, addr.g.lun, addr.g.blk);
    }

    LOG_INFO("exit");
    pthread_exit(NULL);
}

static void blk_write_sync(struct nvm_dev *dev, uint32_t channel, uint32_t lun)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const uint32_t write_unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    struct nvm_addr addrs[write_unit_sectors];
    uint8_t *data = aligned_alloc(geo->sector_nbytes, write_unit_sectors*geo->sector_nbytes);

    struct nvm_addr bbt_addr = {.ppa=0};
    bbt_addr.g.ch = channel;
    bbt_addr.g.lun = lun;

    for (uint32_t blk = 10; blk < geo->nblocks; blk++) {
        LOG_DEBUG("[%2d] ch=%d lun=%d blk=%d", omp_get_thread_num(), channel, lun, blk);

        bbt_addr.g.blk = blk;
        const struct nvm_bbt *bbt = nvm_bbt_get(dev, bbt_addr, NULL);
        if (bbt->blks[blk*2] != NVM_BBT_FREE || bbt->blks[blk*2+1] != NVM_BBT_FREE) {
            LOG_DEBUG("ignore ch=%d lun=%d blk=%d bbt=%d-%d",
                      channel, lun, blk, bbt->blks[blk*2], bbt->blks[blk*2+1]);
            continue;
        }

        for (uint32_t page = 0; page < geo->npages; ++page) {
            int idx = 0;

            //FIXME: Why will be assert
//            if(page % dev->flash_type == 0) {
//                /**
//                 * page 对齐 one short
//                 */
//                bbt_addr.g.blk = blk;
//                const struct nvm_bbt *bbt = nvm_bbt_get(dev, bbt_addr, NULL);
//                if (bbt->blks[blk*2] != NVM_BBT_FREE || bbt->blks[blk*2+1] != NVM_BBT_FREE) {
//                    LOG_DEBUG("ignore ch=%d lun=%d blk=%d page=%d bbt=%d-%d",
//                              channel, lun, blk, page, bbt->blks[blk*2], bbt->blks[blk*2+1]);
//                    break;
//                }
//            }

            for (uint32_t pl = 0; pl < geo->nplanes; ++pl) {
                for (uint32_t sector = 0; sector < geo->nsectors; ++sector) {
                    addrs[idx].g.ch = channel;
                    addrs[idx].g.lun = lun;
                    addrs[idx].g.blk = blk;
                    addrs[idx].g.pg = page;
                    addrs[idx].g.pl = pl;
                    addrs[idx].g.sec = sector;
                    idx++;
                }
            }

            struct nvm_ret ret;
            memset(data, (uint8_t )page, write_unit_sectors*geo->sector_nbytes);
            nvm_addr_write(dev, addrs, write_unit_sectors, data, NULL, 0, &ret);
            if (ret.status != 0 || ret.result != 0) {
                LOG_ERROR("ret status=0x%lx result=0x%x", ret.status, ret.result);
            }
            pthread_yield();
        }
    }
    free(data);
}

static void blk_read_sync(struct nvm_dev *dev, uint32_t channel, uint32_t lun)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    const uint32_t write_unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    struct nvm_addr addrs[write_unit_sectors];
    uint8_t *data = aligned_alloc(geo->sector_nbytes, write_unit_sectors*geo->sector_nbytes);

    struct nvm_addr bbt_addr = {.ppa=0};
    bbt_addr.g.ch = channel;
    bbt_addr.g.lun = lun;

    for (uint32_t blk = 10; blk < geo->nblocks; blk++) {
        LOG_DEBUG("[%2d] ch=%d lun=%d blk=%d", omp_get_thread_num(), channel, lun, blk);

        bbt_addr.g.blk = blk;
        const struct nvm_bbt *bbt = nvm_bbt_get(dev, bbt_addr, NULL);
        if (bbt->blks[blk*2] != NVM_BBT_FREE || bbt->blks[blk*2+1] != NVM_BBT_FREE) {
            LOG_DEBUG("ignore ch=%d lun=%d blk=%d bbt=%d-%d",
                      channel, lun, blk, bbt->blks[blk*2], bbt->blks[blk*2+1]);
            continue;
        }

        for (uint32_t page = 0; page < geo->npages; ++page) {
            int idx = 0;
            for (uint32_t pl = 0; pl < geo->nplanes; ++pl) {
                for (uint32_t sector = 0; sector < geo->nsectors; ++sector) {
                    addrs[idx].g.ch = channel;
                    addrs[idx].g.lun = lun;
                    addrs[idx].g.blk = blk;
                    addrs[idx].g.pg = page;
                    addrs[idx].g.pl = pl;
                    addrs[idx].g.sec = sector;
                    idx++;
                }
            }

            struct nvm_ret ret;
            nvm_addr_read(dev, addrs, write_unit_sectors, data, NULL, 0, &ret);
            if (ret.status != 0 || ret.result != 0) {
                LOG_ERROR("ret status=0x%lx result=0x%x", ret.status, ret.result);
                LOG_DEBUG("mark bbt ch=%d lun=%d blk=%d", channel, lun, blk);
                blk_bbt_mark(dev, addrs[0], NVM_BBT_BAD);
                break;
            } else {
                bool result = true;
                for (uint32_t i = 0; i < sizeof(data); ++i) {
                    if(data[i] != (uint8_t)page) {
                        result = false;
                        break;
                    }
                }
                if(result == false) {
                    LOG_ERROR("verify error ch=%d lun=%d blk=%d", channel, lun, blk);
                    LOG_DEBUG("mark bbt ch=%d lun=%d blk=%d", channel, lun, blk);
                    blk_bbt_mark(dev, addrs[0], NVM_BBT_BAD);
                    break;
                }
            }
        }
    }
    free(data);
}
static void reset_bbt(struct nvm_dev *dev)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);

    LOG_DEBUG("Reset BBT to free...");
    for (uint32_t i = 0; i < geo->nchannels; ++i) {
        for (uint32_t j = 0; j < geo->nluns; ++j) {
            struct nvm_addr addr = {.ppa=0};
            addr.g.ch = i;
            addr.g.lun = j;
            struct nvm_bbt *bbt = nvm_bbt_alloc_cp(nvm_bbt_get(dev, addr, NULL));
            memset(bbt->blks, NVM_BBT_FREE, bbt->nblks);
            nvm_bbt_set(dev, bbt, NULL);
        }
    }
}

static void scan_bad_blocks(struct nvm_dev* dev)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);

    LOG_DEBUG("Erase full disk...");
#if 0
#pragma omp parallel for num_threads(geo->nchannels * geo->nluns)
    for (uint32_t unit = 0; unit < geo->nchannels * geo->nluns; ++unit) {
        uint32_t channel = unit/(uint32_t )geo->nluns;
        uint32_t lun = unit%(uint32_t )geo->nluns;
        LOG_DEBUG("[%2d] blk_erase ch=%d lun=%d", omp_get_thread_num(), channel, lun);
        blk_erase(dev, channel, lun);
    }
#else
    for (uint32_t line = 0; line < geo->nblocks; ++line) {
        LOG_DEBUG("blk line=%d", line);
        blk_erase_line(dev, line);
    }
#endif
    bbt_save_csv("bbt_erase.csv", dev);
    bbt_save_man("bbt_erase.bbt", dev);
    LOG_DEBUG("Erase done...");

    LOG_DEBUG("Program full disk...");
    pthread_t thread_aer;
    pthread_create(&thread_aer, NULL, thread_aer_program, dev);

#pragma omp parallel for num_threads(geo->nchannels * geo->nluns)
    for (uint32_t unit = 0; unit < geo->nchannels * geo->nluns; ++unit) {
        uint32_t channel = unit/(uint32_t )geo->nluns;
        uint32_t lun = unit%(uint32_t )geo->nluns;
        LOG_DEBUG("[%d] blk_write ch=%d lun=%d", omp_get_thread_num(), channel, lun);
        blk_write_sync(dev, channel, lun);
    }
    bbt_save_csv("bbt_write.csv", dev);
    bbt_save_man("bbt_write.bbt", dev);
    LOG_DEBUG("Program done...");

    for (int k = 0; k < 20; ++k) {
        LOG_DEBUG("wait program done %d...", k);
        sleep(1);
    }

    LOG_DEBUG("Read full disk...");
#pragma omp parallel for num_threads(geo->nchannels * geo->nluns)
    for (uint32_t unit = 0; unit < geo->nchannels * geo->nluns; ++unit) {
        uint32_t channel = unit/(uint32_t )geo->nluns;
        uint32_t lun = unit%(uint32_t )geo->nluns;
        LOG_DEBUG("[%d] blk_read ch=%d lun=%d", omp_get_thread_num(), channel, lun);
        blk_read_sync(dev, channel, lun);
    }
    bbt_save_csv("bbt_read.csv", dev);
    bbt_save_man("bbt_read.bbt", dev);
    LOG_DEBUG("Read done...");
}

int main (int argc, char **argv)
{
    struct nvm_dev *dev;

    if (argc < 2) {
        printf("Usage: %s <device_path>\n", argv[0]);
        printf("Usage: %s <device_path> reset\n", argv[0]);
        return -1;
    }
    dev = nvm_dev_open(argv[1]);
    if (dev == NULL) {
        printf("Error: cant' open file %s\n", argv[1]);
        return -1;
    }

    if (nvm_ext_reset(dev)) {
        printf("Error: fail to reset device\n");
        return -1;
    }

    if(argc == 3) {
        /**
         * Non't use original BBT
         */
        if(strcmp(argv[2], "reset") == 0) {
            reset_bbt(dev);
        }
    }

    scan_bad_blocks(dev);

    nvm_dev_close(dev);

    return 0;
}
