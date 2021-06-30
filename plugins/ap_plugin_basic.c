/*! \file plugins/ap_plugin_basic.c
 *  \brief AP plugin supporting basic APs and cells Only
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
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define SKY_LIBEL
#include "libel.h"

/* set VERBOSE_DEBUG to true to enable extra logging */
#ifndef VERBOSE_DEBUG
#define VERBOSE_DEBUG false
#endif

#define IDX(b) ((b)-ctx->beacon)
#define ABS(a) (((a) < 0) ? -(a) : (a))
#define AP_BELOW_RSSI_THRESHOLD(ctx, idx)                                                          \
    (EFFECTIVE_RSSI((ctx)->beacon[(idx)].h.rssi) <                                                 \
        -(int)CONFIG((ctx)->session, cache_neg_rssi_threshold))

/* Attribute priorities are prioritized as follows
 *  highest 1) : Connected
 *          2) : Cached
 *          3) : deviation from ideal distribution
 *
 * Each priority is assigned a value, highest priority has highest value
 * Overall priority value is the sum of the three priorities which allows
 * priority values (priority of beacons) to be compared numerically
 *
 * Connected - value 512 (2^9)
 * Cached - value 256 (2^8)
 * Deviation from ideal rssi is fractional but in the range of 0 through 128.
 * The priority is held as 128 - deviation, making the priority higher when better.
 *
 * Below are the definitions for the property priorities
 */
typedef enum {
    HIGHEST_PRIORITY = 0xffff,
    CONNECTED = 0x200,
    IN_CACHE = 0x100,
    LOWEST_PRIORITY = 0x000
} Property_priority_t;

static Sky_status_t set_priorities(Sky_ctx_t *ctx);

/*! \brief test two APs for equality
 *
 *  @param ctx Skyhook request context
 *  @param a pointer to an AP
 *  @param b pointer to an AP
 *  @param prop pointer to where cached beacon properties are saved if equal
 *  @param diff result of comparison, positive when a is better
 *
 *  @return
 *  if beacons are comparable, return SKY_SUCCESS, and set equivalence
 *  if an error occurs during comparison. return SKY_ERROR
 */
static Sky_status_t equal(
    Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b, Sky_beacon_property_t *prop, bool *equal)
{
    if (!ctx || !a || !b || !equal) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return SKY_ERROR;
    }

    /* Two APs can be compared but others are ordered by type */
    if (a->h.type != SKY_BEACON_AP || b->h.type != SKY_BEACON_AP)
        return SKY_ERROR;

    if (COMPARE_MAC(a, b) == 0) {
        *equal = true;
        /* If user provided property pointer, copy properties
         * from b (useful when getting properties from matching cached beacon */
        if (prop != NULL && b->ap.property.in_cache) {
            prop->in_cache = true;
            prop->used = false; /* Premium plugin supports this property */
        }
    } else
        *equal = false;
    return SKY_SUCCESS;
}

/*! \brief compare AP for ordering when adding to context
 *
 *  AP order is primarily based on signal strength
 *  lowest MAC address is used as tie breaker if strengths are the same
 *
 *  @param ctx Skyhook request context
 *  @param a pointer to an AP
 *  @param b pointer to an AP
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

    /* Move on to other plugins if either beacon is not an AP */
    if (a->h.type != SKY_BEACON_AP || b->h.type != SKY_BEACON_AP)
        return SKY_ERROR;

    /* APs are ordered by rssi value */
    if (a->h.rssi != b->h.rssi)
        *diff = COMPARE_RSSI(a, b);
    else
        *diff = COMPARE_MAC(a, b);
    return SKY_SUCCESS;
}

/*! \brief test two MAC addresses for being members of same virtual Group
 *
 *   Similar means the two mac addresses differ only in one nibble AND
 *   if that nibble is the second-least-significant bit of second hex digit,
 *   then that bit must match too.
 *
 *  @param macA pointer to the first MAC
 *  @param macB pointer to the second MAC
 *  @param pn pointer to nibble index of where they differ if similar (0-11) (needed for premium)
 *
 *  @return negative, 0 or positive
 *  return 0 when NOT similar, negative indicates parent is B, positive parent is A
 *  if macs are similar, and pn is not NULL, *pn is set to nibble index of difference
 */
