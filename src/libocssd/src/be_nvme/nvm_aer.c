//  The MIT License (MIT)
//  Copyright by starblaze (c)
//  Author: suntong - Create at 2018-10-22-14:57:59
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software")
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
/*  code  */
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "nvm_dev.h"
#include "nvm_aer.h"

/*for ioctl*/
#include <errno.h>
#include "nvm_debug.h"
#include "nvm_be_ioctl.h"
#include "lightnvm.h"
#include "nvme_ioctl.h"
#include "nvme.h"

/*for uevent*/
#include <stddef.h>
#include <sys/socket.h>
#include <sys/user.h>
#include <sys/un.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <poll.h>
#include <hal_timer.h>

#define HOTPLUG_BUFFER_SIZE     1024
#define HOTPLUG_NUM_ENVP        32
#define OBJECT_SIZE         	512

#define NVME_ASYNC_CP_COMPARE_WORDS  "NVME_AEN"


static bool firtst_time_initialize_cp = false;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct uevent {
    void *next;
    char buffer[HOTPLUG_BUFFER_SIZE + OBJECT_SIZE];
    char *devpath;
    char *action;
    char *envp[HOTPLUG_NUM_ENVP];
};

/*env variable always follow this format: X=Y, after execute this function*/
/*firstPart == x, secondPart ==y*/
 static int parse_env_variable(char* src, char *firstPart, char *secondPart)
{
    char *tp;
    tp = strtok(src, " = ");
    if(tp != NULL)
    {
        strcpy(firstPart, tp);
    }
    tp = strtok(NULL, " = ");
    if(tp != NULL)
    {
        strcpy(secondPart, tp);
    }
    return 0;
}

static struct uevent * alloc_uevent (void)
{
    return (struct uevent *)malloc(sizeof(struct uevent));
}

/*baidu get-log spec*/
enum{
	NVME_LOG_ID_MPF = 0xC1,
    NVME_LOG_ID_ERASE = 0xC2,
};


//setting timeout value, -1 is infinite
static int sock = 0;

static int close_socket()
{
	if(sock < 0)
	{
		return -1;
	}
	else{
		close(sock);
		return 0;
	}

}

int initialize_aer_cp_block( )
{
	//create thread for socket to lisening to kernel
    struct sockaddr_nl snl;
    int retval;
    int rcvbufsz = 128*1024;
    int rcvsz = 0;
    int rcvszsz = sizeof(rcvsz);
    unsigned int *prcvszsz = (unsigned int *)&rcvszsz;
    const int feature_on = 1;

    pthread_mutex_lock(&mutex);
    if(firtst_time_initialize_cp == false)
    {
        firtst_time_initialize_cp = true;
        pthread_mutex_unlock(&mutex);
    }
    else
    {
        /*have initialized, no need to initialize twice*/
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    memset(&snl, 0x00, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 0x01;

    sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (sock == -1) {
		LOG_ERROR("error getting socket, exit");
        return -1;
    }

    LOG_INFO("reading events from kernel");

    retval = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbufsz, sizeof(rcvbufsz));
    if (retval < 0) {
		LOG_ERROR("error setting receive buffer size for socket, exit");
        return -1;
    }

    retval = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvsz, prcvszsz);
    if (retval < 0) {
		LOG_ERROR("error setting receive buffer size for socket, exit");
        return -1;
    }
    LOG_INFO("receive buffer size for socket is %u ", rcvsz);

    /*  enable receiving of the sender credentials */
    setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &feature_on, sizeof(feature_on));

    retval = bind(sock, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl));
    if (retval < 0) {
		LOG_ERROR("bind failed, exit");
        close_socket(sock);
        return -1;
    }

    return 0;
}

