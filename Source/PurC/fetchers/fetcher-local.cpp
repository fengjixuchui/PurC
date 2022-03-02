/*
 * @file fetcher-local.cpp
 * @author XueShuming
 * @date 2021/11/16
 * @brief The impl for fetcher local.
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
 * but WITHOUT ANY WARRANTY
{
}
 without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "fetcher-internal.h"

#include <wtf/URL.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdlib.h>

struct pcfetcher_local {
    struct pcfetcher base;
    char* base_uri;
};

struct mime_type {
    const char* ext;
    const char* mime;
};

struct mime_type  mime_types[] = {
    { "",       "unknown" },
    { ".hvml",   "text/hvml" },
    { ".html",   "text/html" },
    { ".json",   "application/json" },
};

static const char* get_mime(const char* name)
{
    const char* ext = strrchr(name, '.');
    size_t sz = sizeof(mime_types) / sizeof(struct mime_type);
    for (size_t i = 1; i < sz; i++) {
        if (strcmp(ext, mime_types[i].ext) == 0) {
            return mime_types[i].mime;
        }
    }
    return mime_types[0].mime;
}

struct pcfetcher* pcfetcher_local_init(size_t max_conns, size_t cache_quota)
{
    struct pcfetcher_local* local = (struct pcfetcher_local*)malloc(
            sizeof(struct pcfetcher_local));
    if (local == NULL) {
        return NULL;
    }

    struct pcfetcher* fetcher = (struct pcfetcher*) local;
    fetcher->max_conns = max_conns;
    fetcher->cache_quota = cache_quota;
    fetcher->init = pcfetcher_local_init;
    fetcher->term = pcfetcher_local_term;
    fetcher->set_base_url = pcfetcher_local_set_base_url;
    fetcher->cookie_set = pcfetcher_cookie_local_set;
    fetcher->cookie_get = pcfetcher_cookie_local_get;
    fetcher->cookie_remove = pcfetcher_cookie_loccal_remove;
    fetcher->request_async = pcfetcher_local_request_async;
    fetcher->request_sync = pcfetcher_local_request_sync;
    fetcher->check_response = pcfetcher_local_check_response;

    local->base_uri = NULL;

    return fetcher;
}

int pcfetcher_local_term(struct pcfetcher* fetcher)
{
    if (!fetcher) {
        return 0;
    }

    struct pcfetcher_local* local = (struct pcfetcher_local*)fetcher;
    if (local->base_uri) {
        free(local->base_uri);
    }
    free(local);
    return 0;
}

const char* pcfetcher_local_set_base_url(struct pcfetcher* fetcher,
        const char* base_url)
{
    if (!fetcher) {
        return NULL;
    }

    struct pcfetcher_local* local = (struct pcfetcher_local*)fetcher;
    if (local->base_uri) {
        free(local->base_uri);
    }

    if (base_url == NULL) {
        local->base_uri = NULL;
        return NULL;
    }

    local->base_uri = strdup(base_url);
    return local->base_uri;
}

void pcfetcher_cookie_local_set(struct pcfetcher* fetcher,
        const char* domain, const char* path, const char* name,
        const char* content, time_t expire_time, bool secure)
{
    UNUSED_PARAM(fetcher);
    UNUSED_PARAM(domain);
    UNUSED_PARAM(path);
    UNUSED_PARAM(name);
    UNUSED_PARAM(content);
    UNUSED_PARAM(expire_time);
    UNUSED_PARAM(secure);
}

const char* pcfetcher_cookie_local_get(struct pcfetcher* fetcher,
        const char* domain, const char* path, const char* name,
        time_t *expire, bool *secure)
{
    UNUSED_PARAM(fetcher);
    UNUSED_PARAM(domain);
    UNUSED_PARAM(path);
    UNUSED_PARAM(name);
    UNUSED_PARAM(expire);
    UNUSED_PARAM(secure);
    return NULL;
}

const char* pcfetcher_cookie_loccal_remove(struct pcfetcher* fetcher,
        const char* domain, const char* path, const char* name)
{
    UNUSED_PARAM(fetcher);
    UNUSED_PARAM(domain);
    UNUSED_PARAM(path);
    UNUSED_PARAM(name);
    return NULL;
}

purc_variant_t pcfetcher_local_request_async(
        struct pcfetcher* fetcher,
        const char* url,
        enum pcfetcher_request_method method,
        purc_variant_t params,
        uint32_t timeout,
        response_handler handler,
        void* ctxt)
{
    UNUSED_PARAM(fetcher);
    UNUSED_PARAM(url);
    UNUSED_PARAM(method);
    UNUSED_PARAM(params);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(handler);
    UNUSED_PARAM(ctxt);

    if (!fetcher || !url || !handler) {
        return PURC_VARIANT_INVALID;
    }

    struct pcfetcher_resp_header header;
    purc_rwstream_t rws = pcfetcher_local_request_sync(fetcher, url, method,
            params, timeout, &header);

    if (rws) {
        purc_variant_t req_id = purc_variant_make_string(url, false);
        handler(req_id, ctxt, &header, rws);
        if (header.mime_type) {
            free(header.mime_type);
        }
        return req_id;
    }
    else {
        header.ret_code = 404;
        handler(PURC_VARIANT_INVALID, ctxt, &header, NULL);
        return PURC_VARIANT_INVALID;
    }

    return PURC_VARIANT_INVALID;
}

off_t filesize(const char* filename)
{
    struct stat statbuf;
    stat(filename,&statbuf);
    return statbuf.st_size;
}

purc_rwstream_t pcfetcher_local_request_sync(
        struct pcfetcher* fetcher,
        const char* url,
        enum pcfetcher_request_method method,
        purc_variant_t params,
        uint32_t timeout,
        struct pcfetcher_resp_header *resp_header)
{
    UNUSED_PARAM(fetcher);
    UNUSED_PARAM(url);
    UNUSED_PARAM(method);
    UNUSED_PARAM(params);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(resp_header);

    if (!fetcher || !url) {
        return NULL;
    }
    struct pcfetcher_local* local = (struct pcfetcher_local*)fetcher;
    String uri;
    if (local->base_uri) {
        uri.append(local->base_uri);
    }
    uri.append(url);
    WTF::URL wurl(URL(), uri);
    if (!wurl.isLocalFile()) {
        resp_header->ret_code = 404;
        resp_header->sz_resp = 0;
        resp_header->mime_type = NULL;
        return NULL;
    }

    const StringView path = wurl.path();
    const CString& cpath = path.utf8();

    const char* file = cpath.data();

    purc_rwstream_t rws = purc_rwstream_new_from_file(file, "r");
    if (rws && resp_header) {
        resp_header->ret_code = 200;
        resp_header->sz_resp = filesize(file);
        resp_header->mime_type = strdup(get_mime(file));
        return rws;
    }

    return NULL;
}


int pcfetcher_local_check_response(struct pcfetcher* fetcher,
        uint32_t timeout_ms)
{
    UNUSED_PARAM(fetcher);
    UNUSED_PARAM(timeout_ms);
    return 0;
}


