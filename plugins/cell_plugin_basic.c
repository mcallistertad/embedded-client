/*! \file plugins/premium_cell_plugin.c
 *  \brief Cell plugin supporting selection of best cells
 *  Plugin for Skyhook Embedded Library
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
#include <time.h>
#include <math.h>
#include <stdio.h>
#define SKY_LIBEL
#include "libel.h"

/* set VERBOSE_DEBUG to true to enable extra logging */
#ifndef VERBOSE_DEBUG
#define VERBOSE_DEBUG false
#endif

typedef enum {
    HIGHEST_PRIORITY = 0xffff,
    CONNECTED = 0x200,
    NON_NMR = 0x100,
    LOWEST_PRIORITY = 0x000
} Property_priority_t;

static uint16_t get_priority(Beacon_t *b);

/*! \brief compare cell beacons for equality
 *
 *  @param ctx Skyhook request context
 *  @param a pointer to cell
 *  @param b pointer to cell
 *  @param prop pointer to where b's properties are saved if equal
 *  @param equal result of test, true when equal
 *
 *  @return
 *  if beacons are comparable, return SKY_SUCCESS, equivalence and difference in priority
 *  if an error occurs during comparison. return SKY_ERROR
 */
static Sky_status_t equal(
    Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b, Sky_beacon_property_t *prop, bool *equal)
{
    (void)prop; /* suppress warning unused parameter */
    if (!ctx || !a || !b || !equal) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return SKY_ERROR;
    }
    bool equivalent = false;

    /* Two Cells can be compared but others are ordered by type */
    if (a->h.type != b->h.type || a->h.type == SKY_BEACON_AP || b->h.type == SKY_BEACON_AP ||
        a->h.type == SKY_BEACON_BLE || b->h.type == SKY_BEACON_BLE)
        return SKY_ERROR;

    /* test two cells for equivalence */
    switch (a->h.type) {
    case SKY_BEACON_CDMA:
        if ((a->cell.id2 == b->cell.id2) && (a->cell.id3 == b->cell.id3) &&
            (a->cell.id4 == b->cell.id4)) {
            if (!(a->cell.id2 == SKY_UNKNOWN_ID2 || a->cell.id3 == SKY_UNKNOWN_ID3 ||
                    a->cell.id4 == SKY_UNKNOWN_ID4))
                equivalent = true;
        }
        break;
    case SKY_BEACON_GSM:
        if ((a->cell.id1 == b->cell.id1) && (a->cell.id2 == b->cell.id2) &&
            a->cell.id3 == b->cell.id3 && (a->cell.id4 == b->cell.id4)) {
            if (!((a->cell.id1 == SKY_UNKNOWN_ID1 || a->cell.id2 == SKY_UNKNOWN_ID2 ||
                    a->cell.id3 == SKY_UNKNOWN_ID3 || a->cell.id4 == SKY_UNKNOWN_ID4)))
                equivalent = true;
        }
        break;
    case SKY_BEACON_LTE:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_NR:
        if ((a->cell.id1 == b->cell.id1) && (a->cell.id2 == b->cell.id2) &&
            (a->cell.id4 == b->cell.id4)) {
            if ((a->cell.id1 == SKY_UNKNOWN_ID1) || (a->cell.id2 == SKY_UNKNOWN_ID2) ||
                (a->cell.id4 == SKY_UNKNOWN_ID4)) {
                /* NMR */
                if ((a->cell.id5 == b->cell.id5) && (a->cell.freq == b->cell.freq))
                    equivalent = true;
            } else
                equivalent = true;
        }
        break;
    default:
        break;
    }

    *equal = equivalent;

    return SKY_SUCCESS;
}

/*! \brief compare cell beacons for order when adding to request context
 *
 *  @param ctx Skyhook request context
 *  @param a pointer to cell
 *  @param b pointer to cell
 *  @param diff result of comparison, positive when a is better
 *
 *  @return
 *  if beacons are comparable, return SKY_SUCCESS and difference
 *  (greater than zero if a should be before b)
 *  if an error occurs during comparison. return SKY_ERROR
 */
static Sky_status_t compare(Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b, int *diff)
{
    if (!ctx || !a || !b || !diff) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return SKY_ERROR;
    }

    if (!is_cell_type(a) || !is_cell_type(b))
        return SKY_ERROR;

    if (a->h.priority == 0)
        a->h.priority = (float)get_priority(a);
    if (b->h.priority == 0)
        b->h.priority = (float)get_priority(b);

    if (a->h.age != b->h.age)
        *diff = COMPARE_AGE(a, b);
    else if (a->h.priority != b->h.priority)
        *diff = COMPARE_PRIORITY(a, b);
    else
        *diff = COMPARE_RSSI(a, b);
    return SKY_SUCCESS;
}

