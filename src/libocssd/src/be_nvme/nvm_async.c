//
// Created by user on 18-10-11.
//
#include "nvm_dev.h"
#include <libaio.h>
#include <errno.h>
#include <stdio.h>
#include "libnvm_full.h"
#include <error.h>
#include <nvm_debug.h>
#include <liblightnvm.h>
#include <pthread.h>
#include "nvm_async.h"
#include <mqueue.h>

#define NVM_BE_LBD_ASYNC_DEFAULT_IODEPTH 512
//#define AIO_ALIGN

struct nvm_async_ctx {
    int fd;
    uint32_t depth;        ///< IO depth of the ASYNC CTX
    volatile uint32_t outstanding;    ///< Outstanding IO on the ASYNC CTX
    io_context_t aio_ctx;
    struct io_event *aio_events;
    struct iocb **iocbs;
    /** mutex for iocb */
    pthread_mutex_t mutex;

    pthread_t pthread_write;

    mqd_t mqd;

    uint32_t event_max;
    uint32_t event_min;
};

void debug_log(char * data)
{
    char file[128];
    sprintf(file, "async_%d.log", getpid());
    FILE *fd = fopen(file, "a+");
    fprintf(fd, "%s\n", data);
    fclose(fd);
}

static void *thread_write(void *arg);

struct nvm_async_ctx *nvm_async_init(int fd, uint32_t depth)
{
    struct nvm_async_ctx *ctx = calloc(1, sizeof(*ctx));
    int err;

    if (!depth) {
        depth = NVM_BE_LBD_ASYNC_DEFAULT_IODEPTH;
    }

    ctx->fd = fd;
    ctx->depth = depth;

    ctx->aio_events = calloc(depth, sizeof(struct io_event));
    ctx->iocbs = calloc(depth, sizeof(struct iocb *));

    for (unsigned int i = 0; i < depth; i++) {
        ctx->iocbs[i] = calloc(1, sizeof(struct iocb));
    }

    if (0 != (err = io_queue_init(depth, &ctx->aio_ctx))) {
        LOG_ERROR("io_queue_init=%d", err);
        errno = -err;
        return NULL;
    }

    ctx->event_max = 20;
    ctx->event_min = 1;

    /*char name[16];
    sprintf(name, "/nvm_mq_%05d", getpid());
    LOG_INFO("unlink %s = %d", name, mq_unlink(name));

    ctx->mqd = mq_open(name, O_RDWR | O_CREAT, 0666, NULL);
    LOG_DEBUG("mqueue open %s = %d", name, ctx->mqd);

    struct mq_attr attr;
    mq_getattr(ctx->mqd,&attr);

    attr.mq_maxmsg = depth / 2;

    mq_setattr(ctx->mqd,&attr,NULL);
    LOG_DEBUG("mqueue max depth=%lu", attr.mq_maxmsg);


    if (ctx->mqd < 0) {
        LOG_ERROR("mq_open error=%d", ctx->mqd);
        return NULL;
    }
    pthread_create(&ctx->pthread_write, NULL, thread_write, ctx);*/
    return ctx;
}

int nvm_async_term(struct nvm_async_ctx *ctx)
{
    int err;

    /*pthread_cancel(ctx->pthread_write);
    pthread_join(ctx->pthread_write, NULL);

    if (0 != mq_close(ctx->mqd)) {
        LOG_ERROR("mq_close error");
    }
    char name[16];
    sprintf(name, "/nvm_mq_%05d", getpid());
    if (mq_unlink(name) != 0)
        LOG_ERROR("mq_unlink error");*/

    for (unsigned int i = 0; i < ctx->depth; i++) {
        free(ctx->iocbs[i]);
    }

    free(ctx->aio_events);
    free(ctx->iocbs);

    if (0 != (err = io_queue_release(ctx->aio_ctx))) {
        LOG_ERROR("io_queue_init=%d", err);
        errno = -err;
        return -1;
    }

    free(ctx);
    LOG_INFO("exit");
    return 0;
}

