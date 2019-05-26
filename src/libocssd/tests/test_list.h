//
// Created by jiashu on 11/9/18.
//

#ifndef OCSSD_BAIDU_TEST_LIST_H
#define OCSSD_BAIDU_TEST_LIST_H

#include <cmocka.h>

int unit_dev(char *dev_path);

int unit_addr_conv(char *dev_path);

int unit_extend_cmd(char *dev_path);

int unit_async(char *dev_path);

int unit_bbt(char *dev_path);

int unit_ppa(char *dev_path);

int unit_vblk(char *dev_path);

int unit_cmb(char *dev_path);

int unit_aer(char *dev_path);

int unit_fio_stress(char *dev_path);
int unit_fio_basic(char *dev_path);
int unit_fio_mixed(char *dev_path);

int unit_power_loss(char *dev_path);

int unit_io_status(char *dev_path);

int unit_io_reset(char *dev_path);

void register_test(const char *name, CMUnitTestFunction test_func, CMFixtureFunction setup_func, CMFixtureFunction teardown_func, void *initial_state);

#endif //OCSSD_BAIDU_TEST_LIST_H
