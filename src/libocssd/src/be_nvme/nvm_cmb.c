#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "nvm_cmb.h"
#include "nvm_debug.h"
#include "nvm_dev.h"
#include "nvm_be_ioctl.h"
#include "nvme_ioctl.h"
#include <x86intrin.h>
//#define NAIVE_COPY

#ifdef __AVX2__
#define BIT256COPY
#endif

static int _nvm_read_pm(struct nvm_pm_dev* dev, void* buf, unsigned int length, unsigned int offset);

static int _nvm_write_pm(struct nvm_pm_dev* dev, void* buf, unsigned int length, unsigned int offset);

ssize_t nvm_read_pm(struct nvm_dev* dev, void* buf, uint16_t flags,
                    unsigned int length, unsigned int offset, struct nvm_ret* ret)
		
{
	(void) flags;
	(void) ret; 
	
	return _nvm_read_pm(dev->pm, buf, length, offset);
	
}

ssize_t nvm_write_pm(struct nvm_dev* dev, void* buf, uint16_t flags,
                     unsigned int length, unsigned int offset, struct nvm_ret* ret)
{
	(void) flags;
	(void) ret;

	return _nvm_write_pm(dev->pm, buf, length, offset);
}
					 


struct nvm_pm_dev* nvm_pm_open(const char *dev_path, uint64_t pm_size, uint64_t pm_offset)
{
	struct nvm_pm_dev *dev = NULL;
	uint8_t *buf;

	dev = calloc(1, sizeof(*dev));
    if (!dev) {
        LOG_DEBUG("FAILED: allocating 'struct nvm_pm_dev'");
        return NULL;	// Propagate errno from malloc
    }

	int cmb_fd = open(dev_path, O_RDWR| O_SYNC);
    LOG_INFO("open %s = %d", dev_path, cmb_fd);
	if(cmb_fd < 0)
	{
		LOG_DEBUG("FAILED: can not open pm device %s",dev_path);
		return NULL;
	}
	if(pm_size > MAX_PM_SIZE)
	{
		LOG_DEBUG("WARN: the pm_size %ld is large than %d", pm_size, MAX_PM_SIZE);
	}
	if(pm_offset > MAX_PM_SIZE)
	{
		LOG_DEBUG("WARN: the pm_offset %ld is large than %d", pm_offset, MAX_PM_SIZE);
	}
	
	buf = mmap(NULL, pm_size, PROT_READ | PROT_WRITE, MAP_SHARED, cmb_fd, pm_offset);
	if(buf == (void *)-1)
	{
		return NULL;
		LOG_DEBUG("mmap: %s", strerror(errno));
	}

	dev->fd = cmb_fd;
	dev->buf = buf;
	dev->size = pm_size;
	dev->offset = pm_offset;

    return dev;
}

int nvm_cmb_device_judge(struct nvm_pm_dev *dev)
{
	if (!dev) {
        LOG_DEBUG("FAILED: the device pointer is NULL");
        return -1;	// Propagate errno from malloc
    }
	if(dev->fd == 0)
	{
        LOG_DEBUG("FAILED: dev->cmb_fd is null pointer'");
		return -1;
	}
	return 1;
}

int nvm_pm_close(struct nvm_pm_dev *dev)
{
	//deallocate the memory
	nvm_cmb_device_judge(dev);
	munmap(dev->buf, dev->size);
    close(dev->fd);
	return 0;
}

void bit8PointerCopy(void *dst_p, void *src_p,  unsigned int length)
{
	char *dst = (char*)dst_p;
	char *src = (char*)src_p;

	while(length >= 4)
	{
		length -= 4;
    	dst[0] = src[0];
    	dst[1] = src[1];
    	dst[2] = src[2];
    	dst[3] = src[3];
    	dst += 4;
    	src += 4;
	}
	 while(length-- > 0)
   {
       *dst++ = *src++;
   }
}

#ifdef BIT256COPY
void bit256PointerCopy(void *dst_p, void *src_p, unsigned int length)
{
    __m256i *src = src_p;
    __m256i *dst = dst_p;
	for(uint32_t i = 0 ; i< length/32; i++)
	{
		*(dst+i) = *(src+i);
//		_mm256_stream_si256(&dst[i], src[i]);
	}
}
#endif

int _nvm_read_pm(struct nvm_pm_dev* dev, void* buf, 
                    unsigned int length, unsigned int offset)
{
	//printf("%s", __func__);
	int ret = nvm_cmb_device_judge(dev);
	if(ret < 0)
	{
		return -1;
	}
	if(offset + length > dev->size)
	{
        LOG_DEBUG("FAILED: the read offset and length is beyond the cmb_size");
		return -1;
	}

#ifdef NAIVE_COPY
	bit8PointerCopy(buf, &dev->buf[offset], length);
#elif defined BIT256COPY
	bit256PointerCopy(buf, &dev->buf[offset], length);
#else
	memcpy(buf, &dev->buf[offset], length);
//#if 0
//	char *dstBuf = (char *)buf;
//	char *srcBuf = &dev->buf[offset];
//	long step = length/4;
//#pragma omp parallel for num_threads(4) schedule(static, 1) 
//	for(int i =0; i<length; i=i+step)
//	{
//		//int id = omp_get_thread_num();
//		//printf("the id is %d",id);
//		memcpy(dstBuf+i, srcBuf+i,step);
//	}
#endif
	return 0;
}

inline int _nvm_write_pm(struct nvm_pm_dev* dev, void* buf,
                    unsigned int length, unsigned int offset)
{
	//printf("%s", __func__);
	int ret = nvm_cmb_device_judge(dev);
	if(ret < 0)
	{
		return -1;
	}
	if(offset + length > dev->size)
	{
        LOG_DEBUG("FAILED: the write offset and length is beyond the cmb_size");
		return -1;
	}

#ifdef NAIVE_COPY
	bit8PointerCopy(&dev->buf[offset], buf, length);
#elif defined BIT256COPY
	bit256PointerCopy(&dev->buf[offset], buf, length);
#else
	memcpy(&dev->buf[offset], buf, length);
#endif
	return 0;
}
