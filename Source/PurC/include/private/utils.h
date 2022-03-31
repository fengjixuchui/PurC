/*
 * @file utils.h
 * @author Vincent Wei (https://github.com/VincentWei)
 * @date 2021/07/05
 * @brief The internal utility interfaces.
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of PurC (short for Purring Cat), an HVML interpreter.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PURC_PRIVATE_UTILS_H
#define PURC_PRIVATE_UTILS_H

#include "purc-utils.h"

#include "config.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#if OS(LINUX) || OS(UNIX)
#include <limits.h>
#include <stdio.h>
#endif // OS(LINUX) || OS(UNIX)

#if OS(WINDOWS)
#   define PATH_SEP '\\'
#   define PATH_SEP_STR "\\"
#else
#   define PATH_SEP '/'
#   define PATH_SEP_STR "/"
#endif

#define IS_PATH_SEP(c) ((c) == PATH_SEP)

#define pcutils_html_whitespace(onechar, action, logic)   \
    (onechar action ' '  logic                            \
     onechar action '\t' logic                            \
     onechar action '\n' logic                            \
     onechar action '\f' logic                            \
     onechar action '\r')

static inline size_t
pcutils_power(size_t t, size_t k)
{
    size_t res = 1;

    while (k) {
        if (k & 1) {
            res *= t;
        }

        t *= t;
        k >>= 1;
    }

    return res;
}

static inline size_t
pcutils_hash_hash(const unsigned char *key, size_t key_size)
{
    size_t hash, i;

    for (hash = i = 0; i < key_size; i++) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

/*
 * calloc_a(size_t len, [void **addr, size_t len,...], NULL)
 *
 * allocate a block of memory big enough to hold multiple aligned objects.
 * the pointer to the full object (starting with the first chunk) is returned,
 * all other pointers are stored in the locations behind extra addr arguments.
 * the last argument needs to be a NULL pointer
 */
#define calloc_a(len, ...) pcutils_calloc_a(len, ##__VA_ARGS__, NULL)

#ifdef __cplusplus
extern "C" {
#endif

void pcutils_atom_init_once(void) WTF_INTERNAL;
void pcutils_atom_cleanup_once(void) WTF_INTERNAL;

void *pcutils_calloc_a(size_t len, ...) WTF_INTERNAL;

void pcutils_crc32_begin(uint32_t* crc, uint32_t init);
void pcutils_crc32_update(const void *data, size_t nr_bytes, uint32_t* crc);
void pcutils_crc32_end(uint32_t* crc, uint32_t xor_out);

#define MD5_DIGEST_SIZE          (16)

typedef struct pcutils_md5_ctxt {
    uint32_t lo, hi;
    uint32_t a, b, c, d;
    unsigned char buffer[64];
} pcutils_md5_ctxt;

void pcutils_md5_begin(pcutils_md5_ctxt *ctx);
void pcutils_md5_hash(pcutils_md5_ctxt *ctxt, const void *data, size_t length);
void pcutils_md5_end(pcutils_md5_ctxt *ctxt, unsigned char *resbuf);

/* digest should be long enough (at least 16) to store the returned digest */
void pcutils_md5digest(const char *string, unsigned char *digest);
int pcutils_md5sum(const char *file, unsigned char *md5_buf);

typedef struct pcutils_sha1_ctxt {
  uint32_t      state[5];
  uint32_t      count[2];
  uint8_t       buffer[64];
} pcutils_sha1_ctxt;

#define SHA1_DIGEST_SIZE          (20)

void pcutils_sha1_begin(pcutils_sha1_ctxt *context);
void pcutils_sha1_hash(pcutils_sha1_ctxt *context, const void *data, size_t len);

/* digest should be long enough (at least 20) to store the returned digest */
void pcutils_sha1_end(pcutils_sha1_ctxt *context, uint8_t *digest);

/* hex must be long enough to hold the heximal characters */
void pcutils_bin2hex(const unsigned char *bin, int len, char *hex,
        bool uppercase);

/* bin must be long enough to hold the bytes.
   return the number of bytes converted, <= 0 for error */
int pcutils_hex2bin(const char *hex, unsigned char *bin);

int pcutils_parse_int32(const char *buf, size_t len, int32_t *retval);
int pcutils_parse_uint32(const char *buf, size_t len, uint32_t *retval);
int pcutils_parse_int64(const char *buf, size_t len, int64_t *retval);
int pcutils_parse_uint64(const char *buf, size_t len, uint64_t *retval);
int pcutils_parse_double(const char *buf, size_t len, double *retval);
int pcutils_parse_long_double(const char *buf, size_t len, long double *retval);

#ifdef __cplusplus
}
#endif

#include <math.h>
#include <float.h>

/* securely comparison of floating-point variables */
static inline bool pcutils_equal_doubles(double a, double b)
{
    double max_val = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= max_val * DBL_EPSILON);
}

/* securely comparison of floating-point variables */
static inline bool pcutils_equal_longdoubles(long double a, long double b)
{
    long double max_val = fabsl(a) > fabsl(b) ? fabsl(a) : fabsl(b);
    return (fabsl(a - b) <= max_val * LDBL_EPSILON);
}

#endif /* not defined PURC_PRIVATE_UTILS_H */

