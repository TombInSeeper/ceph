#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include "test_list.h"
#include "liblightnvm.h"

static void fio_workload_max_thread_read(void **state)
{
    char *path = (char *)*state;
    FILE *f = fopen("fio_workload_max_thread_read.fio", "w");
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
size=1536m\n\
verify_pattern=0xa2\n\
do_verify=0\n\
\n\
[write2]\n\
rw=write\n\
bs=8k\n\
ba=8k\n\
offset=30720m\n\
size=1536m\n\
verify_pattern=0xb3\n\
do_verify=0\n\
\n\
[write3]\n\
rw=write\n\
bs=16k\n\
ba=16k\n\
offset=61440m\n\
size=1536m\n\
verify_pattern=0xc4\n\
do_verify=0\n\
\n\
[write4]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=92160m\n\
size=1536m\n\
verify_pattern=0xd5\n\
do_verify=0\n\
\n\
[bwrite1]\n\
stonewall\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=107520m\n\
size=15360m\n\
verify_pattern=0x4f\n\
do_verify=0\n\
\n\
[bwrite2]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=122880m\n\
size=15360m\n\
verify_pattern=0x3d\n\
do_verify=0\n\
\n\
[bwrite3]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=138240m\n\
size=15360m\n\
verify_pattern=0x81\n\
do_verify=0\n\
\n\
[bwrite4]\n\
rw=write\n\
bs=32k\n\
ba=32k\n\
offset=153600m\n\
size=15360m\n\
verify_pattern=0x9c\n\
do_verify=0\n\
\n\
[global]\n\
numjobs=%d\n\
\n\
[read1]\n\
rw=randread\n\
bs=4k\n\
ba=4k\n\
offset=0m\n\
size=1536m\n\
verify_pattern=0xa2\n\
do_verify=1\n\
\n\
[read2]\n\
rw=randread\n\
bs=8k\n\
ba=8k\n\
offset=30720m\n\
size=1536m\n\
verify_pattern=0xb3\n\
do_verify=1\n\
\n\
[read3]\n\
rw=randread\n\
bs=16k\n\
ba=16k\n\
offset=61440m\n\
size=1536m\n\
verify_pattern=0xc4\n\
do_verify=1\n\
\n\
[read4]\n\
rw=randread\n\
bs=32k\n\
ba=32k\n\
offset=92160m\n\
size=1536m\n\
verify_pattern=0xd5\n\
do_verify=1\n\
", path, get_nprocs()
    );
    fclose(f);

    res = system("perl launch_fio.pl ./fio fio_workload_max_thread_read.fio");
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

    return 0;
}


int unit_fio_workload(char *dev_path)
{
    register_test("fio_workload_max_thread_read",  fio_workload_max_thread_read, init_disk, 0, dev_path);


    return 0;
}


