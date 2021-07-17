/**
 * @file variant-set.c
 * @author Xu Xiaohong (freemine)
 * @date 2021/07/09
 * @brief The API for variant.
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


#include "config.h"
#include "private/variant.h"
#include "private/list.h"
#include "private/avl.h"
#include "private/hashtable.h"
#include "private/errors.h"
#include "purc-errors.h"
#include "variant-internals.h"


#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static inline variant_set_t _pcv_set_get_data(purc_variant_t set)
{
    return (variant_set_t)set->sz_ptr[1];
}

static inline size_t _variant_set_get_extra_size(variant_set_t set)
{
    size_t extra = 0;
    if (set->unique_key) {
        extra += strlen(set->unique_key) + 1;
        extra += sizeof(*set->keynames) * set->nr_keynames;
    }
    size_t sz_record = sizeof(struct obj_node) +
        sizeof(purc_variant_t) * set->nr_keynames;
    extra += sz_record * set->objs.count;

    return extra;
}

static inline void _pcv_set_set_data(purc_variant_t set, variant_set_t data)
{
    set->sz_ptr[1]     = (uintptr_t)data;
}

static int _variant_set_keyvals_cmp (const void *k1, const void *k2, void *ptr)
{
    purc_variant_t *kvs1 = (purc_variant_t*)k1;
    purc_variant_t *kvs2 = (purc_variant_t*)k2;
    variant_set_t   set  = (variant_set_t)ptr;

    for (size_t i=0; i<set->nr_keynames; ++i) {
        int t = purc_variant_compare(kvs1[i], kvs2[i]);
        if (t)
            return t;
    }

    return 0;
}

static int _variant_set_init(variant_set_t set, const char *unique_key)
{
    pcutils_avl_init(&set->objs, _variant_set_keyvals_cmp, false, set);

    if (!unique_key || !*unique_key) {
        set->nr_keynames = 1;
        return 0;
    }

    set->unique_key = strdup(unique_key);
    if (!set->unique_key) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    char *ctx = set->unique_key;
    char *tok = strtok_r(ctx, " ", &ctx);
    size_t n = 0;
    while (tok) {
        ++n;
        tok = strtok_r(ctx, " ", &ctx);
    }

    if (n==0) {
        free(set->unique_key);
        set->unique_key = NULL;
        pcinst_set_error(PURC_ERROR_INVALID_VALUE);
        return -1;
    }

    set->keynames = (char**)calloc(n, sizeof(*set->keynames));
    if (!set->keynames) {
        free(set->unique_key);
        set->unique_key = NULL;
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    set->nr_keynames = n;

    strcpy(set->unique_key, unique_key);
    ctx = set->unique_key;
    tok = strtok_r(ctx, " ", &ctx);
    int idx = 0;
    while (tok) {
        set->keynames[idx++] = tok;
        tok = strtok_r(ctx, " ", &ctx);
    }

    return 0;
}

static int
_variant_set_cache_obj_keyval(variant_set_t set,
    purc_variant_t value, purc_variant_t *kvs)
{
    if (set->unique_key) {
        for (size_t i=0; i<set->nr_keynames; ++i) {
            purc_variant_t v;
            v = purc_variant_object_get_c(value, set->keynames[i]);
            kvs[i] = v;
        }
    } else {
        kvs[0] = value;
    }
    return 0;
}

static purc_variant_t _pcv_set_new(void)
{
    purc_variant_t set = pcvariant_get(PVT(_SET));
    if (!set) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return PURC_VARIANT_INVALID;
    }

    set->type          = PVT(_SET);
    set->flags         = PCVARIANT_FLAG_EXTRA_SIZE;

    variant_set_t ptr  = (variant_set_t)calloc(1, sizeof(*ptr));
    _pcv_set_set_data(set, ptr);

    if (!ptr) {
        pcvariant_put(set);
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return PURC_VARIANT_INVALID;
    }

    set->refc          = 1;

    // a valid empty set
    return set;
}

void pcvariant_set_release_obj(struct obj_node *obj)
{
    if (obj->obj) {
        purc_variant_unref(obj->obj);
        obj->obj = PURC_VARIANT_INVALID;
    }
    if (obj->kvs) {
        free(obj->kvs);
        obj->kvs = NULL;
    }
}

static void _variant_set_release_objs(variant_set_t set)
{
    struct obj_node *p, *n;
    avl_remove_all_elements(&set->objs, p, avl, n) {
        pcvariant_set_release_obj(p);
        free(p);
    }
}

static inline void _variant_set_release(variant_set_t set)
{
    _variant_set_release_objs(set);

    free(set->keynames);
    set->keynames = NULL;
    set->nr_keynames = 0;
    free(set->unique_key);
    set->unique_key = NULL;
}

static inline purc_variant_t*
_variant_set_create_kvs (variant_set_t set, purc_variant_t val)
{
    purc_variant_t *kvs;
    kvs = (purc_variant_t*)calloc(set->nr_keynames, sizeof(*kvs));
    if (!kvs) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    if (_variant_set_cache_obj_keyval(set, val, kvs)) {
        free(kvs);
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    return kvs;
}

static struct obj_node*
_variant_set_create_obj_node (variant_set_t set, purc_variant_t val)
{
    struct obj_node *_new = (struct obj_node*)calloc(1, sizeof(*_new));
    if (!_new) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    _new->kvs = _variant_set_create_kvs(set, val);
    if (!_new->kvs) {
        free(_new);
        return NULL;
    }

    _new->avl.key = _new->kvs;
    _new->obj = val;
    purc_variant_ref(val);

    return _new;
}

static int
_variant_set_add_val(variant_set_t set, purc_variant_t val, bool override)
{
    if (!val) {
        pcinst_set_error(PURC_ERROR_INVALID_VALUE);
        return -1;
    }
    struct obj_node *_new;
    _new = _variant_set_create_obj_node(set, val);

    if (!_new) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    struct obj_node *p;
    p = avl_find_element(&set->objs, _new->avl.key, p, avl);

    int err = PURC_ERROR_OK;

    do {
        if (p) {
            if (!override) {
                err = PURC_ERROR_DUPLICATED;
                break;
            }
            if (p->obj == val) {
                // already in
                break;
            }
            // replace-in-site
            for (size_t i=0; i<set->nr_keynames; ++i) {
                p->kvs[i] = _new->kvs[i];
            }
            purc_variant_unref(p->obj);
            p->obj = val;
            purc_variant_ref(val);

            break;
        }

        if (pcutils_avl_insert(&set->objs, &_new->avl)) {
            err = PURC_ERROR_OUT_OF_MEMORY;
            break;
        }

        return 0;
    } while (0);

    if (_new) {
        pcvariant_set_release_obj(_new);
        free(_new);
    }

    if (err) {
        pcinst_set_error(PURC_ERROR_DUPLICATED);
        return -1;
    }

    return 0;
}

static int
_variant_set_add_valsn(variant_set_t set, bool override,
    size_t sz, va_list ap)
{
    size_t i = 0;
    while (i<sz) {
        purc_variant_t v = va_arg(ap, purc_variant_t);
        if (!v) {
            pcinst_set_error(PURC_ERROR_INVALID_VALUE);
            break;
        }

        if (_variant_set_add_val(set, v, override)) {
            break;
        }

        ++i;
    }
    return i<sz ? -1 : 0;
}

purc_variant_t
purc_variant_make_set_c (size_t sz, const char* unique_key,
    purc_variant_t value0, ...)
{
    PCVARIANT_CHECK_FAIL_RET((sz==0 && unique_key && *unique_key &&
        value0==NULL) || (sz>0 && unique_key && *unique_key && value0),
        PURC_VARIANT_INVALID);

    purc_variant_t set = _pcv_set_new();
    if (set==PURC_VARIANT_INVALID) {
        return PURC_VARIANT_INVALID;
    }

    do {
        variant_set_t data = _pcv_set_get_data(set);
        if (_variant_set_init(data, unique_key))
            break;

        if (sz>0) {
            purc_variant_t  v = value0;
            if (_variant_set_add_val(data, v, true))
                break;

            va_list ap;
            va_start(ap, value0);
            int r = _variant_set_add_valsn(data, true, sz-1, ap);
            va_end(ap);
            if (r)
                break;
        }

        size_t extra = _variant_set_get_extra_size(data);
        pcvariant_stat_set_extra_size(set, extra);
        return set;
    } while (0);

    // cleanup
    purc_variant_unref(set);

    return PURC_VARIANT_INVALID;
}

purc_variant_t
purc_variant_make_set (size_t sz, purc_variant_t unique_key,
    purc_variant_t value0, ...)
{
    PCVARIANT_CHECK_FAIL_RET((sz==0 &&
        unique_key && unique_key->type==PVT(_STRING) && value0==NULL) ||
        (sz>0 && unique_key && unique_key->type==PVT(_OBJECT) && value0),
        PURC_VARIANT_INVALID);

    purc_variant_t set = _pcv_set_new();
    if (set==PURC_VARIANT_INVALID) {
        return PURC_VARIANT_INVALID;
    }

    do {
        const char *key = purc_variant_get_string_const(unique_key);
        if (!key) {
            pcinst_set_error(PURC_ERROR_INVALID_VALUE);
            break;
        }
        variant_set_t data = _pcv_set_get_data(set);
        if (_variant_set_init(data, key))
            break;

        if (sz>0) {
            purc_variant_t  v = value0;
            if (_variant_set_add_val(data, v, true))
                break;

            va_list ap;
            va_start(ap, value0);
            int r = _variant_set_add_valsn(data, true, sz-1, ap);
            va_end(ap);
            if (r)
                break;
        }

        size_t extra = _variant_set_get_extra_size(data);
        pcvariant_stat_set_extra_size(set, extra);
        return set;
    } while (0);

    // cleanup
    purc_variant_unref(set);

    return PURC_VARIANT_INVALID;
}

bool
purc_variant_set_add (purc_variant_t set, purc_variant_t value, bool override)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) && value,
        PURC_VARIANT_INVALID);

    variant_set_t data = _pcv_set_get_data(set);
    PC_ASSERT(data);

    if (_variant_set_add_val(data, value, override))
        return false;

    size_t extra = _variant_set_get_extra_size(data);
    pcvariant_stat_set_extra_size(set, extra);
    return true;
}

bool purc_variant_set_remove (purc_variant_t set, purc_variant_t value)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) && value,
        PURC_VARIANT_INVALID);

    variant_set_t data = _pcv_set_get_data(set);
    PC_ASSERT(data);

    if (data->nr_keynames!=1) {
        pcinst_set_error(PURC_ERROR_NOT_SUPPORTED);
        return false;
    }

    purc_variant_t *kvs = _variant_set_create_kvs(data, value);
    if (!kvs)
        return false;

    struct obj_node *p;
    p = avl_find_element(&data->objs, kvs, p, avl);

    if (p) {
        pcutils_avl_delete(&data->objs, &p->avl);
        pcvariant_set_release_obj(p);
        free(p);
    }

    free(kvs);

    return p ? true : false;

    // remove `value` rather than by key that `value` represents
    // PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) && value,
    //     PURC_VARIANT_INVALID);

    // variant_set_t data = _pcv_set_get_data(set);
    // PC_ASSERT(data);

    // struct obj_node *_qry = (struct obj_node*)calloc(1, sizeof(*_qry));
    // if (!_qry) {
    //     pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
    //     return -1;
    // }
    // if (_variant_set_cache_obj_keyval(data, value, &_qry->keyvals)) {
    //     pcvariant_set_release_obj(_qry);
    //     free(_qry);
    //     return -1;
    // }

    // _qry->avl.key = &_qry->keyvals;
    // _qry->obj = value;
    // purc_variant_ref(value);

    // struct obj_node *p;
    // p = avl_find_element(&data->objs, _qry->avl.key, p, avl);
    // if (!p || p->obj!=value) {
    //     pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
    //     pcvariant_set_release_obj(_qry);
    //     free(_qry);
    //     return false;
    // }

    // pcutils_avl_delete(&data->objs, &p->avl);
    // pcvariant_set_release_obj(p);
    // free(p);
    // pcvariant_set_release_obj(_qry);
    // free(_qry);

    // size_t extra = _variant_set_get_extra_size(data);
    // pcvariant_stat_set_extra_size(set, extra);

    // return true;
}

purc_variant_t
purc_variant_set_get_value_c (const purc_variant_t set, const char * match_key)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET) &&
        match_key && *match_key,
        PURC_VARIANT_INVALID);

    // to do
    PC_ASSERT(0);

    return PURC_VARIANT_INVALID;
}

size_t purc_variant_set_get_size(const purc_variant_t set)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET),
        -1);

    variant_set_t data = _pcv_set_get_data(set);
    PC_ASSERT(data);

    return data->objs.count;
}

struct purc_variant_set_iterator {
    purc_variant_t       set;
    struct obj_node     *curr;
};

struct purc_variant_set_iterator*
purc_variant_set_make_iterator_begin (purc_variant_t set)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET),
        NULL);

    variant_set_t data = _pcv_set_get_data(set);
    PC_ASSERT(data);

    if (data->objs.count==0) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
        return NULL;
    }
    
    struct purc_variant_set_iterator *it;
    it = (struct purc_variant_set_iterator*)calloc(1, sizeof(*it));
    if (!it) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    it->set = set;

    struct obj_node *p;
    p = avl_first_element(&data->objs, p, avl);
    PC_ASSERT(p);

    it->curr = p;

    return it;
}

struct purc_variant_set_iterator*
purc_variant_set_make_iterator_end (purc_variant_t set)
{
    PCVARIANT_CHECK_FAIL_RET(set && set->type==PVT(_SET),
        NULL);

    variant_set_t data = _pcv_set_get_data(set);
    PC_ASSERT(data);

    if (data->objs.count==0) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
        return NULL;
    }
    
    struct purc_variant_set_iterator *it;
    it = (struct purc_variant_set_iterator*)calloc(1, sizeof(*it));
    if (!it) {
        pcinst_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    it->set = set;

    struct obj_node *p;
    p = avl_last_element(&data->objs, p, avl);
    PC_ASSERT(p);

    it->curr = p;

    return it;
}

void purc_variant_set_release_iterator (struct purc_variant_set_iterator* it)
{
    if (!it)
        return;
    free(it);
}

bool purc_variant_set_iterator_next (struct purc_variant_set_iterator* it)
{
    PCVARIANT_CHECK_FAIL_RET(it && it->set &&
        it->set->type==PVT(_SET) && it->curr,
        false);

    variant_set_t data = _pcv_set_get_data(it->set);
    PC_ASSERT(data);

    struct obj_node *p;
    p = avl_last_element(&data->objs, p, avl);
    if (it->curr==p) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
        return false;
    }

    it->curr = avl_next_element(it->curr, avl);

    return true;
}

bool purc_variant_set_iterator_prev (struct purc_variant_set_iterator* it)
{
    PCVARIANT_CHECK_FAIL_RET(it && it->set &&
        it->set->type==PVT(_SET) && it->curr,
        false);

    variant_set_t data = _pcv_set_get_data(it->set);
    PC_ASSERT(data);

    struct obj_node *p;
    p = avl_first_element(&data->objs, p, avl);
    if (it->curr==p) {
        pcinst_set_error(PCVARIANT_ERROR_NOT_FOUND);
        return false;
    }

    it->curr = avl_prev_element(it->curr, avl);

    return true;
}

purc_variant_t
purc_variant_set_iterator_get_value (struct purc_variant_set_iterator* it)
{
    PCVARIANT_CHECK_FAIL_RET(it && it->set &&
        it->set->type==PVT(_SET) && it->curr,
        PURC_VARIANT_INVALID);

    return it->curr->obj;
}

void pcvariant_set_release (purc_variant_t value)
{
    variant_set_t data = _pcv_set_get_data(value);
    PC_ASSERT(data);

    _variant_set_release(data);
    free(data);
    _pcv_set_set_data(value, NULL);
}

/* VWNOTE: unnecessary
int pcvariant_set_compare (purc_variant_t lv, purc_variant_t rv)
{
    variant_set_t ldata = _pcv_set_get_data(lv);
    variant_set_t rdata = _pcv_set_get_data(rv);
    PC_ASSERT(ldata && rdata);

    struct obj_node *ln, *rn;
    ln = avl_first_element(&ldata->objs, ln, avl);
    rn = avl_first_element(&rdata->objs, rn, avl);
    for (; ln && rn;
        ln = avl_next_element(ln, avl),
        rn = avl_next_element(rn, avl))
    {
        int t = purc_variant_compare(ln->obj, rn->obj);
        if (t)
            return t;
    }

    return ln ? 1 : -1;
}
*/

