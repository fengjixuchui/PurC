/**
 * @file interpreter.c
 * @author Xu Xiaohong
 * @date 2021/11/18
 * @brief The internal interfaces for interpreter
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

#include "config.h"

#include "internal.h"

#include "private/debug.h"
#include "private/instance.h"
#include "private/runloop.h"
#include "private/dvobjs.h"

#include "ops.h"
#include "../hvml/hvml-gen.h"
#include "../html/parser.h"

#include "hvml-attr.h"
#include "fetcher.h"

#include <stdarg.h>

#define TO_DEBUG 1

struct edom_fragment {
    struct list_head           node;
    purc_variant_t             on;
    purc_variant_t             to;
    purc_variant_t             at;
    char                      *content;
};

void pcintr_stack_init_once(void)
{
    pcrunloop_init_main();
    pcrunloop_t runloop = pcrunloop_get_main();
    PC_ASSERT(runloop);
    init_ops();
}

void pcintr_stack_init_instance(struct pcinst* inst)
{
    struct pcintr_heap *heap = &inst->intr_heap;
    INIT_LIST_HEAD(&heap->coroutines);
    heap->running_coroutine = NULL;
}

static void
stack_frame_release(struct pcintr_stack_frame *frame)
{
    frame->scope = NULL;
    frame->edom_element = NULL;
    frame->pos   = NULL;

    if (frame->ctxt) {
        PC_ASSERT(frame->ctxt_destroy);
        frame->ctxt_destroy(frame->ctxt);
        frame->ctxt  = NULL;
    }

    for (size_t i=0; i<PCA_TABLESIZE(frame->symbol_vars); ++i) {
        PURC_VARIANT_SAFE_CLEAR(frame->symbol_vars[i]);
    }

    PURC_VARIANT_SAFE_CLEAR(frame->attr_vars);
    PURC_VARIANT_SAFE_CLEAR(frame->ctnt_var);
    PURC_VARIANT_SAFE_CLEAR(frame->result_var);
    PURC_VARIANT_SAFE_CLEAR(frame->result_from_child);
    PURC_VARIANT_SAFE_CLEAR(frame->mid_vars);
}

static void
vdom_release(purc_vdom_t vdom)
{
    if (vdom->document) {
        pcvdom_document_destroy(vdom->document);
        vdom->document = NULL;
    }
}

static void
vdom_destroy(purc_vdom_t vdom)
{
    if (!vdom)
        return;

    vdom_release(vdom);

    free(vdom);
}

static void
dump_document(pchtml_html_document_t *doc)
{
    purc_rwstream_t out = purc_rwstream_new_buffer(1024, 1024*1024*16);
    if (!out) {
        purc_clr_error();
        return;
    }

    pchtml_doc_write_to_stream(doc, out);
    purc_rwstream_write(out, "", 1);
    const char *buf = purc_rwstream_get_mem_buffer(out, NULL);
    fprintf(stderr, "html:\n%s\n", buf);
    purc_rwstream_destroy(out);
}

static void
edom_gen_release(struct pcintr_edom_gen *edom_gen)
{
    if (edom_gen->parser) {
        pchtml_html_parser_destroy(edom_gen->parser);
        edom_gen->parser = NULL;
    }
    if (edom_gen->cache) {
        purc_rwstream_write(edom_gen->cache, "", 1); // null-terminator
        const char *s;
        s = (const char*)purc_rwstream_get_mem_buffer(edom_gen->cache, NULL);
        if (0)
            fprintf(stderr, "generated html:\n%s\n", s);
        purc_rwstream_destroy(edom_gen->cache);
        edom_gen->cache = NULL;
    }
    if (edom_gen->doc) {
        if (0)
            dump_document(edom_gen->doc);
        pchtml_html_document_destroy(edom_gen->doc);
        edom_gen->doc = NULL;
    }
}

static int
edom_gen_init(struct pcintr_edom_gen *edom_gen)
{
    edom_gen->parser = pchtml_html_parser_create();
    if (!edom_gen->parser) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    unsigned int ui = pchtml_html_parser_init(edom_gen->parser);
    if (ui) {
        // FIXME: out of memory????
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    edom_gen->doc = pchtml_html_parse_chunk_begin(edom_gen->parser);
    if (!edom_gen->doc) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    edom_gen->cache = purc_rwstream_new_buffer(1024, 1024*1024*16);
    if (!edom_gen->cache) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    return 0;
}

static int
edom_gen_write(struct pcintr_edom_gen *edom_gen, const char *buf, int sz)
{
    PC_ASSERT(edom_gen->parser);
    PC_ASSERT(edom_gen->doc);
    PC_ASSERT(edom_gen->cache);
    ssize_t n = purc_rwstream_write(edom_gen->cache, buf, sz);
    if (n != sz)
        return -1;

    unsigned int ui;
    ui = pchtml_html_parse_chunk_process(edom_gen->parser,
            (unsigned char*)buf, sz);
    if (ui) {
        // FIXME: out of memory????
        return -1;
    }

    // pchtml_html_tree_t *tree = edom_gen->parser->tree;
    // if (!tree)
    //     return 0;

    // pcutils_array_t *open_elements = tree->open_elements;
    // if (!open_elements)
    //     return 0;

    // if (open_elements->length < 1)
    //     return 0;

    // pcdom_element_t *element = open_elements->list[open_elements->length-1];
    // if (!element)
    //     return 0;

    // const unsigned char *tag_name;
    // size_t len;
    // tag_name = pcdom_element_tag_name(element, &len);
    // PC_ASSERT(tag_name);
    // PC_ASSERT(tag_name[len] == 0);
    // fprintf(stderr, "==[%s]==tag_name:[%s]==\n", buf, tag_name);

    return 0;
}

static void
edom_gen_finish(struct pcintr_edom_gen *edom_gen)
{
    if (edom_gen->finished)
        return;
    edom_gen->finished = 1;

    unsigned int ui;
    char buf[] = "";
    PC_ASSERT(sizeof(buf) == 1);
    purc_rwstream_t ws = purc_rwstream_new_from_mem(buf, sizeof(buf));
    if (!ws)
        return;

    ui = pchtml_html_parse_chunk_end(edom_gen->parser);
    PC_ASSERT(ui == 0);
    purc_rwstream_destroy(ws);
}

pcdom_element_t*
pcintr_stack_get_edom_open_element(pcintr_stack_t stack)
{
    PC_ASSERT(stack);
    struct pcintr_edom_gen *edom_gen = &stack->edom_gen;

    pchtml_html_tree_t *tree = edom_gen->parser->tree;
    if (!tree)
        return NULL;

    pcutils_array_t *open_elements = tree->open_elements;
    if (!open_elements)
        return NULL;

    if (open_elements->length < 1)
        return NULL;

    pcdom_element_t *element = open_elements->list[open_elements->length-1];
    if (!element)
        return NULL;

    const unsigned char *tag_name;
    size_t len;
    tag_name = pcdom_element_tag_name(element, &len);
    PC_ASSERT(tag_name);
    PC_ASSERT(tag_name[len] == 0);
    // fprintf(stderr, "==[%s]==tag_name:[%s]==\n", buf, tag_name);
    return element;
}

void
pcintr_stack_write_fragment(pcintr_stack_t stack)
{
    if (!stack->fragment)
        return;
    ssize_t n = purc_rwstream_write(stack->fragment, "", 1);
    PC_ASSERT(n == 1);

    const char *s;
    s = (const char*)purc_rwstream_get_mem_buffer(stack->fragment, NULL);
    PC_ASSERT(s);

    int r = edom_gen_write(&stack->edom_gen, s, strlen(s));
    PC_ASSERT(r == 0);
}

static void
edom_fragment_release(struct edom_fragment *fragment)
{
    if (!fragment)
        return;

    PURC_VARIANT_SAFE_CLEAR(fragment->on);
    PURC_VARIANT_SAFE_CLEAR(fragment->to);
    PURC_VARIANT_SAFE_CLEAR(fragment->at);
    if (fragment->content) {
        free(fragment->content);
        fragment->content = NULL;
    }
}

static void
edom_fragment_destroy(struct edom_fragment *fragment)
{
    if (fragment) {
        edom_fragment_release(fragment);
        free(fragment);
    }
}

static void
edom_fragment_post_process_target_content(pcintr_stack_t stack,
        struct edom_fragment *fragment, struct pcdom_element *target)
{
    struct pcintr_edom_gen *edom_gen = &stack->edom_gen;
    PC_ASSERT(edom_gen->finished);
    pchtml_html_document_t *doc = edom_gen->doc;
    PC_ASSERT(doc);
    pchtml_html_parser_t *parser = edom_gen->parser;
    PC_ASSERT(parser);

    const char *op = "displace";
    purc_variant_t to = fragment->to;
    if (to != PURC_VARIANT_INVALID) {
        PC_ASSERT(purc_variant_is_type(to, PURC_VARIANT_TYPE_STRING));
        op = purc_variant_get_string_const(fragment->to);
    }

    const char *content = fragment->content;

    purc_rwstream_t in;
    in = purc_rwstream_new_from_mem((void*)content, strlen(content));
    if (!in)
        return;

    pcdom_node_t *node = pchtml_html_document_parse_fragment(doc, target,
            in);
    purc_rwstream_destroy(in);
    PC_ASSERT(node);
    PC_ASSERT(node->type == PCDOM_NODE_TYPE_ELEMENT);

    if (strcmp(op, "append") == 0) {
        pcdom_merge_fragment_append(&target->node, node);
    }
    else if (strcmp(op, "prepend") == 0) {
        PC_ASSERT(0);
        pcdom_merge_fragment_prepend(&target->node, node);
    }
    else if (strcmp(op, "insertAfter") == 0) {
        PC_ASSERT(0);
        pcdom_merge_fragment_insert_after(&target->node, node);
    }
    else if (strcmp(op, "insertBefore") == 0) {
        PC_ASSERT(0);
        pcdom_merge_fragment_insert_before(&target->node, node);
    }
    else if (strcmp(op, "displace") == 0) {
        pcdom_node_t *child = target->node.first_child;
        while (child) {
            pcdom_node_remove(child);
            pcdom_node_destroy(child);
            child = target->node.first_child;
        }
        pcdom_merge_fragment_append(&target->node, node);
    }
    else {
        // pcdom_merge_fragment_append(&target->node, node);
        PC_ASSERT(0);
    }
}

static void
edom_fragment_post_process_target_attr(pcintr_stack_t stack,
        struct edom_fragment *fragment, struct pcdom_element *target,
        const char *attr_name)
{
    UNUSED_PARAM(fragment);

    struct pcintr_edom_gen *edom_gen = &stack->edom_gen;
    PC_ASSERT(edom_gen->finished);
    pchtml_html_document_t *doc = edom_gen->doc;
    PC_ASSERT(doc);
    pchtml_html_parser_t *parser = edom_gen->parser;
    PC_ASSERT(parser);

    PC_ASSERT(attr_name);

    pcdom_attr_t *attr;
    attr = pcdom_element_attr_by_name(target,
            (const unsigned char*)attr_name, strlen(attr_name));
    PC_ASSERT(attr);

    const unsigned char *attr_val;
    size_t len;
    attr_val = pcdom_attr_value(attr, &len);
    D("name:value: %s:%.*s", attr_name, (int)len, attr_val);

    const char *content = fragment->content;
    D("content: %s", content);

    const char *op = "displace";
    purc_variant_t to = fragment->to;
    if (to != PURC_VARIANT_INVALID) {
        PC_ASSERT(purc_variant_is_type(to, PURC_VARIANT_TYPE_STRING));
        op = purc_variant_get_string_const(fragment->to);
    }

    if (strcmp(op, "append") == 0) {
        PC_ASSERT(0); // Not implemented yet
    }
    else if (strcmp(op, "prepend") == 0) {
        PC_ASSERT(0); // Not implemented yet
    }
    else if (strcmp(op, "insertAfter") == 0) {
        PC_ASSERT(0); // Not implemented yet
    }
    else if (strcmp(op, "insertBefore") == 0) {
        PC_ASSERT(0); // Not implemented yet
    }
    else if (strcmp(op, "displace") == 0) {
        pcdom_attr_set_value(attr,
                (const unsigned char *)content, strlen(content));
    }
    else {
        PC_ASSERT(0);
    }
}

static void
edom_fragment_post_process_target(pcintr_stack_t stack,
        struct edom_fragment *fragment, struct pcdom_element *target)
{
    purc_variant_t at = fragment->at;
    if (at != PURC_VARIANT_INVALID) {
        PC_ASSERT(purc_variant_is_type(at, PURC_VARIANT_TYPE_STRING));
        const char *s_at = purc_variant_get_string_const(at);
        PC_ASSERT(s_at);
        if (strcmp(s_at, "textContent") == 0) {
            edom_fragment_post_process_target_content(stack, fragment, target);
        }
        else if (strncmp(s_at, "attr.", 5) == 0) {
            edom_fragment_post_process_target_attr(stack, fragment, target,
                s_at + 5);
        }
        else {
            PC_ASSERT(0); // Not implemented yet
        }
    }
}

static void
edom_fragment_post_process(pcintr_stack_t stack,
        struct edom_fragment *fragment)
{
    purc_variant_t on = fragment->on;
    PC_ASSERT(on != PURC_VARIANT_INVALID);
    PC_ASSERT(purc_variant_is_type(on, PURC_VARIANT_TYPE_NATIVE));
    size_t idx = 0;
    while (1) {
        struct pcdom_element *target;
        target = pcdvobjs_get_element_from_elements(on, idx++);
        if (!target)
            break;
        edom_fragment_post_process_target(stack, fragment, target);
    }
}

static void
edom_fragments_post_process(pcintr_stack_t stack)
{
    struct list_head *edom_fragments = &stack->edom_fragments;
    if (!list_empty(edom_fragments)) {
        struct list_head *p, *n;
        list_for_each_safe(p, n, edom_fragments) {
            struct edom_fragment *curr;
            curr = container_of(p, struct edom_fragment, node);
            list_del(p);
            edom_fragment_post_process(stack, curr);
            edom_fragment_destroy(curr);
        }
    }

    struct pcintr_edom_gen *edom_gen = &stack->edom_gen;
    PC_ASSERT(edom_gen->finished);
    pchtml_html_document_t *doc = edom_gen->doc;
    PC_ASSERT(doc);
    dump_document(doc);
}

static void
release_loaded_var(struct pcintr_loaded_var *p)
{
    if (p) {
        if (p->val != PURC_VARIANT_INVALID) {
            purc_variant_unload_dvobj(p->val);
            p->val = PURC_VARIANT_INVALID;
        }
        if (p->name) {
            free(p->name);
            p->name = NULL;
        }
        if (p->so_path) {
            free(p->so_path);
            p->name = NULL;
        }
    }
}

static void
destroy_loaded_var(struct pcintr_loaded_var *p)
{
    if (p) {
        release_loaded_var(p);
        free(p);
    }
}

static int
unload_dynamic_var(struct rb_node *node, void *ud)
{
    struct rb_root *root = (struct rb_root*)ud;
    struct pcintr_loaded_var *p;
    p = container_of(node, struct pcintr_loaded_var, node);
    pcutils_rbtree_erase(node, root);
    destroy_loaded_var(p);

    return 0;
}

static void
loaded_vars_release(pcintr_stack_t stack)
{
    struct rb_root *root = &stack->loaded_vars;
    if (RB_EMPTY_ROOT(root))
        return;

    int r;
    r = pcutils_rbtree_traverse(root, root, unload_dynamic_var);
    PC_ASSERT(r == 0);
}

static void
stack_release(pcintr_stack_t stack)
{
    struct list_head *edom_fragments = &stack->edom_fragments;
    if (!list_empty(edom_fragments)) {
        struct edom_fragment *p, *n;
        list_for_each_entry_reverse_safe(p, n, edom_fragments, node) {
            list_del(&p->node);
            edom_fragment_destroy(p);
        }
    }

    struct list_head *frames = &stack->frames;
    if (!list_empty(frames)) {
        struct pcintr_stack_frame *p, *n;
        list_for_each_entry_reverse_safe(p, n, frames, node) {
            list_del(&p->node);
            --stack->nr_frames;
            stack_frame_release(p);
            free(p);
        }
        PC_ASSERT(stack->nr_frames == 0);
    }

    if (stack->vdom) {
        if (stack->vdom->timers) {
            pcintr_timers_destroy(stack->vdom->timers);
        }
        vdom_destroy(stack->vdom);
        stack->vdom = NULL;
    }

    if (stack->common_variant_observer_list) {
        pcutils_arrlist_free(stack->common_variant_observer_list);
        stack->common_variant_observer_list = NULL;
    }

    if (stack->dynamic_variant_observer_list) {
        pcutils_arrlist_free(stack->dynamic_variant_observer_list);
        stack->dynamic_variant_observer_list = NULL;
    }

    if (stack->native_variant_observer_list) {
        pcutils_arrlist_free(stack->native_variant_observer_list);
        stack->native_variant_observer_list = NULL;
    }

    edom_gen_release(&stack->edom_gen);
    if (stack->fragment) {
        purc_rwstream_destroy(stack->fragment);
        stack->fragment = NULL;
    }

    loaded_vars_release(stack);
}

static void
stack_destroy(pcintr_stack_t stack)
{
    if (stack) {
        stack_release(stack);
        free(stack);
    }
}

static void
stack_init(pcintr_stack_t stack)
{
    INIT_LIST_HEAD(&stack->frames);
    stack->stage = STACK_STAGE_FIRST_ROUND;
    INIT_LIST_HEAD(&stack->edom_fragments);
    stack->loaded_vars = RB_ROOT;
}

void pcintr_stack_cleanup_instance(struct pcinst* inst)
{
    struct pcintr_heap *heap = &inst->intr_heap;
    struct list_head *coroutines = &heap->coroutines;
    if (list_empty(coroutines))
        return;

    struct list_head *p, *n;
    list_for_each_safe(p, n, coroutines) {
        pcintr_coroutine_t co;
        co = container_of(p, struct pcintr_coroutine, node);
        list_del(p);
        struct pcintr_stack *stack = co->stack;
        stack_destroy(stack);
    }
}


static pcintr_coroutine_t
coroutine_get_current(void)
{
    struct pcinst *inst = pcinst_current();
    struct pcintr_heap *heap = &inst->intr_heap;
    return heap->running_coroutine;
}

static void
coroutine_set_current(struct pcintr_coroutine *co)
{
    struct pcinst *inst = pcinst_current();
    struct pcintr_heap *heap = &inst->intr_heap;
    heap->running_coroutine = co;
}

pcintr_stack_t purc_get_stack(void)
{
    struct pcintr_coroutine *co = coroutine_get_current();
    if (!co)
        return NULL;

    return co->stack;
}

struct pcintr_stack_frame*
pcintr_push_stack_frame(pcintr_stack_t stack)
{
    PC_ASSERT(stack);
    struct pcintr_stack_frame *frame;
    frame = (struct pcintr_stack_frame*)calloc(1, sizeof(*frame));
    if (!frame)
        return NULL;

    purc_variant_t undefined = purc_variant_make_undefined();
    if (undefined == PURC_VARIANT_INVALID) {
        free(frame);
        return NULL;
    }
    for (size_t i=0; i<PCA_TABLESIZE(frame->symbol_vars); ++i) {
        frame->symbol_vars[i] = undefined;
        purc_variant_ref(undefined);
    }
    purc_variant_unref(undefined);

    list_add_tail(&frame->node, &stack->frames);
    ++stack->nr_frames;

    struct pcintr_stack_frame *parent;
    parent = pcintr_stack_frame_get_parent(frame);
    if (parent && parent->result_var) {
        PURC_VARIANT_SAFE_CLEAR(
                frame->symbol_vars[PURC_SYMBOL_VAR_QUESTION_MARK]);
        frame->symbol_vars[PURC_SYMBOL_VAR_QUESTION_MARK] = parent->result_var;
        purc_variant_ref(parent->result_var);
    }

    return frame;
}

void
pcintr_pop_stack_frame(pcintr_stack_t stack)
{
    PC_ASSERT(stack);
    PC_ASSERT(stack->nr_frames > 0);

    struct list_head *tail = stack->frames.prev;
    PC_ASSERT(tail != NULL);
    PC_ASSERT(tail != &stack->frames);

    list_del(tail);

    struct pcintr_stack_frame *frame;
    frame = container_of(tail, struct pcintr_stack_frame, node);

    stack_frame_release(frame);
    free(frame);
    --stack->nr_frames;
}

static int
visit_attr(void *key, void *val, void *ud)
{
    struct pcintr_stack_frame *frame;
    frame = (struct pcintr_stack_frame*)ud;
    if (frame->attr_vars == PURC_VARIANT_INVALID) {
        frame->attr_vars = purc_variant_make_object(0,
                PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
        if (frame->attr_vars == PURC_VARIANT_INVALID)
            return -1;
    }

    struct pcvdom_attr *attr = (struct pcvdom_attr*)val;
    struct pcvcm_node *vcm = attr->val;
    purc_variant_t value;
    if (!vcm) {
        struct pcvdom_element *element = frame->pos;
        D("<%s>attr: [%s:]", element->tag_name, attr->key);
        value = purc_variant_make_undefined();
        if (value == PURC_VARIANT_INVALID) {
            return -1;
        }
    }
    else {
        PC_ASSERT(attr->key == key);
        PC_ASSERT(vcm);

        struct pcvdom_element *element = frame->pos;
        PC_ASSERT(element);
        char *s = pcvcm_node_to_string(vcm, NULL);
        D("<%s>attr: [%s:%s]", element->tag_name, attr->key, s);
        free(s);
        purc_clr_error();

        pcintr_stack_t stack = purc_get_stack();
        PC_ASSERT(stack);
        value = pcvcm_eval(vcm, stack);
        if (value == PURC_VARIANT_INVALID) {
            return -1;
        }
    }

    const struct pchvml_attr_entry *pre_defined = attr->pre_defined;
    bool ok;
    if (pre_defined) {
        ok = purc_variant_object_set_by_static_ckey(frame->attr_vars,
                pre_defined->name, value);
        purc_variant_unref(value);
    }
    else {
        PC_ASSERT(attr->key);
        purc_variant_t k = purc_variant_make_string(attr->key, true);
        if (k == PURC_VARIANT_INVALID) {
            purc_variant_unref(value);
            return -1;
        }
        ok = purc_variant_object_set(frame->attr_vars, k, value);
        purc_variant_unref(value);
        purc_variant_unref(k);
    }

    return ok ? 0 : -1;
}

int
pcintr_element_eval_attrs(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element)
{
    struct pcutils_map *attrs = element->attrs;
    if (!attrs)
        return 0;

    PC_ASSERT(frame->pos == element);

    int r = pcutils_map_traverse(attrs, frame, visit_attr);
    if (r)
        return r;

    return 0;
}

int
pcintr_element_eval_vcm_content(struct pcintr_stack_frame *frame,
        struct pcvdom_element *element)
{
    struct pcvcm_node *vcm_content = element->vcm_content;
    if (vcm_content == NULL)
        return 0;

    char *s = pcvcm_node_to_string(vcm_content, NULL);
    D("<%s>vcm_content: [%s]", element->tag_name, s);
    free(s);
    purc_clr_error();

    purc_variant_t v; /* = pcvcm_eval(vcm_content, stack) */
    // NOTE: element is still the owner of vcm_content
    v = purc_variant_make_ulongint((uint64_t)vcm_content);
    if (v == PURC_VARIANT_INVALID)
        return -1;

    PURC_VARIANT_SAFE_CLEAR(frame->ctnt_var);
    frame->ctnt_var = v;

    return 0;
}