int nvm_async_wait_events(struct nvm_async_ctx *ctx, uint32_t min, uint32_t max, struct timespec *timeout)
{
    int r;
    uint32_t nevents = 0;
    uint32_t idx = 0;
    while (ctx->outstanding) {
        r = io_getevents(ctx->aio_ctx, min, max, ctx->aio_events, timeout);
        if (r == 0)
            break;
        else if (r < 0) {
            LOG_ERROR("io_getevents=%d", r);
            return -r;
        } else
            nevents += r;

        for (int i = 0; i < r; i++, idx++) {
            struct io_event *event = &ctx->aio_events[i];
            struct nvm_async_event *evt = event->data;
            ctx->iocbs[--(ctx->outstanding)] = event->obj;
            if (evt != NULL) {
                LOG_INFO("[%d] outstand=%d, evt=%p", idx, ctx->outstanding, event->data);
                evt->result = event->res2;
                if (evt->cb != NULL)
                    evt->cb(evt);
            }
        }
    }
    return nevents;
}

int nvm_async_poke(struct nvm_async_ctx *ctx, uint32_t max_dep)
{
    struct timespec timeout = {0, 0};
    if (!max_dep) {
        max_dep = ctx->depth;
    }

    return nvm_async_wait_events(ctx, 0, max_dep, &timeout);
}

int nvm_async_wait(struct nvm_async_ctx *ctx)
{
    return nvm_async_wait_events(ctx, ctx->outstanding, ctx->depth, NULL);
}

int nvm_async_pwrite(struct nvm_async_ctx *ctx, void *data, size_t len, off_t offset, struct nvm_async_event *event)
{
    if (ctx->outstanding == ctx->depth) {
        LOG_INFO("depth=%d is fulled", ctx->depth);
        errno = EAGAIN;
        return -1;
    }

    struct iocb *iocb = ctx->iocbs[ctx->outstanding++];
    io_prep_pwrite(iocb, ctx->fd, data, len, offset);
    iocb->data = event;

    int r = io_submit(ctx->aio_ctx, 1, &iocb);
    if (r < 0) {
        LOG_ERROR("io_submit=%d", r);
        errno = EIO;
        return -1;
    }

    return 0;
}

int nvm_async_pread(struct nvm_async_ctx *ctx, void *data, size_t len, off_t offset, struct nvm_async_event *event)
{
    if (ctx->outstanding == ctx->depth) {
        LOG_INFO("depth=%d is fulled", ctx->depth);
        errno = EAGAIN;
        return -1;
    }

    struct iocb *iocb = ctx->iocbs[ctx->outstanding++];
    io_prep_pread(iocb, ctx->fd, data, len, offset);
    iocb->data = event;

    int r = io_submit(ctx->aio_ctx, 1, &iocb);
    if (r < 0) {
        errno = EIO;
        LOG_ERROR("io_submit=%d", r);
        return -1;
    }

    return 0;
}

uint32_t nvm_async_get_depth(struct nvm_async_ctx *ctx)
{
    return ctx->depth;
}

uint32_t nvm_async_get_outstanding(struct nvm_async_ctx *ctx)
{
    pthread_mutex_lock(&ctx->mutex);
    uint32_t rlt = ctx->outstanding;
    pthread_mutex_unlock(&ctx->mutex);
    return rlt;
}

