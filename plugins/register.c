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
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define SKY_LIBEL
#include "libel.h"

/* Register the basic plugins
 *
 * Add the entry point tables for each plugin
 *  ap_plugin_basic_table - WiFi beacons
 *  cell_plugin_basic_table - Cellular beacons
 *
 * Each table is added to the end of the list of plugin tables
 * The operations entry points are always called in each plugin in the order they were added
 * Each plugin handles operations for a particular beacon type
 * Each table has entry points to handle the following operations
 *  EQUAL        - Test if two beacons are equal
 *  REMOVE_WORST - Find the least desirable beacon and remove it from the request ctx
 *  MATCH_CACHE  - Find the best cache line that matches the beacons in the request ctx
 *  ADD_TO_CACHE - Copy request ctx beacons to appropriate cache line
 */
extern Sky_plugin_table_t ap_plugin_basic_table;
extern Sky_plugin_table_t cell_plugin_basic_table;

Sky_status_t sky_register_plugins(Sky_plugin_table_t **table)
{
    if (table && sky_plugin_add(table, &ap_plugin_basic_table) == SKY_SUCCESS &&
        sky_plugin_add(table, &cell_plugin_basic_table) == SKY_SUCCESS)
        return SKY_SUCCESS;
    return SKY_ERROR;
}
