/*
 * dev - Device functions
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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "libnvm_full.h"
#include "nvm_be.h"
#include "nvm_dev.h"
#include "nvm_debug.h"

static struct nvm_be *nvm_backends[] = {
		&nvm_be_baidu,
		&nvm_be_ioctl,
		NULL
};

const char *nvm_pmode_str(int pmode) {
	switch (pmode) {
	case NVM_FLAG_PMODE_SNGL:
		return "SNGL";
	case NVM_FLAG_PMODE_DUAL:
		return "DUAL";
	case NVM_FLAG_PMODE_QUAD:
		return "QUAD";
	default:
		return "UNKN";
	}
}

void nvm_dev_pr(const struct nvm_dev *dev)
{
	if (!dev) {
		printf("dev: ~\n");
		return;
	}

	printf("dev_"); nvm_dev_attr_pr(dev);

	switch(nvm_dev_get_verid(dev)) {
	case NVM_SPEC_VERID_12:
		printf("dev_"); nvm_geo_pr(&dev->geo);
		printf("dev_"); nvm_spec_ppaf_nand_pr(&dev->ppaf);
		printf("dev_"); nvm_spec_ppaf_nand_mask_pr(&dev->mask);
		printf("dev_"); nvm_spec_lbaf_pr(NULL);
		printf("dev_"); nvm_spec_lbaz_pr(NULL);
		printf("dev_"); nvm_spec_lbam_pr(NULL);
		break;

	case NVM_SPEC_VERID_20:
		printf("dev_"); nvm_geo_pr(&dev->geo);
		printf("dev_"); nvm_spec_ppaf_nand_pr(NULL);
		printf("dev_"); nvm_spec_ppaf_nand_mask_pr(NULL);
		printf("dev_"); nvm_spec_lbaf_pr(&dev->lbaf);
		printf("dev_"); nvm_spec_lbaz_pr(&dev->lbaz);
		printf("dev_"); nvm_spec_lbam_pr(&dev->lbam);
		break;
	}
}

const struct nvm_geo * nvm_dev_get_geo(const struct nvm_dev *dev)
{
	return &dev->geo;
}

int nvm_dev_get_ws_min(const struct nvm_dev *dev)
{
	switch(dev->verid) {
	case NVM_SPEC_VERID_12:
		return dev->geo.nsectors;
	case NVM_SPEC_VERID_20:
		return dev->idfy.s20.wrt.ws_min;
	}

	return -1;
}

int nvm_dev_get_ws_opt(const struct nvm_dev *dev)
{
	switch(dev->verid) {
	case NVM_SPEC_VERID_12:
		return dev->geo.nplanes * dev->geo.nsectors;
	case NVM_SPEC_VERID_20:
		return dev->idfy.s20.wrt.ws_opt;
	}

	return -1;
}

int nvm_dev_get_mw_cunits(const struct nvm_dev *dev)
{
	switch(dev->verid) {
	case NVM_SPEC_VERID_20:
		return dev->idfy.s20.wrt.mw_cunits;

	case NVM_SPEC_VERID_12:
	default:
		errno = ENOSYS;
		return -1;
	}
}

int nvm_dev_get_verid(const struct nvm_dev *dev)
{
	return dev->verid;
}

int nvm_dev_get_fd(const struct nvm_dev *dev)
{
	return dev->fd;
}

int nvm_dev_get_be_id(const struct nvm_dev *dev)
{
	return dev->be->id;
}

const char *nvm_dev_get_name(const struct nvm_dev *dev)
{
	return dev->name;
}

const char *nvm_dev_get_path(const struct nvm_dev *dev)
{
	return dev->path;
}

uint32_t nvm_dev_get_mccap(const struct nvm_dev *dev)
{
	return dev->mccap;
}

int nvm_dev_get_quirks(const struct nvm_dev *dev)
{
	return dev->quirks;
}

int nvm_dev_set_quirks(struct nvm_dev *dev, int quirks)
{
	dev->quirks = quirks;
	return 0;
}

int nvm_dev_get_pmode(const struct nvm_dev *dev)
{
	return dev->pmode;
}

int nvm_dev_set_pmode(struct nvm_dev *dev, int pmode)
{
	switch (pmode) {
	case NVM_FLAG_PMODE_QUAD:
		if (dev->geo.nplanes < 4) {
			errno = EINVAL;
			return -1;
		}
		/* FALLTHRU */
	case NVM_FLAG_PMODE_DUAL:
		if (dev->geo.nplanes < 2) {
			errno = EINVAL;
			return -1;
		}
		/* FALLTHRU */
	case NVM_FLAG_PMODE_SNGL:
		dev->pmode = pmode;
		return 0;

	default:
		errno = EINVAL;
		return -1;
	}
}

int nvm_dev_get_meta_mode(const struct nvm_dev *dev)
{
	return dev->meta_mode;
}

