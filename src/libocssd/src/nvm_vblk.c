/*
 * vblock - Virtual block functions
 *
 * Copyright (C) 2015-2017 Javier Gonzáles <javier@cnexlabs.com>
 * Copyright (C) 2015-2017 Matias Bjørling <matias@cnexlabs.com>
 * Copyright (C) 2015-2017 Simon A. F. Lund <slund@cnexlabs.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "libnvm_full.h"
#include "nvm_dev.h"
#include "nvm_vblk.h"
#include "nvm_debug.h"
#include <stdbool.h>
#include "nvm_async.h"
#include "nvm_cmd.h"

static inline int NVM_MIN(int x, int y)
{
    return x < y ? x : y;
}

static inline int NVM_MAX(int x, int y)
{
    return x > y ? x : y;
}

struct nvm_vblk *nvm_vblk_alloc(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs)
{
    struct nvm_vblk *vblk;
    const struct nvm_geo *geo;

    if (naddrs > 128) {
        errno = EINVAL;
        return NULL;
    }

    geo = nvm_dev_get_geo(dev);
    if (!geo) {
        errno = EINVAL;
        return NULL;
    }

    vblk = malloc(sizeof(*vblk));
    if (!vblk) {
        errno = ENOMEM;
        return NULL;
    }

    for (int i = 0; i < naddrs; ++i) {
        if (nvm_addr_check(addrs[i], dev)) {
            LOG_DEBUG("FAILED: nvm_addr_check");
            errno = EINVAL;
            free(vblk);
            return NULL;
        }

        vblk->blks[i].ppa = addrs[i].ppa;
        nvm_bbt_get(dev, addrs[i], NULL);
    }

    vblk->nblks = naddrs;
    vblk->dev = dev;
    vblk->pos_write = 0;
    vblk->pos_read = 0;

    switch (nvm_dev_get_verid(dev)) {
        case NVM_SPEC_VERID_12:
            vblk->nbytes = vblk->nblks * geo->nplanes * geo->npages * geo->nsectors * geo->sector_nbytes;
            break;

        case NVM_SPEC_VERID_20:
            vblk->nbytes = vblk->nblks * geo->l.nsectr * geo->l.nbytes;
            break;

        default:
            LOG_ERROR("FAILED: unsupported verid");
            errno = ENOSYS;
            free(vblk);
            return NULL;
    }
//	printf("%s nblks(%d) * nplanes(%ld) * npages(%ld) * nsectors(%ld) * sector_size(%ld)\n", __func__, vblk->nblks, geo->nplanes, geo->npages, geo->nsectors, geo->sector_nbytes);
//	printf("%s size: %ldBytes, %ldKB, %ldMB\n", __func__, vblk->nbytes, vblk->nbytes/1024, vblk->nbytes/1024/1024);
    vblk->aio = vblk->dev->aio;//nvm_async_init(vblk->dev->fd, 0);
    if (vblk->aio == NULL) {
        errno = ENOMEM;
        free(vblk);
        return NULL;
    }

    return vblk;
}

struct nvm_vblk *nvm_vblk_alloc_line(struct nvm_dev *dev, int ch_bgn,
                                     int ch_end, int lun_bgn, int lun_end,
                                     int blk)
{
    const int verid = nvm_dev_get_verid(dev);
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    struct nvm_vblk *vblk;

    vblk = nvm_vblk_alloc(dev, NULL, 0);
    if (!vblk)
        return NULL;    // Propagate errno

    switch (verid) {
        case NVM_SPEC_VERID_12:
            for (int lun = lun_bgn; lun <= lun_end; ++lun) {
                for (int ch = ch_bgn; ch <= ch_end; ++ch) {
                    vblk->blks[vblk->nblks].ppa = 0;
                    vblk->blks[vblk->nblks].g.ch = ch;
                    vblk->blks[vblk->nblks].g.lun = lun;
                    vblk->blks[vblk->nblks].g.blk = blk;
                    nvm_bbt_alloc_cp(nvm_bbt_get(dev, vblk->blks[vblk->nblks], NULL));
                    ++(vblk->nblks);
                }
            }

            vblk->nbytes = vblk->nblks * geo->nplanes * geo->npages * geo->nsectors * geo->sector_nbytes;
            break;

        case NVM_SPEC_VERID_20:
            for (int punit = lun_bgn; punit <= lun_end; ++punit) {
                for (int pugrp = ch_bgn; pugrp <= ch_end; ++pugrp) {
                    vblk->blks[vblk->nblks].ppa = 0;
                    vblk->blks[vblk->nblks].l.pugrp = pugrp;
                    vblk->blks[vblk->nblks].l.punit = punit;
                    vblk->blks[vblk->nblks].l.chunk = blk;
                    ++(vblk->nblks);
                }
            }

            vblk->nbytes = vblk->nblks * geo->l.nsectr * geo->l.nbytes;
            break;

        default:
            LOG_ERROR("FAILED: unsupported verid: %d", verid);
            nvm_buf_free(vblk);
            errno = ENOSYS;
            return NULL;
    }

    for (int i = 0; i < vblk->nblks; ++i) {
        if (nvm_addr_check(vblk->blks[i], dev)) {
            LOG_DEBUG("FAILED: nvm_addr_check");
            free(vblk);
            errno = EINVAL;
            return NULL;
        }
    }

    vblk->min_write_sectors = geo->nsectors * geo->nplanes;
    return vblk;
}

void nvm_vblk_free(struct nvm_vblk *vblk)
{
    free(vblk);
}

static inline int cmd_nblks(int nblks, int cmd_nblks_max)
{
    int count = cmd_nblks_max;

    while (nblks % count && count > 1) --count;

    return count;
}

static inline ssize_t vblk_erase_s12(struct nvm_vblk *vblk)
{
    size_t nerr = 0;
    const struct nvm_geo *geo = nvm_dev_get_geo(vblk->dev);

    const int BLK_NADDRS = geo->nplanes;
    const int CMD_NBLKS = 1;//cmd_nblks(vblk->nblks, vblk->dev->erase_naddrs_max / BLK_NADDRS);

    printf("%s vblk->nblks=%d  CMD_NBLKS=%d\n", __func__, vblk->nblks, CMD_NBLKS);
    for (int off = 0; off < vblk->nblks; off += CMD_NBLKS) {
        ssize_t err;
        struct nvm_ret ret = {0, 0};

        const int nblks = NVM_MIN(CMD_NBLKS, vblk->nblks - off);
        const int naddrs = nblks * BLK_NADDRS;

        struct nvm_addr addrs[naddrs];

        for (int i = 0; i < naddrs; ++i) {
            const int idx = off + (i / BLK_NADDRS);

            addrs[i].ppa = vblk->blks[idx].ppa;
            addrs[i].g.pl = i % geo->nplanes;
        }

//		printf("%s [%2d] ", __func__, off);
//		nvm_addr_pr_man(vblk->dev, addrs[0]);
        err = nvm_cmd_erase(vblk->dev, addrs, naddrs, vblk->dev->pmode, &ret);
        if (err)
            ++nerr;
    }

    if (nerr) {
        errno = EIO;
        return -1;
    }

    vblk->pos_write = 0;
    vblk->pos_read = 0;

    return vblk->nbytes;
}

static inline ssize_t vblk_erase_s20(struct nvm_vblk *vblk)
{
    size_t nerr = 0;

    const int CMD_NBLKS = cmd_nblks(vblk->nblks, vblk->dev->erase_naddrs_max);

    for (int off = 0; off < vblk->nblks; off += CMD_NBLKS) {
        const int naddrs = NVM_MIN(CMD_NBLKS, vblk->nblks - off);
        struct nvm_ret ret = {0, 0};

        struct nvm_addr addrs[naddrs];

        for (int i = 0; i < naddrs; ++i)
            addrs[i].ppa = vblk->blks[off + i].ppa;

        if (nvm_addr_erase(vblk->dev, addrs, naddrs, 0, &ret))
            ++nerr;
    }

    if (nerr) {
        errno = EIO;
        return -1;
    }

    vblk->pos_write = 0;
    vblk->pos_read = 0;

    return vblk->nbytes;
}

static inline ssize_t vblk_erase_line(struct nvm_vblk *vblk)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(vblk->dev);
    /**
     * check bad block
     */
    for (int off = 0; off < vblk->nblks; off++) {
        struct nvm_addr bbt_addr = {.ppa=0};
        bbt_addr.g.ch = vblk->blks[off].g.ch;
        bbt_addr.g.lun = vblk->blks[off].g.lun;

        const struct nvm_bbt *bbt = nvm_bbt_get(vblk->dev, bbt_addr, NULL);

        uint64_t index = vblk->blks[off].g.blk * geo->nplanes;
        if (NVM_BBT_FREE != bbt->blks[index] || NVM_BBT_FREE != bbt->blks[index + 1]) {
            LOG_ERROR("off=%d blk_index=%ld-%ld state=%d & %d\n", off, index, index + 1, bbt->blks[index],
                      bbt->blks[index + 1]);
            return -1;
        }
        //printf("%s [%2d] ", __func__, off);
        //nvm_addr_pr_man(vblk->dev, addrs[0]);
    }

    ssize_t nerr = nvm_addr_erase_sb(vblk->dev, &vblk->blks[0], 1, vblk->dev->pmode, NULL);
    if (nerr) {
        errno = EIO;
        return -1;
    }

    vblk->pos_write = 0;
    vblk->pos_read = 0;

    return 0;
}

