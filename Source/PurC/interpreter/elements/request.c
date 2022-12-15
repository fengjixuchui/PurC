/**
 * @file request.c
 * @author Xue Shuming
 * @date 2022/08/05
 * @brief
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
 *
 */

#include "purc.h"

#include "../internal.h"

#include "private/debug.h"
#include "private/instance.h"
#include "purc-runloop.h"

#include "../ops.h"

#include <pthread.h>
#include <unistd.h>

#define EVENT_SEPARATOR          ':'
#define REQUEST_EVENT_HANDER  "_request_event_handler"

struct ctxt_for_request {
    struct pcvdom_node           *curr;

    purc_variant_t                on;
    purc_variant_t                to;
    purc_variant_t                as;
    purc_variant_t                at;
    purc_variant_t                with;

    unsigned int                  synchronously:1;
    unsigned int                  noreturn:1;
    unsigned int                  bound:1;
    purc_variant_t                request_id;
};

static void
ctxt_for_request_destroy(struct ctxt_for_request *ctxt)
{
    if (ctxt) {
        PURC_VARIANT_SAFE_CLEAR(ctxt->on);
        PURC_VARIANT_SAFE_CLEAR(ctxt->to);
        PURC_VARIANT_SAFE_CLEAR(ctxt->as);
        PURC_VARIANT_SAFE_CLEAR(ctxt->with);
        PURC_VARIANT_SAFE_CLEAR(ctxt->request_id);
        free(ctxt);
    }
}

static void
ctxt_destroy(void *ctxt)
{
    ctxt_for_request_destroy((struct ctxt_for_request*)ctxt);
}

static bool
is_observer_match(struct pcintr_observer *observer, pcrdr_msg *msg,
        purc_variant_t observed, purc_atom_t type, const char *sub_type)
{
    UNUSED_PARAM(observer);
    UNUSED_PARAM(msg);
    UNUSED_PARAM(observed);
    UNUSED_PARAM(type);
    UNUSED_PARAM(sub_type);
    bool match = false;
    if (!purc_variant_is_equal_to(observer->observed, msg->elementValue)) {
        goto out;
    }

    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(MSG, RESPONSE)) == type) {
        match = true;
        goto out;
    }

out:
    return match;
}

static int
observer_handle(pcintr_coroutine_t cor, struct pcintr_observer *observer,
        pcrdr_msg *msg, purc_atom_t type, const char *sub_type, void *data)
{
    UNUSED_PARAM(cor);
    UNUSED_PARAM(observer);
    UNUSED_PARAM(msg);
    UNUSED_PARAM(type);
    UNUSED_PARAM(sub_type);
    UNUSED_PARAM(data);
    UNUSED_PARAM(msg);

    pcintr_set_current_co(cor);

    pcintr_stack_frame_t frame = (pcintr_stack_frame_t)data;

    purc_variant_t payload = msg->data;
    pcintr_set_question_var(frame, payload);

    pcintr_resume(cor, msg);
    pcintr_set_current_co(NULL);
    return 0;
}

static bool
is_css_selector(const char *s)
{
    if (s && (s[0] == '.' || s[0] == '#')) {
        return true;
    }
    return false;
}

static bool
is_rdr(purc_variant_t v)
{
    UNUSED_PARAM(v);
    return false;
}

static int
request_crtn_by_cid(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        purc_atom_t cid)
{
    UNUSED_PARAM(co);

    int ret = 0;
    struct ctxt_for_request *ctxt = (struct ctxt_for_request*)frame->ctxt;
    ctxt->request_id = purc_variant_make_ulongint(cid);
    purc_variant_t to = ctxt->to;

    const char *sub_type = purc_variant_get_string_const(to);
    pcintr_coroutine_post_event(cid,
            PCRDR_MSG_EVENT_REDUCE_OPT_KEEP,
            ctxt->request_id,
            MSG_TYPE_REQUEST, sub_type,
            ctxt->with, ctxt->request_id);

    if (!ctxt->synchronously) {
        goto out;
    }

    pcintr_yield(
            CO_STAGE_FIRST_RUN | CO_STAGE_OBSERVING,
            CO_STATE_STOPPED,
            ctxt->request_id,
            MSG_TYPE_RESPONSE,
            MSG_SUB_TYPE_ASTERISK,
            is_observer_match,
            observer_handle,
            frame,
            true
        );

out:
    return ret;
}

