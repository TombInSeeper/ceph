#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include "liblightnvm.h"
#include "test_list.h"
#include <unistd.h>

void test_dev_open_close(void **state)
{
    char *path = (char *)*state;
	struct nvm_dev *dev;

	dev = nvm_dev_open(path);
	assert_non_null(dev);

	nvm_dev_close(dev);
}

void test_dev_open_close_n(void **state)
{
	char *path = (char *)*state;
	const int n = 4;
	struct nvm_dev *dev[10];

	for (int i = 0; i < n; ++i) {
		dev[i] = nvm_dev_open(path);
		assert_non_null(dev[i]);
		if (!dev[i])
			break;
	}

	for (int i = 0; i < n; ++i) {
		if (!dev[i])
			break;
		nvm_dev_close(dev[i]);
	}
}

int unit_dev(char *dev_path)
{
/*
	const struct CMUnitTest tests[] = {
			cmocka_unit_test_prestate(test_dev_open_close, dev_path),
			cmocka_unit_test_prestate(test_dev_open_close_n, dev_path),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
*/
    
    register_test("dev_open_close", test_dev_open_close, 0, 0, dev_path);
    //FIXME because we has backend thread, can't multi open in one process.
//    register_test("dev_open_close_n", test_dev_open_close_n, 0, 0, dev_path);
	return 0;
}
