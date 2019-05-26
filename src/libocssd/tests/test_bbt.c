#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <string.h>
#include "libnvm_full.h"
#include <time.h>
#include "hal_timer.h"
#include "test_list.h"

#define TEST_BBT_BLKS   (20)

static void verify_bbt(const struct nvm_bbt *bbt, const struct nvm_geo *geo)
{
    uint32_t nbad = 0, ngbad = 0, ndmrk = 0, nhmrk = 0;
    assert_int_equal(bbt->nblks, geo->nplanes * geo->nblocks);

    for (uint64_t i = 0; i < bbt->nblks; ++i) {
        switch (bbt->blks[i]) {
            case NVM_BBT_FREE:
                break;
            case NVM_BBT_BAD:
                ++nbad;
                break;
            case NVM_BBT_GBAD:
                ++ngbad;
                break;
            case NVM_BBT_DMRK:
                ++ndmrk;
                break;
            case NVM_BBT_HMRK:
                ++nhmrk;
                break;
            default:
                print_error("Invalid blk-state");
                break;
        }
    }

    assert_int_equal(bbt->nbad, nbad);
    assert_int_equal(bbt->ngbad, ngbad);
    assert_int_equal(bbt->ndmrk, ndmrk);
    assert_int_equal(bbt->nhmrk, nhmrk);
}

/**
 * Test that we can get all valid bad-block-table from a device
 * @param state
 */
void test_bbt_all_verify(void **state)
{
    struct nvm_dev *dev = *state;
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    struct nvm_ret ret = {0, 0};

    for (uint64_t i = 0; i < geo->nchannels; ++i) {
        for (uint64_t j = 0; j < geo->nluns; ++j) {
            struct nvm_addr addr = {.ppa=0};
            addr.g.ch = i;
            addr.g.lun = j;

            const struct nvm_bbt *bbt = nvm_bbt_get(dev, addr, &ret);    // Verify that we can call it
            assert_non_null(bbt);
            assert_int_equal(0, ret.result);
            assert_int_equal(0, ret.status);
            verify_bbt(bbt, geo);
        }
    }
}

// Test that we can set bbt using `nvm_bbt_set`
//
// @warn
// This will alter the state of the bad-block-table on the device.
// It will most likely leave the bad-block-table in a different state than
// it was in before running this test
//
void bbt_set(void **state, bool cached)
{
    struct nvm_dev *dev = *state;
    struct nvm_ret ret = {0, 0};
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);

    nvm_dev_set_bbts_cached(dev, cached);

    /** 1. backup one bbt */
    struct nvm_addr addr = {.ppa=0};
    addr.g.ch = 0;
    addr.g.lun = 0;
    struct nvm_bbt *bbt_back = nvm_bbt_alloc_cp(nvm_bbt_get(dev, addr, &ret));
    assert_non_null(bbt_back);
    verify_bbt(bbt_back, geo);

    /** 2. alter the state of the bbt */
    struct nvm_bbt *bbt_exp = nvm_bbt_alloc_cp(nvm_bbt_get(dev, addr, &ret));

    int num = 0;
    for (uint64_t i = 0; i < bbt_back->nblks; i++) {
        if (bbt_exp->blks[i] == NVM_BBT_FREE) {
            bbt_exp->blks[i] = NVM_BBT_BAD;
            if (num++ > TEST_BBT_BLKS)
                break;
        }
    }
    assert_int_equal(0, nvm_bbt_set(dev, bbt_exp, &ret));

    /** 3. get the new state of the bbt */
    const struct nvm_bbt *bbt_alter = nvm_bbt_get(dev, addr, &ret);
    assert_non_null(bbt_alter);
    verify_bbt(bbt_alter, geo);

    /** 4. verify the state of these two bbts */
    size_t diff = nvm_buf_diff((char *) bbt_exp->blks, (char *) bbt_alter->blks, bbt_exp->nblks);

    /** 5. recover the state of ocssd's bbt */
    assert_int_equal(0, nvm_bbt_set(dev, bbt_back, &ret));

    assert_int_equal(0, diff);
    nvm_bbt_free(bbt_exp);
}