/*! \brief remove lowest priority cell if request context is full
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static Sky_status_t remove_worst(Sky_ctx_t *ctx)
{
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d cells present. Max %d", NUM_CELLS(ctx),
        CONFIG(ctx->session, total_beacons) - CONFIG(ctx->session, max_ap_beacons));

    /* no work to do if request context is not full of max cell */
    if (NUM_CELLS(ctx) <=
        CONFIG(ctx->session, total_beacons) - CONFIG(ctx->session, max_ap_beacons)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No need to remove cell");
        return SKY_ERROR;
    }

    DUMP_REQUEST_CTX(ctx);

    /* sanity check last beacon, if we get here, it should be a cell */
    if (is_cell_type(&ctx->beacon[NUM_BEACONS(ctx) - 1])) {
        /* cells are in priority order
         * remove last beacon
         */
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: lowest priority cell");
        return remove_beacon(ctx, NUM_BEACONS(ctx) - 1);
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Not a cell?");
    return SKY_ERROR;
}

/*! \brief find cache entry with a match to request ctx
 *
 *   Expire any old cachelines
 *   Compare each cacheline with the request ctx cell beacons:
 *    . compare cells for match using cells and NMR
 *
 *   If any cacheline score meets threshold, accept it
 *   setting get_from to index of cacheline and hit to true.
 *   While searching, keep track of best cacheline to
 *   save a new server response. An empty cacheline is
 *   best, a good match is next, oldest is the fall back.
 *   Best cacheline to 'save_to' is set in the request ctx for later use.
 *
 *  @param ctx Skyhook request context
 *
 *  @return index of best match or empty cacheline or -1
 */
static Sky_status_t match(Sky_ctx_t *ctx)
{
#if CACHE_SIZE
    int i; /* i iterates through cacheline */
    float ratio; /* 0.0 <= ratio <= 1.0 is the degree to which request context matches cacheline
                    In typical case this is the intersection(request context, cache) / union(request context, cache) */
    float bestratio = 0.0f;
    float bestputratio = 0.0f;
    int score; /* score is number of APs found in cacheline */
    int threshold; /* the threshold determined that ratio should meet */
    int bestc = -1;
    int16_t bestput = -1;
    int bestthresh = 0;
    Sky_cacheline_t *cl;

    DUMP_REQUEST_CTX(ctx);
    DUMP_CACHE(ctx);

    /* expire old cachelines and note first empty cacheline as best line to save to */
    for (i = 0; i < ctx->session->num_cachelines; i++) {
        cl = &ctx->session->cacheline[i];
        /* if cacheline is old, mark it empty */
        if (cl->time != TIME_UNAVAILABLE &&
            (ctx->header.time - cl->time) >
                (CONFIG(ctx->session, cache_age_threshold) * SECONDS_IN_HOUR)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cacheline %d expired", i);
            cl->time = CACHE_EMPTY;
        }
        /* if line is empty and it is the first one, remember it */
        if (cl->time == CACHE_EMPTY) {
            if (bestputratio < 1.0) {
                bestput = (int16_t)i;
                bestputratio = 1.0f;
            }
        }
    }

    if (NUM_CELLS(ctx) == 0) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Unable to compare using Cells. No cache match");
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, CACHE_SIZE, (int)round((double)bestputratio * 100));
        return SKY_ERROR;
    }

    DUMP_REQUEST_CTX(ctx);
    DUMP_CACHE(ctx);

    /* score each cacheline wrt beacon match ratio */
    for (i = 0; i < ctx->session->num_cachelines; i++) {
        cl = &ctx->session->cacheline[i];
        threshold = score = 0;
        ratio = 0.0f;
        if (cl->time == CACHE_EMPTY || serving_cell_changed(ctx, cl) == true ||
            cached_gnss_worse(ctx, cl) == true) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "Cache: %d: Score 0 for empty cacheline or cacheline has different cell or worse gnss",
                i);
            continue;
        } else {
            /* count number of matching cells */
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on cell beacons", i);
            threshold = CONFIG(ctx->session, cache_match_all_threshold);
            score = 0;
            for (int j = NUM_APS(ctx); j < NUM_BEACONS(ctx); j++) {
                if (beacon_in_cacheline(ctx, &ctx->beacon[j], &ctx->session->cacheline[i], NULL)) {
#if VERBOSE_DEBUG
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                        "Cell Beacon %d type %s matches cache %d of %d Score %d", j,
                        sky_pbeacon(&ctx->beacon[j]), i, ctx->session->num_cachelines, (int)score);
#endif
                    score = score + 1;
                }
            }
            /* score = number of matching cells */
            /* ratio is 1.0 when score == Number of cells */
            /* ratio if 0.0 when score is less than Number of cells */
            ratio = (float)score == NUM_CELLS(ctx) ? 1.0f : 0.0f;
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache: %d: score %d (%d/%d) vs %d", i,
                (int)round((double)ratio * 100), score, NUM_BEACONS(ctx), threshold);
        }

        if (ratio > bestputratio) {
            bestput = (int16_t)i;
            bestputratio = ratio;
        }
        if (ratio > bestratio) {
            if (bestratio > 0.0)
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "Found better match in cache %d of %d score %d (vs %d)", i,
                    ctx->session->num_cachelines, (int)round((double)ratio * 100), threshold);
            bestc = i;
            bestratio = ratio;
            bestthresh = threshold;
        }
        if (ratio * 100 > (float)threshold)
            break;
    }

    /* make a note of the best match used by add_to_cache */
    ctx->save_to = bestput;
    ctx->get_from = bestc;

    if (bestratio * 100 > (float)bestthresh) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "location in cache, pick cache %d of %d score %d (vs %d)",
            bestc, ctx->session->num_cachelines, (int)round((double)bestratio * 100), bestthresh);
        ctx->hit = true;
    } else {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache match failed. Cache %d, best score %d (vs %d)",
            bestc, (int)round((double)bestratio * 100), bestthresh);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, ctx->session->num_cachelines, (int)round((double)bestputratio * 100));
        ctx->hit = false;
    }
    return SKY_SUCCESS;