static int mac_similar(const uint8_t macA[], const uint8_t macB[], int *pn)
{
    size_t num_diff = 0; // Num hex digits which differ
    size_t idx_diff = 0; // nibble digit which differs
    size_t n;
    int result = 1;

    /* for each nibble, increment count if different */
    for (n = 0; n < MAC_SIZE * 2; n++) {
        if ((macA[n / 2] & NIBBLE_MASK(n)) != (macB[n / 2] & NIBBLE_MASK(n))) {
            if (++num_diff > 1)
                return 0;
            idx_diff = n;
            result = macA[n / 2] - macB[n / 2];
        }
    }

    /* Only one nibble different, but is the Local Administrative bit different */
    if (LOCAL_ADMIN_MASK(macA[0]) != LOCAL_ADMIN_MASK(macB[0])) {
        return 0; /* not similar */
    }

    /* report which nibble is different */
    if (pn)
        *pn = idx_diff;
    return result;
}

#if CACHE_SIZE
/*! \brief count number of cached APs in request ctx relative to a cacheline
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in, otherwise count in request ctx
 *
 *  @return number of cached APs or -1 for fatal error
 */
static int count_cached_aps_in_request_ctx(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int num_aps_cached = 0;
    int j, i;
    if (!ctx || !cl)
        return -1;
    for (j = 0; j < NUM_APS(ctx); j++) {
        for (i = 0; i < NUM_APS(cl); i++) {
            bool equivalent = false;
            equal(ctx, &ctx->beacon[j], &cl->beacon[i], NULL, &equivalent);
            num_aps_cached += equivalent;
        }
    }
#if VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d APs in cache %d", num_aps_cached,
        cl - ctx->session->cacheline);
#endif
    return num_aps_cached;
}
#endif

/*! \brief select between two APs which should be removed,
 *
 *  return negative if j has better properties than i
 *  otherwise return positive if worse (or 0 when same)
 *
 *  @param ctx Skyhook request context
 *
 */
#define CONNECTED_AND_IN_CACHE_ONLY(priority) ((int16_t)(priority) & (CONNECTED | IN_CACHE))
static int cmp_properties(Sky_ctx_t *ctx, int i, int j)
{
    /* Assume i is better than j (positive) unless priority
     * logic dictates this should be reversed */
    return (CONNECTED_AND_IN_CACHE_ONLY(ctx->beacon[i].h.priority) -
            CONNECTED_AND_IN_CACHE_ONLY(ctx->beacon[j].h.priority));
}

/*! \brief try to reduce AP by filtering out virtual AP
 *
 *  When similar, select beacon with highest mac address
 *  unless it better properties, then choose to select the other beacon
 *  Remove the selected beacon with worst properties
 *
 *  @param ctx Skyhook request context
 *
 *  @return true if beacon removed or false otherwise
 */
