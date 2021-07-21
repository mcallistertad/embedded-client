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

/* set VERBOSE_DEBUG to true to enable extra logging */
#ifndef VERBOSE_DEBUG
#define VERBOSE_DEBUG false
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
    Sky_plugin_table_t *p;

    /* check args are sane */
    if (table == NULL || table->magic != SKY_MAGIC || root == NULL)
        return SKY_ERROR;

    if (*root == NULL) { /* if list was empty, add first entry */
        *root = table;
        table->next = NULL;
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
 * equal operation returns true if the beacons of same type are equivalent
 * used to find duplicates or finding beacons in the cache
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param a the first beacon to compare
 *  @param b the second beacon to compare
 *  @param prop where to store the properties of second beacon if identical to first
 *  @param equal where to save the result of equivalence test
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_equal(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *a, Beacon_t *b,
    Sky_beacon_property_t *prop, bool *equal)
{
    Sky_plugin_table_t *p;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_request_ctx(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid request context");
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
    }

    p = ctx->session->plugins;
    while (p) {
        if (p->equal)
            ret = p->equal(ctx, a, b, prop, equal);
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : "Error");
#endif
        if (ret != SKY_ERROR) {
            set_error_status(sky_errno, SKY_ERROR_NONE);
            return ret;
        }
        p = p->next; /* move on to next plugin */
    }
    return set_error_status(sky_errno, SKY_ERROR_NO_PLUGIN);
}