static int
request_crtn_by_uri(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        const char *uri, char *host_name, char *app_name,
        char *runner_name, enum HVML_RUN_RES_TYPE res_type, char *res_name)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(uri);
    UNUSED_PARAM(host_name);
    UNUSED_PARAM(app_name);
    UNUSED_PARAM(runner_name);
    UNUSED_PARAM(res_type);
    UNUSED_PARAM(res_name);

    int ret = -1;
    bool is_same_runner = false;
    struct pcinst *curr_inst = pcinst_current();

    if (strcmp(host_name, PCINTR_HVML_RUN_CURR_ID) != 0 &&
            strcmp(host_name, PCRDR_LOCALHOST) != 0) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "invalid host_name '%s' vs '%s'", host_name, PCRDR_LOCALHOST);
        goto out;
    }

    if (strcmp(app_name, PCINTR_HVML_RUN_CURR_ID) != 0 &&
            strcmp(app_name, curr_inst->app_name) != 0) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "invalid app_name '%s' vs '%s'", app_name, curr_inst->app_name);
        goto out;
    }

    if (strcmp(runner_name, PCINTR_HVML_RUN_CURR_ID) == 0 ||
            strcmp(runner_name, curr_inst->runner_name) == 0) {
        is_same_runner = true;
    }

    if (res_type == HVML_RUN_RES_TYPE_CHAN) {
        purc_set_error(PURC_ERROR_NOT_IMPLEMENTED);
        PC_WARN("not implemented on '%s' for request.\n", uri);
        goto out;
    }
    else if (res_type == HVML_RUN_RES_TYPE_CRTN) {
        pcintr_coroutine_t dest_co = NULL;
        if (strcmp(co->token, res_name) == 0 ||
                (co->is_main && strcmp(res_name, CRTN_TOKEN_MAIN) == 0)) {
            purc_set_error_with_info(PURC_ERROR_NOT_SUPPORTED,
                    "Can not send request to current coroutine '%s'", co->token);
            goto out;
        }
        else if (strcmp(res_name, CRTN_TOKEN_FIRST) == 0) {
            pcintr_coroutine_t first = pcintr_get_first_crtn(curr_inst);
            if (co == first) {
                purc_set_error_with_info(PURC_ERROR_NOT_SUPPORTED,
                        "Can not send request to first coroutine '%s'", co->token);
                goto out;
            }
            dest_co = first;
        }
        else if (strcmp(res_name, CRTN_TOKEN_LAST) == 0) {
            pcintr_coroutine_t last = pcintr_get_last_crtn(curr_inst);
            if (co == last) {
                purc_set_error_with_info(PURC_ERROR_NOT_SUPPORTED,
                        "Can not send request to last coroutine '%s'", co->token);
                goto out;
            }
            dest_co = last;
        }
        if (dest_co) {
            ret = request_crtn_by_cid(co, frame, dest_co->cid);
            goto out;
        }
    }

    purc_set_error(PURC_ERROR_NOT_IMPLEMENTED);
    PC_WARN("not implemented on '%s' for request.\n", uri);
out:
    return ret;
}

static int
request_elements(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        const char *selector)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(selector);
    purc_set_error(PURC_ERROR_NOT_IMPLEMENTED);
    PC_WARN("not implemented on '%s' for request.\n", selector);
    return -1;
}

static int
request_rdr(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        purc_variant_t rdr)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(rdr);
    purc_set_error(PURC_ERROR_NOT_IMPLEMENTED);
    PC_WARN("not implemented on '$RDR' for request.\n");
    return -1;
}