/* param- timeout: wait for timeout(ms), negative value means the infinite wait*/
/* ret : -1 means have not recv, 0 means have recv the async aer completion */
/* reslut: return the completion Command specific status value (Completion Queue Entry Dword 0) */
int recv_aer_cp(long timeout, __u32 *result )
{
	int i;
    char *pos;
    size_t bufpos;
    ssize_t buflen;
    struct uevent *uev;
    char *buffer;
    struct msghdr smsg;
    struct iovec iov;
    struct cmsghdr *cmsg;
    struct ucred *cred;
    char cred_msg[CMSG_SPACE(sizeof(struct ucred))];
    static char buf[HOTPLUG_BUFFER_SIZE + OBJECT_SIZE];
    memset(buf, 0x00, sizeof(buf));
    iov.iov_base = &buf;
    iov.iov_len = sizeof(buf);
    memset (&smsg, 0x00, sizeof(struct msghdr));
    smsg.msg_iov = &iov;
    smsg.msg_iovlen = 1;
    smsg.msg_control = cred_msg;
    smsg.msg_controllen = sizeof(cred_msg);

	if(sock < 0)
	{
		LOG_ERROR("have not initilize or failed initialize the aer block");
		return -1;
	}

	/**/
	struct pollfd pfd;
	pfd.fd = sock;
	pfd.events = POLLIN;

	int ret = poll(&pfd, 1, timeout); //timeout
	switch (ret) {
	    case -1:
	        // Error
			LOG_ERROR("error happen when polling");
	        return -1;
	    case 0:
	        // Timeout
			LOG_ERROR("time out happen");
	        return -1;
	    default:
	        buflen = recvmsg(sock, &smsg, 0);
    		if (buflen < 0) {
    		    if (errno != EINTR)
				{
					LOG_ERROR("error receiving message");
				}
    		    return -1;
    		}
	}

    cmsg = CMSG_FIRSTHDR(&smsg);
    if (cmsg == NULL || cmsg->cmsg_type != SCM_CREDENTIALS) {
		LOG_ERROR("no sender credentials received, message ignored");
        return -1;
    }

    cred = (struct ucred *)CMSG_DATA(cmsg);
    if (cred->uid != 0) {
		LOG_ERROR("sender uid=%d, message ignored", cred->uid);
        return -1;
    }
    /* skip header */
    bufpos = strlen(buf) + 1;
    if (bufpos < sizeof("a@/d") || bufpos >= sizeof(buf)) {
		LOG_ERROR("invalid message length");
        return -1;
    }
    /* check message header */
    if (strstr(buf, "@/") == NULL) {
		LOG_ERROR("unrecognized message header");
        return -1;
    }
    uev = alloc_uevent();
    if (!uev) {
		LOG_ERROR("allocate uevent failed");
        return -1;
    }
    if ((size_t)buflen > sizeof(buf)-1)
	{
		buflen = sizeof(buf)-1;
	}
    /*
     * Copy the shared receive buffer contents to buffer private
     * to this uevent so we can immediately reuse the shared buffer.
     */
    memcpy(uev->buffer, buf, HOTPLUG_BUFFER_SIZE + OBJECT_SIZE);
    buffer = uev->buffer;
    buffer[buflen] = '\0';
    /* save start of payload */
    bufpos = strlen(buffer) + 1;
    /* action string */
    uev->action = buffer;
    pos = strchr(buffer, '@');
    if (!pos) {
		LOG_ERROR("bad action string '%s' ", buffer);
        return -1;
    }
    pos[0] = '\0';
    /* sysfs path */
    uev->devpath = &pos[1];
    /* hotplug events have the environment attached - reconstruct envp[] */
    for (i = 0; (bufpos < (size_t)buflen) && (i < HOTPLUG_NUM_ENVP-1); i++) {
        int keylen;
        char *key;
        key = &buffer[bufpos];
        keylen = strlen(key);
        uev->envp[i] = key;
        bufpos += keylen + 1;
    }
    uev->envp[i] = NULL;
    //printf("uevent '%s' from '%s'.\n", uev->action, uev->devpath);
    /* print payload environment */
	char  env_first_part[1024];
	char  env_second_part[1024];
	char  nvme_async_cp_compare_words[1024];
	memset(nvme_async_cp_compare_words, 0x00, sizeof(nvme_async_cp_compare_words));
	sprintf(nvme_async_cp_compare_words, "%s", NVME_ASYNC_CP_COMPARE_WORDS);

    for (i = 0; uev->envp[i] != NULL; i++)
	{
		memset(env_first_part, 0x00, sizeof(env_first_part));
		memset(env_second_part, 0x00, sizeof(env_second_part));
		//printf("%s\n", uev->envp[i]);
 		parse_env_variable(uev->envp[i], env_first_part, env_second_part);
		if(strcmp(nvme_async_cp_compare_words, env_first_part) == 0)
		{
			*result = (int)strtol(env_second_part,NULL, 16);
			return 0;
		}
	}
	return -1;
}

