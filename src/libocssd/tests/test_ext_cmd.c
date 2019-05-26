//
// Created by jiash on 12/09/18.
//
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <string.h>
#include "liblightnvm.h"

void test_baidu_cmd(void **state)
{
    struct nvm_dev *dev = *state;

    assert_int_equal(0, nvm_ext_reset(dev));

    struct nvm_addr addr;
    uint32_t ebc;
    addr.ppa = 0;
    assert_int_equal(0, nvm_ext_error_bit_count(dev, addr, &ebc));
    printf("%s PPA= 0x%lx ErrorBitCount=%d\n", __func__, addr.ppa, ebc);

    assert_int_equal(0, nvm_ext_format(dev));

    uint16_t pe_buf[1024];
    assert_int_equal(0, nvm_ext_get_pe(dev, pe_buf, sizeof(pe_buf)));
}

//static char *dev_path = "/dev/nvme0n1";
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

int unit_extend_cmd(char *dev_path)
{
    strcpy(g_dev_path, dev_path);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_baidu_cmd),
    };
    return cmocka_run_group_tests(tests, setup, teardown);
}

