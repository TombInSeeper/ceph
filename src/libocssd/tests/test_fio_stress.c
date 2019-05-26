#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <stdlib.h>
#include "test_list.h"

// Test using fio
static void fio_4k_seq_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_4k_seq_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xa4\n\
do_verify=1\n\
\n\
[write2]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xb9\n\
do_verify=1\n\
\n\
[write3]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x3d\n\
do_verify=1\n\
\n\
[write4]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xc1\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_4k_seq_stress.fio");
    assert_int_equal(0, res);
}

static void fio_4k_rand_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_4k_rand_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=0\n\
\n\
[write2]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=0\n\
\n\
[write3]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=0\n\
\n\
[write4]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=4k\n\
ba=4k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=1\n\
\n\
[read2]\n\
rw=randread\n\
bs=4k\n\
ba=4k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=1\n\
\n\
[read3]\n\
rw=randread\n\
bs=4k\n\
ba=4k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=1\n\
\n\
[read4]\n\
rw=randread\n\
bs=4k\n\
ba=4k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_4k_rand_stress.fio");
    assert_int_equal(0, res);
}



static void fio_8k_seq_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_8k_seq_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xa4\n\
do_verify=1\n\
\n\
[write2]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xb9\n\
do_verify=1\n\
\n\
[write3]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x3d\n\
do_verify=1\n\
\n\
[write4]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xc1\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_8k_seq_stress.fio");
    assert_int_equal(0, res);
}

static void fio_8k_rand_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_8k_rand_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=0\n\
\n\
[write2]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=0\n\
\n\
[write3]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=0\n\
\n\
[write4]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=8k\n\
ba=8k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=1\n\
\n\
[read2]\n\
rw=randread\n\
bs=8k\n\
ba=8k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=1\n\
\n\
[read3]\n\
rw=randread\n\
bs=8k\n\
ba=8k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=1\n\
\n\
[read4]\n\
rw=randread\n\
bs=8k\n\
ba=8k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_8k_rand_stress.fio");
    assert_int_equal(0, res);
}




static void fio_16k_seq_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_16k_seq_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xa4\n\
do_verify=1\n\
\n\
[write2]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xb9\n\
do_verify=1\n\
\n\
[write3]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x3d\n\
do_verify=1\n\
\n\
[write4]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xc1\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_16k_seq_stress.fio");
    assert_int_equal(0, res);
}

static void fio_16k_rand_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_16k_rand_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=0\n\
\n\
[write2]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=0\n\
\n\
[write3]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=0\n\
\n\
[write4]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=16k\n\
ba=16k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=1\n\
\n\
[read2]\n\
rw=randread\n\
bs=16k\n\
ba=16k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=1\n\
\n\
[read3]\n\
rw=randread\n\
bs=16k\n\
ba=16k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=1\n\
\n\
[read4]\n\
rw=randread\n\
bs=16k\n\
ba=16k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_16k_rand_stress.fio");
    assert_int_equal(0, res);
}




static void fio_32k_seq_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_32k_seq_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xa4\n\
do_verify=1\n\
\n\
[write2]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xb9\n\
do_verify=1\n\
\n\
[write3]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x3d\n\
do_verify=1\n\
\n\
[write4]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xc1\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_32k_seq_stress.fio");
    assert_int_equal(0, res);
}

static void fio_32k_rand_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_32k_rand_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=0\n\
\n\
[write2]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=0\n\
\n\
[write3]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=0\n\
\n\
[write4]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=32k\n\
ba=32k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xb5\n\
do_verify=1\n\
\n\
[read2]\n\
rw=randread\n\
bs=32k\n\
ba=32k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xca\n\
do_verify=1\n\
\n\
[read3]\n\
rw=randread\n\
bs=32k\n\
ba=32k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x4e\n\
do_verify=1\n\
\n\
[read4]\n\
rw=randread\n\
bs=32k\n\
ba=32k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xd2\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_32k_rand_stress.fio");
    assert_int_equal(0, res);
}



static void fio_96k_seq_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_96k_seq_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xa7\n\
do_verify=1\n\
\n\
[write2]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0x39\n\
do_verify=1\n\
\n\
[write3]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x0d\n\
do_verify=1\n\
\n\
[write4]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xe1\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_96k_seq_stress.fio");
    assert_int_equal(0, res);
}

static void fio_96k_rand_stress(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_96k_rand_stress.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=256\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xc5\n\
do_verify=0\n\
\n\
[write2]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xda\n\
do_verify=0\n\
\n\
[write3]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x5e\n\
do_verify=0\n\
\n\
[write4]\n\
rw=write\n\
bs=96k\n\
ba=96k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xf2\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=96k\n\
ba=96k\n\
offset=0m\n\
size=15360m\n\
verify_pattern=0xc5\n\
do_verify=1\n\
\n\
[read2]\n\
rw=randread\n\
bs=96k\n\
ba=96k\n\
offset=30720m\n\
size=15360m\n\
verify_pattern=0xda\n\
do_verify=1\n\
\n\
[read3]\n\
rw=randread\n\
bs=96k\n\
ba=96k\n\
offset=61440m\n\
size=15360m\n\
verify_pattern=0x5e\n\
do_verify=1\n\
\n\
[read4]\n\
rw=randread\n\
bs=96k\n\
ba=96k\n\
offset=92160m\n\
size=15360m\n\
verify_pattern=0xf2\n\
do_verify=1\n\
", path
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_96k_rand_stress.fio");
    assert_int_equal(0, res);
}


static int init_disk(void **state)
{
    char *dev_path = *state;
    char cmd_str[1024];
    sprintf(cmd_str, "./init_disk %s > log_init_disk.log\n", dev_path);
//    printf("%s\n", cmd_str);
    system(cmd_str);
    return 0;
}

int unit_fio_stress(char *dev_path)
{
    register_test("fio_4k_seq_stress",  fio_4k_seq_stress, init_disk, 0, dev_path);
    register_test("fio_4k_rand_stress", fio_4k_rand_stress, init_disk, 0, dev_path);

    register_test("fio_8k_seq_stress",  fio_8k_seq_stress, init_disk, 0, dev_path);
    register_test("fio_8k_rand_stress", fio_8k_rand_stress, init_disk, 0, dev_path);

    register_test("fio_16k_seq_stress",  fio_16k_seq_stress, init_disk, 0, dev_path);
    register_test("fio_16k_rand_stress", fio_16k_rand_stress, init_disk, 0, dev_path);

    register_test("fio_32k_seq_stress",  fio_32k_seq_stress, init_disk, 0, dev_path);
    register_test("fio_32k_rand_stress", fio_32k_rand_stress, init_disk, 0, dev_path);

    register_test("fio_96k_seq_stress",  fio_96k_seq_stress, init_disk, 0, dev_path);
    register_test("fio_96k_rand_stress", fio_96k_rand_stress, init_disk, 0, dev_path);

    return 0;
}