int close_aer_cp_block()
{
	LOG_INFO("close the aer block for recv async completion message");

	close_socket();
	return 0;
}

static inline int ioctl_baidu_admin(int fd, struct nvm_cmd *cmd, struct nvm_ret *ret)
{
    const int err = ioctl(fd, NVME_IOCTL_ADMIN_CMD, cmd);

	if(ret)
	{
		 ret->result = cmd->admin.result;
		 ret->status = (uint64_t)err;
	}
    if (err == -1)
        return err;		// Propagate errno from IOCTL error

    if (cmd->admin.result) {	// Construct errno on cmd error
        errno = EIO;
        return -1;
    }
    return 0;
}


int nvme_get_log13(int fd, __u32 nsid, __u8 log_id, __u8 lsp, __u64 lpo,
                 __u16 lsi, bool rae, __u32 data_len, void *data)
{

	struct nvm_cmd cmd = {.cdw={0}};
	cmd.admin.opcode = nvme_admin_get_log_page;
	cmd.admin.nsid  = nsid;
	cmd.admin.addr  = (__u64)(uintptr_t) data;
	cmd.admin.data_len	= data_len;
	__u32 numd = (data_len >> 2) - 1;
	__u16 numdu = numd >> 16, numdl = numd & 0xffff;

	cmd.admin.cdw10 = log_id | (numdl << 16) | (rae ? 1 << 15 : 0);
	if (lsp)
        cmd.admin.cdw10 |= lsp << 8;

	cmd.admin.cdw11 = numdu | (lsi << 16);
	cmd.admin.cdw12 = lpo;
	cmd.admin.cdw13 = (lpo >> 32);

	const int err = ioctl_baidu_admin(fd, &cmd, NULL);
	return err;
}


int nvme_get_log(int fd, __u32 nsid, __u8 log_id, bool rae,
		 __u32 data_len, void *data)
{
	void *ptr = data;
	__u32 offset = 0, xfer_len = data_len;
	int ret;

	/*
	 * 4k is the smallest possible transfer unit, so by
	 * restricting ourselves for 4k transfers we avoid having
	 * to check the MDTS value of the controller.
	 */
	do {
		xfer_len = data_len - offset;
		if (xfer_len > 4096)
			xfer_len = 4096;

		ret = nvme_get_log13(fd, nsid, log_id, NVME_NO_LOG_LSP,
				     offset, 0, rae, xfer_len, ptr);
		if (ret)
			return ret;

		offset += xfer_len;
		ptr = xfer_len + (uint8_t *)ptr;

	} while (offset < data_len);

	return 0;
}

ssize_t nvm_get_mef_log(struct nvm_dev* dev, struct nvm_log_page* log)
{
	if(!dev)
	{
		LOG_ERROR("FAILED: NULL dev ponter");
		return -1;
	}

	return nvme_get_log(dev->fd, NVME_NSID_ALL, NVME_LOG_ID_ERASE, false, sizeof(*log), log);
}

void show_baidu_log(struct nvm_log_page *log )
{
	LOG_DEBUG("Error Log show");
	LOG_DEBUG(".................");
	LOG_DEBUG("error_count  : %d", log->nppas);
	uint32_t len;
	if(log->nppas > 127)
	    len = 127;
	else
	    len = log->nppas;

	for(uint32_t i= 0; i< len; i++)
	{
        LOG_DEBUG("[%3d]: ppa=0x%016lx", i, log->ppas[i]);
	}
	LOG_DEBUG(".................");
}

struct nvm_aer_t {
    volatile bool flag_out;
    pthread_t thread_set;
//    pthread_t thread_time;
    pthread_mutex_t event_mutex;
    pthread_cond_t event_cond;
//    pthread_mutex_t time_mutex;
//    pthread_cond_t time_cond;
};

struct ext_aer_t {
    uint32_t type   :3; //02:00
    uint32_t resv1  :5; //07:03
    uint32_t info   :8; //15:08
    uint32_t log_id :8; //23:16
    uint32_t resv2  :8; //31:24
};

