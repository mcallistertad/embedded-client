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
        return "op:match_cache";
    case SKY_OP_ADD_TO_CACHE:
        return "op:add_to_cache";
    case SKY_OP_EQUAL:
        return "op:equal";
    default:
        return "?";
    }
}
#endif

void log_plugin(Sky_ctx_t *ctx, Sky_plugin_table_t *p, sky_operation_t op, char *str)
{
#ifdef VERBOSE_DEBUG
    logfmt(sky_basename(p->name), str_plugin_op(op), ctx, SKY_LOG_LEVEL_DEBUG, "%s", str);
#endif
}

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
    fprintf(stderr, "IN PLUGIN ADD\n");
    Sky_plugin_table_t **p = root;

    if (!root)
        return SKY_ERROR;

    /* TODO add table to end of chain */
    while (p) {
        fprintf(stderr, "IN WHILE\n");
        if (*p == NULL) {
            *p = table;
            break;
        }
        p = (Sky_plugin_table_t **)*p;
    }

    return SKY_SUCCESS;
}

/*! \brief call the nth operation in the appropriate plugin
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param op the operation index
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_call(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, sky_operation_t n, ...)
{
    va_list argp;
    Sky_plugin_table_t *p = ctx->plugin;
    Sky_status_t ret = SKY_ERROR;

    va_start(argp, n);

    if (!validate_workspace(ctx)) {
        log_plugin(ctx, p, n, "invalid workspace");
        return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }
    if (!p || !p->name) {
        log_plugin(ctx, p, n, "invalid plugin");
        return sky_return(sky_errno, SKY_ERROR_NO_PLUGIN);
    }

    /* The following determines how the operation is called:
         *   SKY_OP_NAME             - get pointer to name of plugin
         *   SKY_OP_EQUAL            - All plugins called until -1, can't compare, 0 better one indicated, 1 same
         *   SKY_OP_CACHE_MATCH      - All plugins called until success if cacheline index returned
         *   SKY_OP_REMOVE_WORST     - All plugins called until success if one removed
         *   SKY_OP_ADD_TO_CACHE     - All plugins called until success if cache updated
         */
    switch (n) {
    case SKY_OP_EQUAL: {
        Beacon_t *a = va_arg(argp, Beacon_t *);
        Beacon_t *b = va_arg(argp, Beacon_t *);
        Sky_beacon_property_t *prop = va_arg(argp, Sky_beacon_property_t *);

        while (p) {
            log_plugin(ctx, p, n, "calling plugin");
            ret = (*p->equal)(ctx, a, b, prop);
            log_plugin(ctx, p, n,
                (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
            if (ret != SKY_ERROR)
                break;
            p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
        }
        return ret;
    }
    case SKY_OP_ADD_TO_CACHE: {
        Sky_location_t *loc = va_arg(argp, Sky_location_t *);

        while (p) {
            log_plugin(ctx, p, n, "calling plugin");
            ret = (*p->add_to_cache)(ctx, loc);
            log_plugin(ctx, p, n,
                (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
            if (ret != SKY_ERROR)
                break;
            p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
        }
        return ret;
    }
    case SKY_OP_CACHE_MATCH: {
        int *arg = va_arg(argp, int *);

        while (p) {
            log_plugin(ctx, p, n, "calling plugin");
            ret = (*p->match_cache)(ctx, arg);
            log_plugin(ctx, p, n,
                (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
            if (ret != SKY_ERROR)
                break;
            p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
        }
        return ret;
    }
    case SKY_OP_REMOVE_WORST: {
        while (p) {
            log_plugin(ctx, p, n, "calling plugin");
            ret = (*p->remove_worst)(ctx);
            log_plugin(ctx, p, n,
                (ret == SKY_SUCCESS) ? "Success" : (ret == SKY_FAILURE) ? "Failure" : "Error");
            if (ret != SKY_ERROR)
                break;
            p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
        }
        return ret;
    }
    default:
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }
    return ret;
}