ssize_t nvm_vblk_erase(struct nvm_vblk *vblk)
{
    const int verid = nvm_dev_get_verid(nvm_vblk_get_dev(vblk));

    switch (verid) {
        case NVM_SPEC_VERID_12:
            return vblk_erase_line(vblk);

        case NVM_SPEC_VERID_20:
            return vblk_erase_s20(vblk);

        default:
            LOG_ERROR("FAILED: unsupported verid: %d", verid);
            errno = ENOSYS;
            return -1;
    }
}

static inline int _cmd_nspages(int nblks, int cmd_nspages_max)
{
    int cmd_nspages = cmd_nspages_max;

    while (nblks % cmd_nspages && cmd_nspages > 1) --cmd_nspages;

    return cmd_nspages;
}

static inline ssize_t
vblk_wr_sync(struct nvm_vblk *vblk, const void *buf, size_t count, size_t offset, bool flag_write)
{
    size_t nerr = 0;
    uint8_t *data = (uint8_t *) buf;
    const uint16_t PMODE = (uint16_t) nvm_dev_get_pmode(vblk->dev);
    const struct nvm_geo *geo = nvm_dev_get_geo(vblk->dev);
    /** 最小写单元=8sectors */
//	const uint32_t write_unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors * vblk->dev->flash_type);
    const uint32_t write_unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    /** 数据对齐=8sectors*4096bytes=32KB */
    const uint32_t ALIGN = write_unit_sectors * (uint32_t) geo->sector_nbytes;

    const uint32_t bgn = (uint32_t) offset / ALIGN;
    const uint32_t end = bgn + ((uint32_t) count / ALIGN);
    uint8_t *padding_buf = NULL;

    if (offset + count > vblk->nbytes) {        // Check bounds
        printf("%s offset=%ld count=%ld size error %ld\n", __func__, offset, count, offset + count);
        errno = EINVAL;
        return -1;
    }
    if ((count % ALIGN) || (offset % ALIGN)) {    // Check align
        printf("%s ALIGN(%d) error count=%ld offset=%ld\n", __func__, ALIGN, offset, count);
        errno = EINVAL;
        return -1;
    }

    if (!buf) {    // Allocate and use a padding buffer
        const size_t nbytes = write_unit_sectors * geo->sector_nbytes;

        padding_buf = nvm_buf_alloc(geo, nbytes);
        if (!padding_buf) {
            LOG_ERROR("FAILED: nvm_buf_alloc(padding)");
            errno = ENOMEM;
            return -1;
        }
        nvm_buf_fill((char*)padding_buf, nbytes);
    }

#ifdef _OPENMP
#pragma omp parallel for num_threads(vblk->nblks) schedule(static, 1) reduction(+:nerr) ordered
#endif
    for (uint32_t off = bgn; off < end; off += 1) {
        struct nvm_ret ret = {0, 0};
        struct nvm_addr addrs[write_unit_sectors];
        uint8_t *buf_off;
        const int idx = off % vblk->nblks;

        if (padding_buf)
            buf_off = padding_buf;
        else
            buf_off = &data[(off - bgn) * ALIGN];

        for (uint32_t i = 0; i < write_unit_sectors; ++i) {
            addrs[i].ppa = vblk->blks[idx].ppa;
//			addrs[i].g.pg = i / (geo->nsectors * geo->nplanes) + (off / vblk->nblks) * vblk->dev->flash_type;
            addrs[i].g.pg = i / (geo->nsectors * geo->nplanes) + (off / vblk->nblks) * 1;
            addrs[i].g.pl = (i / geo->nsectors) % geo->nplanes;
            addrs[i].g.sec = i % geo->nsectors;

//			printf("%s [%3d]\t", __func__, i, addrs[i].ppa);
//			nvm_addr_pr_man(vblk->dev, addrs[i]);
            break;
        }

        ssize_t err = 0;
        if (flag_write == true) {
//			printf("MP[%2d]\t write_unit=%4u\t idx=%2d\t start_ppa= %016lx\n",
//					omp_get_thread_num(), off, idx, addrs[0].ppa);
            err = nvm_addr_write(vblk->dev, addrs, write_unit_sectors, buf_off, NULL, PMODE, &ret);
        } else {
//			printf("MP[%2d]\t read_unit=%4u\t idx=%2d\t start_ppa= %016lx\n",
//					omp_get_thread_num(), off, idx, addrs[0].ppa);
            err = nvm_addr_read(vblk->dev, addrs, write_unit_sectors, buf_off, NULL, PMODE, &ret);
        }
        if (err)
            ++nerr;

#ifdef _OPENMP
#pragma omp ordered
        {}
#endif
    }

    if (nerr) {
        errno = EIO;
        return -1;
    }

    return count;
}