int nvm_dev_set_meta_mode(struct nvm_dev *dev, int meta_mode)
{
	switch (meta_mode) {
	case NVM_META_MODE_NONE:
		dev->meta_mode = NVM_META_MODE_NONE;
		return 0;
	case NVM_META_MODE_ALPHA:
		dev->meta_mode = NVM_META_MODE_ALPHA;
		return 0;
	case NVM_META_MODE_CONST:
		dev->meta_mode = NVM_META_MODE_CONST;
		return 0;

	default:
		errno = EINVAL;
		return -1;
	}
}

const struct nvm_spec_ppaf_nand *nvm_dev_get_ppaf(const struct nvm_dev *dev)
{
	return &dev->ppaf;
}

const struct nvm_spec_ppaf_nand_mask *nvm_dev_get_ppaf_mask(const struct nvm_dev *dev)
{
	return &dev->mask;
}

const struct nvm_spec_lbaf *nvm_dev_get_lbaf(const struct nvm_dev *dev)
{
	return &dev->lbaf;
}

int nvm_dev_get_nsid(const struct nvm_dev *dev)
{
	return dev->nsid;
}

int nvm_dev_get_erase_naddrs_max(const struct nvm_dev *dev)
{
	return dev->erase_naddrs_max;
}

int nvm_dev_set_erase_naddrs_max(struct nvm_dev *dev, int naddrs)
{
	if (naddrs > NVM_NADDR_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (naddrs < 1) {
		errno = EINVAL;
		return -1;
	}

	dev->erase_naddrs_max = naddrs;

	return 0;
}

int nvm_dev_get_read_naddrs_max(const struct nvm_dev *dev)
{
	return dev->read_naddrs_max;
}

int nvm_dev_set_read_naddrs_max(struct nvm_dev *dev, int naddrs)
{
	if (naddrs > NVM_NADDR_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (naddrs < 1) {
		errno = EINVAL;
		return -1;
	}
	if (dev->pmode && (naddrs % (dev->geo.nplanes))) {
		errno = EINVAL;
		return -1;
	}

	dev->read_naddrs_max = naddrs;

	return 0;
}

int nvm_dev_get_write_naddrs_max(const struct nvm_dev *dev)
{
	return dev->write_naddrs_max;
}

int nvm_dev_set_write_naddrs_max(struct nvm_dev *dev, int naddrs)
{
	if (naddrs > NVM_NADDR_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (naddrs < 1) {
		errno = EINVAL;
		return -1;
	}
	
	if (dev->verid != NVM_SPEC_VERID_20) {
		if (dev->pmode && (naddrs % (dev->geo.nplanes * dev->geo.nsectors))) {
			errno = EINVAL;
			return -1;
		}
		if (naddrs % (dev->geo.nsectors)) {
			errno = EINVAL;
			return -1;
		}
	}

	dev->write_naddrs_max = naddrs;

	return 0;
}

int nvm_dev_get_bbts_cached(const struct nvm_dev *dev)
{
	return dev->bbts_cached;
}

int nvm_dev_set_bbts_cached(struct nvm_dev *dev, int bbts_cached)
{
	switch(bbts_cached) {
	case 0:
	case 1:
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	dev->bbts_cached = bbts_cached;

	return 0;
}

struct nvm_dev * nvm_dev_openf(const char *dev_path, int flags) {
	struct nvm_dev *dev = NULL;

	int be = flags & NVM_BE_ALL;

	for (int i = 0; nvm_backends[i]; ++i) {
		if (be && !(nvm_backends[i]->id & be))
			continue;

		dev = nvm_backends[i]->open(dev_path, 0x0);
		if (dev) {
			dev->be = nvm_backends[i];
			break;
		}
	}

	if (!dev) {
		errno = errno ? errno : EIO;
		return NULL;
	}

	dev->bbts_cached = 1;
	dev->nbbts = dev->geo.nchannels * dev->geo.nluns;
	dev->bbts = malloc(sizeof(*dev->bbts) * dev->nbbts);
	if (!dev->bbts) {
		LOG_ERROR("FAILED: malloc dev->bbts");
		errno = ENOMEM;
		return NULL;
	}

	for (size_t i = 0; i < dev->nbbts; ++i)
		dev->bbts[i] = NULL;

	return dev;
}


static dev_open_times = 0 ;
static dev_close_times = 0 ;

struct nvm_dev *nvm_dev_open(const char *dev_path)
{

	LOG_DEBUG("open nvm_dev %d times",++dev_open_times);
	return nvm_dev_openf(dev_path, 0x0);
}

void nvm_dev_close(struct nvm_dev *dev)
{
	LOG_DEBUG("close nvm_dev %d times",++dev_close_times);
	if (!dev)
		return;
	nvm_bbt_flush_all(dev, NULL);
	dev->be->close(dev);
	free(dev->bbts);
	free(dev);
}

int nvm_ver_major(void)
{
	return LNVM_VERSION_MAJOR;
}

int nvm_ver_minor(void)
{
	return LNVM_VERSION_MINOR;
}

int nvm_ver_patch(void)
{
	return LNVM_VERSION_PATCH;
}

void nvm_ver_pr(void)
{
	printf("Ver { major(%d), minor(%d), patch(%d) }\n",
		   nvm_ver_major(), nvm_ver_minor(), nvm_ver_patch());
}