#else
    ctx->from_cache = -1;
    ctx->hit = false;
    (void)ctx; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif
}

/*! \brief Assign relative priority value to cell based on attributes
 *
 * Priority is based on the attributes
 *  1. connected
 *  2. nmr
 *  3. strength
 *
 *  @param b pointer to beacon we want to prioritize
 *
 *  @return priority
 */
static uint16_t get_priority(Beacon_t *b)
{
    int score = 0;

    if (b->h.connected)
        score |= CONNECTED;
    if (!is_cell_nmr(b))
        score |= NON_NMR;

    score |= (128 - b->h.type);
    return score;
}

#ifdef UNITTESTS
TEST_FUNC(test_cell_plugin)
{
    GROUP("remove worst");
    TEST("remove_worst chooses lowest priority Air type by default", ctx, {
        Sky_errno_t sky_errno;
        uint32_t value;
        NBIOT_NMR(a, 10, -108, 25, 255);
        UMTS_NMR(b, 10, -108, 0, 412);
        LTE(c, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        LTE_NMR(d, 10, -108, 387, 1000);
        NR_NMR(e, 10, -108, 0, 0);

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(sky_set_option(ctx, &sky_errno, CONF_TOTAL_BEACONS, 6) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_TOTAL_BEACONS, &value) &&
               value == 6);
        ASSERT(SKY_SUCCESS == sky_add_cell_nb_iot_neighbor_beacon(ctx, &sky_errno, b.cell.id5,
                                  b.cell.freq, TIME_UNAVAILABLE, b.h.rssi));
        ASSERT(SKY_SUCCESS == sky_add_cell_umts_neighbor_beacon(ctx, &sky_errno, b.cell.id5,
                                  b.cell.freq, TIME_UNAVAILABLE, b.h.rssi));
        ASSERT(SKY_SUCCESS == sky_add_cell_lte_beacon(ctx, &sky_errno, c.cell.id3, c.cell.id4,
                                  c.cell.id1, c.cell.id2, c.cell.id5, c.cell.freq, c.cell.ta,
                                  TIME_UNAVAILABLE, c.h.rssi, c.h.connected));
        ASSERT(SKY_SUCCESS == sky_add_cell_lte_neighbor_beacon(ctx, &sky_errno, b.cell.id5,
                                  b.cell.freq, TIME_UNAVAILABLE, b.h.rssi));
        ASSERT(SKY_SUCCESS == sky_add_cell_nr_neighbor_beacon(ctx, &sky_errno, b.cell.id5,
                                  b.cell.freq, TIME_UNAVAILABLE, b.h.rssi));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 0);
        /*
         *       0       LTE     311,480,25614,25664526,387 1000MHz rssi:-108 ta:0 age:0
         *       1    NR-NMR     0 412MHz rssi:-108 age:0
         *       2   LTE-NMR     0 412MHz rssi:-108 age:0
         */
        ASSERT(ctx->beacon[0].h.type == SKY_BEACON_LTE);
        ASSERT(ctx->beacon[1].h.type == SKY_BEACON_NR);
        ASSERT(ctx->beacon[2].h.type == SKY_BEACON_LTE);
        ASSERT(ctx->beacon[0].h.connected == true);
        ASSERT(ctx->beacon[1].h.connected == false);
        ASSERT(ctx->beacon[2].h.connected == false);
    });
    TEST("remove_worst respects connected properties", ctx, {
        Sky_errno_t sky_errno;
        uint32_t value;
        LTE(a, 10, -108, false, 311, 480, 25614, 25664526, 387, 1000);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 0, 412);
        NBIOT(c, 10, -108, false, 515, 2, 20263, 15664525, 25, 255);
        CDMA(d, 10, -108, false, 5000, 16683, 25614, 22265, 0, 0);
        GSM(e, 10, -108, false, 515, 2, 20263, 22265, 0, 0);

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(sky_set_option(ctx, &sky_errno, CONF_TOTAL_BEACONS, 6) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_TOTAL_BEACONS, &value) &&
               value == 6);
        ASSERT(SKY_SUCCESS == sky_add_cell_lte_beacon(ctx, &sky_errno, a.cell.id3, a.cell.id4,
                                  a.cell.id1, a.cell.id2, a.cell.id5, a.cell.freq, a.cell.ta,
                                  TIME_UNAVAILABLE, a.h.rssi, a.h.connected));
        ASSERT(SKY_SUCCESS == sky_add_cell_umts_beacon(ctx, &sky_errno, b.cell.id3, b.cell.id4,
                                  b.cell.id1, b.cell.id2, b.cell.id5, b.cell.freq, TIME_UNAVAILABLE,
                                  b.h.rssi, b.h.connected));
        ASSERT(SKY_SUCCESS == sky_add_cell_nb_iot_beacon(ctx, &sky_errno, c.cell.id1, c.cell.id2,
                                  c.cell.id4, c.cell.id3, c.cell.id5, c.cell.freq, TIME_UNAVAILABLE,
                                  c.h.rssi, c.h.connected));
        ASSERT(SKY_SUCCESS == sky_add_cell_cdma_beacon(ctx, &sky_errno, d.cell.id2, d.cell.id3,
                                  d.cell.id4, TIME_UNAVAILABLE, d.h.rssi, d.h.connected));
        ASSERT(SKY_SUCCESS == sky_add_cell_gsm_beacon(ctx, &sky_errno, e.cell.id3, e.cell.id4,
                                  e.cell.id1, e.cell.id2, e.cell.ta, TIME_UNAVAILABLE, e.h.rssi,
                                  e.h.connected));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 0);
        /*          0 *    UMTS     515,2,32768,16843545,0 0MHz rssi:-108 ta:0 age:0
         *          1       LTE     311,480,25614,25664526,387 1000MHz rssi:-108 ta:0 age:0
         *          2    NB-IoT     515,2,20263,15664525,25 255MHz rssi:-108 ta:0 age:0
         */
        ASSERT(ctx->beacon[0].h.type == SKY_BEACON_UMTS);
        ASSERT(ctx->beacon[1].h.type == SKY_BEACON_LTE);
        ASSERT(ctx->beacon[2].h.type == SKY_BEACON_NBIOT);
    });
}

static Sky_status_t unit_tests(void *_ctx)
{
    GROUP_CALL("Remove Worst", test_cell_plugin);
    return SKY_SUCCESS;
}

#endif

/* * * * * * Plugin access table * * * * *
 *
 * Each plugin is registered via the access table
 * The tables for each plugin are formed into a linked list
 *
 * For a given operation, each registered plugin is
 * called for that operation until a plugin returns success.
 */

Sky_plugin_table_t cell_plugin_basic_table = {
    .next = NULL, /* Pointer to next plugin table */
    .magic = SKY_MAGIC, /* Mark table so it can be validated */
    .name = __FILE__,
    /* Entry points */
    .equal = equal, /* Compare two beacons for equality */
    .compare = compare, /* Compare priority of two beacons for ordering in request context */
    .remove_worst = remove_worst, /* Remove least compare beacon from request context */
    .cache_match = match, /* Find best match between request context and cachelines */
    .add_to_cache = NULL, /* Copy request context beacons to a cacheline */
#ifdef UNITTESTS
    .unit_tests = unit_tests, /* Unit Tests */
#endif

};
