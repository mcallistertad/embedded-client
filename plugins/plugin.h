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

#define SKY_PLUGIN_TABLE(t) sky_plugin_op_##t

typedef enum sky_operations {
    SKY_OP_NEXT = 0,
    SKY_OP_NAME,
    SKY_OP_EQUAL,
    SKY_OP_REMOVE_WORST,
    SKY_OP_SCORE_CACHELINE,
    SKY_OP_ADD_TO_CACHE,
    SKY_OP_MAX, /* Add more operations before this */
} sky_operation_t;

typedef Sky_status_t (*Sky_plugin_op_t)(Sky_ctx_t *ctx, ...);

Sky_status_t sky_plugin_init(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Sky_plugin_op_t *table);
Sky_status_t sky_plugin_call(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, sky_operation_t n, ...);

extern Sky_plugin_op_t SKY_PLUGIN_TABLE(premium_ap_plugin)[SKY_OP_MAX];

Sky_status_t remove_beacon(Sky_ctx_t *ctx, int index);
Sky_status_t insert_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b, int *index);
int find_oldest(Sky_ctx_t *ctx);

#endif
