/**
 * @file test.c
 * @author Xue Shuming
 * @date 2022/01/11
 * @brief The ops for <test>
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
#include "private/executor.h"
#include "purc-runloop.h"

#include "../ops.h"

#include "purc-executor.h"

#include <pthread.h>
#include <unistd.h>

struct ctxt_for_test {
    struct pcvdom_node *curr;
    purc_variant_t on;
    purc_variant_t by;
    purc_variant_t in;
    purc_variant_t with;

    purc_exec_ops_t         ops;
    purc_exec_inst_t        exec_inst;
    purc_exec_iter_t        it;

    bool handle_differ;
};

static void
ctxt_for_test_destroy(struct ctxt_for_test *ctxt)
{
    if (ctxt) {
        if (ctxt->exec_inst) {
            ctxt->ops->destroy(ctxt->exec_inst);
            ctxt->exec_inst = NULL;
        }
        PURC_VARIANT_SAFE_CLEAR(ctxt->by);
        PURC_VARIANT_SAFE_CLEAR(ctxt->on);
        PURC_VARIANT_SAFE_CLEAR(ctxt->in);
        PURC_VARIANT_SAFE_CLEAR(ctxt->with);

        free(ctxt);
    }
}

static void
ctxt_destroy(void *ctxt)
{
    ctxt_for_test_destroy((struct ctxt_for_test*)ctxt);
}

static int
post_process_dest_data(pcintr_coroutine_t co, struct pcintr_stack_frame *frame)
{
    UNUSED_PARAM(co);

    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;

    purc_variant_t on;
    on = ctxt->on;
    if (on == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_ARGUMENT_MISSED,
                "vdom attribute 'on' for element <test> undefined");
        return -1;
    }

    purc_variant_t by;
    by = ctxt->by;

    purc_variant_t with;
    with = ctxt->with;

    if (by != PURC_VARIANT_INVALID) {
        const char *rule = purc_variant_get_string_const(by);
        bool ok = purc_get_executor(rule, &ctxt->ops);
        if (!ok)
            return -1;

        purc_exec_inst_t exec_inst;
        exec_inst = ctxt->ops->create(PURC_EXEC_TYPE_CHOOSE, on, false);
        if (!exec_inst)
            return -1;

        exec_inst->with = with;

        ctxt->exec_inst = exec_inst;

        int r = -1;
        purc_variant_t value;
        value = ctxt->ops->choose(exec_inst, rule);
        if (value != PURC_VARIANT_INVALID) {
            r = pcintr_set_question_var(frame, value);
            purc_variant_unref(value);
            if (r == 0)
                purc_clr_error();
        }
        ok = ctxt->ops->destroy(ctxt->exec_inst);
        ctxt->exec_inst = NULL;
        return r ? -1 : 0;
    }

    int r;
    r = pcintr_set_question_var(frame, on);

    return r ? -1 : 0;
}

static int
post_process(pcintr_coroutine_t co, struct pcintr_stack_frame *frame)
{
    UNUSED_PARAM(co);

    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;

    int r = post_process_dest_data(co, frame);
    if (r)
        return r;

    purc_variant_t in;
    in = ctxt->in;
    if (in != PURC_VARIANT_INVALID) {
        if (!purc_variant_is_string(in)) {
            purc_set_error(PURC_ERROR_INVALID_VALUE);
            return -1;
        }

        purc_variant_t elements = pcintr_doc_query(co,
                purc_variant_get_string_const(in), frame->silently);
        if (elements == PURC_VARIANT_INVALID) {
            purc_set_error(PURC_ERROR_INVALID_VALUE);
            return -1;
        }

        pcintr_set_at_var(frame, elements);
        purc_variant_unref(elements);
    }

    return 0;
}

static int
process_attr_on(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;
    if (ctxt->on != PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_DUPLICATED,
                "vdom attribute '%s' for element <%s>",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    ctxt->on = val;
    purc_variant_ref(val);

    return 0;
}

static int
process_attr_in(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;
    if (ctxt->in != PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_DUPLICATED,
                "vdom attribute '%s' for element <%s>",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    ctxt->in = val;
    purc_variant_ref(val);

    return 0;
}

static int
process_attr_by(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;
    if (ctxt->by != PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_DUPLICATED,
                "vdom attribute '%s' for element <%s>",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    ctxt->by = val;
    purc_variant_ref(val);

    return 0;
}

static int
process_attr_with(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element,
        purc_atom_t name, purc_variant_t val)
{
    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;
    if (ctxt->with != PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_DUPLICATED,
                "vdom attribute '%s' for element <%s>",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    if (val == PURC_VARIANT_INVALID) {
        purc_set_error_with_info(PURC_ERROR_INVALID_VALUE,
                "vdom attribute '%s' for element <%s> undefined",
                purc_atom_to_string(name), element->tag_name);
        return -1;
    }
    ctxt->with = val;
    purc_variant_ref(val);

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

    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, ON)) == name) {
        return process_attr_on(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, IN)) == name) {
        return process_attr_in(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, BY)) == name) {
        return process_attr_by(frame, element, name, val);
    }
    if (pchvml_keyword(PCHVML_KEYWORD_ENUM(HVML, WITH)) == name) {
        return process_attr_with(frame, element, name, val);
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
    if (stack->except) {
        return NULL;
    }

    pcintr_check_insertion_mode_for_normal_element(stack);

    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);

    struct ctxt_for_test *ctxt = frame->ctxt;
    if (!ctxt) {
        ctxt = (struct ctxt_for_test*)calloc(1, sizeof(*ctxt));
        if (!ctxt) {
            purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
            return NULL;
        }

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
    if (r) {
        return ctxt;
    }

    if (!ctxt->with) {
        purc_variant_t caret = pcintr_get_symbol_var(frame,
                PURC_SYMBOL_VAR_CARET);
        if (caret && !purc_variant_is_undefined(caret)) {
            ctxt->with = caret;
            purc_variant_ref(ctxt->with);
        }
    }

    purc_clr_error();

    if (ctxt->on == PURC_VARIANT_INVALID
            && ctxt->with != PURC_VARIANT_INVALID) {
        ctxt->handle_differ = !purc_variant_booleanize(ctxt->with);
    }
    else {
        r = post_process(stack->co, frame);
        if (r)
            return ctxt;
    }

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

    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;
    if (ctxt) {
        ctxt_for_test_destroy(ctxt);
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

    if (frame->result_from_child)
        return NULL;

    struct ctxt_for_test *ctxt;
    ctxt = (struct ctxt_for_test*)frame->ctxt;
    if (!ctxt)
        return NULL;

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
                if (ctxt->handle_differ) {
                    if (element->tag_id == PCHVML_TAG_DIFFER) {
                        return element;
                    }
                    goto again;
                }
                else if (element->tag_id != PCHVML_TAG_DIFFER) {
                    return element;
                }
                goto again;
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

struct pcintr_element_ops* pcintr_get_test_ops(void)
{
    return &ops;
}