#ifdef AIO_ALIGN
ssize_t nvm_addr_async_protected_write(struct nvm_dev* dev, struct cmd_ctx* ctx, uint16_t flags,
                                       int head_idx, int fake_write, int no_meta)
{
    (void) no_meta;
    (void) flags;
    (void) head_idx;
    (void) fake_write;
    struct nvm_async_ctx *aio = dev->aio;
    size_t len = ctx->naddrs*dev->geo.sector_nbytes;

    LOG_INFO("naddrs=%d", ctx->naddrs);
    for (int i = 0; i < 1; ++i)
        nvm_addr_pr_man(dev, ctx->addrs[i]);

#ifdef AIO_DEBUG
    fprintf(dev->aio->debug_fd, "0x%016lx\n", ctx->addrs[0].ppa);
#endif

    ctx->data_len = len;
    uint8_t *data_align;
    if((uint64_t )ctx->data % dev->geo.sector_nbytes != 0) {
        data_align = aligned_alloc(dev->geo.sector_nbytes, len);
        LOG_INFO("data=%p, data_align=%p", ctx->data, data_align);
        memcpy(data_align, ctx->data, len);
        ctx->private_data = data_align;
        ctx->opcode = NVM_S12_OPC_WRITE;
    } else {
        data_align = ctx->data;
        ctx->private_data = NULL;
    }

    if (aio->outstanding == aio->depth) {
        LOG_INFO("depth=%d is fulled", aio->depth);
        while (1) {
            int poke = nvm_async_poke(aio, 0);
            if (poke > 0) {
                LOG_INFO("async poke event=%d", poke);
                break;
            }
        }
    }

    struct iocb *iocb = aio->iocbs[aio->outstanding++];
    off_t offset = nvm_addr_gen2dev(dev, ctx->addrs[0]) * 512;
    io_prep_pwrite(iocb, aio->fd, data_align, len, offset);
    iocb->data = ctx;

    int r = io_submit(aio->aio_ctx, 1, &iocb);
    if (r < 0) {
        LOG_ERROR("io_submit=%d", r);
        errno = EIO;
        return -1;
    }

    return 0;
}

ssize_t nvm_addr_async_read(struct nvm_dev* dev, struct cmd_ctx* ctx, uint16_t flags, int no_meta)
{
    (void) no_meta;
    (void) flags;
    struct nvm_async_ctx *aio = dev->aio;
    size_t len = ctx->naddrs*dev->geo.sector_nbytes;

    LOG_INFO("naddrs=%d", ctx->naddrs);
    for (int i = 0; i < 1; ++i)
        nvm_addr_pr_man(dev, ctx->addrs[i]);

    ctx->data_len = len;
    uint8_t *data_align;
    if((uint64_t )ctx->data % dev->geo.sector_nbytes != 0) {
        data_align = aligned_alloc(dev->geo.sector_nbytes, len);
        LOG_INFO("data=%p, data_align=%p", ctx->data, data_align);
        ctx->private_data = data_align;
        ctx->opcode = NVM_S12_OPC_READ;
    } else {
        data_align = ctx->data;
        ctx->private_data = NULL;
    }

    if (aio->outstanding == aio->depth) {
        LOG_INFO("depth=%d is fulled", aio->depth);
        while (1) {
            if(aio->outstanding < aio->depth)
                break;
        }
    }

    struct iocb *iocb = aio->iocbs[aio->outstanding++];
    off_t offset = nvm_addr_gen2dev(dev, ctx->addrs[0]) * 512;
    io_prep_pread(iocb, aio->fd, data_align, len, offset);
    iocb->data = ctx;
//    LOG_INFO("ctx=%p", ctx);

    int r = io_submit(aio->aio_ctx, 1, &iocb);
    if (r < 0) {
        LOG_ERROR("io_submit=%d", r);
        errno = EIO;
        return -1;
    }

    return 0;
}

ssize_t nvm_get_async_cmd_event(struct nvm_dev* dev, struct result_ctx* rctx)
{
    struct nvm_async_ctx *aio = dev->aio;
    struct timespec timeout = {.tv_sec=1, .tv_nsec=0};

    uint32_t nevents = 0;
    rctx->max_count = 0;
    uint32_t idx = 0;
    int r = io_getevents(aio->aio_ctx, 1, 20, aio->aio_events, &timeout);
    if (r == 0)
        return 0;
    else if (r < 0) {
        LOG_ERROR("io_getevents=%d", r);
        return -r;
    } else
        nevents += r;

    for (int i = 0; i < r; i++, idx++) {
        struct io_event *event = &aio->aio_events[i];
        aio->iocbs[--(aio->outstanding)] = event->obj;

        struct cmd_ctx *evt = event->data;
//        LOG_INFO("[%d] evt=%p", i, event->data);
        if (evt != NULL) {
            rctx->ctx[i] = evt;
            rctx->ctx[i]->status = event->res2;
            rctx->ctx[i]->result = (uint32_t )event->res;//data len
            LOG_INFO("[%d] status=0x%lx, result=%ld", i, event->res2, event->res);
            if(evt->private_data != NULL) {
                if(evt->opcode == NVM_S12_OPC_READ) {
                    LOG_INFO("read data from %p", evt->private_data);
                    memcpy(evt->data, evt->private_data, evt->data_len);
                }
                free(evt->private_data);
                LOG_INFO("free data_align=%p", evt->private_data);
            }
        }
    }

    rctx->max_count = nevents;
    LOG_INFO("max_count=%d, outstand=%d", rctx->max_count, aio->outstanding);
    return 0;
}
#else