static inline ssize_t
vblk_s12_async(struct nvm_vblk *vblk, const void *buf, size_t count, size_t offset, bool flag_write)
{
    uint8_t *data = (uint8_t *) buf;
    const struct nvm_geo *geo = nvm_dev_get_geo(vblk->dev);
    /** 最小写单元=8sectors */
    const uint32_t write_unit_sectors = (uint32_t) (geo->nplanes * geo->nsectors);
    /** 数据对齐=8sectors*4096bytes=32KB */
    const uint32_t ALIGN = write_unit_sectors * (uint32_t) geo->sector_nbytes;

    const uint32_t bgn = (uint32_t) offset / ALIGN;
    const uint32_t end = bgn + ((uint32_t) count / ALIGN);
    uint8_t *padding_buf = NULL;

    if (offset + count > vblk->nbytes) {        // Check bounds
        LOG_WARN("offset=%ld count=%ld size error %ld\n", offset, count, offset + count);
        errno = EINVAL;
        return -1;
    }
    if ((count % ALIGN) || (offset % ALIGN)) {    // Check align
        LOG_WARN("ALIGN error count=%ld offset=%ld\n", offset, count);
        errno = EINVAL;
        return -1;
    }
    if (vblk->aio == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!buf) {    // Allocate and use a padding buffer
        const size_t nbytes = write_unit_sectors * geo->sector_nbytes;
        padding_buf = nvm_buf_alloc(geo, nbytes);
        if (!padding_buf) {
            LOG_ERROR("FAILED: nvm_buf_alloc(padding)");
            errno = ENOMEM;
            return -1;
        }
        nvm_buf_fill((char *) padding_buf, nbytes);
    }
#if 0
    for (uint32_t off = bgn; off < end; off += 1) {
        struct nvm_addr addrs[write_unit_sectors];
        uint8_t *buf_off;
        const int idx = off % vblk->nblks;

        if (padding_buf)
            buf_off = padding_buf;
        else
            buf_off = &data[(off - bgn) * ALIGN];

        for (uint32_t i = 0; i < write_unit_sectors; ++i) {
            addrs[i].ppa = vblk->blks[idx].ppa;
            addrs[i].g.pg = i / (geo->nsectors * geo->nplanes) + (off / vblk->nblks) * 1;
            addrs[i].g.pl = (i / geo->nsectors) % geo->nplanes;
            addrs[i].g.sec = i % geo->nsectors;
//			printf("%s [%3d]\t", __func__, i, addrs[i].ppa);
//			nvm_addr_pr_man(vblk->dev, addrs[i]);
            break;
        }

        uint64_t dev_addr = nvm_addr_gen2dev(vblk->dev, addrs[0]) * 512;
//		printf("MP[%2d] unit=%4u idx=%2d start_ppa= %016lx\n", omp_get_thread_num(), off, idx, dev_addr);
        int err;
        while (1) {
            if (flag_write == true) {
                err = nvm_async_pwrite(vblk->aio, buf_off, write_unit_sectors * 4096, dev_addr, NULL);
            } else {
                err = nvm_async_pread(vblk->aio, buf_off, write_unit_sectors * 4096, dev_addr, NULL);
            }
            if (err < 0) {
                if (errno == EAGAIN) {
                    while (1) {
                        int poke = nvm_async_poke(vblk->aio, 0);
                        if (poke > 0) {
							LOG_INFO("async poke event=%d", poke);
                            break;
                        }
                    }
                } else return -1;
            } else break;
        }
    }
    nvm_async_wait(vblk->aio);
#else
    static struct cmd_ctx ctx[256];
    bool ctx_state[256] = {0};
    uint32_t ctx_id = 0;
    uint32_t ctx_cnt = 0;

    for (uint32_t off = bgn; off < end; off += 1) {
        struct nvm_addr addrs[write_unit_sectors];
        uint8_t *buf_off;
        const int idx = off % vblk->nblks;

        if (padding_buf)
            buf_off = padding_buf;
        else
            buf_off = &data[(off - bgn) * ALIGN];

        for (uint32_t i = 0; i < write_unit_sectors; ++i) {
            addrs[i].ppa = vblk->blks[idx].ppa;
            addrs[i].g.pg = i / (geo->nsectors * geo->nplanes) + (off / vblk->nblks) * 1;
            addrs[i].g.pl = (i / geo->nsectors) % geo->nplanes;
            addrs[i].g.sec = i % geo->nsectors;
            break;
        }

        for (uint32_t j = 0; j < 256; ++j) {
            if(ctx_state[j] == false) {
                ctx_id = j;
                ctx_state[j] = true;
                break;
            }
        }

        ctx[ctx_id].data = buf_off;
        ctx[ctx_id].addrs = addrs;
        ctx[ctx_id].naddrs = write_unit_sectors;

        if (flag_write == true) {
            nvm_addr_async_protected_write(vblk->dev, &ctx[ctx_id], 0, 0, 0, 0);
        } else {
            nvm_addr_async_read(vblk->dev, &ctx[ctx_id], 0, 0);
        }
        ctx_cnt ++;
        LOG_INFO("ctx_id=%d, ctx_cnt=%d", ctx_id, ctx_cnt);
        if( ctx_cnt == 256 ) {
            struct result_ctx ctx_rlt;
            nvm_get_async_cmd_event(vblk->dev, &ctx_rlt);
            for (int i = 0; i < ctx_rlt.max_count; ++i) {
                for (int j = 0; j < 256; ++j) {
                    if(ctx_rlt.ctx[i] == &ctx[j]) {
                        ctx_state[j] = false;
                    }
                }
            }
            ctx_cnt -= ctx_rlt.max_count;
        }
    }
    while (1) {
        struct result_ctx ctx_rlt;
        nvm_get_async_cmd_event(vblk->dev, &ctx_rlt);
        LOG_INFO("ctx_rlt.max_count=%d", ctx_rlt.max_count);
        if(nvm_async_get_outstanding(vblk->dev->aio) == 0)
            break;
    }
#endif

    return count;
}