static void
after_pushed(pcintr_coroutine_t co, struct pcintr_stack_frame *frame)
{
    if (frame->ops.after_pushed) {
        frame->ops.after_pushed(co->stack, frame->pos);
    }

    frame->next_step = NEXT_STEP_SELECT_CHILD;
}

static void
on_popping(pcintr_coroutine_t co, struct pcintr_stack_frame *frame)
{
    bool ok = true;
    if (frame->ops.on_popping) {
        ok = frame->ops.on_popping(co->stack, frame->ctxt);
        D("ok: %s", ok ? "true" : "false");
    }

    if (ok) {
        pcintr_stack_t stack = co->stack;
        pcintr_pop_stack_frame(stack);
    } else {
        frame->next_step = NEXT_STEP_RERUN;
    }
}

static void
on_rerun(pcintr_coroutine_t co, struct pcintr_stack_frame *frame)
{
    bool ok = false;
    if (frame->ops.rerun) {
        ok = frame->ops.rerun(co->stack, frame->ctxt);
    }

    PC_ASSERT(ok);

    frame->next_step = NEXT_STEP_SELECT_CHILD;
}

static void
on_select_child(pcintr_coroutine_t co, struct pcintr_stack_frame *frame)
{
    struct pcvdom_element *element = NULL;
    if (frame->ops.select_child) {
        element = frame->ops.select_child(co->stack, frame->ctxt);
    }

    if (element == NULL) {
        frame->next_step = NEXT_STEP_ON_POPPING;
    }
    else {
        frame->next_step = NEXT_STEP_SELECT_CHILD;

        // push child frame
        pcintr_stack_t stack = co->stack;
        struct pcintr_stack_frame *child_frame;
        child_frame = pcintr_push_stack_frame(stack);
        if (!child_frame) {
            pcintr_pop_stack_frame(stack);
            purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
            return;
        }

        child_frame->ops = pcintr_get_ops_by_element(element);
        child_frame->pos = element;
        if (pcvdom_element_is_hvml_native(element)) {
            child_frame->scope = frame->scope;
            child_frame->edom_element = frame->edom_element;
            PC_ASSERT(child_frame->scope);
            // child_frame->scope = element;
        }
        else {
            purc_clr_error();
            child_frame->scope = element;
            child_frame->edom_element = frame->edom_element;
        }
        child_frame->next_step = NEXT_STEP_AFTER_PUSHED;
    }
}

