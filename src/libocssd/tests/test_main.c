/*************************************************************
 *
 * Copyright (C) 2017-2018 by Beijing Starblaze Technology Co., Ltd.
 *
 * NOTICE:  All information contained herein is, and remains the
 * property of Beijing Starblaze Technology Co., Ltd and its
 * suppliers, if any.  The intellectual and technical concepts
 * contained herein are proprietary to Beijing Starblaze
 * Technology Co., Ltd and are protected by patent or trade
 * secret or copyright law.
 *
 * Dissemination of this information or reproduction of this
 * material is strictly forbidden unless prior written permission
 * is obtained from Beijing Starblaze Technology Co., Ltd. Source
 * code from this file can only be used on Starblaze ASIC products.
 *
 * Distribution of source code or binaries derived from this file
 * is not permitted except as specifically allowed By Non_Disclosure
 * Agreement between your company and Beijing Starblaze Technology
 * Co., Ltd. All copies of this source code modified or unmodified
 * must retain this entire copyright notice and comment as is.
 *
 * You should have received a copy of the Non-Disclosure Agreement
 * along with this program; if not, Please contact your manager
 * immediately.
 *
 *************************************************************/

#include <liblightnvm.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <nvm_debug.h>
#include <stdlib.h>
#include <assert.h>

#include "test_list.h"

typedef int (*unit_t)(char *dev_path);

unit_t units[] = {
    unit_dev,
//    unit_addr_conv,
    unit_bbt,
//    unit_extend_cmd,
    unit_cmb,
//    unit_aer,
//    unit_async,
    unit_power_loss,
    unit_io_status,
    unit_io_reset,
    unit_ppa,
    unit_vblk,
    unit_fio_basic,
    unit_fio_stress,
    unit_fio_mixed, 
};

#define MAX_TEST_COUNT 5000

static struct CMUnitTest g_tests[MAX_TEST_COUNT];
static unsigned int g_test_idx = 0;

static char g_runs[MAX_TEST_COUNT][1024];
static unsigned int g_run_idx = 0;

void register_test(const char *name, CMUnitTestFunction test_func, CMFixtureFunction setup_func, CMFixtureFunction teardown_func, void *initial_state)
{
    if (g_test_idx >= MAX_TEST_COUNT) {
        printf("Error: internal: adding test that exceeds max test count %d\n", MAX_TEST_COUNT);
        assert(0);
    }

    g_tests[g_test_idx].name = name;
    g_tests[g_test_idx].test_func = test_func;
    g_tests[g_test_idx].setup_func = setup_func;
    g_tests[g_test_idx].teardown_func = teardown_func;
    g_tests[g_test_idx].initial_state = initial_state;
    g_test_idx++;
}

struct CMUnitTest *get_test(const char *name)
{
    unsigned int i = 0;
    for (i = 0; i < g_test_idx; i++) {
        if (strcmp(g_tests[i].name, name) == 0) {
            return &(g_tests[i]);
        }
    }
    return 0;
}

void show_test()
{
    unsigned int i = 0;
    for (i = 0; i < g_test_idx; i++) {
        printf("    %02d - %s\n", i, g_tests[i].name);
    }
}

static int read_pattern_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (! f) {
	return -1;
    }
    int line = 0;
    char content[2048];
    while (fgets(content, 2048, f) != (char*)0) {
        line++;
        // Erease leading space
        char *ptr = content;
        while (*ptr == ' ' || *ptr == '\t') {
            ptr++;
        }
        if (ptr[0] == '#' || ptr[0] == '\n') {
            continue;
        }
        ptr[strlen(ptr)-1] = '\0';
	strcpy(g_runs[g_run_idx], ptr);
	g_run_idx++;
    }
    fclose(f);
    return 0;
}

static unsigned int file_exists(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (! f) {
    	return 0;
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    int total = sizeof(units) / sizeof(unit_t);
    int i;

    if (argc != 2 && argc != 3) {
        // Fake init
        for (i = 0; i < total; i++) {
            assert(units[i]("fake") == 0);
        }
        printf("A list of available test:\n");
        show_test();
        printf("Usage: %s <dev_path> <optional_pattern_name>\n", argv[0]);
        return -1;
    }

    // Initialize all test
    for (i = 0; i < total; i++) {
        assert(units[i](argv[1]) == 0);
    }

    if (! file_exists(argv[1])) {
	    printf("Error: device %s does not exists\n", argv[1]);
	    return -1;
    }

    if (argc == 2) {
        // Run all tests
        printf("Run all test cases...\n");
        return _cmocka_run_group_tests("all_tests", g_tests, g_test_idx, NULL, NULL);
    } else {
        // Run specific test
        struct CMUnitTest *ut = get_test(argv[2]);
        if (ut == NULL) {
    	    // Not a test, check if this is a file
    	    if (read_pattern_file(argv[2]) == -1) {
            	printf("Error: given test or pattern file %s does not exists\n", argv[2]);
            	return -1;
	        }
        }
	    if (g_run_idx) {
            // Form a new group and test
            struct CMUnitTest *l_tests = (struct CMUnitTest*)malloc(sizeof(struct CMUnitTest) * g_run_idx);
	        for (i = 0; (unsigned int)i < g_run_idx; i++) {
                struct CMUnitTest *tmp_ut = get_test(g_runs[i]);
                l_tests[i] = *tmp_ut;
//		        printf("Runing test '%s'...\n", g_runs[i]);
//                _cmocka_run_group_tests(g_runs[i], tmp_ut, 1, NULL, NULL);
	        }
            return _cmocka_run_group_tests("tests in pattern file", l_tests, g_run_idx, NULL, NULL);
	    } else {
            printf("Run specific test '%s'...\n", argv[2]);
            return _cmocka_run_group_tests(argv[2], ut, 1, NULL, NULL);
        }
    }

    return 0;
}


