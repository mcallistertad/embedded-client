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
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define SKY_LIBEL
#include "libel.h"

/* Uncomment VERBOSE_DEBUG to enable extra logging */
// #ifndef VERBOSE_DEBUG
// #define VERBOSE_DEBUG
// #endif

typedef enum {
    MOST_DESIRABLE = 0xffff,
    CONNECTED = 0x800,
    VAP = 0x400,
    IN_CACHE = 0x100,
    LEAST_DESIRABLE = 0x000
} Rank_t;

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define EFFECTIVE_RSSI(rssi) ((rssi) == -1 ? (-127) : (rssi))
/* when comparing age, lower value (younger) is better so invert difference */
#define COMPARE_AGE(a, b) (-((a) - (b)))
/* when comparing mac, lower value is better so invert difference */
#define COMPARE_MAC(a, b) (-memcmp((a), (b), MAC_SIZE))

static int get_desirablility(Sky_ctx_t *ctx, Beacon_t **beacons_by_rssi, int idx);

/* APs are ordered in the request context in rank order.
 * Most highly desired are connected APs, Virtual groups, Used and cached,
 * next APs are ordered based on an ideal uniform distribution of signal strength.
 * Least desirable AP is at the end of the list of APs
 */

/*! \brief compare APs for equality
 *
 *  APs rank includes how well a given AP fits to the ideal
 *  distribution of signal strengths of all APs. Thus adding
 *  a new AP may require re-sorting all APs
 *  @param ctx Skyhook request context
 *  @param a pointer to an AP
 *  @param b pointer to an AP
 *  @param prop pointer to where b's properties are saved if equal
 *  @param diff result of comparison, positive when a is better
 *
 *  @return
 *  if beacons are comparable, return SKY_SUCCESS, equivalence and difference in rank
 *  if an error occurs during comparison. return SKY_ERROR
 */
static Sky_status_t equal(
    Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b, Sky_beacon_property_t *prop, bool *equal, int *diff)
{
    if (!ctx || !a || !b) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return SKY_ERROR;
    }

    /* Two APs can be compared but others are ordered by type */
    if (a->h.type != SKY_BEACON_AP || b->h.type != SKY_BEACON_AP)
        return SKY_ERROR;

    /* if two APs are identical, return SKY_SUCCESS */
    if (COMPARE_MAC(a->ap.mac, b->ap.mac) == 0) {
        if (equal)
            *equal = true;
        if (prop != NULL && b->ap.property.in_cache) {
            prop->in_cache = true;
            prop->used = false;
        }
    } else if (equal)
        *equal = false;

    /* calculate rank score only if a place to save it was provided */

    if (diff) {
        if (a->h.age != b->h.age)
            *diff = (int)COMPARE_AGE(a->h.age, b->h.age);
        else if (a->h.rank == b->h.rank) {
            if (a->h.rssi != b->h.rssi)
                *diff = a->h.rssi - b->h.rssi;
            else if (COMPARE_MAC(a->ap.mac, b->ap.mac) != 0)
                *diff = COMPARE_MAC(a->ap.mac, b->ap.mac);
            else
                *diff = 1; /* a is better, arbitrarily */
        } else
            *diff = a->h.rank - b->h.rank;
    }
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
 *  @param pn pointer to nibble index of where they differ if similar (0-11)
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
/*! \brief count number of cached APs in workspace relative to a cacheline
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in, otherwise count in workspace
 *
 *  @return number of cached APs or -1 for fatal error
 */
