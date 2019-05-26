//
// Created by user on 18-10-13.
//

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include "liblightnvm.h"
#include <string.h>
#include <fcntl.h>
#include <nvm_async.h>
#include <nvm_debug.h>
#include <stdlib.h>

void async_cb(struct nvm_async_event *evt)
{
    LOG_INFO("%s arg=%s result=%ld", __func__, (char*)evt->cb_arg, evt->result);
}

uint64_t HexStr2Int(char *buf)
{
    uint64_t result = 0;
    uint64_t tmp;
    uint64_t len, i;

    len = strlen(buf);
    printf("len=%ld \n", len);
    for (i = 0; i < len; i++) {
        if (*buf >= 'A' && *buf <= 'Z')
            tmp = *buf - 'A' + 10;
        else if (*buf >= 'a' && *buf <= 'z')
            tmp = *buf - 'a' + 10;
        else
            tmp = *buf - '0';


        result *= 16;
        result += tmp;
        buf++;
    }
    return result;
}

static char g_dev_path[1024];

#define DATA_SIZE 4096
/**
 * use pwrite and pread to test
 * @param state
 */
void test_stand_file_sync(void **state)
{
	(void) state;
    int fd = open("stand_sync.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(fd <= 0) {
        assert_true(fd > 0);
        return;
    }

    uint32_t count = 10;
    uint8_t data[DATA_SIZE*count];
    uint8_t recv[DATA_SIZE*count];

    for(uint32_t i=0; i<count; i++) {
        memset(&data[i*DATA_SIZE], i+48, DATA_SIZE);
        ssize_t rlt = pwrite(fd, &data[i*DATA_SIZE], DATA_SIZE, i*DATA_SIZE);
        assert_int_equal(rlt, DATA_SIZE);
    }
    for(uint32_t i=0; i<count; i++) {
        ssize_t rlt = pread(fd, &recv[i*DATA_SIZE], DATA_SIZE, i*DATA_SIZE);
        assert_int_equal(rlt, DATA_SIZE);
    }
    assert_memory_equal(data, recv, DATA_SIZE*count);

    close(fd);
}

/**
 * 2.use async mode of pwrite and pread to test
 * @param state
 */
void test_stand_file_async(void **state)
{
	(void) state;
    int fd = open("stand_async.txt", O_RDWR | O_CREAT | O_TRUNC | O_DIRECT, 0666);
    if(fd <= 0) {
        assert_true(fd > 0);
        return;
    }

	uint32_t count = 10;
	uint8_t *data = aligned_alloc(4096, DATA_SIZE*count);
	assert_non_null(data);
    uint8_t *recv = aligned_alloc(4096, DATA_SIZE*count);
	assert_non_null(recv);
    struct nvm_async_ctx *ctx = nvm_async_init(fd, count);
    assert_non_null(ctx);

	for(uint32_t i=0; i<count; i++) {
		memset(&data[i*DATA_SIZE], i+48, DATA_SIZE);
		nvm_async_event_t event;
		event.cb = async_cb;
		event.cb_arg = "pwrite";
		int rlt = nvm_async_pwrite(ctx, &data[i*DATA_SIZE], DATA_SIZE, i*DATA_SIZE, &event);
		assert_int_equal(rlt, 0);
	}
    assert_int_equal(count, nvm_async_wait(ctx));
    assert_int_equal(0, nvm_async_poke(ctx, 0));

	for(uint32_t i=0; i<count; i++) {
		nvm_async_event_t event;
		event.cb = async_cb;
		event.cb_arg = "pread";
		int rlt = nvm_async_pread(ctx, &recv[i*DATA_SIZE], DATA_SIZE, i*DATA_SIZE, &event);
		assert_int_equal(rlt, 0);
	}
	assert_int_equal(count, nvm_async_wait(ctx));
    assert_int_equal(0, nvm_async_poke(ctx, 0));

	nvm_async_term(ctx);
    assert_memory_equal(data, recv, DATA_SIZE*count);
	free(data);
	free(recv);
    close(fd);
}

void test_baidu_async(void **state)
{
    (void) state;
    struct nvm_dev *dev = nvm_dev_open(g_dev_path);
    assert_non_null(dev);

    uint32_t count = 32;
//    uint8_t *data = aligned_alloc(4096, DATA_SIZE*count);
    uint8_t *data = malloc(DATA_SIZE*count);
    assert_non_null(data);

    struct nvm_addr addr;

    struct cmd_ctx ctx[count];
    for(uint32_t i=0; i<count; i++) {
        addr.ppa = 0;
        addr.g.blk = i;
        ctx[i].data = data;
        ctx[i].addrs = &addr;
        ctx[i].naddrs = 1;
        assert_int_equal(0, nvm_addr_async_read(dev, &ctx[i], 0, 0));
    }
    struct result_ctx ctx_out;
    assert_int_equal(0, nvm_get_async_cmd_event(dev, &ctx_out));
    assert_int_equal(0, nvm_get_async_cmd_event(dev, &ctx_out));
    assert_int_equal(0, nvm_get_async_cmd_event(dev, &ctx_out));
//    assert_int_equal(1, ctx_out.max_count);

    free(data);
    nvm_dev_close(dev);
}

int unit_async(char *dev_path)
{
    strcpy(g_dev_path, dev_path);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_stand_file_sync),
            cmocka_unit_test(test_stand_file_async),
			cmocka_unit_test(test_baidu_async),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
