/*
 * nvm_debug - liblightnvm debugging macros (internal)
 *
 * Copyright (C) 2015-2017 Javier Gonzáles <javier@cnexlabs.com>
 * Copyright (C) 2015-2017 Matias Bjørling <matias@cnexlabs.com>
 * Copyright (C) 2015-2017 Simon A. F. Lund <slund@cnexlabs.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __INTERNAL_NVM_DEBUG_H
#define __INTERNAL_NVM_DEBUG_H

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...) \
	SELECT_10TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
		TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_10TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10


#define LOG_RAW(...) \
    do{\
        if(LOG_LEVEL <= 1){\
            printf(__VA_ARGS__); \
            fflush(stdout);\
    }} while(0);

#define LOG_INFO(...) \
    do{\
        if(LOG_LEVEL <= 1){\
            printf("[INFO] %s: %s(%d): " FIRST(__VA_ARGS__) "\n" , \
            strrchr(__FILE__,'/')+1, __func__, __LINE__ REST(__VA_ARGS__));\
            fflush(stdout);\
    }} while(0);

#define LOG_DEBUG(...) \
    do{\
        if(LOG_LEVEL <= 2){\
            printf("[DEBUG] %s: %s(%d): " FIRST(__VA_ARGS__) "\n" , \
            strrchr(__FILE__,'/')+1, __func__, __LINE__ REST(__VA_ARGS__));\
            fflush(stdout);\
    }} while(0);

#define LOG_WARN(...) \
    do{\
        if(LOG_LEVEL <= 3){\
            printf("[WARN] %s: %s(%d): " FIRST(__VA_ARGS__) "\n" , \
            strrchr(__FILE__,'/')+1, __func__, __LINE__ REST(__VA_ARGS__));\
            fflush(stdout);\
    }} while(0);

#define LOG_ERROR(...) \
    do{\
        if(LOG_LEVEL <= 0xff){\
            printf("[ERROR] %s: %s(%d): " FIRST(__VA_ARGS__) "\n" , \
            strrchr(__FILE__,'/')+1, __func__, __LINE__ REST(__VA_ARGS__));\
            fflush(stdout);\
            exit(1);\
    }} while(0);

/**
 * Macro to suppress warnings on unused arguments.
 */
#ifdef __GNUC__
#  define NVM_UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define NVM_UNUSED(x) UNUSED_ ## x

#endif

#endif /* __INTERNAL_NVM_DEBUG_H */
