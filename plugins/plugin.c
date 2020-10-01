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
// #define VERBOSE_DEBUG 1

/*! \brief add a plugin table to the list of plugins
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param table the table for the next plugin to add
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_init(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Sky_plugin_op_t *table)
{
    if (!validate_workspace(ctx))
        return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    if (!table)
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* TODO add table to end of chain */
    ctx->plugin = table;

    debug_plugin(ctx);

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
    Sky_plugin_op_t *p = ctx->plugin;

    va_start(argp, n);

    if (!validate_workspace(ctx))
        return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    if (!p)
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* The following determines how the operation is called:
     *   SKY_OP_COMPARE          - All plugins called, -1, can't compare, 0 better one indicated, 1 same
     *   SKY_OP_REMOVE_WORST     - All plugins called, success if one removed
     *   SKY_OP_SCORE_CACHELINE  - All plugins called, success if cacheline index returned
     *   SKY_OP_ADD_TO_CACHE     - All pligins called, success if cache updated
     */
    switch (n) {
    case SKY_OP_NAME: {
        char *buf = va_arg(argp, char *);
        int len = va_arg(argp, int);

        return (*p[n])(ctx, buf, len);
    }
    case SKY_OP_EQUAL: {
        Beacon_t *a = va_arg(argp, Beacon_t *);
        Beacon_t *b = va_arg(argp, Beacon_t *);
        int *diff = va_arg(argp, int *);

        return (*p[n])(ctx, a, b, diff);
    }
    case SKY_OP_REMOVE_WORST:
    case SKY_OP_SCORE_CACHELINE:
    case SKY_OP_ADD_TO_CACHE:
        return (*p[n])(ctx);
    default:
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }
    return SKY_SUCCESS;
}
