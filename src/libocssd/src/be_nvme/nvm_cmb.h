#ifndef __INTERNAL_NVM_CMB_H
#define __INTERNAL_NVM_CMB_H
#include "nvm_dev.h"

#define DEFAULT_PM_SIZE   512*4096
#define DEFAULT_PM_OFFSET 256*4096
#define MAX_PM_SIZE 	  1024*4096

struct nvm_pm_dev{
	int fd;	
	uint8_t *buf;
	uint64_t size;
	uint64_t offset;
};

//open the nvme pm device
struct nvm_pm_dev* nvm_pm_open(const char *dev_path, uint64_t pm_size, uint64_t pm_offset);

//close the nvme pm device
int nvm_pm_close(struct nvm_pm_dev *dev);

#endif /* __INTERNAL_NVM_CMB_H */
