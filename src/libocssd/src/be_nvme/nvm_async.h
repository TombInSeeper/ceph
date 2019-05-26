//
// Created by user on 18-10-11.
//

#ifndef OCSSD_BAIDU_NVM_ASYNC_H
#define OCSSD_BAIDU_NVM_ASYNC_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

/**
 * Opaque asynchronous context as returned by 'nvm_async_init'
 *
 * @see nvm_async_init
 * @see nvm_async_term
 *
 * @struct nvm_async_ctx
 */
struct nvm_async_ctx;

/**
 * Forward declaration, see nvm_ret further down
 */
struct nvm_ret;

struct nvm_async_event;
/**
 * Signature of function used with asynchronous callbacks.
 */
typedef void (*nvm_async_cb)(struct nvm_async_event *evt);

/**
 * IO ASYNC command context per IO, setup this struct inside nvm_ret per call to
 * the nvm_cmd IO functions and set the CMD option NVM_CMD_ASYNC.
 *
 * @see nvm_async_cb
 * @see nvm_async_init
 */
typedef struct nvm_async_event {
    nvm_async_cb cb;        ///< User provided callback function
    void *cb_arg;            ///< User provided callback arguments
    uint64_t result;
}nvm_async_event_t;

/**
 * Allocate an asynchronous context for command submission of the given depth
 * for submission of commands to the given device
 *
 * @param dev Associated device
 * @param depth Maximum iodepth / qdepth, maximum number of outstanding commands
 * of the returned context
 * @param flags TBD
 */
struct nvm_async_ctx *nvm_async_init(int fd, uint32_t depth);

/**
 * Get the I/O depth of the context.
 *
 * @param ctx Asynchronous context
 */
uint32_t nvm_async_get_depth(struct nvm_async_ctx *ctx);

/**
 * Get the number of outstanding I/O.
 *
 * @param ctx Asynchronous context
 */
uint32_t nvm_async_get_outstanding(struct nvm_async_ctx *ctx);

/**
 * Tear down the given ASYNC context
 */
int nvm_async_term(struct nvm_async_ctx *ctx);

int nvm_async_wait_events(struct nvm_async_ctx *ctx, uint32_t min, uint32_t max, struct timespec *timeout);
/**
 * Process completions from the given ASYNC context.
 *
 * Set process 'max' to limit number of completions, 0 means no max.
 *
 * @return On success, number of completions processed, may be 0. On error, -1
 * is returned and errno set to indicate the error.
 */
int nvm_async_poke(struct nvm_async_ctx *ctx, uint32_t max);

/**
 * Wait for completion of all outstanding commands in the given 'ctx'
 *
 * @return On success, number of completions processed, may be 0. On error, -1
 * is returned and errno set to indicate the error.
 */
int nvm_async_wait(struct nvm_async_ctx *ctx);

int nvm_async_pwrite(struct nvm_async_ctx *ctx, void *data, size_t len, off_t offset, struct nvm_async_event *event);

int nvm_async_pread(struct nvm_async_ctx *ctx, void *data, size_t len, off_t offset, struct nvm_async_event *event);

#endif //OCSSD_BAIDU_NVM_ASYNC_H