static bool remove_virtual_ap(Sky_ctx_t *ctx)
{
    int i, j;
    int cmp;
    Beacon_t *vap_a = NULL;
    Beacon_t *vap_b = NULL;
    Beacon_t *worst_vap = NULL;

    if (NUM_APS(ctx) <= CONFIG(ctx->session, max_ap_beacons)) {
        return false;
    }

    /* look for any AP beacon that is 'similar' to another */
    if (ctx->beacon[0].h.type != SKY_BEACON_AP) {
        LOGFMT(ctx, SKY_LOG_LEVEL_CRITICAL, "beacon type not WiFi");
        return false;
    }

    /* Compare all beacons, looking for similar macs
     * find the lowest of two beacons with similar macs, with
     * the lowest priority
     */
    for (j = NUM_APS(ctx) - 1; j > 0; j--) {
        for (i = j - 1; i >= 0; i--) {
            if ((cmp = mac_similar(ctx->beacon[i].ap.mac, ctx->beacon[j].ap.mac, NULL)) < 0) {
                /* beacons are similar and i has lower mac */
                /* j has higher mac so we will remove it unless properties indicate otherwise
                 */
                if (cmp_properties(ctx, j, i) > 0) {
                    vap_a = &ctx->beacon[i];
                    vap_b = &ctx->beacon[j];
                } else {
                    vap_a = &ctx->beacon[j];
                    vap_b = &ctx->beacon[i];
                }
            } else if (cmp > 0) {
                /* beacons are similar and j has lower mac */
                /* situation is exactly reversed (i has higher mac) but logic is otherwise
                 * identical
                 */
                if (cmp_properties(ctx, i, j) > 0) {
                    vap_a = &ctx->beacon[j];
                    vap_b = &ctx->beacon[i];
                } else {
                    vap_a = &ctx->beacon[i];
                    vap_b = &ctx->beacon[j];
                }
            } else
                continue;
#if VERBOSE_DEBUG
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d similar to %d%s%s", IDX(vap_a), IDX(vap_b),
                vap_b->h.connected ? " (connected)" : "",
                vap_b->ap.property.in_cache ? " (cached)" : "");
            dump_ap(ctx, "similar A:", vap_a, __FILE__, __FUNCTION__);
            dump_ap(ctx, "similar B:", vap_b, __FILE__, __FUNCTION__);
#else
            (void)vap_b;
#endif
            /* if new match is similar to existing best candidate,
             * select new match if it is worse properties,
             * else if not similar, choose new match if lower mac and same properties or worse properties.
             */
            if (worst_vap == vap_a)
                continue;
            if (worst_vap == NULL)
                worst_vap = vap_a;
            else if (mac_similar(worst_vap->ap.mac, vap_a->ap.mac, NULL)) {
                /* same virtual group. Check if worse properties */
                if (cmp_properties(ctx, IDX(vap_a), IDX(worst_vap)) < 0)
                    worst_vap = vap_a;
            } else {
                /* different virtual group. Choose one based on properties or MAC */
                if (cmp_properties(ctx, IDX(vap_a), IDX(worst_vap)) == 0 &&
                    COMPARE_MAC(vap_a, worst_vap) > 0)
                    worst_vap = vap_a;
                else if (cmp_properties(ctx, IDX(vap_a), IDX(worst_vap)) < 0)
                    worst_vap = vap_a;
            }
            if (worst_vap == vap_a)
                dump_ap(ctx, "worst vap >>>:", vap_a, __FILE__, __FUNCTION__);
        }
    }
    if (worst_vap != NULL)
        return (remove_beacon(ctx, IDX(worst_vap)) == SKY_SUCCESS);
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "no match");
    return false;
}

/*! \brief try to reduce AP by filtering out the worst one
 *
 *  Request Context AP beacons are stored in decreasing rssi order
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static Sky_status_t remove_worst(Sky_ctx_t *ctx)
{
    int idx_of_worst;

    idx_of_worst = set_priorities(ctx);
    /* no work to do if request context is not full of max APs */
    if (NUM_APS(ctx) <= CONFIG(ctx->session, max_ap_beacons)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No need to remove AP");
        return SKY_ERROR;
    }

    DUMP_REQUEST_CTX(ctx);
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Overall worst AP idx:%d", idx_of_worst);

    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates or remove one based on age, rssi distribution etc */
    if (!remove_virtual_ap(ctx)) {
        return remove_beacon(ctx, idx_of_worst);
    }
    return SKY_SUCCESS;
}

/*! \brief find cache entry with a match to request ctx
 *
 *   Expire any old cachelines
 *   Compare each cacheline with the request ctx beacons:
 *    . If request ctx has enough cached APs, compare them with low threshold
 *    . If just a few APs, compare all APs with higher threshold
 *    . If no APs, compare cells for 100% match
 *
 *   If any cacheline score meets threshold, accept it.
 *   While searching, keep track of best cacheline to
 *   save a new server response. An empty cacheline is
 *   best, a good match is next, oldest is the fall back.
 *   Best cacheline to 'save_to' is set in the request ctx for later use.
 *
 *  @param ctx Skyhook request context
 *  @param idx cacheline index of best match or empty cacheline or -1
 *
 *  @return index of best match or empty cacheline or -1
 */