ssize_t nvm_vblk_pwrite(struct nvm_vblk *vblk, const void *buf, size_t count, size_t offset)
{
    const int verid = nvm_dev_get_verid(nvm_vblk_get_dev(vblk));

    switch (verid) {
        case NVM_SPEC_VERID_12:
//		return vblk_wr_sync(vblk, buf, count, offset, true);
        return vblk_s12_async(vblk, buf, count, offset, true);

        default:
            LOG_ERROR("FAILED: unsupported verid: %d", verid);
            errno = ENOSYS;
            return -1;
    }
}

ssize_t nvm_vblk_write(struct nvm_vblk *vblk, const void *buf, size_t count)
{
    ssize_t nbytes = nvm_vblk_pwrite(vblk, buf, count, vblk->pos_write);

    if (nbytes < 0)
        return nbytes;        // Propagate errno

    vblk->pos_write += nbytes;    // All is good, increment write position

    return nbytes;            // Return number of bytes written
}

ssize_t nvm_vblk_pad(struct nvm_vblk *vblk)
{
    return nvm_vblk_write(vblk, NULL, vblk->nbytes - vblk->pos_write);
}

ssize_t nvm_vblk_pread(struct nvm_vblk *vblk, void *buf, size_t count, size_t offset)
{
    const int verid = nvm_dev_get_verid(nvm_vblk_get_dev(vblk));

    switch (verid) {
        case NVM_SPEC_VERID_12:
//        return vblk_wr_sync(vblk, buf, count, offset, false);
            return vblk_s12_async(vblk, buf, count, offset, false);

        default:
            LOG_ERROR("FAILED: unsupported verid: %d", verid);
            errno = ENOSYS;
            return -1;
    }
}

