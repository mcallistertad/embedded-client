/*! \file plugins/plugin.c
 *  \brief utilities - Skyhook Embedded Library
 *
 * Copyright (c) 2020 Skyhook, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#define SKY_LIBEL
#include "libel.h"

/* Uncomment VERBOSE_DEBUG to enable extra logging */
// #define VERBOSE_DEBUG

/*! \brief add a plugin table to the list of plugins
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param table the table for the next plugin to add
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_add(Sky_plugin_table_t **root, Sky_plugin_table_t *table)
{
    Sky_plugin_table_t *p;

    /* check args are sane */
    if (table->magic != SKY_MAGIC || root == NULL)
        return SKY_ERROR;

    if (*root == NULL) { /* if list was empty, add first entry */
        *root = table;
        table->next = NULL;
        p = table;
    } else
        p = *root; /* otherwise pick up pointer to first table */

    /* find end of list of plugins */
    while (p) {
        if (p->magic != SKY_MAGIC)
            /* table seems corrupt */
            return SKY_ERROR;
        if (p == table)
            /* if plugin already registered, do nothing */
            return SKY_SUCCESS;
        if (p->next == NULL) {
            /* add new table to end of linked list */
            p->next = table;
            /* mark new end of linked list */
            table->next = NULL;
            break;
        }
        /* keep looking for end of linked list */
        p = p->next;
    }

    return SKY_SUCCESS;
}

/*! \brief call the equal operation in the registered plugins
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param op the operation index
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_equal(
    Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *a, Beacon_t *b, Sky_beacon_property_t *prop)
{
    Sky_plugin_table_t *p = ctx->plugin;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_workspace(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid workspace");
        return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    while (p) {
        ret = (*p->equal)(ctx, a, b, prop);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
#endif
        if (ret != SKY_ERROR)
            break;
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return ret;
}

/*! \brief call the remove_worst operation in the registered plugins
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_remove_worst(Sky_ctx_t *ctx, Sky_errno_t *sky_errno)
{
    Sky_plugin_table_t *p = ctx->plugin;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_workspace(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid workspace");
        return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    while (p) {
        ret = (*p->remove_worst)(ctx);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
#endif
        if (ret != SKY_ERROR)
            break;
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return ret;
}

/*! \brief call the cache_match operation in the registered plugins
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param op the operation index
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_cache_match(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int *idx)
{
    Sky_plugin_table_t *p = ctx->plugin;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_workspace(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid workspace");
        return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    while (p) {
        ret = (*p->cache_match)(ctx, idx);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
#endif
        if (ret != SKY_ERROR)
            break;
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return ret;
}

/*! \brief call the add_to_cache operation in the registered plugins
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param op the operation index
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_add_to_cache(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Sky_location_t *loc)
{
    Sky_plugin_table_t *p = ctx->plugin;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_workspace(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid workspace");
        return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    while (p) {
        ret = (*p->add_to_cache)(ctx, loc);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
#endif
        if (ret != SKY_ERROR)
            break;
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return ret;
}

#ifdef UNITTESTS

BEGIN_TESTS(plugin_test)
    MOCK_SKY_CTX(ctx);

    GROUP("sky_plugin_add");

    TEST("should return SKY_ERROR if table is corrupt (magic != SKY_MAGIC)");
    {
        Sky_plugin_table_t *root = NULL;
        Sky_plugin_table_t table;
        table.magic = 0;
        ASSERT( SKY_ERROR == sky_plugin_add(&root, &table) &&
                SKY_ERROR == sky_plugin_add(&root, (Sky_plugin_table_t *)ctx) );
    }

    TEST("should return SKY_ERROR if table or root pointer is NULL");
    {
        Sky_plugin_table_t *root = NULL;
        Sky_plugin_table_t table;
        ASSERT( SKY_ERROR == sky_plugin_add(NULL, NULL)  &&
                SKY_ERROR == sky_plugin_add(&root, NULL)  &&
                SKY_ERROR == sky_plugin_add(NULL, &table) );
    }

    TEST("should return SKY_SUCCESS if table is added twice to same list");
    {
        Sky_plugin_table_t *root = NULL;
        Sky_plugin_table_t table = {
            .next = NULL,
            .magic = SKY_MAGIC,
            .name = __FILE__,
        };

        ASSERT( SKY_SUCCESS == sky_plugin_add(&root, &table) &&
                SKY_SUCCESS == sky_plugin_add(&root, &table) );
    }

    CLOSE_SKY_CTX(ctx);

END_TESTS();

#endif
