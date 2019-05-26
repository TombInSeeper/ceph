/*
 * hal_timer - internal header for liblightnvm
 *
 * Copyright (C) 2015-2017 Javier Gonzáles <javier@cnexlabs.com>
 * Copyright (C) 2015-2017 Matias Bjørling <matias@cnexlabs.com>
 * Copyright (C) 2015-2017 Simon A. F. Lund <slund@cnexlabs.com>
 * Copyright (C) 2018      Klaus Jensen <klaus.jensen@cnexlabs.com>
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
#ifndef __INTERNAL_HAL_TIMER_H
#define __INTERNAL_HAL_TIMER_H

#include <stdint.h>
#include <stdio.h>

#define PR_GREEN "\033[32m"
#define PR_CLEAN "\033[0m"

/*
 * A simple timer.
 */
struct hal_timer {
	uint64_t start;
	uint64_t stop;
};

/**
 * Start a timer.
 *
 * @returns clock sample in nano seconds.
 */
uint64_t hal_timer_start(struct hal_timer *t);

/**
 * Stop a timer.
 *
 * @returns clock sample in nano seconds.
 */
uint64_t hal_timer_stop(struct hal_timer *t);

/**
 * Get the elapsed time in seconds.
 *
 * @returns elapsed time in seconds as a floating point number.
 */
double hal_timer_elapsed_secs(struct hal_timer *t);

/**
 * Get the elapsed time in seconds.
 *
 * @returns elapsed time in seconds as a floating point number.
 */
double hal_timer_elapsed(struct hal_timer *t);

/**
 * Get the elapsed time in milliseconds.
 *
 * @returns elapsed time in milliseconds as a floating point number.
 */
double hal_timer_elapsed_msecs(struct hal_timer *t);

/**
 * Get the elapsed time in microseconds.
 *
 * @returns elapsed time in microseconds as a floating point number.
 */
double hal_timer_elapsed_usecs(struct hal_timer *t);

/**
 * Get the elapsed time in nanoseconds.
 *
 * @returns elapsed time in nanoseconds.
 */
uint64_t hal_timer_elapsed_nsecs(struct hal_timer *t);

/**
 * Print the elapsed time in seconds as a floating point number.
 */
void hal_timer_pr(struct hal_timer *t, const char *prefix);

/**
 * Print the elapsed time in seconds and the associated data rate in MB/s.
 */
void hal_timer_bw_pr(struct hal_timer *t, const char *prefix, size_t nbytes);

#endif /* __INTERNAL_HAL_TIMER_H */