static int
post_process(pcintr_coroutine_t co, struct pcintr_stack_frame *frame)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);

    int ret = 0;
    struct ctxt_for_request *ctxt = (struct ctxt_for_request*)frame->ctxt;
    purc_variant_t on = ctxt->on;
    purc_variant_t to = ctxt->to;
    if (!on || !to || !purc_variant_is_string(to)) {
        purc_set_error(PURC_ERROR_INVALID_VALUE);
        ret = -1;
        goto out;
    }

    if (purc_variant_is_ulongint(on)) {
        uint64_t u64;
        purc_variant_cast_to_ulongint(on, &u64, true);
        purc_atom_t dest_cid = (purc_atom_t) u64;
        ctxt->request_id = purc_variant_ref(on);
        ret = request_crtn_by_cid(co, frame, dest_cid);
    }
    else if (purc_variant_is_string(on)) {
        const char *s_to = purc_variant_get_string_const(on);
        char host_name[PURC_LEN_HOST_NAME + 1];
        char app_name[PURC_LEN_APP_NAME + 1];
        char runner_name[PURC_LEN_RUNNER_NAME + 1];
        char res_name[PURC_LEN_IDENTIFIER + 1];
        enum HVML_RUN_RES_TYPE res_type = HVML_RUN_RES_TYPE_INVALID;

        if (is_css_selector(s_to)) {
            ret = request_elements(co, frame, s_to);
        }
        else if (pcintr_parse_hvml_run_uri(s_to, host_name, app_name,
                    runner_name, &res_type, res_name)) {
            ret = request_crtn_by_uri(co, frame, s_to, host_name, app_name,
                    runner_name, res_type, res_name);
        }
        else {
            purc_set_error(PURC_ERROR_INVALID_VALUE);
            ret = -1;
            PC_WARN("not implemented on '%s' for request.\n",
                    purc_variant_get_string_const(on));
        }
    }
    else if (is_rdr(on)) {
        ret = request_rdr(co, frame, on);
    }
    else {
        purc_set_error(PURC_ERROR_NOT_SUPPORTED);
        ret = -1;
        PC_WARN("not supported on with type '%s' for request.\n",
                pcvariant_typename(on));
        goto out;
    }

out:
    if (ret == 0 && ctxt->request_id && ctxt->as
            && !ctxt->synchronously && !ctxt->noreturn) {
        const char *name = purc_variant_get_string_const(ctxt->as);
        ret = pcintr_bind_named_variable(&co->stack,
                frame, name, ctxt->at, false, true, ctxt->request_id);
        if (ret == 0) {
            ctxt->bound = 1;
        }
    }
    return ret;
}

static int
process_attr_on(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    PURC_VARIANT_SAFE_CLEAR(ctxt->on);
    ctxt->on = purc_variant_ref(val);

    return 0;
}

static int
process_attr_to(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    PURC_VARIANT_SAFE_CLEAR(ctxt->to);
    ctxt->to = purc_variant_ref(val);

    return 0;
}

static int
process_attr_as(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;
    if (val == PURC_VARIANT_INVALID || !purc_variant_is_string(val)) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    PURC_VARIANT_SAFE_CLEAR(ctxt->as);
    ctxt->as = purc_variant_ref(val);

    return 0;
}

static int
process_attr_at(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    PURC_VARIANT_SAFE_CLEAR(ctxt->at);
    ctxt->at = purc_variant_ref(val);

    return 0;
}


static int
process_attr_with(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    PURC_VARIANT_SAFE_CLEAR(ctxt->with);
    ctxt->with = purc_variant_ref(val);

    return 0;
}

static int
attr_found_val(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val,
        struct pcvdom_attr *attr,
        void *ud)
{
    UNUSED_PARAM(attr);
    UNUSED_PARAM(ud);

    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;

    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, ON)) == name) {
        return process_attr_on(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, TO)) == name) {
        return process_attr_to(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, AS)) == name) {
        return process_attr_as(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, AT)) == name) {
        return process_attr_at(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, WITH)) == name) {
        return process_attr_with(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, SYNCHRONOUSLY)) == name) {
        ctxt->synchronously = 1;
        return 0;
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, SYNC)) == name) {
        ctxt->synchronously = 1;
        return 0;
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, ASYNCHRONOUSLY)) == name) {
        ctxt->synchronously = 0;
        return 0;
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, ASYNC)) == name) {
        ctxt->synchronously = 0;
        return 0;
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, NORETURN)) == name
            || pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, NO_RETURN)) == name) {
        ctxt->noreturn = 1;
        return 0;
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, SILENTLY)) == name) {
        return 0;
    }

    /* ignore other attr */
    return 0;
}

