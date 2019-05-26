#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "nvm_dev.h"
#include "nvm_cmb.h"
#include <time.h>
#include <getopt.h>
#include <unistd.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../include/nvm_debug.h"
#include "test_list.h"
//#include <kvnvme-API.h>
//#include "test_intf.c"


static struct nvm_dev *pm_dev;
FILE *fp;


struct timespec mStart;
struct timespec mStop;
static void updateTime()
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &mStart);	
	mStop  = mStart;
}

static long long elapseMs()
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &mStop);	
	long  long diffInNanos = (mStop.tv_sec-mStart.tv_sec)*(long long)1e9 + (mStop.tv_nsec-mStart.tv_nsec);
	return diffInNanos;
}

struct timespec mStartEvery;
struct timespec mStopEvery;
void updateTimeEvery()
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &mStartEvery);	
	mStopEvery  = mStartEvery;
}

long long elapseMsEvery()
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &mStopEvery);	
	long  long diffInNanos = (mStopEvery.tv_sec-mStartEvery.tv_sec)*(long long)1e9 + (mStopEvery.tv_nsec-mStartEvery.tv_nsec);
	return diffInNanos;
}
void print_out_elapse_time()
{
	LOG_INFO("[DBG]:The elapse Time is %lld ns or %lld us\n", elapseMs(), elapseMs()/1000);	
}