ssize_t nvm_vblk_read(struct nvm_vblk *vblk, void *buf, size_t count)
{
    ssize_t nbytes = nvm_vblk_pread(vblk, buf, count, vblk->pos_read);

    if (nbytes < 0)
        return nbytes;        // Propagate `errno`

    vblk->pos_read += nbytes;    // All is good, increment read position

    return nbytes;            // Return number of bytes read
}

static inline ssize_t vblk_copy_s20(struct nvm_vblk *src, struct nvm_vblk *dst)
{
    size_t nerr = 0;

    const size_t offset = 0;
    const size_t count = src->nbytes;

    const uint32_t WS_MIN = nvm_dev_get_ws_min(src->dev);

    const struct nvm_geo *geo = nvm_dev_get_geo(src->dev);
    const size_t nchunks = src->nblks;

    const size_t sectr_nbytes = geo->l.nbytes;
    const size_t nsectr = count / sectr_nbytes;

    const size_t sectr_bgn = offset / sectr_nbytes;
    const size_t sectr_end = sectr_bgn + (count / sectr_nbytes) - 1;

    const size_t cmd_nsectr_max = (NVM_NADDR_MAX / WS_MIN) * WS_MIN;

    if (nsectr % WS_MIN) {
        LOG_DEBUG("FAILED: unaligned nsectr: %zu", nsectr);
        errno = EINVAL;
        return -1;
    }
    if (sectr_bgn % WS_MIN) {
        LOG_DEBUG("FAILED: unaligned sectr_bgn: %zu", sectr_bgn);
        errno = EINVAL;
        return -1;
    }

    for (size_t sectr_ofz = sectr_bgn; sectr_ofz <= sectr_end; sectr_ofz += cmd_nsectr_max) {
        struct nvm_ret ret = {0, 0};

        const size_t cmd_nsectr = NVM_MIN(sectr_end - sectr_ofz + 1, cmd_nsectr_max);

        struct nvm_addr addrs_src[cmd_nsectr];
        struct nvm_addr addrs_dst[cmd_nsectr];

        for (size_t idx = 0; idx < cmd_nsectr; ++idx) {
            const size_t sectr = sectr_ofz + idx;
            const size_t wunit = sectr / WS_MIN;
            const size_t rnd = wunit / nchunks;

            const size_t chunk = wunit % nchunks;
            const size_t chunk_sectr = sectr % WS_MIN + rnd * WS_MIN;

            addrs_src[idx].val = src->blks[chunk].val;
            addrs_src[idx].l.sectr = chunk_sectr;

            addrs_dst[idx].val = dst->blks[chunk].val;
            addrs_dst[idx].l.sectr = chunk_sectr;
        }

        const ssize_t err = nvm_cmd_copy(src->dev, addrs_src,
                                         addrs_dst, cmd_nsectr, 0x0,
                                         &ret);
        if (err)
            ++nerr;
    }

    if (nerr) {
        LOG_ERROR("FAILED: nvm_cmd_write, nerr(%zu)", nerr);
        errno = EIO;
        return -1;
    }

    return count;
}

