#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include "liblightnvm.h"
#include <nvm_debug.h>
#include <log_csv.h>
#include <nvm_vblk.h>
#include "hal_timer.h"
#include "test_list.h"

#define TEST_VBLK_NUMS 1

void vblk_throughput_base(void **state, uint32_t sectors)
{
    struct nvm_dev *dev = *state;

    uint32_t len = TEST_VBLK_NUMS;
    struct nvm_vblk *vblk[len];
    nvm_vblk_create(dev, vblk, len);

    if(sectors != vblk[0]->min_write_sectors) {
        for (uint32_t i = 0; i < len; ++i) {
            vblk[i]->min_write_sectors = sectors;
        }
    }

    size_t nbytes = nvm_vblk_get_nbytes(vblk[0]);

	struct nvm_buf_set *bufs = nvm_buf_set_alloc(dev, nbytes, 0);
	assert_non_null(bufs);
	nvm_buf_set_fill(bufs);

    struct hal_timer time;
    for (uint32_t i = 0; i < len; ++i) {
        hal_timer_start(&time);
        assert_int_equal(0, nvm_vblk_erase(vblk[i]));
        hal_timer_stop(&time);
        hal_timer_pr(&time, "nvm_vblk_erase");
    }

    hal_timer_start(&time);
    for (uint32_t i = 0; i < len; ++i) {
        assert_int_equal(nbytes, nvm_vblk_write(vblk[i], bufs->write, nbytes));
    }
    hal_timer_stop(&time);
    hal_timer_bw_pr(&time, "nvm_vblk_write", nbytes);

    sleep(5);

    hal_timer_start(&time);
    for (uint32_t i = 0; i < len; ++i) {
        assert_int_equal(nbytes, nvm_vblk_read(vblk[i], bufs->read, nbytes));
    }
    hal_timer_stop(&time);
    hal_timer_bw_pr(&time, "nvm_vblk_read", nbytes);

    assert_int_equal(0, nvm_buf_diff(bufs->write, bufs->read, nbytes));

    for (uint32_t j = 0; j < len; ++j) {
        nvm_vblk_free(vblk[j]);
    }
	nvm_buf_set_free(bufs);
}

void vblk_throughput_8sectors(void **state)
{
    vblk_throughput_base(state, 8);
}

void vblk_throughput_32sectors(void **state)
{
    vblk_throughput_base(state, 32);
}

void vblk_seq_latency(void **state)
{
    struct nvm_dev *dev = *state;
    uint32_t len = TEST_VBLK_NUMS;
    struct nvm_vblk *vblk[len];
    struct hal_timer time;
    uint32_t size = 1*1024*1024;

    nvm_vblk_create(dev, vblk, len);

    size_t vblk_size = nvm_vblk_get_nbytes(vblk[0]);
    size_t num = vblk_size/size*len;

    struct nvm_buf_set *bufs = nvm_buf_set_alloc(dev, size, 0);
    assert_non_null(bufs);

    for (uint32_t i = 0; i < len; ++i) {
        hal_timer_start(&time);
        assert_int_equal(0, nvm_vblk_erase(vblk[i]));
        hal_timer_stop(&time);
        hal_timer_pr(&time, "nvm_vblk_erase");
    }

    double us_write[num];
    uint32_t cnt_write = 0;
    for (uint32_t i = 0; i < len; ++i) {
        for (uint32_t j = 0; j < vblk_size/size; ++j) {
            hal_timer_start(&time);
            assert_int_equal(size, nvm_vblk_write(vblk[i], bufs->write, size));
            hal_timer_stop(&time);
            us_write[cnt_write++] = hal_timer_elapsed_usecs(&time);
        }
    }

    sleep(5);

    double us_read[num];
    uint32_t cnt_read = 0;
    for (uint32_t i = 0; i < len; ++i) {
        for (uint32_t j = 0; j < vblk_size/size; ++j) {
            hal_timer_start(&time);
            assert_int_equal(size, nvm_vblk_read(vblk[i], bufs->read, size));
            hal_timer_stop(&time);
            us_read[cnt_read++] = hal_timer_elapsed_usecs(&time);
        }
    }
    log_csv("vblk_seq_latency", "Time (us)", "write", us_write, num);
    log_csv("vblk_seq_latency", "Time (us)", "read", us_read, num);

    for (uint32_t j = 0; j < len; ++j) {
        nvm_vblk_free(vblk[j]);
    }
    nvm_buf_set_free(bufs);
}

void vblk_mix_latency(void **state)
{
    struct nvm_dev *dev = *state;
    uint32_t len = TEST_VBLK_NUMS;
    struct nvm_vblk *vblk[len];
    struct hal_timer time;
    uint32_t size = 1*1024*1024;

    nvm_vblk_create(dev, vblk, len);

    size_t vblk_size = nvm_vblk_get_nbytes(vblk[0]);
    size_t num = vblk_size/size*len;

    struct nvm_buf_set *bufs = nvm_buf_set_alloc(dev, size, 0);
    assert_non_null(bufs);

    for (uint32_t i = 0; i < len; ++i) {
        hal_timer_start(&time);
        assert_int_equal(0, nvm_vblk_erase(vblk[i]));
        hal_timer_stop(&time);
        hal_timer_pr(&time, "nvm_vblk_erase");
    }

    double us_write[num];
    uint32_t cnt_write = 0;
    double us_read[num];
    uint32_t cnt_read = 0;

    for (uint32_t i = 0; i < len; ++i) {
        for (uint32_t j = 0; j < vblk_size/size; ++j) {
            hal_timer_start(&time);
            assert_int_equal(size, nvm_vblk_write(vblk[i], bufs->write, size));
            hal_timer_stop(&time);
            us_write[cnt_write++] = hal_timer_elapsed_usecs(&time);
            hal_timer_start(&time);
            assert_int_equal(size, nvm_vblk_read(vblk[i], bufs->read, size));
            hal_timer_stop(&time);
            us_read[cnt_read++] = hal_timer_elapsed_usecs(&time);
            assert_memory_equal(bufs->write, bufs->read, size);
        }
    }

    log_csv("vblk_mix_latency", "Time (us)", "write", us_write, num);
    log_csv("vblk_mix_latency", "Time (us)", "read", us_read, num);

    for (uint32_t j = 0; j < len; ++j) {
        nvm_vblk_free(vblk[j]);
    }
    nvm_buf_set_free(bufs);
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

int unit_vblk(char *dev_path)
{
    strcpy(g_dev_path, dev_path);

    register_test("io_vblk_throughput_32k", vblk_throughput_8sectors, setup, teardown, dev_path);
    register_test("io_vblk_throughput_96k", vblk_throughput_32sectors, setup, teardown, dev_path);
    register_test("io_vblk_seq_latency", vblk_seq_latency, setup, teardown, dev_path);
    register_test("io_vblk_mix_latency", vblk_mix_latency, setup, teardown, dev_path);
    return 0;
}
