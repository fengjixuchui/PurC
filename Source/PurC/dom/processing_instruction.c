/**
 * @file processing_instruction.c 
 * @author
 * @date 2021/07/02
 * @brief The complementation of processing instruction.
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
 * This implementation of HTML parser is derived from Lexbor
 * <https://github.com/lexbor/lexbor>, which is licensed under the Apache
 * License, Version 2.0:
 *
 * Copyright (C) 2018-2020 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */


#include "private/dom.h"

pcdom_processing_instruction_t *
pcdom_processing_instruction_interface_create(pcdom_document_t *document)
{
    pcdom_processing_instruction_t *element;

    element = pcutils_mraw_calloc(document->mraw,
                                 sizeof(pcdom_processing_instruction_t));
    if (element == NULL) {
        return NULL;
    }

    pcdom_node_t *node = pcdom_interface_node(element);

    node->owner_document = pcdom_document_owner(document);
    node->type = PCDOM_NODE_TYPE_PROCESSING_INSTRUCTION;

    return element;
}

pcdom_processing_instruction_t *
pcdom_processing_instruction_interface_destroy(pcdom_processing_instruction_t *processing_instruction)
{
    return pcutils_mraw_free(
        pcdom_interface_node(processing_instruction)->owner_document->mraw,
        processing_instruction);
}
