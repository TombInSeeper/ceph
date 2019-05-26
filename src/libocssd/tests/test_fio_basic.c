#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <stdlib.h>
#include "test_list.h"
#include "liblightnvm.h"


// Test using fio

static unsigned long long int g_sb_bytes = 0;

static void fio_4k_seq_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_4k_seq_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0x4b\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_4k_seq_basic.fio");
    assert_int_equal(0, res);
}

static void fio_4k_seq_basic_unalign(void **state)
{
    // NOTE - unaligned is handled by memcpy the data to a new aligned data buffer, so we do not needs to
    //        test all unalign value.
    char *path = (char *)*state;
    FILE *f = fopen("fio_4k_seq_basic_503b_align.fio", "w");
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
mem_align=503b\n\
size=%0lldm\n\
verify_pattern=0x8c\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_4k_seq_basic_503b_align.fio");
    assert_int_equal(0, res);

    f = fopen("fio_4k_seq_basic_1111b_align.fio", "w");
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
mem_align=1111b\n\
size=%0lldm\n\
verify_pattern=0x93\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_4k_seq_basic_1111b_align.fio");
    assert_int_equal(0, res);

}

static void fio_32k_seq_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_32k_seq_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0x6a\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_32k_seq_basic.fio -b");
    assert_int_equal(0, res);
}


static void fio_96k_seq_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_96k_seq_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0x90\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_96k_seq_basic.fio -b");
    assert_int_equal(0, res);
}


static void fio_16k_seq_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_16k_seq_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0x5c\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_16k_seq_basic.fio");
    assert_int_equal(0, res);
}


static void fio_8k_seq_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_8k_seq_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0x14\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_8k_seq_basic.fio");
    assert_int_equal(0, res);
}

static void fio_4k_rand_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_4k_rand_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0x12\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=4k\n\
ba=4k\n\
size=%0lldm\n\
verify_pattern=0x12\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_4k_rand_basic.fio -i");
    assert_int_equal(0, res);
}

static void fio_4k_qd1_rand_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_4k_qd1_rand_basic.fio", "w");
    int res;
    fprintf(f, "\
[global]\n\
direct=1\n\
iodepth=1\n\
ioengine=ocssd\n\
filename=%s\n\
\n\
[write1]\n\
rw=write\n\
bs=4k\n\
ba=4k\n\
size=%0lldm\n\
verify_pattern=0x83\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=4k\n\
ba=4k\n\
size=%0lldm\n\
verify_pattern=0x83\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024, g_sb_bytes / 1024 / 1024
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_4k_qd1_rand_basic.fio -l");
    assert_int_equal(0, res);
}

static void fio_8k_rand_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_8k_rand_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0xcd\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=8k\n\
ba=8k\n\
size=%0lldm\n\
verify_pattern=0xcd\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_8k_rand_basic.fio");
    assert_int_equal(0, res);
}


static void fio_16k_rand_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_16k_rand_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0xcd\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=16k\n\
ba=16k\n\
size=%0lldm\n\
verify_pattern=0xcd\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_16k_rand_basic.fio");
    assert_int_equal(0, res);
}

static void fio_32k_rand_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_32k_rand_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0xcd\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=32k\n\
ba=32k\n\
size=%0lldm\n\
verify_pattern=0xcd\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_32k_rand_basic.fio");
    assert_int_equal(0, res);
}


static void fio_96k_rand_basic(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_96k_rand_basic.fio", "w");
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
size=%0lldm\n\
verify_pattern=0x9d\n\
do_verify=0\n\
\n\
[read1]\n\
stonewall\n\
rw=randread\n\
bs=96k\n\
ba=96k\n\
size=%0lldm\n\
verify_pattern=0x9d\n\
do_verify=1\n\
\n\
", path, g_sb_bytes / 1024 / 1024 * 10, g_sb_bytes / 1024 / 1024 * 10
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_96k_rand_basic.fio");
    assert_int_equal(0, res);
}



static int init_disk(void **state)
{
    char *dev_path = *state;
    char cmd_str[1024];
    sprintf(cmd_str, "./init_disk %s > log_init_disk.log\n", dev_path);
    system(cmd_str);

   // Calculate super block size
    struct nvm_dev *dev = nvm_dev_open(dev_path);
    assert_non_null(dev);
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    assert_non_null(geo);
    nvm_dev_close(dev);

    g_sb_bytes = geo->nchannels * geo->nluns * geo->nplanes * geo->npages * geo->page_nbytes;
    assert_true(g_sb_bytes != 0);
    assert_true(g_sb_bytes % (1024 * 1024) == 0);


    return 0;
}

int unit_fio_basic(char *dev_path)
{

    register_test("fio_4k_seq_basic", fio_4k_seq_basic, init_disk, 0, dev_path);
    register_test("fio_8k_seq_basic", fio_8k_seq_basic, init_disk, 0, dev_path);
    register_test("fio_16k_seq_basic", fio_16k_seq_basic, init_disk, 0, dev_path);
    register_test("fio_32k_seq_basic", fio_32k_seq_basic, init_disk, 0, dev_path);
    register_test("fio_96k_seq_basic", fio_96k_seq_basic, init_disk, 0, dev_path);

    register_test("fio_4k_rand_basic", fio_4k_rand_basic, init_disk, 0, dev_path);
    register_test("fio_8k_rand_basic", fio_8k_rand_basic, init_disk, 0, dev_path);
    register_test("fio_16k_rand_basic", fio_16k_rand_basic, init_disk, 0, dev_path);
    register_test("fio_32k_rand_basic", fio_32k_rand_basic, init_disk, 0, dev_path);
    register_test("fio_96k_rand_basic", fio_96k_rand_basic, init_disk, 0, dev_path);


    register_test("fio_4k_qd1_rand_basic", fio_4k_qd1_rand_basic, init_disk, 0, dev_path);
    //register_test("fio_48k_seq_basic", fio_48k_seq_basic, init_disk, 0, dev_path);
    register_test("fio_4k_seq_basic_unalign", fio_4k_seq_basic_unalign, init_disk, 0, dev_path);
    //register_test("fio_64k_seq_basic", fio_64k_seq_basic, init_disk, 0, dev_path);

    return 0;
}