static void
execute_one_step(pcintr_coroutine_t co)
{
    pcintr_stack_t stack = co->stack;
    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(frame);

    if (frame->preemptor) {
        preemptor_f preemptor = frame->preemptor;
        frame->preemptor = NULL;
        preemptor(co, frame);
    }
    else {
        switch (frame->next_step) {
            case NEXT_STEP_AFTER_PUSHED:
                after_pushed(co, frame);
                break;
            case NEXT_STEP_ON_POPPING:
                on_popping(co, frame);
                break;
            case NEXT_STEP_RERUN:
                on_rerun(co, frame);
                break;
            case NEXT_STEP_SELECT_CHILD:
                on_select_child(co, frame);
                break;
            default:
                PC_ASSERT(0);
        }
    }

    PC_ASSERT(co->state == CO_STATE_RUN);
    co->state = CO_STATE_READY;
    bool no_frames = list_empty(&co->stack->frames);
    if (no_frames) {
        if (co->stack->stage == STACK_STAGE_FIRST_ROUND)
            edom_gen_finish(&stack->edom_gen);
        edom_fragments_post_process(stack);
        co->stack->stage = STACK_STAGE_EVENT_LOOP;
        // do not run execute_one_step until event's fired if co->waits > 0
        if (co->waits) { // FIXME:
            co->state = CO_STATE_WAIT;
            return;
        }
        co->state = CO_STATE_TERMINATED;
        D("co terminating: %p", co);
    }
    else {
        frame = pcintr_stack_get_bottom_frame(stack);
        if (frame && frame->preemptor) {
            PC_ASSERT(0); // Not implemented yet
        }
        // continue coroutine even if it's in wait state
    }
}