static int count_cached_aps_in_workspace(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int num_aps_cached = 0;
    int j, i;
    if (!ctx || !cl)
        return -1;
    for (j = 0; j < NUM_APS(ctx); j++) {
        for (i = 0; i < NUM_APS(cl); i++) {
            bool equivalent = false;
            equal(ctx, &ctx->beacon[j], &cl->beacon[i], NULL, &equivalent, NULL);
            num_aps_cached += equivalent;
        }
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(
        ctx, SKY_LOG_LEVEL_DEBUG, "%d APs in cache %d", num_aps_cached, cl - ctx->state->cacheline);
#endif
    return num_aps_cached;
}
#endif

/*! \brief select between two virtual APs which should be removed,
 *  and then remove it
 *
 *  keep beacons with more desirable properties
 *
 *  @param ctx Skyhook request context
 *
 *  @return true if beacon removed or false otherwise
 */
#define RANK_ATTRIBUTES(x) ((x)&0xFF00)
static bool remove_poorest_of_pair(Sky_ctx_t *ctx, int i, int j)
{
    /* Assume we'll keep i and discard j. Then, use rank
     * logic to see if this should be reversed:
     * If upper attribute bits of rank of j are higher
     * keep j. Otherwise keep i.
     */
    int tmp;

    if (RANK_ATTRIBUTES(ctx->beacon[j].ap.h.rank) > RANK_ATTRIBUTES(ctx->beacon[i].h.rank)) {
        tmp = i;
        i = j;
        j = tmp;
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: %d similar to %d%s%s", j, i,
        ctx->beacon[i].ap.h.connected ? " (connected)" : "",
        ctx->beacon[i].ap.property.in_cache ? " (cached)" : "");
    return (remove_beacon(ctx, j) == SKY_SUCCESS);
}

/*! \brief try to reduce AP by filtering out virtual AP
 *
 *  When similar, remove beacon with highesr mac address
 *  unless it is in cache, then choose to remove the uncached beacon
 *
 *  @param ctx Skyhook request context
 *
 *  @return true if beacon removed or false otherwise
 */
static bool remove_virtual_ap(Sky_ctx_t *ctx)
{
    int i, j;
    int cmp;

    if (NUM_APS(ctx) <= CONFIG(ctx->state, max_ap_beacons)) {
        return false;
    }

    /* look for any AP beacon that is 'similar' to another */
    if (ctx->beacon[0].h.type != SKY_BEACON_AP) {
        LOGFMT(ctx, SKY_LOG_LEVEL_CRITICAL, "beacon type not WiFi");
        return false;
    }

    /* Compare all beacons, looking for similar macs
     * Try to keep lowest of two beacons with similar macs, unless
     * the lower one is connected or in cache and the other is not
     */
    for (j = NUM_APS(ctx) - 1; j > 0; j--) {
        for (i = j - 1; i >= 0; i--) {
            if ((cmp = mac_similar(ctx->beacon[i].ap.mac, ctx->beacon[j].ap.mac, NULL)) < 0) {
                /* j has higher mac so we will remove it unless connected or in cache indicate otherwise
                 *
                 */
                return remove_poorest_of_pair(ctx, i, j);
            } else if (cmp > 0) {
                /* situation is exactly reversed (i has higher mac) but logic is otherwise
                 * identical
                 */
                return remove_poorest_of_pair(ctx, j, i);
            }
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "no match");
    return false;
}

static Sky_status_t remove_worst(Sky_ctx_t *ctx)
{
    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates or remove one based on age or rssi distribution */
    if (!remove_virtual_ap(ctx)) {
        return remove_beacon(ctx, ctx->ap_len - 1);
    }
    return SKY_SUCCESS;
}

/*! \brief find cache entry with a match to workspace
 *
 *   Expire any old cachelines
 *   Compare each cacheline with the workspace beacons:
 *    . If workspace has enough cached APs, compare them with low threshold
 *    . If just a few APs, compare all APs with higher threshold
 *    . If no APs, compare cells for 100% match
 *
 *   If any cacheline score meets threshold, accept it.
 *   While searching, keep track of best cacheline to
 *   save a new server response. An empty cacheline is
 *   best, a good match is next, oldest is the fall back.
 *   Best cacheline to 'save_to' is set in the workspace for later use.
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
    int err; /* err breaks the seach due to bad value */
    float ratio; /* 0.0 <= ratio <= 1.0 is the degree to which workspace matches cacheline
                    In typical case this is the intersection(workspace, cache) / union(workspace, cache) */
    float bestratio = 0.0f;
    float bestputratio = 0.0f;
    int score; /* score is number of APs found in cacheline */
    int threshold; /* the threshold determined that ratio should meet */
    int num_aps_cached = 0;
    int bestc = -1;
    int16_t bestput = -1;
    int bestthresh = 0;
    Sky_cacheline_t *cl;
    bool result = false;

    if (!idx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad parameter");
        return SKY_ERROR;
    }

    /* expire old cachelines and note first empty cacheline as best line to save to */
    for (i = 0, err = false; i < CACHE_SIZE; i++) {
        cl = &ctx->state->cacheline[i];
        /* if cacheline is old, mark it empty */
        if (cl->time != 0 && ((uint32_t)(*ctx->gettime)(NULL)-cl->time) >
                                 (CONFIG(ctx->state, cache_age_threshold) * SECONDS_IN_HOUR)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache line %d expired", i);
            cl->time = 0;
        }
        /* if line is empty and it is the first one, remember it */
        if (cl->time == 0) {
            if (bestputratio < 1.0) {
                bestput = (int16_t)i;
                bestputratio = 1.0f;
            }
        }
    }

    if (NUM_APS(ctx) == 0) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No APs");
        return SKY_ERROR;
    }

    DUMP_WORKSPACE(ctx);
    DUMP_CACHE(ctx);

    /* score each cache line wrt beacon match ratio */
    for (i = 0, err = false; i < CACHE_SIZE; i++) {
        cl = &ctx->state->cacheline[i];
        threshold = score = 0;
        ratio = 0.0f;
        if (cl->time == 0 || cell_changed(ctx, cl) == true) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "Cache: %d: Score 0 for empty cacheline or cell change", i);
            continue;
        } else {
            /* count number of matching APs in workspace and cache */
            if ((num_aps_cached = count_cached_aps_in_workspace(ctx, cl)) < 0) {
                err = true;
                break;
            } else if (NUM_APS(ctx) && NUM_APS(cl)) {
                /* Score based on ALL APs */
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on ALL APs", i);
                score = num_aps_cached;
                int unionAB = NUM_APS(ctx) + NUM_APS(cl) - num_aps_cached;
                threshold = CONFIG(ctx->state, cache_match_used_threshold);
                ratio = (float)score / (float)unionAB;
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: score %d (%d/%d) vs %d", i,
                    (int)round((double)ratio * 100), score, unionAB, threshold);
                result = true;
            }
        }

        if (ratio > bestputratio) {
            bestput = (int16_t)i;
            bestputratio = ratio;
        }
        if (ratio > bestratio) {
            if (bestratio > 0.0)
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "Found better match in cache %d of %d score %d (vs %d)", i, CACHE_SIZE,
                    (int)round((double)ratio * 100), threshold);
            bestc = i;
            bestratio = ratio;
            bestthresh = threshold;
        }
        if (ratio * 100 > (float)threshold)
            break;
    }
    if (err) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad parameters counting APs");
        return SKY_ERROR;
    }

    /* make a note of the best match used by add_to_cache */
    ctx->save_to = bestput;

    if (result && (bestratio * 100) > (float)bestthresh) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "location in cache, pick cache %d of %d score %d (vs %d)",
            bestc, CACHE_SIZE, (int)round((double)bestratio * 100), bestthresh);
        *idx = bestc;
        return SKY_SUCCESS;
    }
    if (result) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No Cache match found. Cache %d, best score %d (vs %d)",
            bestc, (int)round((double)bestratio * 100), bestthresh);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, CACHE_SIZE, (int)round((double)bestputratio * 100));
        return SKY_SUCCESS;
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Unable to compare using APs. No cache match");
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d", bestput,
        CACHE_SIZE, (int)round((double)bestputratio * 100));
    return SKY_ERROR;
