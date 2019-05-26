#ifndef _NVM_AER_H
#define _NVM_AER_H
#include <linux/types.h>
#include <unistd.h>

/* show the baidu log page */
/* param- err_log: baidu spec log*/
void show_baidu_log(struct nvm_log_page *err_log );

/*initialize the aer completion block*/
/* ret : 0 means initialize fail - 1 means initialize succeed*/
int initialize_aer_cp_block( );

/* get the async evnet request completion notification*/
/* param- timeout: wait for timeout(ms), negative value means the infinite wait*/
/* ret : -1 means have not recv, 0 means have recv the async aer completion */
/* reslut: return the completion Command specific status value (Completion Queue Entry Dword 0)*/
int recv_aer_cp(long timeout, __u32 *result );

/*close the aer completion block*/
/* ret : -1 means close fail- 0 means close succeed*/
int close_aer_cp_block();

int nvm_aer_open(struct nvm_dev *dev);
int nvm_aer_close(struct nvm_dev *dev);
#endif //_NVM_AER_H
