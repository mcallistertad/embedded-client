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
    if (table == NULL || table->magic != SKY_MAGIC || root == NULL)
        return SKY_ERROR;

    if (*root == NULL) { /* if list was empty, add first entry */
        *root = table;
        table->next = NULL;
        p = table;
        return SKY_SUCCESS;
    }
    p = *root; /* pick up first entry */

    /* find end of list of plugins */
    while (p) {
        if (p->magic != SKY_MAGIC) {
            /* table seems corrupt */
            return SKY_ERROR;
        } else if (p == table) {
            /* if plugin already registered, do nothing */
            return SKY_SUCCESS;
        } else if (p->next == NULL) {
            /* add new table to end of linked list */
            p->next = table;
            /* mark new end of linked list */
            table->next = NULL;
            return SKY_SUCCESS;
        }
        /* keep looking for end of linked list */
        p = p->next;
    }

    /* should never get here */
    return SKY_ERROR;
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
    Sky_plugin_table_t *p;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_workspace(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid workspace");
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    p = ctx->plugin;
    while (p) {
        if (p->equal)
            ret = p->equal(ctx, a, b, prop);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" :
            (ret == SKY_FAILURE) ? "Failure" :
                                   "Error");
#endif
        if (ret != SKY_ERROR) {
            set_error_status(sky_errno, SKY_ERROR_NONE);
            return ret;
        }
        p = p->next; /* move on to next plugin */
    }
    return set_error_status(sky_errno, SKY_ERROR_NO_PLUGIN);
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
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    while (p) {
        if (p->remove_worst)
            ret = (*p->remove_worst)(ctx);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" :
            (ret == SKY_FAILURE) ? "Failure" :
                                   "Error");
#endif
        if (ret != SKY_ERROR) {
            set_error_status(sky_errno, SKY_ERROR_NONE);
            return ret;
        }
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return set_error_status(sky_errno, SKY_ERROR_NO_PLUGIN);
}

/*! \brief call the cache_match operation in the registered plugins
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param op the operation index
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_get_matching_cacheline(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int *idx)
{
    Sky_plugin_table_t *p = ctx->plugin;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_workspace(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid workspace");
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    while (p) {
        if (p->cache_match)
            ret = (*p->cache_match)(ctx, idx);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" :
            (ret == SKY_FAILURE) ? "Failure" :
                                   "Error");
#endif
        if (ret != SKY_ERROR) {
            set_error_status(sky_errno, SKY_ERROR_NONE);
            return ret;
        }
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return set_error_status(sky_errno, SKY_ERROR_NO_PLUGIN);
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
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);
    }

    while (p) {
        if (p->add_to_cache)
            ret = (*p->add_to_cache)(ctx, loc);
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" :
            (ret == SKY_FAILURE) ? "Failure" :
                                   "Error");
#endif
        if (ret != SKY_ERROR) {
            set_error_status(sky_errno, SKY_ERROR_NONE);
            return ret;
        }
        p = (Sky_plugin_table_t *)p->next; /* move on to next plugin */
    }
    return set_error_status(sky_errno, SKY_ERROR_NO_PLUGIN);
}

#ifdef UNITTESTS

static Sky_status_t operation_add_to_cache(Sky_ctx_t *ctx, Sky_location_t *loc)
{
    (void)ctx;
    (void)loc;
    return SKY_ERROR;
}

BEGIN_TESTS(plugin_test)
GROUP("sky_plugin_equal");

TEST("should return SKY_SUCCESS when 2 identical beacons and NULL prop are passed", ctx, {
    AP(a, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    AP(b, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    Sky_errno_t sky_errno;

    ASSERT(SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL));
});

TEST("should return SKY_SUCCESS when 2 identical beacons and prop.in_cache is true", ctx, {
    AP(a, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    AP(b, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    Sky_errno_t sky_errno;
    Sky_beacon_property_t prop = { false, false };
    b.ap.property.in_cache = true;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, &prop)) && prop.in_cache);
});

GROUP("sky_plugin_add");

TEST("should return SKY_ERROR if table is corrupt (magic != SKY_MAGIC) or root is NULL", ctx, {
    Sky_plugin_table_t *root = NULL;
    Sky_plugin_table_t table;
    table.magic = 0;
    ASSERT(SKY_ERROR == sky_plugin_add(&root, &table) &&
           SKY_ERROR == sky_plugin_add(&root, (Sky_plugin_table_t *)ctx));
});

TEST("should return SKY_ERROR if table or root pointer is NULL", ctx, {
    Sky_plugin_table_t *root = NULL;
    Sky_plugin_table_t table;
    ASSERT(SKY_ERROR == sky_plugin_add(NULL, NULL) && SKY_ERROR == sky_plugin_add(&root, NULL) &&
           SKY_ERROR == sky_plugin_add(NULL, &table));
});

TEST("should return SKY_SUCCESS if table is added twice to same list", ctx, {
    Sky_plugin_table_t *root = NULL;
    Sky_plugin_table_t table = {
        .next = NULL,
        .magic = SKY_MAGIC,
        .name = __FILE__,
    };

    ASSERT(SKY_SUCCESS == sky_plugin_add(&root, &table) &&
           SKY_SUCCESS == sky_plugin_add(&root, &table));
});

TEST("should return SKY_ERROR if no plugin operation found to provide result", ctx, {
    AP(a, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    AP(b, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    Sky_errno_t errno = SKY_ERROR_NONE;
    Sky_location_t loc = { 0 };
    int idx = -1;
    Sky_plugin_table_t table1 = {
        .next = NULL,
        .magic = SKY_MAGIC,
        .name = "test1",
        .add_to_cache = operation_add_to_cache,
        .equal = NULL,
        .remove_worst = NULL,
        .cache_match = NULL,
    };
    Sky_plugin_table_t table2 = {
        .next = NULL,
        .magic = SKY_MAGIC,
        .name = "test2",
        .add_to_cache = NULL,
        .equal = NULL,
        .remove_worst = NULL,
        .cache_match = NULL,
    };

    /* clear registration of standard plugins */
    ctx->plugin = NULL;
    /* add single access table with empty operations */
    ASSERT(SKY_SUCCESS == sky_plugin_add((void *)&ctx->plugin, &table1));
    ASSERT(SKY_SUCCESS == sky_plugin_add((void *)&ctx->plugin, &table2));
    ASSERT((Sky_plugin_table_t *)ctx->plugin == &table1);
    ASSERT(((Sky_plugin_table_t *)ctx->plugin)->next == &table2);
    ASSERT(((Sky_plugin_table_t *)ctx->plugin)->next->next == NULL);
    ASSERT(SKY_ERROR == sky_plugin_add_to_cache(ctx, &errno, &loc));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
    errno = SKY_ERROR_NONE;
    ASSERT(SKY_ERROR == sky_plugin_equal(ctx, &errno, &a, &b, NULL));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
    errno = SKY_ERROR_NONE;
    ASSERT(SKY_ERROR == sky_plugin_remove_worst(ctx, &errno));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
    errno = SKY_ERROR_NONE;
    ASSERT(SKY_ERROR == sky_plugin_get_matching_cacheline(ctx, &errno, &idx));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
});

END_TESTS();

#endif