static Sky_status_t match(Sky_ctx_t *ctx, int *idx)
{
#if CACHE_SIZE
    int i; /* i iterates through cacheline */
    float ratio; /* 0.0 <= ratio <= 1.0 is the degree to which request ctx matches cacheline
                    In typical case this is the intersection(request ctx, cache) / union(request ctx, cache) */
    float bestratio = 0.0f;
    float bestputratio = 0.0f;
    int score; /* score is number of APs found in cacheline */
    int threshold; /* the threshold determined that ratio should meet */
    int num_aps_cached = 0;
    int bestc = -1;
    int16_t bestput = -1;
    int bestthresh = 0;
    Sky_cacheline_t *cl;

    if (!idx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad parameter");
        return SKY_ERROR;
    }

    /* expire old cachelines and note first empty cacheline as best line to save to */
    for (i = 0; i < ctx->session->num_cachelines; i++) {
        cl = &ctx->session->cacheline[i];
        /* if cacheline is old, mark it empty */
        if (cl->time != CACHE_EMPTY &&
            (ctx->header.time - cl->time) >
                (CONFIG(ctx->session, cache_age_threshold) * SECONDS_IN_HOUR)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache line %d expired", i);
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

    if (NUM_APS(ctx) == 0) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Unable to compare using APs. No cache match");
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, CACHE_SIZE, (int)round((double)bestputratio * 100));
        return SKY_ERROR;
    }

    DUMP_REQUEST_CTX(ctx);
    DUMP_CACHE(ctx);

    /* score each cache line wrt beacon match ratio */
    for (i = 0; i < ctx->session->num_cachelines; i++) {
        cl = &ctx->session->cacheline[i];
        threshold = score = 0;
        ratio = 0.0f;
        if (cl->time == CACHE_EMPTY) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score 0 for empty cacheline", i);
            continue;
        } else if (serving_cell_changed(ctx, cl) == true) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score 0 for serving cell change", i);
            continue;
        } else {
            /* count number of matching APs in request ctx and cache */
            if ((num_aps_cached = count_cached_aps_in_request_ctx(ctx, cl)) < 0) {
                *idx = -1;
                return SKY_SUCCESS;
            } else if (NUM_APS(ctx) && NUM_APS(cl)) {
                /* Score based on ALL APs */
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on ALL APs", i);
                score = num_aps_cached;
                int unionAB = NUM_APS(ctx) + NUM_APS(cl) - num_aps_cached;
                threshold = CONFIG(ctx->session, cache_match_all_threshold);
                ratio = (float)score / unionAB;
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: score %d (%d/%d) vs %d", i,
                    (int)round((double)ratio * 100), score, unionAB, threshold);
            }
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

    if ((bestratio * 100) > (float)bestthresh) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "location in cache, pick cache %d of %d score %d (vs %d)",
            bestc, ctx->session->num_cachelines, (int)round((double)bestratio * 100), bestthresh);
        *idx = bestc;
    } else {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No Cache match found. Cache %d, best score %d (vs %d)",
            bestc, (int)round((double)bestratio * 100), bestthresh);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, ctx->session->num_cachelines, (int)round((double)bestputratio * 100));
        *idx = -1;
    }
    return SKY_SUCCESS;
#else
    *idx = -1;
    (void)ctx; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif
}

/*! \brief add location to cache
 *
 *   The location is saved in the cacheline indicated by bestput (set by find_best_match)
 *   unless this is -1, in which case, location is saved in oldest cacheline.
 *
 *  @param ctx Skyhook request context
 *  @param loc pointer to location info
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
static Sky_status_t to_cache(Sky_ctx_t *ctx, Sky_location_t *loc)
{
#if CACHE_SIZE
    int i = ctx->save_to;
    int j;
    Sky_cacheline_t *cl;

    /* compare current time to Mar 1st 2019 */
    if (loc->time <= TIMESTAMP_2019_03_01) {
        return SKY_ERROR;
    }

    /* if best 'save-to' location was not set by beacon_score, use oldest */
    if (i < 0) {
        i = find_oldest(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "find_oldest chose cache %d of %d", i,
            ctx->session->num_cachelines);
    }
    cl = &ctx->session->cacheline[i];
    if (loc->location_status != SKY_LOCATION_STATUS_SUCCESS) {
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Won't add unknown location to cache");
        cl->time = CACHE_EMPTY; /* clear cacheline */
        LOGFMT(
            ctx, SKY_LOG_LEVEL_DEBUG, "clearing cache %d of %d", i, ctx->session->num_cachelines);
        return SKY_ERROR;
    } else if (cl->time == CACHE_EMPTY)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Saving to empty cache %d of %d", i,
            ctx->session->num_cachelines);
    else
        LOGFMT(
            ctx, SKY_LOG_LEVEL_DEBUG, "Saving to cache %d of %d", i, ctx->session->num_cachelines);

    cl->num_beacons = NUM_BEACONS(ctx);
    cl->num_ap = NUM_APS(ctx);
    cl->gnss = ctx->gnss;
    cl->loc = *loc;
    cl->time = loc->time;

    for (j = 0; j < NUM_BEACONS(ctx); j++) {
        cl->beacon[j] = ctx->beacon[j];
        if (cl->beacon[j].h.type == SKY_BEACON_AP) {
            cl->beacon[j].ap.property.in_cache = false;
        }
    }
    DUMP_CACHE(ctx);
    return SKY_SUCCESS;