ssize_t nvm_vblk_copy(struct nvm_vblk *src, struct nvm_vblk *dst,
                      int NVM_UNUSED(flags))
{
    const int verid = nvm_dev_get_verid(nvm_vblk_get_dev(src));

    if (src->dev != dst->dev) {
        LOG_ERROR("FAILED: unsupported cross device copy");
        errno = ENOSYS;
        return -1;
    }

    if (src->nblks != dst->nblks) {
        LOG_WARN("FAILED: unbalanced vblks");
        errno = ENOSYS;
        return -1;
    }

    if (src->nbytes != dst->nbytes) {
        LOG_DEBUG("FAILED: unbalanced vblks");
        errno = ENOSYS;
        return -1;
    }

    switch (verid) {
        case NVM_SPEC_VERID_20:
            return vblk_copy_s20(src, dst);

        case NVM_SPEC_VERID_12:
            LOG_DEBUG("FAILED: not implemented, verid: %d", verid);
            errno = ENOSYS;
            return -1;

        default:
            LOG_DEBUG("FAILED: unsupported, verid: %d", verid);
            errno = ENOSYS;
            return -1;
    }
}

struct nvm_addr *nvm_vblk_get_addrs(struct nvm_vblk *vblk)
{
    return vblk->blks;
}

int nvm_vblk_get_naddrs(struct nvm_vblk *vblk)
{
    return vblk->nblks;
}