int create_result_file(char *testMode)
{
	struct tm *t;
	time_t tt;
	time(&tt);
	struct stat st = {0};	
	if (stat("./cmb_result", &st) == -1) {
	    mkdir("./cmb_result", 0777);
	}
	t = localtime(&tt);
	char fileNameBuf[128];
	sprintf(fileNameBuf,"./cmb_result/test-%s%4d%02d%02d%02d:%02d:%02d.csv",testMode, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	fp = fopen(fileNameBuf, "w+");
	if(fp == NULL)
	{
		LOG_DEBUG("open csv failed = %s", fileNameBuf);
		return -1;
	}
	fprintf(fp,"%s,%s,%s,%s,%s,%s\n", "Granularity(Byte)","loopTime","transferAmount(Byte)", "averageTime(us)", "totalTime(us)", "bandwidth(GB/s)");
	return 0;
}

int close_result_file()
{
	fclose(fp);
	return 0;
}

/*mode = 0 ： read test*/
/*mode = 1 :  write test*/
/*other undefine*/
void test_cmb_transfer_ability(int mode,  int test_time)
{
	//test granurity
	switch(mode)
	{
		case 0:
			LOG_INFO("[INFO]:read transfer ability test\n");	
			break;
		case 1:
			LOG_INFO("[INFO]:write transfer ability test\n");	
			break;
		default:
			break;
	}
	
	
	double transferAmount = 0;
	double bandwidth = 0;
	long   offset = 0;
	char* hostBuf = aligned_alloc(4096, 2*1024*1024*sizeof(char));
	memset(hostBuf, 0x00, 2*1024*1024);
	long testTotalTime = 0;
	double averageTransferTime =0;

	//for(int i=2; i< 4; i++)
	for(int i=0; i< 8; i++)
	{
		int length = 8*pow(2,i);
		updateTime();		
		testTotalTime = test_time;
		for(int j=0; j<testTotalTime; j++)
		{
			//offset = j% 1024;
			offset = (length * j)%(1024*1024*2);
			//offset = 0;
			//printf("[DBG]: the offset is %d\n", offset);
			switch(mode)
			{
				case 0:
					nvm_read_pm(pm_dev, &hostBuf[offset], 0, length, offset, NULL);
					break;
				case 1:
					nvm_write_pm(pm_dev, &hostBuf[offset], 0, length, offset, NULL);
					break;
				default:
					break;
			}	
		}
		transferAmount = length*testTotalTime;
		bandwidth	   = transferAmount /(elapseMs());
		averageTransferTime = (double)(elapseMs()/1000)/testTotalTime;
		LOG_INFO("[INFO]: Granularity=[%d] Byte, transfer amount=[%f/%ld] Byte, transfer time=[%lld/%f] us, bandwidth=[%f] GB/s\n",length, 
				transferAmount, testTotalTime, elapseMs()/1000,averageTransferTime, bandwidth);
		fprintf(fp,"%d,%ld,%f,%f,%lld,%f\n", length,testTotalTime,transferAmount, averageTransferTime, elapseMs()/1000, bandwidth);
		}
	free(hostBuf);

//#if 0
	/*4kbyte test*/
	uint64_t *write4kByteBuf = aligned_alloc(4096, 4096*sizeof(char));
	memset(write4kByteBuf, 0x00, 4096);
	updateTime();		
	testTotalTime = test_time;
	for(int j=0; j<testTotalTime; j++)
	{
		offset = j*4096 % (2*1024*1024);
		switch(mode)
		{
		case 0:
				//updateTimeEvery();
				nvm_read_pm(pm_dev, write4kByteBuf, 0, 4096, offset, NULL);
				//printf("[DBG]:j = %d, time = %lld\n",j, elapseMsEvery()/1000);
				break;
		case 1:
				nvm_write_pm(pm_dev, write4kByteBuf, 0, 4096, offset, NULL);
				break;
			default:
				break;
		}	
	}
	
	transferAmount = 4*testTotalTime;
	bandwidth	   = transferAmount /(elapseMs())*1024;
	averageTransferTime = (double)(elapseMs()/1000)/testTotalTime;
	LOG_INFO("[INFO]: Granularity=[%d] Byte, transfer amount=[%f/%ld] KByte, transfer time=[%lld/%f] us, bandwidth=[%f] GB/s\n",4096, 
				transferAmount, testTotalTime, elapseMs()/1000,averageTransferTime, bandwidth);
	fprintf(fp,"%d,%ld,%f,%f,%lld,%f\n", 4096,testTotalTime,transferAmount, averageTransferTime, elapseMs()/1000, bandwidth);
	free(write4kByteBuf);
	/*end 4kbyte test*/
	//usleep(5000000);

	/*128kbyte test*/
	uint64_t *write128kByteBuf = aligned_alloc(4096, 128*1024*sizeof(char));
	if(!write128kByteBuf)
	{
		LOG_DEBUG("[DBG]:error allocated\n");
	}
	memset(write128kByteBuf, 0x00, 128*1024);
	updateTime();		
	testTotalTime = test_time/100;
	//testTotalTime = 10;
	for(int j=0; j<testTotalTime; j++)
	{
		//offset = j% 512;
		offset = j*128*1024 % (2*1024*1024);
		switch(mode)
		{
			case 0:
				//updateTimeEvery();
				nvm_read_pm(pm_dev, write128kByteBuf, 0, 128*1024, offset,NULL);
				//printf("[DBG]:j = %d, time = %lld\n",j, elapseMsEvery()/1000);
				break;
			case 1:
				nvm_write_pm(pm_dev, write128kByteBuf, 0, 128*1024, offset, NULL);
				break;
			default:
				break;
		}		
	}
	/*over large*/
	transferAmount = 128*testTotalTime;
	bandwidth	   = transferAmount /(elapseMs())*1024;
	averageTransferTime = (double)(elapseMs()/1000)/testTotalTime;
	LOG_INFO("[INFO]: Granularity=[%d] KByte, transfer amount=[%f/%ld] KByte, transfer time=[%lld/%f] us, bandwidth=[%f] GB/s\n",128, 
				transferAmount, testTotalTime, elapseMs()/1000,averageTransferTime, bandwidth);
	fprintf(fp,"%dK,%ld,%f,%f,%lld,%f\n", 128,testTotalTime,transferAmount, averageTransferTime, elapseMs()/1000, bandwidth);
	free(write128kByteBuf);
	/*end 128kbyte test*/

	/*512kbyte test*/
	//usleep(5000000);
	uint64_t *write512kByteBuf = aligned_alloc(4096, 512*1024*sizeof(char));
	if(!write512kByteBuf)
	{
		LOG_DEBUG("[DBG]:error allocated\n");
	}
	memset(write512kByteBuf, 0x00, 512*1024);
	updateTime();		
	testTotalTime = test_time/100;
	for(int j=0; j<testTotalTime; j++)
	{
		//offset = j% 512;
		offset = j*512*1024 % (2*1024*1024);
		switch(mode)
		{
			case 0:
				//updateTimeEvery();
				nvm_read_pm(pm_dev, write512kByteBuf, 0, 512*1024, offset,NULL);
				//printf("[DBG]:j = %d, time = %lld\n",j, elapseMsEvery()/1000);
				break;
			case 1:
				nvm_write_pm(pm_dev, write512kByteBuf, 0, 512*1024, offset, NULL);
				break;
			default:
				break;
		}
	}
	/*over large*/
	transferAmount = 512*testTotalTime;
	bandwidth	   = transferAmount /(elapseMs())*1024;
	averageTransferTime = (double)(elapseMs()/1000)/testTotalTime;
	LOG_INFO("[INFO]: Granularity=[%d] KByte, transfer amount=[%f/%ld] KByte, transfer time=[%lld/%f] us, bandwidth=[%f] GB/s\n",512, 
				transferAmount, testTotalTime, elapseMs()/1000,averageTransferTime, bandwidth);
	fprintf(fp,"%dK,%ld,%f,%f,%lld,%f\n", 512,testTotalTime,transferAmount, averageTransferTime, elapseMs()/1000, bandwidth);
	free(write512kByteBuf);

	assert_true(true);
	/*end 512kbyte test*/
//#endif
}

void print_CMB_info(char *buf, unsigned int size)
{
	LOG_INFO("[INFO]: print out the buf info\n");
	for(unsigned int i=0; i<(size/1024); i++)
	{
		for(int j=0; j< 1024; j++)
		{
			LOG_INFO("%02x", buf[512*i+j]);
		}
		LOG_INFO("\n");	
	}
}

void test_read_write_cmb(void **state)
{
	(void)state;
	char *bufForWrite = malloc(2*1024*1024*sizeof(char));
	char *bufForRead  = malloc(2*1024*1024*sizeof(char));
	assert_non_null(bufForWrite);
	assert_non_null(bufForRead);
	memset(bufForWrite, 0x00, 2*1024*1024);
	memset(bufForRead, 0x00, 2*1024*1024);
	/*generate random data*/
	for(uint64_t i= 0; i< 2*1024*1024; i++)
	{
		char randomData = rand()%128;
		//printf("the random data is %d\n",(int)randomData);
		bufForWrite[i] = randomData;
	}
	LOG_INFO("[INFO]: write the test data to cmb\n");
	nvm_write_pm(pm_dev, bufForWrite, 0,  2*1024*1024, 0, NULL);
	LOG_INFO("[INFO]: read the test data from cmb\n");
	nvm_read_pm(pm_dev, bufForRead, 0, 2*1024*1024, 0,  NULL);
	LOG_INFO("[INFO]: compare the read and write buf\n");
	for(uint64_t i= 0; i< 2*1024*1024;i++)
	{
		if(memcmp(&bufForWrite[i], &bufForRead[i],1)==0)
		{
			//do nothing
		}
		else
		{
			LOG_INFO("the buf[%ld] For Write is %d, otherwise the buf[%ld] for read is %d\n", i,*(&bufForWrite[i]),i,*(&bufForRead[i]));
			LOG_INFO("[DBG]: the buf[%ld] is not the same\n",i);
			free(bufForWrite);
			free(bufForRead);
			assert_true(false);
			return;
		}
	}
	assert_true(true);
	LOG_INFO("[INFO]: succeed!!! read and write buf are the same\n");
	free(bufForWrite);
	free(bufForRead);
}

void test_read_write_at_random_granularity(void **state)
{
	(void)state;
	char *bufForWrite = malloc(2*1024*1024*sizeof(char));
	char *bufForRead  = malloc(2*1024*1024*sizeof(char));
	assert_non_null(bufForWrite);
	assert_non_null(bufForRead);

	int granurity_random_data;
	uint64_t length_control	= 0;
	memset(bufForWrite, 0x00, 2*1024*1024);
	memset(bufForRead, 0x00, 2*1024*1024);
	/*generate random data*/
	for(uint64_t i= 0; i< 2*1024*1024; i++)
	{
		char randomData = rand()%256;
		//printf("the random data is %d\n",(int)randomData);
		bufForWrite[i] = randomData;
	}
	LOG_INFO("[INFO]: write the test data to cmb\n");

	while(true)
	{
		granurity_random_data = rand()%1024;
		if(length_control + granurity_random_data <= 2*1024*1024)
		{
			nvm_write_pm(pm_dev, &bufForWrite[length_control], 0, granurity_random_data,  length_control, NULL);
			length_control = granurity_random_data + length_control;
		}
		else
		{
			//do nothing
			break;
		}
	}
	//judge the cmb have been write completely, if not, write again
	if(length_control < 2*1024*1024)
	{
		int need_to_write_length = 2*1024*1024 - length_control;
		nvm_write_pm(pm_dev, &bufForWrite[length_control], 0, need_to_write_length,  length_control, NULL);
	}
	else
	{
		//do nothing no need to write
	}

	nvm_write_pm(pm_dev, bufForWrite, 0,  2*1024*1024, 0, NULL);
	LOG_INFO("[INFO]: read the test data from cmb\n");
	nvm_read_pm(pm_dev, bufForRead, 0, 2*1024*1024, 0,  NULL);
	LOG_INFO("[INFO]: compare the read and write buf\n");

	for(uint64_t i= 0; i< 2*1024*1024;i++)
	{
		if(memcmp(&bufForWrite[i], &bufForRead[i],1)==0)
		{
			//do nothing
		}
		else
		{
			LOG_INFO("the buf[%ld] For Write is %d, while the buf[%ld] for read is %d\n", i,*(&bufForWrite[i]),i,*(&bufForRead[i]));
			LOG_INFO("[DBG]: the buf[%ld] is not the same\n",i);
			free(bufForWrite);
			free(bufForRead);
			assert_true(false);
			return;
		}
	}
	assert_true(true);
	LOG_INFO("[INFO]: succeed!!! read and write buf are the same\n");
	free(bufForWrite);
	free(bufForRead);
}


void test_read_write_with_reset(void **state)
{
	(void)state;
	char *bufForWrite = malloc(2*1024*1024*sizeof(char));
	char *bufForRead  = malloc(2*1024*1024*sizeof(char));
	assert_non_null(bufForWrite);
	assert_non_null(bufForRead);

	int granurity_random_data;
	uint64_t length_control	= 0;
	memset(bufForWrite, 0x00, 2*1024*1024);
	memset(bufForRead, 0x00, 2*1024*1024);
	/*generate random data*/
	for(uint64_t i= 0; i< 2*1024*1024; i++)
	{
		char randomData = rand()%256;
		//printf("the random data is %d\n",(int)randomData);
		bufForWrite[i] = randomData;
	}
	LOG_INFO("[INFO]: write the test data to cmb\n");


	while(true)
	{
		granurity_random_data = rand()%1024;
		if(length_control + granurity_random_data <= 2*1024*1024)
		{
			nvm_write_pm(pm_dev, &bufForWrite[length_control], 0, granurity_random_data,  length_control, NULL);
			length_control = granurity_random_data + length_control;
		}
		else
		{
			//do nothing
			break;
		}
	}
	//judge the cmb have been write completely, if not, write again
	if(length_control < 2*1024*1024)
	{
		int need_to_write_length = 2*1024*1024 - length_control;
		nvm_write_pm(pm_dev, &bufForWrite[length_control], 0, need_to_write_length,  length_control, NULL);
	}
	else
	{
		//do nothing no need to write
	}

	nvm_write_pm(pm_dev, bufForWrite, 0,  2*1024*1024, 0, NULL);
	LOG_INFO("[INFO]: read the test data from cmb\n");

	//reset the device
	int rlt = nvm_ext_reset(pm_dev);
	assert_int_equal(rlt, 0);

	nvm_read_pm(pm_dev, bufForRead, 0, 2*1024*1024, 0,  NULL);
	LOG_INFO("[INFO]: compare the read and write buf\n");

	for(uint64_t i= 0; i< 2*1024*1024;i++)
	{
		if(memcmp(&bufForWrite[i], &bufForRead[i],1)==0)
		{
			//do nothing
		}
		else
		{
			LOG_INFO("the buf[%ld] For Write is %d, while the buf[%ld] for read is %d\n", i,*(&bufForWrite[i]),i,*(&bufForRead[i]));
			LOG_INFO("[DBG]: the buf[%ld] is not the same\n",i);
			free(bufForWrite);
			free(bufForRead);
			assert_true(false);
			return;
		}
	}
	assert_true(true);
	LOG_INFO("[INFO]: succeed!!! read and write buf are the same\n");
	free(bufForWrite);
	free(bufForRead);
}

void test_read_after_write_random(void **state)
{
	(void)state;
	char *bufForWrite = malloc(2*1024*1024*sizeof(char));
	char *bufForRead  = malloc(2*1024*1024*sizeof(char));
	char *buffForCompare = malloc(2*1024*1024*sizeof(char));
	assert_non_null(bufForWrite);
	assert_non_null(bufForRead);
	assert_non_null(buffForCompare);
	memset(bufForWrite, 0x00, 2*1024*1024);
	memset(bufForRead, 0x00, 2*1024*1024);
	memset(buffForCompare, 0x00, 2*1024*1024);

	int granurity_random_data;
	long offset_random_data;
	
	//clear all the pm buffer with 0
	nvm_write_pm(pm_dev,  bufForWrite, 0, 2*1024*1024, 0, NULL);

	/*generate random data*/
	for(uint64_t i= 0; i< 2*1024*1024; i++)
	{
		char randomData = rand()%256;
		//printf("the random data is %d\n",(int)randomData);
		bufForWrite[i] = randomData;
	}
	LOG_INFO("[INFO]: write the test data to cmb\n");

	//judge the cmb have been write completely, if not, write again
	for(int i= 0; i< 10000; i++)
	{
		offset_random_data = rand() % (2*1000*1000);
		granurity_random_data = rand()%1024;
		if(offset_random_data + granurity_random_data > 2*1024*1024)
		{
			//do nothing, skip
		}
		else
		{
			memcpy(&buffForCompare[offset_random_data], &bufForWrite[offset_random_data], granurity_random_data);
			nvm_write_pm(pm_dev,  &bufForWrite[offset_random_data], 0,  granurity_random_data, offset_random_data, NULL);
		}
	}

	LOG_INFO("[INFO]: read the test data from cmb\n");

	//reset the device
	//int rlt = nvm_ext_reset(pm_dev);
	//assert_int_equal(rlt, 0);

	nvm_read_pm(pm_dev, bufForRead, 0, 2*1024*1024, 0,  NULL);
	LOG_INFO("[INFO]: compare the read and write buf\n");

	for(uint64_t i= 0; i< 2*1024*1024;i++)
	{
		if(memcmp(&buffForCompare[i], &bufForRead[i],1)==0)
		{
			//do nothing
		}
		else
		{
			LOG_INFO("the buf[%ld] For Write is %d, while the buf[%ld] for read is %d\n", i,*(&bufForWrite[i]),i,*(&bufForRead[i]));
			LOG_INFO("[DBG]: the buf[%ld] is not the same\n",i);
			free(bufForWrite);
			free(bufForRead);
			assert_true(false);
			return;
		}
	}
	assert_true(true);
	LOG_INFO("[INFO]: succeed!!! read and write buf are the same\n");
	free(bufForWrite);
	free(bufForRead);
}

void test_read_transfer_ability(void **state)
{
	(void)state;
	char testMode[64];
	memset(testMode, 0x00, 64);
	memcpy(testMode, "read", strlen("read"));
	create_result_file(testMode);
	test_cmb_transfer_ability(0, 1000);
	close_result_file();
}

void test_write_transfer_ability(void **state)
{
	(void)state;
	char testMode[64];
	memset(testMode, 0x00, 64);
	memcpy(testMode, "write", strlen("write"));	
	create_result_file(testMode);
	test_cmb_transfer_ability(1, 100000);
	close_result_file();
}

int readFlag;
int writeFlag;
static const struct option long_option[]={
	{"device",required_argument, NULL, 'd'},
	{"read",no_argument, &readFlag, 'r'},
	{"write",no_argument, &writeFlag, 'w'},
	{NULL,0, NULL, 0}
};


//static char *dev_path = "/dev/nvme0n1";
char g_dev_path[1024];

static int setup(void **state)
{
    struct nvm_dev *dev = nvm_dev_open(g_dev_path);
    assert_non_null(dev);
	pm_dev = dev;
	assert_non_null(pm_dev);
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

int unit_cmb(char *dev_path)
{
    int argc = 0;
    char **argv = NULL;
    strcpy(g_dev_path, dev_path);
	//int err = 0;
	LOG_INFO("test start\n");
	char pm_buf[4096];
    sprintf(pm_buf, "/sys/block/%sn%d/device/device/resource0", "nvme0", 1);

	//generate radom seed
    srand((unsigned int)time(NULL));
	/*option parse*/
	int opt=0;
	while((opt = getopt_long(argc, argv, "d:w:l",long_option,NULL)) != -1)
	{
		switch(opt)
		{
			case 0: break;
			case 'd':
				LOG_INFO("[INFO]:the dev path is %s\n",optarg);
				memcpy(pm_buf, optarg, strlen(optarg));
				break;
			case 'w':
				LOG_INFO("[INFO]:write test\n");
				break;
			case  'l':
				LOG_INFO("[INFO]:read test\n");
				break;				
			default:
				break;
		}
	}
	

	/*create the csv file for plot*/
	LOG_INFO("open %s\n", pm_buf);

/*
	const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_read_write_cmb),
            cmocka_unit_test(test_read_transfer_ablity),
            cmocka_unit_test(test_write_transfer_ablity),
			cmocka_unit_test(test_read_write_at_random_granularity),
			cmocka_unit_test(test_read_write_with_reset),
			cmocka_unit_test(test_read_after_write_random),
    };

	const struct CMUnitTest tests_read[] = {
            cmocka_unit_test(test_read_write_cmb),
            cmocka_unit_test(test_read_transfer_ablity),
    };

	const struct CMUnitTest tests_write[] = {
            cmocka_unit_test(test_read_write_cmb),
            cmocka_unit_test(test_write_transfer_ablity),
    };

	if(readFlag)
	{
		return cmocka_run_group_tests(tests_read, setup, teardown);	
	}
	else if(writeFlag)
	{
		return cmocka_run_group_tests(tests_write, setup, teardown);		
	}
	else
	{
		return cmocka_run_group_tests(tests_all, setup, teardown);	
	}
*/

    register_test("cmb_read_write_basic", test_read_write_cmb, setup, teardown, dev_path);
	//cmb write somedata, then reset device , read the cmb data , compare the read data with the write data, assert which is same
    register_test("cmb_read_write_with_reset", test_read_write_with_reset, setup, teardown, dev_path);
    register_test("cmb_read_after_write_random", test_read_after_write_random, setup, teardown, dev_path);
    register_test("cmb_read_write_at_random_granularity", test_read_write_at_random_granularity, setup, teardown, dev_path);
    register_test("cmb_read_transfer_ability", test_read_transfer_ability, setup, teardown, dev_path);
    register_test("cmb_write_transfer_ability", test_write_transfer_ability, setup, teardown, dev_path);

	return 0;
}