struct async_private {
    struct cmd_ctx *ctx;
    uint8_t *data_align;
    size_t data_len;
    bool do_write;
    off_t offset;
};

static void *thread_write(void *arg)
{
    struct nvm_async_ctx *aio = arg;
    struct async_private *obj = NULL;
    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

    struct mq_attr mqAttr;
    mq_getattr(aio->mqd, &mqAttr);
    LOG_INFO("mqAttr.mq_msgsize=%ld", mqAttr.mq_msgsize);

    while (1) {

        ssize_t rlt = mq_receive(aio->mqd, (void *) &obj, mqAttr.mq_msgsize, 0);
        if (rlt != sizeof(obj)) {
            LOG_ERROR("mq_receive=%ld", rlt);
        }

        pthread_mutex_lock(&aio->mutex);
        {
            LOG_INFO("out[%d] obj=%p, ppa=0x%016lx", aio->outstanding, obj, obj->ctx->addrs[0].ppa);
            struct iocb *iocb = aio->iocbs[aio->outstanding++];
            io_prep_pwrite(iocb, aio->fd, obj->data_align, obj->data_len, obj->offset);
            iocb->data = obj;

            int r = io_submit(aio->aio_ctx, 1, &iocb);
            if (r < 0) {
                LOG_ERROR("io_submit=%d", r);
                break;
            }
        }
        pthread_mutex_unlock(&aio->mutex);
    }

    pthread_mutex_unlock(&aio->mutex);
    LOG_INFO("Exit");
    pthread_exit(NULL);
}

ssize_t nvm_addr_async_protected_write(struct nvm_dev *dev, struct cmd_ctx *ctx, uint16_t flags,
                                       int head_idx, int fake_write, int no_meta)
{
    (void) no_meta;
    (void) flags;
    (void) head_idx;
    (void) fake_write;
    struct nvm_async_ctx *aio = dev->aio;

    struct async_private *obj = malloc(sizeof(struct async_private));
    obj->ctx = ctx;
    obj->data_len = ctx->naddrs * dev->geo.sector_nbytes;
    obj->data_align = aligned_alloc(dev->geo.sector_nbytes, obj->data_len);
    memcpy(obj->data_align, ctx->data, obj->data_len);
    obj->do_write = true;

    /*while (1) {
        uint32_t outstanding = nvm_async_get_outstanding(aio);
        if (outstanding < aio->depth)
            break;
    }*/
#if 1
    obj->offset = nvm_addr_gen2dev(dev, ctx->addrs[0]) * 512;
    LOG_RAW("write ");
    for (int i = 0; i < 1; ++i)
        nvm_addr_pr_man(dev, ctx->addrs[i]);

//    struct nvm_addr addr;
//    addr.ppa = ctx->addrs[0].ppa;
//    char log[1024]={0};
//    sprintf(log, "%s ppa=0x%016lx obj=%p ch=%ld lun=%ld blk=%ld pl=%ld pg=%ld sec=%ld",
//            __func__, ctx->addrs[0].ppa, obj,
//            addr.g.ch, addr.g.lun, addr.g.blk, addr.g.pl, addr.g.pg, addr.g.sec);
//    debug_log(log);

    ssize_t rlt = mq_send(aio->mqd, (void *) &obj, sizeof(obj), 0);
    if (rlt != 0) {
        LOG_ERROR("mq_send=%ld", rlt);
        free(obj);
        return -1;
    }
#else
    pthread_mutex_lock(&aio->mutex);
    struct iocb *iocb = aio->iocbs[aio->outstanding++];
    pthread_mutex_unlock(&aio->mutex);

    off_t offset = nvm_addr_gen2dev(dev, ctx->addrs[0]) * 512;
    io_prep_pwrite(iocb, aio->fd, obj->data_align, obj->data_len, offset);
    iocb->data = obj;

    int r = io_submit(aio->aio_ctx, 1, &iocb);
    if (r < 0) {
        LOG_ERROR("io_submit=%d", r);
        errno = EIO;
        return -1;
    }
#endif
    return 0;
}

