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

#ifdef VERBOSE_DEBUG
static char *str_plugin_op(sky_operation_t op)
{
    switch (op) {
    case SKY_OP_REMOVE_WORST:
        return "op:remove_worst";
    case SKY_OP_CACHE_MATCH:
        return "op:cache_match";
    case SKY_OP_ADD_TO_CACHE:
        return "op:add_to_cache";
    case SKY_OP_EQUAL:
        return "op:equal";
    default:
        return "?";
    }
}
#endif

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
    Sky_plugin_table_t **p = root;

    if (!root)
        return SKY_ERROR;

    /* TODO add table to end of chain */
    while (p) {
        if (*p == NULL) {
            *p = table;
            break;
        }
        p = (Sky_plugin_table_t **)*p;
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
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "calling %s", p->name);
        ret = (*p->equal)(ctx, a, b, prop);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
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
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "calling %s", p->name);
        ret = (*p->remove_worst)(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
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
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "calling %s", p->name);
        ret = (*p->cache_match)(ctx, idx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
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
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "calling %s", p->name);
        ret = (*p->add_to_cache)(ctx, loc);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
        if (ret != SKY_ERROR)
            break;
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return ret;
}