size_t nvm_vblk_get_nbytes(struct nvm_vblk *vblk)
{
    return vblk->nbytes;
}

size_t nvm_vblk_get_pos_read(struct nvm_vblk *vblk)
{
    return vblk->pos_read;
}

size_t nvm_vblk_get_pos_write(struct nvm_vblk *vblk)
{
    return vblk->pos_write;
}

struct nvm_dev *nvm_vblk_get_dev(struct nvm_vblk *vblk)
{
    if (!vblk)
        return NULL;

    return vblk->dev;
}

int nvm_vblk_set_pos_read(struct nvm_vblk *vblk, size_t pos)
{
    if (pos > vblk->nbytes) {
        errno = EINVAL;
        return -1;
    }

    vblk->pos_read = pos;

    return 0;
}

int nvm_vblk_set_pos_write(struct nvm_vblk *vblk, size_t pos)
{
    if (pos > vblk->nbytes) {
        errno = EINVAL;
        return -1;
    }

    vblk->pos_write = pos;

    return 0;
}

void nvm_vblk_pr(struct nvm_vblk *vblk)
{
    printf("vblk:\n");
    printf("  dev: {pmode: '%s'}\n", nvm_pmode_str(nvm_dev_get_pmode(vblk->dev)));
    printf("  nblks: %"PRIi32"\n", vblk->nblks);
    printf("  nmbytes: %zu\n", vblk->nbytes >> 20);
    printf("  pos_write: %zu\n", vblk->pos_write);
    printf("  pos_read: %zu\n", vblk->pos_read);
    nvm_addr_prn(vblk->blks, vblk->nblks, vblk->dev);
}