#else
    (void)ctx; /* suppress warning unused parameter */
    (void)idx; /* suppress warning unused parameter */
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
    uint32_t now = (*ctx->gettime)(NULL);
    Sky_cacheline_t *cl;

    /* compare current time to Mar 1st 2019 */
    if (now <= TIMESTAMP_2019_03_01) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Don't have good time of day! %u", now);
        return SKY_ERROR;
    }

    /* if best 'save-to' location was not set by beacon_score, use oldest */
    if (i < 0) {
        i = find_oldest(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "find_oldest chose cache %d of %d", i, CACHE_SIZE);
    }
    cl = &ctx->state->cacheline[i];
    if (loc->location_status != SKY_LOCATION_STATUS_SUCCESS) {
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Won't add unknown location to cache");
        cl->time = 0; /* clear cacheline */
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "clearing cache %d of %d", i, CACHE_SIZE);
        return SKY_ERROR;
    } else if (cl->time == 0)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Saving to empty cache %d of %d", i, CACHE_SIZE);
    else
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Saving to cache %d of %d", i, CACHE_SIZE);

    cl->len = NUM_BEACONS(ctx);
    cl->ap_len = NUM_APS(ctx);
    cl->loc = *loc;
    cl->time = now;

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

/*! \brief Assign relative score to AP based on attributes
 *
 * Desirable attributes are connected, virtual groups and cached APs
 * and best fit to rssi uniform distribution
 *
 *  @param ctx pointer to request context
 *  @param b pointer to AP
 *  @param beacons_by_rssi rssi ordered array of all beacons
 *  @param idx index of beacon we want to rank
 *
 *  @return score
 */
