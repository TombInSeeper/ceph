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
#ifndef __INTERNAL_NVM_TIMER_H
#define __INTERNAL_NVM_TIMER_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "hal_timer.h"

uint64_t _clock_sample(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (uint64_t) (ts.tv_nsec + ts.tv_sec * 1e9);
}

/**
 * Start a timer.
 *
 * @returns clock sample in nano seconds.
 */
uint64_t hal_timer_start(struct hal_timer *t)
{
	t->start = _clock_sample();
	return t->start;
}

/**
 * Stop a timer.
 *
 * @returns clock sample in nano seconds.
 */
uint64_t hal_timer_stop(struct hal_timer *t)
{
	t->stop = _clock_sample();
	return t->stop;
}

/**
 * Get the elapsed time in seconds.
 *
 * @returns elapsed time in seconds as a floating point number.
 */
double hal_timer_elapsed_secs(struct hal_timer *t)
{
	return (t->stop - t->start) / (double)1e9;
}

/**
 * Get the elapsed time in seconds.
 *
 * @returns elapsed time in seconds as a floating point number.
 */
double hal_timer_elapsed(struct hal_timer *t)
{
	return hal_timer_elapsed_secs(t);
}

/**
 * Get the elapsed time in milliseconds.
 *
 * @returns elapsed time in milliseconds as a floating point number.
 */
double hal_timer_elapsed_msecs(struct hal_timer *t)
{
	return (t->stop - t->start) / (double)1e6;
}

/**
 * Get the elapsed time in microseconds.
 *
 * @returns elapsed time in microseconds as a floating point number.
 */
double hal_timer_elapsed_usecs(struct hal_timer *t)
{
	return (t->stop - t->start) / (double)1e3;
}

/**
 * Get the elapsed time in nanoseconds.
 *
 * @returns elapsed time in nanoseconds.
 */
uint64_t hal_timer_elapsed_nsecs(struct hal_timer *t)
{
	return t->stop - t->start;
}

/**
 * Print the elapsed time in seconds as a floating point number.
 */
void hal_timer_pr(struct hal_timer *t, const char *prefix)
{
	printf("%s: {elapsed: %lf ms}\n", prefix, hal_timer_elapsed_msecs(t));
}

/**
 * Print the elapsed time in seconds and the associated data rate in MB/s.
 */
void hal_timer_bw_pr(struct hal_timer *t, const char *prefix, size_t nbytes)
{
	double secs = hal_timer_elapsed_secs(t);
    double us = hal_timer_elapsed_usecs(t);
	double mb = nbytes / (double)(1024*1024);
    size_t kb = nbytes / 1024;

	if (0) {
		printf("%s:\t", prefix);
		printf("{time: "PR_GREEN"%.4fs(%.3fus)"PR_CLEAN",\t", secs, us);
		printf("size: "PR_GREEN"%.2fMB(%ldKB)"PR_CLEAN",\t", mb, kb);
		printf("speed: "PR_GREEN"%.2fMB/s"PR_CLEAN"}\n", mb / secs);
	}
	else {
		printf("%s,\t", prefix);
		printf("time_us,\t %.3f,\t", us);
		printf("size_kb, %6ld,\t", kb);
		printf("speed_mb/s, %.2f\n", mb / secs);
	}
}

#endif /* __INTERNAL_NVM_TIMER_H */
