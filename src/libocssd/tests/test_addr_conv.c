#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <string.h>
#include "libnvm_full.h"

static void conv(struct nvm_dev *dev, int func)
{
    size_t tsectr = 0;
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);

    switch (nvm_dev_get_verid(dev)) {
        case NVM_SPEC_VERID_12:
            tsectr = geo->nchannels * geo->nluns * geo->nplanes *
                     geo->nblocks * geo->npages * geo->nsectors;
            break;

        case NVM_SPEC_VERID_20:
            tsectr = geo->l.npugrp * geo->l.npunit * geo->l.nchunk * geo->l.nsectr;
            break;

        default:
            print_error("INVALID VERID");
            return;
    }

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t sectr = 0; sectr < tsectr; ++sectr) {
        struct nvm_addr exp = {.val = 0};
        struct nvm_addr act = {.val = 0};
        uint64_t conv;

        switch (nvm_dev_get_verid(dev)) {
            case NVM_SPEC_VERID_12:
                exp.g.sec = sectr % geo->nsectors;
                exp.g.pg = (sectr / geo->nsectors) % geo->npages;
                exp.g.blk = ((sectr / geo->nsectors) / geo->npages) % geo->nblocks;
                exp.g.pl = (((sectr / geo->nsectors) / geo->npages) / geo->nblocks) % geo->nplanes;
                exp.g.lun = ((((sectr / geo->nsectors) / geo->npages) / geo->nblocks) / geo->nplanes) % geo->nluns;
                exp.g.ch = (((((sectr / geo->nsectors) / geo->npages) / geo->nblocks) / geo->nplanes) / geo->nluns) %
                           geo->nchannels;
                break;

            case NVM_SPEC_VERID_20:
                exp.l.sectr = sectr % geo->l.nsectr;
                exp.l.chunk = (sectr / geo->l.nsectr) % geo->l.nchunk;
                exp.l.punit = ((sectr / geo->l.nsectr) / geo->l.nchunk) % geo->l.npunit;
                exp.l.pugrp = (((sectr / geo->l.nsectr) / geo->l.nchunk) / geo->l.npunit) % geo->l.npugrp;
                break;
        }

        assert_true(!nvm_addr_check(exp, dev));

        switch (func) {
            case 0:
                conv = nvm_addr_gen2dev(dev, exp);
                act = nvm_addr_dev2gen(dev, conv);
                break;

            case 1:
                conv = nvm_addr_gen2off(dev, exp);
                act = nvm_addr_off2gen(dev, conv);
                break;

            case 2:
                conv = nvm_addr_gen2lba(dev, exp);
                act = nvm_addr_lba2gen(dev, conv);
                break;

            case 3:
                conv = nvm_addr_gen2dev(dev, exp);
                conv = nvm_addr_dev2lba(dev, conv);
                act = nvm_addr_lba2gen(dev, conv);
                break;

            case 4:
                conv = nvm_addr_gen2dev(dev, exp);
                conv = nvm_addr_dev2off(dev, conv);
                act = nvm_addr_off2gen(dev, conv);
                break;

            default:
                print_error("Invalid format");
                break;
        }

        assert_int_equal(act.val, exp.val);
        if (act.val != exp.val) {
            printf("Expected: ");
            nvm_addr_prn(&exp, 1, dev);
            printf("Got:      ");
            nvm_addr_prn(&act, 1, dev);
        }
    }
}

void test_FMT_GEN_DEV(void **state)
{
    struct nvm_dev *dev = *state;
    conv(dev, 0);    ///< gen -> dev -> gen
}

void test_FMT_GEN_OFF(void **state)
{
    struct nvm_dev *dev = *state;
    conv(dev, 1);    ///< gen -> off -> gen
}

void test_FMT_GEN_LBA(void **state)
{
    struct nvm_dev *dev = *state;
    conv(dev, 2);    ///< gen -> LBA -> gen
}

void test_FMT_GEN_DEV_LBA(void **state)
{
    struct nvm_dev *dev = *state;
    conv(dev, 3);    ///< gen -> dev -> LBA -> gen
}

void test_FMT_GEN_DEV_OFF(void **state)
{
    struct nvm_dev *dev = *state;
    conv(dev, 4);    ///< gen -> dev -> off -> gen
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

int unit_addr_conv(char *dev_path)
{
    strcpy(g_dev_path, dev_path);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_FMT_GEN_DEV),
            cmocka_unit_test(test_FMT_GEN_LBA),
            cmocka_unit_test(test_FMT_GEN_OFF),
            cmocka_unit_test(test_FMT_GEN_DEV_LBA),
            cmocka_unit_test(test_FMT_GEN_DEV_OFF),
    };
    return cmocka_run_group_tests(tests, setup, teardown);
}