static int get_desirablility(Sky_ctx_t *ctx, Beacon_t **beacons_by_rssi, int idx)
{
    int score = 0;
    int ideal_rssi, lowest_rssi, highest_rssi;
    float band;

    Beacon_t *b = beacons_by_rssi[idx];
    if (b->h.connected)
        score |= CONNECTED;
    if (NUM_VAPS(b)) /* Virtual group */
        score |= VAP;
    if (b->ap.property.in_cache) {
        score |= IN_CACHE;
    }
    highest_rssi = EFFECTIVE_RSSI(beacons_by_rssi[0]->h.rssi);
    lowest_rssi = EFFECTIVE_RSSI(beacons_by_rssi[NUM_APS(ctx) - 1]->h.rssi);
    /* divide the total rssi range equally between all the APs */
    band = (float)(highest_rssi - lowest_rssi) / (float)(NUM_APS(ctx) - 1);
    ideal_rssi = highest_rssi - (int)(band * (float)idx);

    /* include beacon strength score in low order bits */
    score |= (128 - ABS(ideal_rssi - EFFECTIVE_RSSI(beacons_by_rssi[idx]->h.rssi)));
#ifdef VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d rssi:%d ideal:%d rank:%02X:%d", idx,
        EFFECTIVE_RSSI(beacons_by_rssi[idx]->h.rssi), ideal_rssi, score >> 8, score & 0xFF);
#endif
    return score;
}

/*! \brief qsort comparison of rssi descending order
*
*  @param a element pointer
*  @param b element pointer
*
*  @return -1, 0 or 1 based on relative rssi value
*/
static int compare_rssi_qsort(const void *pa, const void *pb)
{
    Beacon_t *a = *(Beacon_t **)pa;
    Beacon_t *b = *(Beacon_t **)pb;

    return EFFECTIVE_RSSI(b->h.rssi) - EFFECTIVE_RSSI(a->h.rssi);
}

/*! \brief rank the APs - give each AP a desirability rating
*
*  @param ctx Skyhook request context
*
*  @return SKY_SUCCESS if beacons successfully ranked or SKY_ERROR
*/
static Sky_status_t rank(Sky_ctx_t *ctx)
{
    Beacon_t *beacons_by_rssi[MAX_AP_BEACONS + 1]; /* room for all APs in workspace */

    for (int i = 0; i < NUM_APS(ctx); i++)
        beacons_by_rssi[i] = &ctx->beacon[i];

    /* sort by rssi */
    qsort(beacons_by_rssi, NUM_APS(ctx), sizeof(Beacon_t *), &compare_rssi_qsort);

    /* based of range of rssi values, score each beacon by
     * it's attributes and deviation from a uniform distribution
     */
    for (int j = 0; j < NUM_APS(ctx); j++)
        beacons_by_rssi[j]->h.rank = get_desirablility(ctx, beacons_by_rssi, j);

    if (NUM_APS(ctx) == NUM_BEACONS(ctx))
        return SKY_SUCCESS; /* all beacons ranked */
    else
        return SKY_ERROR; /* continue to next plugin */
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
    .equal = equal, /*Compare two beacons */
    .remove_worst = remove_worst, /* Remove least desirable beacon from workspace */
    .cache_match = match, /* Find best match between workspace and cache lines */
    .add_to_cache = to_cache, /* Copy workspace beacons to a cacheline */
    .rank = rank /* update rank value of beacons in workspace */
};