ssize_t nvm_addr_async_read(struct nvm_dev *dev, struct cmd_ctx *ctx, uint16_t flags, int no_meta)
{
    (void) no_meta;
    (void) flags;
    struct nvm_async_ctx *aio = dev->aio;
    struct iocb *iocb;

    LOG_RAW("read ");
    for (int i = 0; i < 1; ++i)
        nvm_addr_pr_man(dev, ctx->addrs[i]);

    struct async_private *obj = malloc(sizeof(struct async_private));
    obj->ctx = ctx;
    obj->data_len = ctx->naddrs * dev->geo.sector_nbytes;
    obj->data_align = aligned_alloc(dev->geo.sector_nbytes, obj->data_len);
    obj->do_write = false;
    LOG_INFO("obj=%p, data=%p, data_align=%p, size=%ld", obj, ctx->data, obj->data_align, obj->data_len);

    pthread_mutex_lock(&aio->mutex);
    {
        uint32_t outstanding = (aio)->outstanding;
        if (outstanding >= aio->depth)
        {
            free(obj);
            pthread_mutex_unlock(&aio->mutex);
            return -1;
        }
    }
    iocb = aio->iocbs[aio->outstanding++];
    pthread_mutex_unlock(&aio->mutex);

    off_t offset = nvm_addr_gen2dev(dev, ctx->addrs[0]) * 512;
    io_prep_pread(iocb, aio->fd, obj->data_align, obj->data_len, offset);
    iocb->data = obj;

    int r = io_submit(aio->aio_ctx, 1, &iocb);
    if (r < 0) {
        LOG_ERROR("io_submit=%d", r);
        errno = EIO;
        return -1;
    }

    return 0;
}

ssize_t nvm_get_async_cmd_event(struct nvm_dev *dev, struct result_ctx *rctx)
{
    struct nvm_async_ctx *aio = dev->aio;
    struct timespec timeout = {
        .tv_sec  = 0,
        .tv_nsec = rctx->t_ns
    };
    uint32_t nevents = 0;
    rctx->max_count = 0;
    uint32_t idx = 0;

    int r = io_getevents(aio->aio_ctx, aio->event_min, aio->event_max, aio->aio_events, &timeout);
    if (r == 0) {
        goto out;
    } else if (r < 0) {
        LOG_ERROR("io_getevents=%d", r);
        return -r;
    } else
        nevents += r;

    for (uint32_t i = 0; i < nevents; i++, idx++) {
        struct io_event *event = &aio->aio_events[i];

        pthread_mutex_lock(&aio->mutex);
        aio->iocbs[--(aio->outstanding)] = event->obj;
        pthread_mutex_unlock(&aio->mutex);

        struct async_private *evt = event->data;
        {
            rctx->ctx[i] = evt->ctx;
            rctx->ctx[i]->status = event->res2;
            rctx->ctx[i]->result = (uint32_t) event->res;
            LOG_INFO("[%d] status=0x%lx, result=%ld", i, event->res2, event->res);
            if (evt->do_write == false) {
                LOG_INFO("read data from %p, len=%ld", evt->data_align, evt->data_len);
                memcpy(evt->ctx->data, evt->data_align, evt->data_len);
            }
            LOG_INFO("[%d] free obj=%p", i, evt);
            free(evt->data_align);
            free(evt);
        }

    }
    out:
    rctx->max_count = nevents;
    static uint64_t debug_cnt = 0;
    debug_cnt += nevents;
    LOG_INFO("max_count=%d, outstand=%d, cnt=%ld", rctx->max_count, aio->outstanding, debug_cnt);
    return 0;
}

#endif

/**
 * change aio event threshold
 * @param dev
 * @param max
 * @param min
 * @return
 */
int nvm_async_set_config(struct nvm_dev *dev, uint32_t max, uint32_t min)
{
    if (min <= max && max <= 20) {
        dev->aio->event_max = max;
        dev->aio->event_min = min;
        return 0;
    }
    return -1;
}