static void
dump_stack_frame(pcintr_stack_t stack, struct pcintr_stack_frame *frame,
        size_t level)
{
    UNUSED_PARAM(stack);

    if (level == 0) {
        fprintf(stderr, "document\n");
        return;
    }
    pcvdom_element_t scope = frame->scope;
    PC_ASSERT(scope);
    pcvdom_element_t pos = frame->pos;
    for (size_t i=0; i<level; ++i) {
        fprintf(stderr, "  ");
    }
    fprintf(stderr, "scope:<%s>; pos:<%s>\n",
        scope->tag_name, pos ? pos->tag_name : NULL);
}

static void
dump_err_except_info(purc_variant_t err_except_info)
{
    if (purc_variant_is_type(err_except_info, PURC_VARIANT_TYPE_STRING)) {
        fprintf(stderr, "err_except_info: %s\n",
                purc_variant_get_string_const(err_except_info));
    }
    else {
        char buf[1024];
        buf[0] = '\0';
        int r = pcvariant_serialize(buf, sizeof(buf), err_except_info);
        PC_ASSERT(r >= 0);
        if ((size_t)r>=sizeof(buf)) {
            buf[sizeof(buf)-1] = '\0';
            buf[sizeof(buf)-2] = '.';
            buf[sizeof(buf)-3] = '.';
            buf[sizeof(buf)-4] = '.';
        }
        fprintf(stderr, "err_except_info: %s\n", buf);
    }
}