/**
 * Test that we can set bbt using `nvm_bbt_mark`
 * @param state
 */
void bbt_mark(void **state, bool cached)
{
    struct nvm_dev *dev = *state;
    struct nvm_ret ret = {0, 0};
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);

    nvm_dev_set_bbts_cached(dev, cached);

    /** 1. backup one bbt */
    struct nvm_addr addr = {.ppa=0};
    addr.g.ch = 0;
    addr.g.lun = 0;
    struct nvm_bbt *bbt_back = nvm_bbt_alloc_cp(nvm_bbt_get(dev, addr, &ret));
    assert_non_null(bbt_back);
    verify_bbt(bbt_back, geo);

    /** 2. alter the state of the bbt */
    const struct nvm_bbt *bbt_alter = nvm_bbt_get(dev, addr, &ret);
    assert_non_null(bbt_alter);
    verify_bbt(bbt_alter, geo);

    uint64_t blk_idx[bbt_alter->nblks];
    int num = 0;
    for (uint64_t i = 0; i < bbt_alter->nblks; i++) {
        struct nvm_addr tmp = {.ppa=0};
        tmp.g.ch = addr.g.ch;
        tmp.g.lun = addr.g.lun;
        tmp.g.blk = i / geo->nplanes;
        tmp.g.pl = i % geo->nplanes;
        if (bbt_alter->blks[i] == NVM_BBT_FREE) {
            assert_int_equal(0, nvm_bbt_mark(dev, &tmp, 1, NVM_BBT_BAD, &ret));
            blk_idx[num] = i;
            if (num++ > TEST_BBT_BLKS)
                break;
        }
    }

    /** 3. get the new state of the bbt */
    assert_non_null(nvm_bbt_get(dev, addr, &ret));

    /** 4. verify the state of these two bbts */
    size_t diff = 0;
    for (int j = 0; j < num; ++j) {
        if (bbt_alter->blks[blk_idx[j]] != NVM_BBT_BAD)
            diff++;
    }

    /** 5. recover the state of ocssd's bbt */
    assert_int_equal(0, nvm_bbt_set(dev, bbt_back, &ret));
    assert_int_equal(0, diff);
}

void test_bbt_direct_set(void **state)
{
    bbt_set(state, false);
}
void test_bbt_cached_set(void **state)
{
    bbt_set(state, true);
}
void test_bbt_direct_mark(void **state)
{
    bbt_mark(state, false);
}
void test_bbt_cached_mark(void **state)
{
    bbt_mark(state, true);
}

static char g_dev_path[1024];

static int setup(void **state)
{
    struct nvm_dev *dev = nvm_dev_open(g_dev_path);
    assert_non_null(dev);
    assert_int_equal(NVM_SPEC_VERID_12, nvm_dev_get_verid(dev));
    *state = dev;
    return 0;
}

static int teardown(void **state)
{
    struct nvm_dev *dev = *state;
    nvm_dev_close(dev);
    return 0;
}

int unit_bbt(char *dev_path)
{
    strcpy(g_dev_path, dev_path);
/*
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(bbt_all_verify, setup, teardown),
            cmocka_unit_test_setup_teardown(bbt_direct_set, setup, teardown),
            cmocka_unit_test_setup_teardown(bbt_direct_mark, setup, teardown),
            cmocka_unit_test_setup_teardown(bbt_cached_set, setup, teardown),
            cmocka_unit_test_setup_teardown(bbt_cached_mark, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
*/
    register_test("bbt_all_verify",     test_bbt_all_verify, setup, teardown, 0);
    register_test("bbt_direct_set",     test_bbt_direct_set, setup, teardown, 0);
    register_test("bbt_direct_mark",    test_bbt_direct_mark, setup, teardown, 0);
    register_test("bbt_cached_set",     test_bbt_cached_set, setup, teardown, 0);
    register_test("bbt_cached_mark",    test_bbt_cached_mark, setup, teardown, 0);
    return 0;
}

