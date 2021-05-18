/* \brief Skyhook Embedded Library
*
* Copyright (c) 2019 Skyhook, Inc.
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
#ifndef SKY_PLUGIN_H
#define SKY_PLUGIN_H

#include <stdarg.h>

typedef Sky_status_t (*Sky_plugin_equal_t)(
    Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b, Sky_beacon_property_t *prop, bool *equal);
typedef Sky_status_t (*Sky_plugin_compare_t)(Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b, int *diff);
typedef Sky_status_t (*Sky_plugin_remove_worst_t)(Sky_ctx_t *ctx);
typedef Sky_status_t (*Sky_plugin_cache_match_t)(Sky_ctx_t *ctx, int *idx);
typedef Sky_status_t (*Sky_plugin_add_to_cache_t)(Sky_ctx_t *ctx, Sky_location_t *loc);

/* Each plugin has a table which provides entry points for the following operations */
typedef struct plugin_table {
    struct plugin_table *next; /* Pointer to next table or NULL */
    uint32_t magic; /* Mark table so it can be validated */
    char *name;
    /* Entry points */
    Sky_plugin_equal_t equal; /* Compare two beacons for equality */
    Sky_plugin_compare_t compare; /* Compare two beacons used to position */
    Sky_plugin_remove_worst_t remove_worst; /* Remove least desirable beacon from workspace */
    Sky_plugin_cache_match_t cache_match; /* Find best match between workspace and cache lines */
    Sky_plugin_add_to_cache_t add_to_cache; /* Copy workspace beacons to a cacheline */
} Sky_plugin_table_t;

Sky_status_t sky_register_plugins(Sky_plugin_table_t **root);
Sky_status_t sky_plugin_add(Sky_plugin_table_t **root, Sky_plugin_table_t *table);
Sky_status_t sky_plugin_equal(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *a, Beacon_t *b,
    Sky_beacon_property_t *prop, bool *equal);
Sky_status_t sky_plugin_compare(
    Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *a, Beacon_t *b, int *diff);
Sky_status_t sky_plugin_remove_worst(Sky_ctx_t *ctx, Sky_errno_t *sky_errno);
Sky_status_t sky_plugin_get_matching_cacheline(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int *idx);
Sky_status_t sky_plugin_add_to_cache(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Sky_location_t *loc);

#endif