static void*
after_pushed(pcintr_stack_t stack, pcvdom_element_t pos)
{
    if (stack->except)
        return NULL;

    pcintr_check_insertion_mode_for_normal_element(stack);

    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);

    struct ctxt_for_request *ctxt = frame->ctxt;
    if (!ctxt) {
        ctxt = (struct ctxt_for_request*)calloc(1, sizeof(*ctxt));
        if (!ctxt) {
            purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
            return NULL;
        }

        ctxt->synchronously = 1;

        frame->ctxt = ctxt;
        frame->ctxt_destroy = ctxt_destroy;

        frame->pos = pos; // ATTENTION!!
    }

    if (0 != pcintr_stack_frame_eval_attr_and_content(stack, frame, false)) {
        return NULL;
    }

    frame->attr_vars = purc_variant_make_object(0,
            PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
    if (frame->attr_vars == PURC_VARIANT_INVALID)
        return ctxt;

    struct pcvdom_element *element = frame->pos;

    int r;
    r = pcintr_walk_attrs(frame, element, stack, attr_found_val);
    if (r)
        return ctxt;

    if (!ctxt->with) {
        purc_variant_t caret = pcintr_get_symbol_var(frame,
                PURC_SYMBOL_VAR_CARET);
        if (caret && !purc_variant_is_undefined(caret)) {
            ctxt->with = caret;
            purc_variant_ref(ctxt->with);
        }
    }

    r = post_process(stack->co, frame);
    if (r)
        return ctxt;

    return ctxt;
}

static bool
on_popping(pcintr_stack_t stack, void* ud)
{
    UNUSED_PARAM(ud);

    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);

    if (frame->ctxt == NULL)
        return true;

    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;
    if (ctxt) {
        ctxt_for_request_destroy(ctxt);
        frame->ctxt = NULL;
    }

    return true;
}

static void
on_element(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        struct pcvdom_element *element)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(element);
}

static void
on_content(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        struct pcvdom_content *content)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(content);
}

static void
on_comment(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        struct pcvdom_comment *comment)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(comment);
}

static pcvdom_element_t
select_child(pcintr_stack_t stack, void* ud)
{
    UNUSED_PARAM(ud);

    pcintr_coroutine_t co = stack->co;
    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);

    if (stack->back_anchor == frame)
        stack->back_anchor = NULL;

    if (frame->ctxt == NULL)
        return NULL;

    if (stack->back_anchor)
        return NULL;

    struct ctxt_for_request *ctxt;
    ctxt = (struct ctxt_for_request*)frame->ctxt;

    struct pcvdom_node *curr;

again:
    curr = ctxt->curr;

    if (curr == NULL) {
        struct pcvdom_element *element = frame->pos;
        struct pcvdom_node *node = &element->node;
        node = pcvdom_node_first_child(node);
        curr = node;
    }
    else {
        curr = pcvdom_node_next_sibling(curr);
    }

    ctxt->curr = curr;

    if (curr == NULL) {
        purc_clr_error();
        return NULL;
    }

    switch (curr->type) {
        case PCVDOM_NODE_DOCUMENT:
            purc_set_error(PURC_ERROR_NOT_IMPLEMENTED);
            break;
        case PCVDOM_NODE_ELEMENT:
            {
                pcvdom_element_t element = PCVDOM_ELEMENT_FROM_NODE(curr);
                on_element(co, frame, element);
                return element;
            }
        case PCVDOM_NODE_CONTENT:
            on_content(co, frame, PCVDOM_CONTENT_FROM_NODE(curr));
            goto again;
        case PCVDOM_NODE_COMMENT:
            on_comment(co, frame, PCVDOM_COMMENT_FROM_NODE(curr));
            goto again;
        default:
            purc_set_error(PURC_ERROR_NOT_IMPLEMENTED);
    }

    purc_set_error(PURC_ERROR_NOT_SUPPORTED);
    return NULL; // NOTE: never reached here!!!
}

static struct pcintr_element_ops
ops = {
    .after_pushed       = after_pushed,
    .on_popping         = on_popping,
    .rerun              = NULL,
    .select_child       = select_child,
};

struct pcintr_element_ops* pcintr_get_request_ops(void)
{
    return &ops;
}


