/*
 * nvm_spec - Printers and helper functions
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
#include "nvm_be.h"
#include "nvm_dev.h"
#include "nvm_debug.h"

#define NVM_I64_FMT	"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"\
			"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"\
			"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"\
			"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"

#define NVM_I64_TO_STR(val) \
	(val & (1ULL << 63) ? '1' : '0'), \
	(val & (1ULL << 62) ? '1' : '0'), \
	(val & (1ULL << 61) ? '1' : '0'), \
	(val & (1ULL << 60) ? '1' : '0'), \
	(val & (1ULL << 59) ? '1' : '0'), \
	(val & (1ULL << 58) ? '1' : '0'), \
	(val & (1ULL << 57) ? '1' : '0'), \
	(val & (1ULL << 56) ? '1' : '0'), \
	(val & (1ULL << 55) ? '1' : '0'), \
	(val & (1ULL << 54) ? '1' : '0'), \
	(val & (1ULL << 53) ? '1' : '0'), \
	(val & (1ULL << 52) ? '1' : '0'), \
	(val & (1ULL << 51) ? '1' : '0'), \
	(val & (1ULL << 50) ? '1' : '0'), \
	(val & (1ULL << 49) ? '1' : '0'), \
	(val & (1ULL << 48) ? '1' : '0'), \
	(val & (1ULL << 47) ? '1' : '0'), \
	(val & (1ULL << 46) ? '1' : '0'), \
	(val & (1ULL << 45) ? '1' : '0'), \
	(val & (1ULL << 44) ? '1' : '0'), \
	(val & (1ULL << 43) ? '1' : '0'), \
	(val & (1ULL << 42) ? '1' : '0'), \
	(val & (1ULL << 41) ? '1' : '0'), \
	(val & (1ULL << 40) ? '1' : '0'), \
	(val & (1ULL << 39) ? '1' : '0'), \
	(val & (1ULL << 38) ? '1' : '0'), \
	(val & (1ULL << 37) ? '1' : '0'), \
	(val & (1ULL << 36) ? '1' : '0'), \
	(val & (1ULL << 35) ? '1' : '0'), \
	(val & (1ULL << 34) ? '1' : '0'), \
	(val & (1ULL << 33) ? '1' : '0'), \
	(val & (1ULL << 32) ? '1' : '0'), \
	(val & (1ULL << 31) ? '1' : '0'), \
	(val & (1ULL << 30) ? '1' : '0'), \
	(val & (1ULL << 29) ? '1' : '0'), \
	(val & (1ULL << 28) ? '1' : '0'), \
	(val & (1ULL << 27) ? '1' : '0'), \
	(val & (1ULL << 26) ? '1' : '0'), \
	(val & (1ULL << 25) ? '1' : '0'), \
	(val & (1ULL << 24) ? '1' : '0'), \
	(val & (1ULL << 23) ? '1' : '0'), \
	(val & (1ULL << 22) ? '1' : '0'), \
	(val & (1ULL << 21) ? '1' : '0'), \
	(val & (1ULL << 20) ? '1' : '0'), \
	(val & (1ULL << 19) ? '1' : '0'), \
	(val & (1ULL << 18) ? '1' : '0'), \
	(val & (1ULL << 17) ? '1' : '0'), \
	(val & (1ULL << 16) ? '1' : '0'), \
	(val & (1ULL << 15) ? '1' : '0'), \
	(val & (1ULL << 14) ? '1' : '0'), \
	(val & (1ULL << 13) ? '1' : '0'), \
	(val & (1ULL << 12) ? '1' : '0'), \
	(val & (1ULL << 11) ? '1' : '0'), \
	(val & (1ULL << 10) ? '1' : '0'), \
	(val & (1ULL << 9) ? '1' : '0'), \
	(val & (1ULL << 8) ? '1' : '0'), \
	(val & (1ULL << 7) ? '1' : '0'), \
	(val & (1ULL << 6) ? '1' : '0'), \
	(val & (1ULL << 5) ? '1' : '0'), \
	(val & (1ULL << 4) ? '1' : '0'), \
	(val & (1ULL << 3) ? '1' : '0'), \
	(val & (1ULL << 2) ? '1' : '0'), \
	(val & (1ULL << 1) ? '1' : '0'), \
	(val & (1ULL << 0) ? '1' : '0')

#define NVM_I32_FMT	"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"\
			"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"

#define NVM_I32_TO_STR(val) \
	(val & (1ULL << 31) ? '1' : '0'), \
	(val & (1ULL << 30) ? '1' : '0'), \
	(val & (1ULL << 29) ? '1' : '0'), \
	(val & (1ULL << 28) ? '1' : '0'), \
	(val & (1ULL << 27) ? '1' : '0'), \
	(val & (1ULL << 26) ? '1' : '0'), \
	(val & (1ULL << 25) ? '1' : '0'), \
	(val & (1ULL << 24) ? '1' : '0'), \
	(val & (1ULL << 23) ? '1' : '0'), \
	(val & (1ULL << 22) ? '1' : '0'), \
	(val & (1ULL << 21) ? '1' : '0'), \
	(val & (1ULL << 20) ? '1' : '0'), \
	(val & (1ULL << 19) ? '1' : '0'), \
	(val & (1ULL << 18) ? '1' : '0'), \
	(val & (1ULL << 17) ? '1' : '0'), \
	(val & (1ULL << 16) ? '1' : '0'), \
	(val & (1ULL << 15) ? '1' : '0'), \
	(val & (1ULL << 14) ? '1' : '0'), \
	(val & (1ULL << 13) ? '1' : '0'), \
	(val & (1ULL << 12) ? '1' : '0'), \
	(val & (1ULL << 11) ? '1' : '0'), \
	(val & (1ULL << 10) ? '1' : '0'), \
	(val & (1ULL << 9) ? '1' : '0'), \
	(val & (1ULL << 8) ? '1' : '0'), \
	(val & (1ULL << 7) ? '1' : '0'), \
	(val & (1ULL << 6) ? '1' : '0'), \
	(val & (1ULL << 5) ? '1' : '0'), \
	(val & (1ULL << 4) ? '1' : '0'), \
	(val & (1ULL << 3) ? '1' : '0'), \
	(val & (1ULL << 2) ? '1' : '0'), \
	(val & (1ULL << 1) ? '1' : '0'), \
	(val & (1ULL << 0) ? '1' : '0')

#define NVM_I16_FMT	"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"

#define NVM_I16_TO_STR(val) \
	(val & (1ULL << 15) ? '1' : '0'), \
	(val & (1ULL << 14) ? '1' : '0'), \
	(val & (1ULL << 13) ? '1' : '0'), \
	(val & (1ULL << 12) ? '1' : '0'), \
	(val & (1ULL << 11) ? '1' : '0'), \
	(val & (1ULL << 10) ? '1' : '0'), \
	(val & (1ULL << 9) ? '1' : '0'), \
	(val & (1ULL << 8) ? '1' : '0'), \
	(val & (1ULL << 7) ? '1' : '0'), \
	(val & (1ULL << 6) ? '1' : '0'), \
	(val & (1ULL << 5) ? '1' : '0'), \
	(val & (1ULL << 4) ? '1' : '0'), \
	(val & (1ULL << 3) ? '1' : '0'), \
	(val & (1ULL << 2) ? '1' : '0'), \
	(val & (1ULL << 1) ? '1' : '0'), \
	(val & (1ULL << 0) ? '1' : '0')

#define NVM_I8_FMT	"%c%c%c%c%c%c%c%c"

#define NVM_I8_TO_STR(val) \
	(val & (1ULL << 7) ? '1' : '0'), \
	(val & (1ULL << 6) ? '1' : '0'), \
	(val & (1ULL << 5) ? '1' : '0'), \
	(val & (1ULL << 4) ? '1' : '0'), \
	(val & (1ULL << 3) ? '1' : '0'), \
	(val & (1ULL << 2) ? '1' : '0'), \
	(val & (1ULL << 1) ? '1' : '0'), \
	(val & (1ULL << 0) ? '1' : '0')

void nvm_spec_lgeo_pr(const struct nvm_spec_lgeo *lgeo)
{
	printf("lgeo:");
	
	if (!lgeo) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  npugrp: %u\n", lgeo->npugrp);
	printf("  npunit: %u\n", lgeo->npunit);
	printf("  nchunk: %u\n", lgeo->nchunk);
	printf("  nsectr: %u\n", lgeo->nsectr);
}

void nvm_spec_wrt_pr(const struct nvm_spec_wrt *wrt)
{
	printf("wrt:");

	if (!wrt) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  ws_min: %u\n", wrt->ws_min);
	printf("  ws_opt: %u\n", wrt->ws_opt);
	printf("  mw_cunits: %u\n", wrt->mw_cunits);
}

void nvm_spec_perf_pr(const struct nvm_spec_perf *perf)
{
	printf("perf:");

	if (!perf) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  trdt: %u\n", perf->trdt);
	printf("  trdm: %u\n", perf->trdm);
	printf("  twrt: %u\n", perf->twrt);
	printf("  twrm: %u\n", perf->twrm);
	printf("  tcet: %u\n", perf->tcet);
	printf("  tcem: %u\n", perf->tcem);
}

void nvm_spec_ppaf_nand_pr(const struct nvm_spec_ppaf_nand *ppaf)
{
	printf("ppaf:");

	if (!ppaf) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  ch_off: %02u\n", ppaf->n.ch_off);
	printf("  ch_len: %02u\n", ppaf->n.ch_len);
	printf("  lun_off: %02u\n", ppaf->n.lun_off);
	printf("  lun_len: %02u\n", ppaf->n.lun_len);
	printf("  pl_off: %02u\n", ppaf->n.pl_off);
	printf("  pl_len: %02u\n", ppaf->n.pl_len);
	printf("  blk_off: %02u\n", ppaf->n.blk_off);
	printf("  blk_len: %02u\n", ppaf->n.blk_len);
	printf("  pg_off: %02u\n", ppaf->n.pg_off);
	printf("  pg_len: %02u\n", ppaf->n.pg_len);
	printf("  sec_off: %02u\n", ppaf->n.sec_off);
	printf("  sec_len: %02u\n", ppaf->n.sec_len);
}

void nvm_spec_ppaf_nand_mask_pr(const struct nvm_spec_ppaf_nand_mask *mask)
{
	printf("ppaf_mask:");

	if (!mask) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  ch:  '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(mask->n.ch));
	printf("  lun: '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(mask->n.lun));
	printf("  pl:  '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(mask->n.pl));
	printf("  blk: '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(mask->n.blk));
	printf("  pg:  '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(mask->n.pg));
	printf("  sec: '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(mask->n.sec));
}

void nvm_spec_lbaf_pr(const struct nvm_spec_lbaf *lbaf)
{
	printf("lbaf:");

	if (!lbaf) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  pugrp: %u\n", lbaf->pugrp);
	printf("  punit: %u\n", lbaf->punit);
	printf("  chunk: %u\n", lbaf->chunk);
	printf("  sectr: %u\n", lbaf->sectr);
}

void nvm_spec_lbaz_pr(const struct nvm_spec_lbaz *lbaz)
{
	printf("lbaz:");

	if (!lbaz) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  pugrp: %lu\n", lbaz->pugrp);
	printf("  punit: %lu\n", lbaz->punit);
	printf("  chunk: %lu\n", lbaz->chunk);
	printf("  sectr: %lu\n", lbaz->sectr);
}

void nvm_spec_lbam_pr(const struct nvm_spec_lbam *lbam)
{
	printf("lbam:");

	if (!lbam) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	printf("  pugrp:  '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(lbam->pugrp));
	printf("  punit:  '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(lbam->punit));
	printf("  chunk:  '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(lbam->chunk));
	printf("  sectr:  '"NVM_I64_FMT"'\n", NVM_I64_TO_STR(lbam->sectr));
}

static void nvm_spec_idfy_s12_pr(const struct nvm_spec_idfy *identify)
{
	struct nvm_spec_idfy_s12 idfy = identify->s12;

	printf("idfy:\n");
	printf("  verid: "NVM_I8_FMT"\n", NVM_I8_TO_STR(idfy.verid));
	printf("  vnvmt: %u\n", idfy.vnvmt);
	printf("  cgroups: %u,\n", idfy.cgroups);
	printf("  cap("NVM_I32_FMT"),\n", NVM_I32_TO_STR(idfy.cap));
	printf("  dom("NVM_I32_FMT"),\n", NVM_I32_TO_STR(idfy.dom));

	printf("spec_identify_cgrps:\n");

	for (int i = 0; i < idfy.cgroups; ++i) {
		struct nvm_spec_idfy_cgrp grp = idfy.grp[i];

		printf("  mtype: 0x%02x\n", grp.mtype);
		if (grp.mtype)
			continue;

		printf("  fmtype: "NVM_I8_FMT"\n", NVM_I8_TO_STR(grp.fmtype));
		printf("  num_ch: %u\n", grp.num_ch);
		printf("  num_luns: %u\n", grp.num_lun);
		printf("  num_pln: %u\n", grp.num_pln);
		printf("  num_blk: %u\n", grp.num_blk);
		printf("  num_pg: %u\n", grp.num_pg);
		printf("  fpg_sz: %u\n", grp.fpg_sz);
		printf("  csecs: %u\n", grp.csecs);
		printf("  sos: %u\n", grp.sos);
		printf("  trdt: %d\n", grp.trdt);
		printf("  trdm: %d\n", grp.trdm);
		printf("  tprt: %d\n", grp.tprt);
		printf("  tprm: %d\n", grp.tprm);
		printf("  tbet: %d\n", grp.tbet);
		printf("  tbem: %d\n", grp.tbem);
		printf("  mpos: "NVM_I32_FMT"\n", NVM_I32_TO_STR(grp.mpos));
		printf("  mccap: "NVM_I32_FMT"\n", NVM_I32_TO_STR(grp.mccap));
		printf("  cpar: %d\n", grp.cpar);
		printf("  mts: NOT_IMPLEMENTED\n");
	}
}

static void nvm_spec_idfy_s20_pr(const struct nvm_spec_idfy *identify)
{
	struct nvm_spec_idfy_s20 idfy = identify->s20;

	printf("idfy:\n");
	printf("  verid: "NVM_I8_FMT"\n", NVM_I8_TO_STR(idfy.verid));
	printf("  verid_minor: "NVM_I8_FMT"\n", NVM_I8_TO_STR(idfy.verid_minor));
	printf("  mccap: "NVM_I32_FMT"\n", NVM_I32_TO_STR(idfy.mccap));
	nvm_spec_lbaf_pr(&idfy.lbaf);
	nvm_spec_lgeo_pr(&idfy.lgeo);
	nvm_spec_wrt_pr(&idfy.wrt);
	nvm_spec_perf_pr(&idfy.perf);
}

void nvm_spec_idfy_pr(const struct nvm_spec_idfy *idfy, int NVM_UNUSED(quirks))
{
	if (!idfy) {
		printf("nvm_spec_idfy: ~\n");
		return;
	}
	switch(idfy->s.verid) {
	case NVM_SPEC_VERID_12:
		nvm_spec_idfy_s12_pr(idfy);
		break;

	case NVM_SPEC_VERID_20:
		nvm_spec_idfy_s20_pr(idfy);
		break;

	default:
		printf("nvm_spec_idfy:\n");
		printf("  verid: "NVM_I8_FMT",\n",
		       NVM_I8_TO_STR(idfy->s.verid));
		printf("  verid_minor: "NVM_I8_FMT",\n",
		       NVM_I8_TO_STR(idfy->s.verid_minor));
	}
}

void nvm_spec_bbt_pr(const struct nvm_spec_bbt *bbt)
{
	if (!bbt) {
		printf("bbt: ~\n");
		return;
	}

	printf("bbt:\n");
	printf("  tblkid: %c%c%c%c\n",
		   bbt->tblid[0], bbt->tblid[1], bbt->tblid[2], bbt->tblid[3]);
	printf("  verid: %u\n", bbt->verid);
	printf("  revid: %u\n", bbt->revid);
	printf("  rvsd1: %u\n", bbt->rvsd1);
	printf("  tblks: %u\n", bbt->tblks);
	printf("  tfact: %u\n", bbt->tfact);
	printf("  tgrown: %u\n", bbt->tgrown);
	printf("  tdresv: %u\n", bbt->tdresv);
	printf("  thresv: %u\n", bbt->thresv);
	printf("  rsvd2: ~\n");

	printf("bbt_blks:\n");
	for (uint32_t i = 0; i < bbt->tblks; ++i)
		printf("  [%4d] - 0x%02x\n", i, bbt->blk[i]);
}

void nvm_spec_rprt_pr(const struct nvm_spec_rprt *rprt)
{
	if (!rprt) {
		printf("rprt: ~\n");
		return;
	}

	printf("rprt:\n");
	printf("  ndescr: %u\n", rprt->ndescr);

	printf("rprt_descr:");
	if (!rprt->ndescr) {
		printf(" ~\n");
		return;
	}

	printf("\n");
	for (uint64_t i = 0; i < rprt->ndescr; ++i) {
		const struct nvm_spec_rprt_descr *descr = &rprt->descr[i];

		printf("  [%4ld] { ", i);
		printf("slba: 0x%016lX, ", descr->addr);
		printf("cnlb: 0x%04lx, ", descr->naddrs);
		printf("wp: 0x%016lx, ", descr->wp);
		printf("cs: 0x%02X, ", descr->cs);
		printf("ct: 0x%02X, ", descr->ct);
		printf("wli: 0x%02x", descr->wli);
		printf(" }\n");
	}
}

void nvm_geo_pr(const struct nvm_geo *geo)
{
    printf("geo:");

    if (!geo) {
        printf(" ~\n");
        return;
    }

    printf("\n");
    printf("  verid: 0x%02X\n", geo->verid);
    switch (geo->verid) {
        case NVM_SPEC_VERID_12:
            printf("  nchannels: %zu\n", geo->nchannels);
            printf("  nluns: %zu\n", geo->nluns);
            printf("  nplanes: %zu\n", geo->nplanes);
            printf("  nblocks: %zu\n", geo->nblocks);
            printf("  npages: %zu\n", geo->npages);
            printf("  nsectors: %zu\n", geo->nsectors);
            printf("  page_nbytes: %zu\n", geo->page_nbytes);
            printf("  sector_nbytes: %zu\n", geo->sector_nbytes);
            printf("  meta_nbytes: %zu\n", geo->meta_nbytes);
            break;

        case NVM_SPEC_VERID_20:
            printf("  npugrp: %zu\n", geo->l.npugrp);
            printf("  npunit: %zu\n", geo->l.npunit);
            printf("  nchunk: %zu\n", geo->l.nchunk);
            printf("  nsectr: %zu\n", geo->l.nsectr);
            printf("  nbytes: %zu\n", geo->l.nbytes);
            printf("  nbytes_oob: %zu\n", geo->l.nbytes_oob);
            break;
    }

    printf("  tbytes: %zu\n", geo->tbytes);
    printf("  tmbytes: %zu\n", geo->tbytes >> 20);

}

void nvm_dev_attr_pr(const struct nvm_dev *dev)
{
	if (!dev) {
		printf("attr: ~\n");
		return;
	}

	printf("attr:\n");
	printf("  verid: 0x%02x\n", nvm_dev_get_verid(dev));
	printf("  be_id: 0x%02x\n", nvm_dev_get_be_id(dev));
	printf("  name: '%s'\n", nvm_dev_get_name(dev));
	printf("  path: '%s'\n", nvm_dev_get_path(dev));
	printf("  fd: %d\n", nvm_dev_get_fd(dev));
	printf("  ssw: %"PRIu64"\n", dev->ssw);
	printf("  mccap: "NVM_I32_FMT"\n",
		   NVM_I32_TO_STR(nvm_dev_get_mccap(dev)));
	printf("  pmode: '%s'\n", nvm_pmode_str(nvm_dev_get_pmode(dev)));

	printf("  erase_naddrs_max: %d\n", dev->erase_naddrs_max);
	printf("  read_naddrs_max: %d\n", dev->read_naddrs_max);
	printf("  write_naddrs_max: %d\n",dev->write_naddrs_max);

	printf("  meta_mode: %d\n", nvm_dev_get_meta_mode(dev));
	printf("  bbts_cached: %d\n", nvm_dev_get_bbts_cached(dev));

	printf("  bbts_cached: %08x\n", nvm_dev_get_bbts_cached(dev));

	printf("  quirks: "NVM_I8_FMT"\n",
		   NVM_I8_TO_STR(nvm_dev_get_quirks(dev)));
}

