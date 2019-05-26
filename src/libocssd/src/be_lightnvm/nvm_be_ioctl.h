/*
 * nvm_be_ioctl - internal header for IOCTL backend
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
#ifndef __INTERNAL_NVM_BE_IOCTL_H
#define __INTERNAL_NVM_BE_IOCTL_H

/**
 * Encapsulation of commonly used fields of (Open-Channel) NVMe commands
 */
struct nvm_cmd {
	union {
		struct {
			uint8_t opcode;		///< cdw00
			uint8_t flags;
			uint16_t control;
			uint16_t nppas;		///< cdw01
			uint16_t rsvd;
			uint64_t metadata;	///< cdw02-03
			uint64_t addr;		///< cdw04-05
			uint64_t ppa_list;	///< cdw06-07
			uint32_t metadata_len;	///< cdw08
			uint32_t data_len;	///< cdw09
			uint64_t status;	///< cdw10-11
			uint32_t result;	///< cdw12
			uint32_t rsvd3[3];	///< cdw13-15
		} vuser;	///< Common Open-Channel NVMe IO cmd. fields

		struct {
			uint8_t opcode;
			uint8_t flags;
			uint8_t rsvd[2];
			uint32_t nsid;			///< cdw01
			uint32_t cdw2;
			uint32_t cdw3;
			uint64_t metadata;	///< cdw04-05
			uint64_t addr;		///< cdw06-07
			uint32_t metadata_len;	///< cdw08
			uint32_t data_len;	///< cdw09
			uint64_t ppa_list;
			uint16_t nppas;
			uint16_t control;
			uint32_t cdw13;
			uint32_t cdw14;
			uint32_t cdw15;
			uint64_t status;
			uint32_t result;
			uint32_t timeout_ms;
		} vadmin;	///< Common Open-Channel NVMe admin cmd. fields

		struct {
			uint8_t opcode;			///< cdw00
			uint8_t flags;
			uint16_t rsvd1;
			uint32_t nsid;			///< cdw01
			uint32_t cdw2;
			uint32_t cdw3;
			uint64_t metadata;	///< cdw04-05
			uint64_t addr;		///< cdw06-07
			uint32_t metadata_len;
			uint32_t data_len;
			uint32_t cdw10;
			uint32_t cdw11;
			uint32_t cdw12;
			uint32_t cdw13;
			uint32_t cdw14;
			uint32_t cdw15;
			uint32_t timeout_ms;
			uint32_t result;
		} admin;	///< Common NVMe admin cmd. fields

		struct {
			uint8_t opcode;
			uint8_t flags;
			uint16_t control;
			uint16_t nblocks;
			uint16_t rsvd;
			uint64_t metadata;
			uint64_t addr;
			uint64_t slba;
			uint32_t dsmgmt;
			uint32_t reftag;
			uint16_t apptag;
			uint16_t appmask;
		} user;		///< Common NVMe IO cmd. fields

		struct {
			uint8_t opcode;
			uint8_t flags;
			uint16_t rsvd;
			uint32_t cdw[19];
		} shared;	///< Shared fields among commands

		uint32_t cdw[20];	///< Command as array of dwords
	};
};

enum nvm_be_ioctl_flags {
	NVM_BE_IOCTL_WRITABLE = 0x1
};

#endif /* __INTERNAL_NVM_BE_IOCTL */