uint32_t nvm_vblk_validBBT(struct nvm_dev *dev, uint32_t idx[], uint32_t len)
{
    const struct nvm_bbt *bbt[len];
    const struct nvm_geo *geo = &dev->geo;

    int num = 0;
    for (uint64_t i = 0; i < geo->nchannels; ++i) {
        for (uint64_t j = 0; j < geo->nluns; ++j) {
            struct nvm_addr addr = {.ppa=0};
            addr.g.ch = i;
            addr.g.lun = j;
            bbt[num++] = nvm_bbt_get(dev, addr, NULL);
        }
    }

    uint32_t valid = 0;
    for (uint64_t k = 0; k < bbt[0]->nblks; k += geo->nplanes) {

        uint64_t tmp = 0;
        uint8_t rlt = 0;
        for (uint64_t i = 0; i < geo->nchannels; ++i) {
            for (uint64_t j = 0; j < geo->nluns; ++j) {
                rlt |= bbt[tmp]->blks[k] | bbt[tmp]->blks[k + 1];
                tmp++;
            }
        }
        if (rlt == 0) {
//			printf("%s [%4d] = %4ld\n", __func__, valid, k/geo->nplanes);
            idx[valid++] = (uint32_t) (k / geo->nplanes);
            if (valid == len)
                break;
        }
    }
    return valid;
}

bool nvm_vblk_line_valid(struct nvm_dev *dev, struct nvm_vblk *vblk)
{
    const struct nvm_geo *geo = &dev->geo;
    uint32_t index = vblk->blks[0].g.blk;

    uint8_t rlt = 0;
    for (uint64_t i = 0; i < geo->nchannels; ++i) {
        for (uint64_t j = 0; j < geo->nluns; ++j) {
            struct nvm_addr addr = {.ppa=0};
            const struct nvm_bbt *bbt;
            addr.g.ch = i;
            addr.g.lun = j;
            bbt = nvm_bbt_get(dev, addr, NULL);
            rlt |= bbt->blks[index * 2] | bbt->blks[index * 2 + 1];
            if (rlt != 0)
                return false;
        }
    }

    return true;
}

int nvm_vblk_create(struct nvm_dev *dev, struct nvm_vblk *vblk[], uint32_t vblk_len)
{
    uint32_t idx[dev->geo.nblocks];
    uint32_t valid = nvm_vblk_validBBT(dev, idx, sizeof(idx) / sizeof(uint32_t));
    (void)valid;
//    printf("%s valid BBT nums=%d\n", __func__, valid);

    for (uint32_t i = 0; i < vblk_len; ++i) {
        vblk[i] = nvm_vblk_alloc_line(dev, 0, dev->geo.nchannels - 1, 0, dev->geo.nluns - 1, idx[i]);
//		bool rlt = nvm_vblk_line_valid(dev, vblk[i]);
    }
    return 0;
}