static void
dump_stack(pcintr_stack_t stack)
{
    fprintf(stderr, "dumping stacks of corroutine [%p] ......\n", &stack->co);
    PC_ASSERT(stack);
    fprintf(stderr, "error_except: generated @%s[%d]:%s()\n",
            basename((char*)stack->file), stack->lineno, stack->func);
    purc_atom_t     error_except = stack->error_except;
    purc_variant_t  err_except_info = stack->err_except_info;
    if (error_except) {
        fprintf(stderr, "error_except: %s\n",
                purc_atom_to_string(error_except));
    }
    if (err_except_info) {
        dump_err_except_info(err_except_info);
    }
    fprintf(stderr, "nr_frames: %zd\n", stack->nr_frames);
    struct list_head *frames = &stack->frames;
    size_t level = 0;
    if (!list_empty(frames)) {
        struct list_head *p;
        list_for_each(p, frames) {
            struct pcintr_stack_frame *frame;
            frame = container_of(p, struct pcintr_stack_frame, node);
            dump_stack_frame(stack, frame, level++);
        }
    }
}

static void
dump_c_stack(void)
{
    struct pcinst *inst = pcinst_current();
    fprintf(stderr, "dumping stacks of purc instance [%p]......\n", inst);
    pcinst_dump_stack();
}

static int run_coroutines(void *ctxt)
{
    UNUSED_PARAM(ctxt);

    struct pcinst *inst = pcinst_current();
    struct pcintr_heap *heap = &inst->intr_heap;
    struct list_head *coroutines = &heap->coroutines;
    size_t readies = 0;
    size_t waits = 0;
    if (!list_empty(coroutines)) {
        struct list_head *p, *n;
        list_for_each_safe(p, n, coroutines) {
            struct pcintr_coroutine *co;
            co = container_of(p, struct pcintr_coroutine, node);
            switch (co->state) {
                case CO_STATE_READY:
                    co->state = CO_STATE_RUN;
                    coroutine_set_current(co);
                    pcvariant_push_gc();
                    execute_one_step(co);
                    pcvariant_pop_gc();
                    coroutine_set_current(NULL);
                    ++readies;
                    break;
                case CO_STATE_WAIT:
                    ++waits;
                    break;
                case CO_STATE_RUN:
                    PC_ASSERT(0);
                    break;
                case CO_STATE_TERMINATED:
                    PC_ASSERT(0);
                    break;
                default:
                    PC_ASSERT(0);
            }
            pcintr_stack_t stack = co->stack;
            PC_ASSERT(stack);
            if (stack->except) {
                dump_stack(stack);
                dump_c_stack();
                co->state = CO_STATE_TERMINATED;
            }
            if (co->state == CO_STATE_TERMINATED) {
                stack->stage = STACK_STAGE_TERMINATING;
                list_del(&co->node);
                stack_destroy(stack);
            }
        }
    }

    if (readies) {
        pcintr_coroutine_ready();
    }
    else if (waits==0) {
        pcrunloop_t runloop = pcrunloop_get_current();
        PC_ASSERT(runloop);
        pcrunloop_stop(runloop);
    }

    return 0;
}

void pcintr_coroutine_ready(void)
{
    pcrunloop_t runloop = pcrunloop_get_current();
    PC_ASSERT(runloop);
    pcrunloop_dispatch(runloop, run_coroutines, NULL);
}

struct pcintr_stack_frame*
pcintr_stack_get_bottom_frame(pcintr_stack_t stack)
{
    if (!stack)
        return NULL;

    if (stack->nr_frames < 1)
        return NULL;

    struct list_head *tail = stack->frames.prev;
    return container_of(tail, struct pcintr_stack_frame, node);
}

struct pcintr_stack_frame*
pcintr_stack_frame_get_parent(struct pcintr_stack_frame *frame)
{
    if (!frame)
        return NULL;

    struct list_head *n = frame->node.prev;
    if (!n)
        return NULL;

    return container_of(n, struct pcintr_stack_frame, node);
}

purc_vdom_t
purc_load_hvml_from_string(const char* string)
{
    purc_rwstream_t in;
    in = purc_rwstream_new_from_mem ((void*)string, strlen(string));
    if (!in)
        return NULL;
    purc_vdom_t vdom = purc_load_hvml_from_rwstream(in);
    purc_rwstream_destroy(in);
    return vdom;
}

purc_vdom_t
purc_load_hvml_from_file(const char* file)
{
    purc_rwstream_t in;
    in = purc_rwstream_new_from_file(file, "r");
    if (!in)
        return NULL;
    purc_vdom_t vdom = purc_load_hvml_from_rwstream(in);
    purc_rwstream_destroy(in);
    return vdom;
}

PCA_EXPORT purc_vdom_t
purc_load_hvml_from_url(const char* url)
{
    UNUSED_PARAM(url);
    PC_ASSERT(0); // Not implemented yet
    return NULL;
}

static struct pcvdom_document*
load_document(purc_rwstream_t in)
{
    struct pchvml_parser *parser = NULL;
    struct pcvdom_gen *gen = NULL;
    struct pcvdom_document *doc = NULL;
    struct pchvml_token *token = NULL;
    parser = pchvml_create(0, 0);
    if (!parser)
        goto error;

    gen = pcvdom_gen_create();
    if (!gen)
        goto error;

again:
    if (token)
        pchvml_token_destroy(token);

    token = pchvml_next_token(parser, in);
    if (!token)
        goto error;

    if (pcvdom_gen_push_token(gen, parser, token))
        goto error;

    if (!pchvml_token_is_type(token, PCHVML_TOKEN_EOF)) {
        goto again;
    }

    doc = pcvdom_gen_end(gen);
    goto end;

error:
    doc = pcvdom_gen_end(gen);
    if (doc) {
        pcvdom_document_destroy(doc);
        doc = NULL;
    }

end:
    if (token)
        pchvml_token_destroy(token);

    if (gen)
        pcvdom_gen_destroy(gen);

    if (parser)
        pchvml_destroy(parser);

    return doc;
}