#else
    (void)ctx; /* suppress warning unused parameter */
    (void)loc; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif
}

/*! \brief Compute an AP's priority value.
 *
 * An AP's priority is based on the following attributes, in priority order:
 * 1. its connected flag value
 * 2. whether it's present in the cache
 * 3. the deviation of its RSSI value from the ideal
 *
 * The computed priority is really just a concatenation of these 3 components
 * expressed as a single floating point quantity, partitioned in the following way:
 * 1. connected flag: bit 9
 * 2. present in cache: bit 8
 * 3. RSSI deviation from ideal: bits 0-7 plus the fractional part
 *
 *  @param ctx pointer to request context
 *  @param b pointer to AP
 *
 *  @return computed priority
 */
static float get_priority(Sky_ctx_t *ctx, Beacon_t *b)
{
    float priority = 0;
    float deviation;
    int lowest_rssi, highest_rssi;
    float band_width, ideal_rssi;

    if (b->h.connected)
        priority += (float)CONNECTED;
    if (b->ap.property.in_cache) {
        priority += (float)IN_CACHE;
    }
    /* Compute the range of RSSI values across all APs. */
    /* (Note that the list of APs is in rssi order so index 0 is the strongest beacon.) */
    highest_rssi = EFFECTIVE_RSSI(ctx->beacon[0].h.rssi);
    lowest_rssi = EFFECTIVE_RSSI(ctx->beacon[NUM_APS(ctx) - 1].h.rssi);

    /* Find the deviation of the AP's RSSI from its ideal RSSI. Subtract this number from
     * 128 so that smaller deviations are considered better.
     */
    band_width = (float)(highest_rssi - lowest_rssi) / (float)(NUM_APS(ctx) - 1);
    ideal_rssi = (float)highest_rssi - band_width * (float)(IDX(b));
    deviation = ABS(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi);
    priority += 128 - deviation;

#if VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d rssi:%d ideal:%d.%d priority:%d.%d", IDX(b),
        EFFECTIVE_RSSI(b->h.rssi), ideal_rssi, (int)((ideal_rssi - (int)ideal_rssi) * 10),
        (int)priority, (int)((priority - (int)priority) * 10));
#endif
    return priority;
}

/*! \brief assign priority value to all beacons
 *
 * use get_priority to assign a priority to each beacon in request context
 * process APs from the middle and remember the worst AP found
 * if the weakest AP is below threshold, find the worst weak AP
 *
 *  @param ctx Skyhook request context
 *
 *  @return idx of beacon with lowest priority
 */
static int set_priorities(Sky_ctx_t *ctx)
{
    int j, jump, up_down;
    int idx_of_worst = NUM_APS(ctx) / 2;
    float priority_of_worst = HIGHEST_PRIORITY;
    bool weak_only;

    /* if weakest AP is below threshold
     * look for lowest priority weak beacon */
    weak_only = (AP_BELOW_RSSI_THRESHOLD(ctx, NUM_APS(ctx) - 1));

    /* search from middle of range looking for worst AP */
    for (jump = 0, up_down = -1, j = NUM_APS(ctx) / 2; j >= 0 && j < NUM_APS(ctx);
         jump++, j += up_down * jump, up_down = -up_down) {
        ctx->beacon[j].h.priority = get_priority(ctx, &ctx->beacon[j]);
        if ((!weak_only || AP_BELOW_RSSI_THRESHOLD(ctx, j)) &&
            ctx->beacon[j].h.priority < priority_of_worst) {
            idx_of_worst = j;
            priority_of_worst = ctx->beacon[j].h.priority;
        }
    }

    return idx_of_worst;
}

/* * * * * * Plugin access table * * * * *
 *
 * Each plugin is registered via the access table
 * The tables for each plugin are formed into a linked list
 *
 * For a given operation, each registered plugin is
 * called for that operation until a plugin returns success.
 */

Sky_plugin_table_t ap_plugin_basic_table = {
    .next = NULL, /* Pointer to next plugin table */
    .magic = SKY_MAGIC, /* Mark table so it can be validated */
    .name = __FILE__,
    /* Entry points */
    .equal = equal, /* Compare two beacons for equality*/
    .compare = compare, /*Compare two beacons for ordering in request context */
    .remove_worst = remove_worst, /* Remove lowest priority beacon from  */
    .cache_match = match, /* Find best match between request context and cache lines */
    .add_to_cache = to_cache, /* Copy request context beacons to a cacheline */
};