/*! \brief call the compare operation in the registered plugins
 *
 * compare operation is used to order beacons of same type
 *
 *  @param ctx Skyhook request context
 *  @param code the sky_errno_t code to return
 *  @param a the first beacon to compare
 *  @param b the second beacon to compare
 *  @param diff where to save the result of comparison
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_compare(
    Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *a, Beacon_t *b, int *diff)
{
    Sky_plugin_table_t *p;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_request_ctx(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "inconsistency found in request context");
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
    }

    p = ctx->session->plugins;
    while (p) {
        if (p->compare)
            ret = p->compare(ctx, a, b, diff);
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : "Error");
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
    Sky_plugin_table_t *p = ctx->session->plugins;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_request_ctx(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid request context");
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
    }

    while (p) {
        if (p->remove_worst)
            ret = (*p->remove_worst)(ctx);
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : "Error");
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
Sky_status_t sky_plugin_match_cache(Sky_ctx_t *ctx, Sky_errno_t *sky_errno)
{
    Sky_plugin_table_t *p = ctx->session->plugins;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_request_ctx(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid request context");
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
    }

    while (p) {
        if (p->cache_match)
            ret = (*p->cache_match)(ctx);
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : "Error");
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
 *  @param loc the location being saved to cache
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_plugin_add_to_cache(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Sky_location_t *loc)
{
    Sky_plugin_table_t *p = ctx->session->plugins;
    Sky_status_t ret = SKY_ERROR;

    if (!validate_request_ctx(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "invalid request context");
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
    }

    while (p) {
        if (p->add_to_cache)
            ret = (*p->add_to_cache)(ctx, loc);
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s returned %s", p->name,
            (ret == SKY_SUCCESS) ? "Success" : "Error");
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

TEST("should return SKY_SUCCESS and equal when 2 identical beacons and NULL prop are passed", ctx, {
    AP(a, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    AP(b, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && equal);
});

TEST("should return SKY_SUCCESS and equal when 2 identical beacons and in_cache is true", ctx, {
    AP(a, "ABCDEFAACCDD", 1605291372, -108, 4433, false);
    AP(b, "ABCDEFAACCDD", 1605291372, -108, 4433, true);
    Sky_errno_t sky_errno;
    Sky_beacon_property_t prop = { false, false };
    b.ap.property.in_cache = true;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, &prop, &equal)) && equal &&
           prop.in_cache);
});

TEST("should return SKY_SUCCESS and equal with 2 different AP and equal", ctx, {
    AP(a, "ABCDEFAACCDD", 1605291372, -108, 4433, false);
    AP(b, "ABCDEFAACCEE", 1605291372, -78, 422, true);
    Sky_errno_t sky_errno;
    Sky_beacon_property_t prop = { false, false };
    b.ap.property.in_cache = true;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, &prop, &equal)) && !equal &&
           !prop.in_cache);
});

TEST("should return SKY_SUCCESS and equal with 2 identical NR cell beacons", ctx, {
    NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
    NR(b, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && equal);
});

TEST("should return SKY_SUCCESS and equal with 2 identical LTE cell beacons", ctx, {
    LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
    LTE(b, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && equal);
});

TEST("should return SKY_SUCCESS and equal with 2 identical UMTS cell beacons", ctx, {
    UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
    UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && equal);
});

TEST("should return SKY_SUCCESS and equal with 2 identical NBIOT cell beacons", ctx, {
    NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
    NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && equal);
});

TEST("should return SKY_SUCCESS and equal with 2 identical CDMA cell beacons", ctx, {
    CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
    CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && equal);
});

TEST("should return SKY_SUCCESS and equal with 2 identical GSM cell beacons", ctx, {
    GSM(a, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
    GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && equal);
});

TEST("should return SKY_SUCCESS and not equal with one connected with different cells", ctx, {
    LTE(a, 10, -108, true, 210, 485, 25614, 25664526, 387, 1000);
    LTE(b, 10, -108, false, 311, 480, 25614, 25664526, 387, 1000);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && !equal);
});

TEST("should return SKY_SUCCESS and not equal with one NMR with same cell type", ctx, {
    LTE(a, 10, -108, true, 110, 485, 25614, 25664526, 387, 1000);
    LTE_NMR(b, 10, -108, 387, 1000);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && !equal);
});

TEST("should return SKY_SUCCESS and not equal with two NMR one younger", ctx, {
    LTE_NMR(a, 8, -10, 38, 100);
    LTE_NMR(b, 10, -108, 387, 1000);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && !equal);
});

TEST("should return SKY_SUCCESS and not equal with two NMR one stronger", ctx, {
    LTE_NMR(a, 10, -10, 38, 100);
    LTE_NMR(b, 10, -108, 387, 1000);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && !equal);
});

TEST("should return SKY_SUCCESS and not equal with two very similar cells", ctx, {
    LTE(a, 10, -108, true, 110, 485, 25614, 25664526, 387, 1000);
    LTE(b, 10, -108, true, 110, 222, 25614, 25664526, 45, 1000);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && !equal);
});

TEST("should return SKY_SUCCESS and not equal with two NMR very similar", ctx, {
    LTE_NMR(a, 10, -108, 387, 1000);
    LTE_NMR(b, 10, -108, 38, 100);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_SUCCESS == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && !equal);
});

TEST("should return SKY_ERROR one NMR with different cell type", ctx, {
    CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
    LTE_NMR(b, 10, -108, 387, 1000);
    Sky_errno_t sky_errno;
    bool equal = false;

    ASSERT((SKY_ERROR == sky_plugin_equal(ctx, &sky_errno, &a, &b, NULL, &equal)) && !equal);
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
    Sky_location_t loc = {
        0.0,
        0.0,
        0,
        0,
        0,
        0,
        0,
        0,
    };
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
    ctx->session->plugins = NULL;
    /* add single access table with empty operations */
    ASSERT(SKY_SUCCESS == sky_plugin_add((void *)&ctx->session->plugins, &table1));
    ASSERT(SKY_SUCCESS == sky_plugin_add((void *)&ctx->session->plugins, &table2));
    ASSERT((Sky_plugin_table_t *)ctx->session->plugins == &table1);
    ASSERT(((Sky_plugin_table_t *)ctx->session->plugins)->next == &table2);
    ASSERT(((Sky_plugin_table_t *)ctx->session->plugins)->next->next == NULL);
    ASSERT(SKY_ERROR == sky_plugin_add_to_cache(ctx, &errno, &loc));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
    errno = SKY_ERROR_NONE;
    ASSERT(SKY_ERROR == sky_plugin_equal(ctx, &errno, &a, &b, NULL, NULL));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
    errno = SKY_ERROR_NONE;
    ASSERT(SKY_ERROR == sky_plugin_remove_worst(ctx, &errno));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
    errno = SKY_ERROR_NONE;
    ASSERT(SKY_ERROR == sky_plugin_match_cache(ctx, &errno));
    ASSERT(errno == SKY_ERROR_NO_PLUGIN);
});

/* call any plugin specific tests */
sky_plugin_unit_tests(_ctx);

END_TESTS();

#endif