#define BUILDIN_VAR_HVML        "HVML"
#define BUILDIN_VAR_SYSTEM      "SYSTEM"
#define BUILDIN_VAR_T           "T"
#define BUILDIN_VAR_DOC         "DOC"
#define BUILDIN_VAR_SESSION     "SESSION"
#define BUILDIN_VAR_EJSON       "EJSON"

static bool
bind_doc_named_variable(pcintr_stack_t stack, const char* name,
        purc_variant_t var)
{
    if (var == PURC_VARIANT_INVALID) {
        return false;
    }

    if (!pcintr_bind_document_variable(stack->vdom, name, var)) {
        purc_variant_unref(var);
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return false;
    }
    purc_variant_unref(var);
    return true;
}

static bool
init_buidin_doc_variable(pcintr_stack_t stack)
{
    // $TIMERS
    stack->vdom->timers = pcintr_timers_init(stack);
    if (!stack->vdom->timers) {
        return false;
    }

    // $HVML
    if(!bind_doc_named_variable(stack, BUILDIN_VAR_HVML, pcdvobjs_get_hvml())) {
        return false;
    }

    // $SYSTEM
    if(!bind_doc_named_variable(stack, BUILDIN_VAR_SYSTEM,
                pcdvobjs_get_system())) {
        return false;
    }

    // $T
    if(!bind_doc_named_variable(stack, BUILDIN_VAR_T,
                pcdvobjs_get_t())) {
        return false;
    }

    // $DOC
    pchtml_html_document_t *doc = stack->edom_gen.doc;
    pcdom_document_t *document = (pcdom_document_t*)doc;
    if(!bind_doc_named_variable(stack, BUILDIN_VAR_DOC,
                pcdvobjs_make_doc_variant(document))) {
        return false;
    }

    // TODO : bind by  purc_bind_variable
    // begin
    // $SESSION
    if(!bind_doc_named_variable(stack, BUILDIN_VAR_SESSION,
                pcdvobjs_get_session())) {
        return false;
    }

    // $EJSON
    if(!bind_doc_named_variable(stack, BUILDIN_VAR_EJSON,
                pcdvobjs_get_ejson())) {
        return false;
    }
    // end

    return true;
}

