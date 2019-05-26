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
#include <hal_timer.h>
#include <pthread.h>
#include "liblightnvm.h"
#include "../include/nvm_debug.h"
#include "test_list.h"


#if 0
struct timespec mStart;
struct timespec mStop;
static void updateTime()
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &mStart);
	mStop  = mStart;
}
long long elapseMs()
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &mStop);	
	long  long diffInNanos = (mStop.tv_sec-mStart.tv_sec)*(long long)1e9 + (mStop.tv_nsec-mStart.tv_nsec);
	return diffInNanos;
}
#endif

void test_aer_get_log(void **state)
{
    LOG_INFO("%s\n",__func__);
    struct nvm_dev *dev = *state;

    //nvm_baidu_error_injection(dev, addr,  INJECT_ERASE);
    //initialize the aer block
#if 0 
    initialize_aer_cp_block();
    __u32 result;
    while(++i < 50)
    {
        if(recv_aer_cp(-1, &result) > 0)
        {
            printf("[INFO]: get the async completion result = %d\n", result);
            break;
        }
    }
#endif
    int entries = 1;
    struct nvm_log_page *err_log;
    err_log = calloc(entries, sizeof(struct nvm_log_page));
	if (!err_log) {
		fprintf(stderr, "could not alloc buffer for error log\n");
		return;
	}

    int ret = nvm_get_async_error_event(dev, err_log);
    show_baidu_log(err_log);
    assert_int_equal(ret, 0);
	ret = nvm_get_mef_log(dev, err_log);
    show_baidu_log(err_log);
    //int ret = nvm_get_mef_log_with_timeout(dev, err_log, 60*1000);
    assert_int_equal(ret, 0);
    if(ret < 0)
    {
        //do nothing have not recv any event request
    }
    else
    {
#if 0
        show_baidu_log(err_log);
#endif
    }
    
    free(err_log);
}

static void *thread_aer_program(void *arg)
{
    struct nvm_dev *dev = arg;
    struct nvm_log_page log;

    while (1) {
        if(0 != nvm_get_async_error_event(dev, &log))
            break;
        show_baidu_log(&log);
    }

    LOG_DEBUG("exit");
    pthread_exit(NULL);
}

void test_aer(void **state)
{
    struct nvm_dev *dev = *state;

    pthread_t thread_aer;
    pthread_create(&thread_aer, NULL, thread_aer_program, dev);

    for (int i = 0; i < 60*1; ) {
        sleep(1);
        i++;
        LOG_INFO("timeout %d s", i);
    }
    LOG_DEBUG("ready to exit");
}

void test_aer_set(void **state)
{
    struct nvm_dev *dev = *state;
    struct hal_timer time;
    hal_timer_start(&time);
    nvm_ext_aer_set(dev, NULL);
    hal_timer_stop(&time);
    hal_timer_pr(&time, __func__);
}

void test_aer_clean(void **state)
{
    struct nvm_dev *dev = *state;
    struct hal_timer time;
    hal_timer_start(&time);
    nvm_ext_aer_clean(dev);
    hal_timer_stop(&time);
    hal_timer_pr(&time, __func__);
}

static char g_dev_path[1024];

static int setup(void **state)
{
    struct nvm_dev *dev = nvm_dev_open(g_dev_path);
    assert_non_null(dev);
    if (!dev)
        return -1;

    *state = dev;
    return 0;
}

static int teardown(void **state)
{
    struct nvm_dev *dev = *state;
    nvm_dev_close(dev);
    return 0;
}



int unit_aer(char *dev_path)
{
    strcpy(g_dev_path, dev_path);

    register_test("aer_program", test_aer, setup, teardown, dev_path);
    return 0;

}