static void *thread_run_clean(void *arg)
{
    struct nvm_dev *dev = arg;
    uint32_t time = 50;
    LOG_INFO("start timer %ds", time);
    LOG_INFO("start pid=%ld", pthread_self());

    sleep(time);
    nvm_ext_aer_clean(dev);

    LOG_INFO("exit");
    pthread_exit(NULL);
}

static void *thread_run_set(void *arg)
{
    struct nvm_dev *dev = arg;
    struct nvm_aer_t *aer = dev->aer;
    struct nvm_ret ret;
    pthread_t thread_time;

    while (1) {
        pthread_create(&thread_time, NULL, thread_run_clean, dev);

        nvm_ext_aer_set(dev, &ret);
        struct ext_aer_t *aer_rlt = (struct ext_aer_t *) &ret.result;
        LOG_INFO("pid=%ld status=0x%lx, event, type=%d info=%d id=0x%02x",
                 pthread_self(), ret.status, aer_rlt->type, aer_rlt->info, aer_rlt->log_id);

        if (ret.status != 0) {
            LOG_ERROR("nvm_ext_aer_set exit status=0x%lx", ret.status);
            break;
        } else if (aer_rlt->type != NVME_AER_VS) {
            LOG_ERROR("nvm_ext_aer_set exit type(%d)!=NVME_AER_VS", aer_rlt->type);
            break;
        } else if (aer_rlt->info == 1 && aer->flag_out == true) {
            LOG_INFO("nvm_aer exit");
            pthread_mutex_lock(&aer->event_mutex);
            pthread_cond_signal(&aer->event_cond);
            pthread_mutex_unlock(&aer->event_mutex);
            break;
        } else if (aer_rlt->info == 0 && aer_rlt->log_id == NVME_LOG_ID_MPF) {
            pthread_mutex_lock(&aer->event_mutex);
            pthread_cond_signal(&aer->event_cond);
            pthread_mutex_unlock(&aer->event_mutex);

            LOG_INFO("fetch a nvm_aer event, type=%d info=%d id=0x%02x",
                      aer_rlt->type, aer_rlt->info, aer_rlt->log_id);
        }
        pthread_cancel(thread_time);
        usleep(1000*10);//FIXME bug
    }
    pthread_cancel(thread_time);
    LOG_INFO("exit pid=%ld", aer->thread_set);
    pthread_exit(NULL);
}

int nvm_aer_open(struct nvm_dev *dev)
{
    struct nvm_aer_t *aer = malloc(sizeof(struct nvm_aer_t));
    LOG_INFO("dev->aer=%p", aer);
    if (aer == NULL)
        return -1;

    pthread_mutex_init(&aer->event_mutex, NULL);
    pthread_cond_init(&aer->event_cond, NULL);

    dev->aer = aer;

    pthread_create(&aer->thread_set, NULL, thread_run_set, dev);
    LOG_INFO("create pid=%ld", aer->thread_set);
    usleep(100*1000);

    return 0;
}

int nvm_aer_close(struct nvm_dev *dev)
{
    struct nvm_aer_t *aer = dev->aer;

    if(aer != NULL) {
        LOG_INFO("set flag_out");
        aer->flag_out = true;
        nvm_ext_aer_clean(dev);
        pthread_cancel(aer->thread_set);

        LOG_INFO("join pid=%ld", aer->thread_set);
        pthread_join(aer->thread_set, NULL);

        pthread_mutex_destroy(&aer->event_mutex);
        pthread_cond_destroy(&aer->event_cond);
        free(aer);
        LOG_INFO("exit");
        usleep(100*1000);//FIXME can use pthread_timedjoin_np
    }
    return 0;
}

ssize_t nvm_get_async_error_event(struct nvm_dev *dev, struct nvm_log_page *log)
{
    struct nvm_aer_t *aer = dev->aer;

    pthread_mutex_lock(&aer->event_mutex);
    pthread_cond_wait(&aer->event_cond, &aer->event_mutex);
    pthread_mutex_unlock(&aer->event_mutex);

    if(aer->flag_out == true)
        return -1;

    nvme_get_log(dev->fd, NVME_NSID_ALL, NVME_LOG_ID_MPF, false, sizeof(struct nvm_log_page), log);

    for (uint32_t i = 0; i < log->nppas; ++i) {
        struct nvm_addr addr = nvm_addr_dev2gen(dev, log->ppas[i]);
        log->ppas[i] = addr.ppa;
    }
    return 0;
}