purc_vdom_t
purc_load_hvml_from_rwstream(purc_rwstream_t stream)
{
    struct pcvdom_document *doc = NULL;
    doc = load_document(stream);
    if (!doc)
        return NULL;

    purc_vdom_t vdom = (purc_vdom_t)calloc(1, sizeof(*vdom));
    if (!vdom) {
        pcvdom_document_destroy(doc);
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    vdom->document = doc;

    pcintr_stack_t stack = (pcintr_stack_t)calloc(1, sizeof(*stack));
    if (!stack) {
        vdom_destroy(vdom);
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    stack_init(stack);

    stack->vdom = vdom;
    stack->co.stack = stack;
    stack->co.state = CO_STATE_READY;

    if (edom_gen_init(&stack->edom_gen)) {
        stack_destroy(stack);
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    if(!init_buidin_doc_variable(stack)) {
        stack_destroy(stack);
        return NULL;
    }

    struct pcintr_stack_frame *frame;
    frame = pcintr_push_stack_frame(stack);
    if (!frame) {
        stack_destroy(stack);
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    // frame->next_step = on_vdom_start;
    frame->ops = *pcintr_get_document_ops();

    struct pcinst *inst = pcinst_current();
    struct pcintr_heap *heap = &inst->intr_heap;
    struct list_head *coroutines = &heap->coroutines;
    list_add_tail(&stack->co.node, coroutines);

    pcintr_coroutine_ready();

    // FIXME: double-free, potentially!!!
    return vdom;
}

bool
purc_run(purc_variant_t request, purc_event_handler handler)
{
    UNUSED_PARAM(request);
    UNUSED_PARAM(handler);

    pcfetcher_init(10, 1024);
    pcrunloop_run();
    pcfetcher_term();

    return true;
}

int
pcintr_printf_to_edom(pcintr_stack_t stack, const char *fmt, ...)
{
    // TODO: 1. alloc?; 2. two-scans?
    char buf[1024];

    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    PC_ASSERT(r >= 0 && (size_t)r < sizeof(buf));
    va_end(ap);

    ssize_t sz;
    sz = edom_gen_write(&stack->edom_gen, buf, r);
    PC_ASSERT(sz == 0);
    return 0;
}

int
pcintr_printf_to_fragment(pcintr_stack_t stack,
        purc_variant_t on, purc_variant_t to, purc_variant_t at,
        const char *fmt, ...)
{
    PC_ASSERT(stack->fragment);

    struct edom_fragment *fragment;
    fragment = (struct edom_fragment*)calloc(1, sizeof(*fragment));
    fragment->on = on;
    purc_variant_ref(on);
    fragment->to = to;
    if (to != PURC_VARIANT_INVALID)
        purc_variant_ref(to);
    fragment->at = at;
    if (at != PURC_VARIANT_INVALID)
        purc_variant_ref(at);

    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(frame);
    PC_ASSERT(frame->edom_element);

    va_list ap, ap_dup;
    va_start(ap, fmt);
    va_copy(ap_dup, ap);
    int r = vsnprintf(NULL, 0, fmt, ap);
    PC_ASSERT(r >= 0);
    va_end(ap);

    char *buf = (char*)malloc(r+1);
    if (!buf) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    r = vsnprintf(buf, r+1, fmt, ap_dup);
    PC_ASSERT(r >= 0);

    fragment->content = buf;

    list_add_tail(&fragment->node, &stack->edom_fragments);
    return 0;
}

int
pcintr_printf_start_element_to_edom(pcintr_stack_t stack)
{
    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(frame);

    struct pcvdom_element *element = frame->scope;
    PC_ASSERT(element);

    int r = pcintr_printf_to_edom(stack, "<%s", element->tag_name);
    PC_ASSERT(r == 0);

    if (frame->attr_vars) {
        purc_variant_t k, v;
        foreach_key_value_in_variant_object(frame->attr_vars, k, v)
            // fprintf(stderr, "=========[%s]=======\n", pcvariant_typename(k));
            const char *key = purc_variant_get_string_const(k);
            PC_ASSERT(key);
            pcintr_printf_to_edom(stack, " %s", key);

            const char *val = purc_variant_get_string_const(v);
            if (val) {
                // FIXME: escape???
                pcintr_printf_to_edom(stack, "='%s'", val);
            }
        end_foreach;
    }

    if (element->self_closing) {
        return pcintr_printf_to_edom(stack, "/>");
    }
    else {
        return pcintr_printf_to_edom(stack, ">");
    }
}

int
pcintr_printf_end_element_to_edom(pcintr_stack_t stack)
{
    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(frame);

    struct pcvdom_element *element = frame->scope;
    PC_ASSERT(element);

    if (element->self_closing) {
        return 0;
    }
    else {
        return pcintr_printf_to_edom(stack, "</%s>", element->tag_name);
    }
}

int
pcintr_printf_vcm_content_to_edom(pcintr_stack_t stack, purc_variant_t vcm)
{
    PC_ASSERT(purc_variant_is_type(vcm, PURC_VARIANT_TYPE_ULONGINT));
    bool ok;
    uint64_t u64;
    ok = purc_variant_cast_to_ulongint(vcm, &u64, false);
    PC_ASSERT(ok);

    struct pcvcm_node *vcm_content;
    vcm_content = (struct pcvcm_node*)u64;
    PC_ASSERT(vcm_content);

    purc_variant_t v = pcvcm_eval(vcm_content, stack);
    if (v == PURC_VARIANT_INVALID)
        return -1;

    const char *s = purc_variant_get_string_const(v);
    int r = pcintr_printf_to_edom(stack, "%s", s);
    purc_variant_unref(v);
    if (r)
        return -1;

    return 0;
}

int
pcintr_set_symbol_var_at_sign(void)
{
    pcintr_stack_t stack = purc_get_stack();
    PC_ASSERT(stack);

    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(frame);
    PC_ASSERT(frame->scope);

    // purc_variant_t at = pcdvobjs_make_element_variant(frame->edom_element);
    purc_variant_t at = pcdvobjs_make_elements(frame->edom_element);
    if (at == PURC_VARIANT_INVALID)
        return -1;
    PURC_VARIANT_SAFE_CLEAR(frame->symbol_vars[PURC_SYMBOL_VAR_AT_SIGN]);
    frame->symbol_vars[PURC_SYMBOL_VAR_AT_SIGN] = at;

    return 0;
}

static bool
set_object_by(purc_variant_t obj, struct pcintr_dynamic_args *arg)
{
    purc_variant_t dynamic;
    dynamic = purc_variant_make_dynamic(arg->getter, arg->setter);
    if (dynamic == PURC_VARIANT_INVALID)
        return false;

    bool ok = purc_variant_object_set_by_static_ckey(obj, arg->name, dynamic);
    if (!ok) {
        purc_variant_unref(dynamic);
        return false;
    }

    return true;
}

purc_variant_t
pcintr_make_object_of_dynamic_variants(size_t nr_args,
    struct pcintr_dynamic_args *args)
{
    purc_variant_t obj;
    obj = purc_variant_make_object_by_static_ckey(0,
            NULL, PURC_VARIANT_INVALID);

    if (obj == PURC_VARIANT_INVALID)
        return PURC_VARIANT_INVALID;

    for (size_t i=0; i<nr_args; ++i) {
        struct pcintr_dynamic_args *arg = args + i;
        if (!set_object_by(obj, arg)) {
            purc_variant_unref(obj);
            return false;
        }
    }

    return obj;
}

int add_observer_into_list(struct pcutils_arrlist* list,
        struct pcintr_observer* observer)
{
    observer->list = list;
    return pcutils_arrlist_add(list, observer);
}

void del_observer_from_list(struct pcutils_arrlist* list,
        struct pcintr_observer* observer)
{
    size_t n = pcutils_arrlist_length(list);
    int pos = -1;
    for (size_t i = 0; i < n; i++) {
        if (observer == pcutils_arrlist_get_idx(list, i)) {
            pos = i;
            break;
        }
    }

    if (pos > -1) {
        pcutils_arrlist_del_idx(list, pos, 1);
    }
}

void observer_free_func(void *data)
{
    if (data) {
        struct pcintr_observer* observer = (struct pcintr_observer*)data;
        if (observer->listener) {
            purc_variant_revoke_listener(observer->observed,
                    observer->listener);
        }
        free(observer->msg_type);
        free(observer->sub_type);
        free(observer);
    }
}

struct pcintr_observer*
pcintr_register_observer(purc_variant_t observed,
        purc_variant_t for_value, pcvdom_element_t scope,
        pcdom_element_t *edom_element,
        pcvdom_element_t pos,
        struct pcvar_listener* listener
        )
{
    UNUSED_PARAM(for_value);

    pcintr_stack_t stack = purc_get_stack();
    struct pcutils_arrlist* list = NULL;
    if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_DYNAMIC)) {
        if (stack->dynamic_variant_observer_list == NULL) {
            stack->dynamic_variant_observer_list = pcutils_arrlist_new(
                    observer_free_func);
        }
        list = stack->dynamic_variant_observer_list;
    }
    else if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_NATIVE)) {
        if (stack->native_variant_observer_list == NULL) {
            stack->native_variant_observer_list = pcutils_arrlist_new(
                    observer_free_func);
        }
        list = stack->native_variant_observer_list;
    }
    else {
        if (stack->common_variant_observer_list == NULL) {
            stack->common_variant_observer_list = pcutils_arrlist_new(
                    observer_free_func);
        }
        list = stack->common_variant_observer_list;
    }

    if (!list) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    const char* for_value_str = purc_variant_get_string_const(for_value);
    char* value = strdup(for_value_str);
    if (!value) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    char* p = value;
    char* msg_type = strtok_r(p, ":", &p);
    if (!msg_type) {
        //TODO : purc_set_error();
        free(value);
        return NULL;
    }

    char* sub_type = strtok_r(p, ":", &p);

    struct pcintr_observer* observer =  (struct pcintr_observer*)calloc(1,
            sizeof(struct pcintr_observer));
    if (!observer) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        free(value);
        return NULL;
    }
    observer->observed = observed;
    observer->scope = scope;
    observer->edom_element = edom_element;
    observer->pos = pos;
    observer->msg_type = strdup(msg_type);
    observer->sub_type = sub_type ? strdup(sub_type) : NULL;
    observer->listener = listener;
    add_observer_into_list(list, observer);

    free(value);
    return observer;
}

bool
pcintr_revoke_observer(struct pcintr_observer* observer)
{
    if (!observer) {
        return true;
    }

    del_observer_from_list(observer->list, observer);
    return true;
}

struct pcintr_observer*
pcintr_find_observer(pcintr_stack_t stack, purc_variant_t observed,
        purc_variant_t msg_type, purc_variant_t sub_type)
{
    if (observed == PURC_VARIANT_INVALID ||
            msg_type == PURC_VARIANT_INVALID) {
        return NULL;
    }
    const char* msg = purc_variant_get_string_const(msg_type);
    const char* sub = (sub_type != PURC_VARIANT_INVALID) ?
        purc_variant_get_string_const(sub_type) : NULL;

    struct pcutils_arrlist* list = NULL;
    if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_DYNAMIC)) {
        list = stack->dynamic_variant_observer_list;
    }
    else if (purc_variant_is_type(observed, PURC_VARIANT_TYPE_NATIVE)) {
        list = stack->native_variant_observer_list;
    }
    else {
        list = stack->common_variant_observer_list;
    }

    if (!list) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    size_t n = pcutils_arrlist_length(list);
    for (size_t i = 0; i < n; i++) {
        struct pcintr_observer* observer = pcutils_arrlist_get_idx(list, i);
        if (observer->observed == observed &&
                (strcmp(observer->msg_type, msg) == 0) &&
                (
                 (observer->sub_type && strcmp(observer->sub_type, sub) == 0) ||
                 (observer->sub_type == sub)
                 )
                ) {
            return observer;
        }
    }
    return NULL;
}

bool
pcintr_is_observer_empty(pcintr_stack_t stack)
{
    if (!stack) {
        return false;
    }

    if (stack->native_variant_observer_list
            && pcutils_arrlist_length(stack->native_variant_observer_list)) {
        return false;
    }

    if (stack->dynamic_variant_observer_list
            && pcutils_arrlist_length(stack->dynamic_variant_observer_list)) {
        return false;
    }

    if (stack->common_variant_observer_list
            && pcutils_arrlist_length(stack->common_variant_observer_list)) {
        return false;
    }
    return true;
}

struct pcintr_message {
    pcintr_stack_t stack;
    purc_variant_t source;
    purc_variant_t type;
    purc_variant_t sub_type;
    purc_variant_t extra;
};

struct pcintr_message*
pcintr_message_create(pcintr_stack_t stack, purc_variant_t source,
        purc_variant_t type, purc_variant_t sub_type, purc_variant_t extra)
{
    struct pcintr_message* msg = (struct pcintr_message*)malloc(
            sizeof(struct pcintr_message));
    msg->stack = stack;

    msg->source = source;
    purc_variant_ref(msg->source);

    msg->type = type;
    purc_variant_ref(msg->type);

    msg->sub_type = sub_type;
    if (sub_type != PURC_VARIANT_INVALID) {
        purc_variant_ref(msg->sub_type);
    }

    msg->extra = extra;
    if (extra != PURC_VARIANT_INVALID) {
        purc_variant_ref(msg->extra);
    }
    return msg;
}

void
pcintr_message_destroy(struct pcintr_message* msg)
{
    if (msg) {
        purc_variant_unref(msg->source);
        purc_variant_unref(msg->type);
        purc_variant_unref(msg->sub_type);
        purc_variant_unref(msg->extra);
        free(msg);
    }
}

static int
pcintr_handle_message(void *ctxt)
{
    struct pcintr_message* msg = (struct pcintr_message*) ctxt;

    struct pcintr_observer* observer = pcintr_find_observer(msg->stack,
            msg->source, msg->type, msg->sub_type);
    if (observer == NULL) {
        return 0;
    }

    // FIXME:
    // push stack frame
    pcintr_stack_t stack = msg->stack;
    struct pcintr_stack_frame *frame;
    frame = pcintr_push_stack_frame(stack);
    if (!frame) {
        pcintr_pop_stack_frame(stack);
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    frame->ops = pcintr_get_ops_by_element(observer->pos);
    frame->scope = observer->scope;
    frame->pos = observer->pos;
    frame->edom_element = observer->edom_element;
    frame->next_step = NEXT_STEP_AFTER_PUSHED;

    stack->co.state = CO_STATE_READY;
    pcintr_coroutine_ready();
    return 0;
}

void
pcintr_dispatch_message(pcintr_stack_t stack, purc_variant_t source,
        purc_variant_t type, purc_variant_t sub_type, purc_variant_t extra)
{
    struct pcintr_message* msg = pcintr_message_create(stack, source, type,
            sub_type, extra);

    pcrunloop_t runloop = pcrunloop_get_current();
    PC_ASSERT(runloop);
    pcrunloop_dispatch(runloop, pcintr_handle_message, msg);
}

void
pcintr_set_base_uri(const char* base_uri)
{
    pcfetcher_set_base_url(base_uri);
}

purc_variant_t
pcintr_load_from_uri(const char* uri)
{
    if (uri == NULL) {
        return PURC_VARIANT_INVALID;
    }

    purc_variant_t ret = PURC_VARIANT_INVALID;
    struct pcfetcher_resp_header resp_header = {0};
    purc_rwstream_t resp = pcfetcher_request_sync(
            uri,
            PCFETCHER_REQUEST_METHOD_GET,
            NULL,
            10,
            &resp_header);
    if (resp_header.ret_code == 200) {
        size_t sz_content = 0;
        char* buf = (char*)purc_rwstream_get_mem_buffer(resp, &sz_content);
        // FIXME:
        purc_clr_error();
        ret = purc_variant_make_from_json_string(buf, sz_content);
        purc_rwstream_destroy(resp);
    }

    if (resp_header.mime_type) {
        free(resp_header.mime_type);
    }
    return ret;
}

#define DOC_QUERY         "query"

purc_variant_t
pcintr_doc_query(purc_vdom_t vdom, const char* css)
{
    purc_variant_t ret = PURC_VARIANT_INVALID;
    if (vdom == NULL || css == NULL) {
        goto end;
    }

    purc_variant_t doc = pcvdom_document_get_variable(vdom, BUILDIN_VAR_DOC);
    if (doc == PURC_VARIANT_INVALID) {
        PC_ASSERT(0);
        goto end;
    }

    struct purc_native_ops *ops = purc_variant_native_get_ops (doc);
    if (ops == NULL) {
        PC_ASSERT(0);
        goto end;
    }

    purc_nvariant_method native_func = ops->property_getter(DOC_QUERY);
    if (!native_func) {
        PC_ASSERT(0);
        goto end;
    }

    purc_variant_t arg = purc_variant_make_string(css, false);
    if (arg == PURC_VARIANT_INVALID) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        goto end;
    }

    ret = native_func (purc_variant_native_get_entity(doc), 1, &arg);
    purc_variant_unref(arg);
end:
    return ret;
}

bool
pcintr_load_dynamic_variant(pcintr_stack_t stack,
    const char *name, size_t len)
{
    char NAME[PATH_MAX+1];
    snprintf(NAME, sizeof(NAME), "%.*s", (int)len, name);

    struct rb_root *root = &stack->loaded_vars;

    struct rb_node **pnode = &root->rb_node;
    struct rb_node *parent = NULL;
    struct rb_node *entry = NULL;
    while (*pnode) {
        struct pcintr_loaded_var *p;
        p = container_of(*pnode, struct pcintr_loaded_var, node);

        int ret = strcmp(NAME, p->name);

        parent = *pnode;

        if (ret < 0)
            pnode = &parent->rb_left;
        else if (ret > 0)
            pnode = &parent->rb_right;
        else{
            return true;
        }
    }

    char so[PATH_MAX+1];
    int n = snprintf(so, sizeof(so), "libpurc-dvobj-%.*s.so", (int)len, name);
    if (n<0 || (size_t)n >= sizeof(so)) {
        purc_set_error(PURC_ERROR_OVERFLOW);
        return false;
    }

    struct pcintr_loaded_var *p = NULL;

    purc_variant_t v = purc_variant_load_dvobj_from_so(so, NAME);
    if (v == PURC_VARIANT_INVALID)
        return false;

    p = (struct pcintr_loaded_var*)calloc(1, sizeof(*p));
    if (!p) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        goto error;
    }

    p->val = v;

    p->name = strdup(NAME);
    p->so_path = strdup(so);
    if (!p->name || !p->so_path) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        goto error;
    }

    entry = &p->node;

    pcutils_rbtree_link_node(entry, parent, pnode);
    pcutils_rbtree_insert_color(entry, root);

    if (pcintr_bind_document_variable(stack->vdom, NAME, v)) {
        return true;
    }

error:
    destroy_loaded_var(p);

    return false;
}

