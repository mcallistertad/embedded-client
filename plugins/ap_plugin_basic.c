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
#include <limits.h>
#define SKY_LIBEL
#include "libel.h"

/* Uncomment VERBOSE_DEBUG to enable extra logging */
// #define VERBOSE_DEBUG

#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define EFFECTIVE_RSSI(b) ((b) == -1 ? (-127) : (b))

/*! \brief compare beacons for equality
 *
 *  if beacons are equivalent, return SKY_SUCCESS otherwise SKY_FAILURE
 *  if an error occurs during comparison. return SKY_ERROR
 */
static Sky_status_t beacon_equal(
    Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b, Sky_beacon_property_t *prop)
{
    if (!ctx || !a || !b) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return SKY_ERROR;
    }

#ifdef VERBOSE_DEBUG
    dump_beacon(ctx, "a:", a, __FILE__, __FUNCTION__);
    dump_beacon(ctx, "b:", b, __FILE__, __FUNCTION__);
#endif
    /* Two APs can be compared but others are ordered by type */
    if (a->h.type != SKY_BEACON_AP || b->h.type != SKY_BEACON_AP)
        return SKY_ERROR;

    /* test two APs for equivalence */
    switch (a->h.type) {
    case SKY_BEACON_AP:
        if (memcmp(a->ap.mac, b->ap.mac, MAC_SIZE) == 0) {
            if (prop != NULL && b->ap.property.in_cache)
                prop->in_cache = true;
            return SKY_SUCCESS;
        }
        break;
    default:
        break;
    }
    return SKY_FAILURE;
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
 *  @return -1, 0 or 1
 *  return 0 when NOT similar, -1 indicates parent is B, 1 parent is A
 *  if macs are similar, and index is not NULL, index is set to nibble index of difference
 *  if macs are identical, 1 is returned
 */
static int mac_similar(Sky_ctx_t *ctx, uint8_t macA[], uint8_t macB[], int *pn)
{
    /* Return 1 (true) if OUIs are identical and no more than 1 hex digits
     * differ between the two MACs. Else return 0 (false).
     */
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

/*! \brief try to remove one AP by selecting an AP which leaves best spread of rssi values
 *
 *  @param ctx Skyhook request context
 *
 *  @return boot true if beacon removed or false
 */
static bool remove_worst_ap_by_rssi(Sky_ctx_t *ctx)
{
    int i, reject, jump, up_down;
    float band_range, worst;
    float ideal_rssi[MAX_AP_BEACONS + 1];
    Beacon_t *b;

    if (NUM_APS(ctx) <= CONFIG(ctx->cache, max_ap_beacons))
        return false;

    if (ctx->beacon[0].h.type != SKY_BEACON_AP)
        return false;

    /* what share of the range of rssi values does each beacon represent */
    band_range = (EFFECTIVE_RSSI(ctx->beacon[0].h.rssi) -
                     EFFECTIVE_RSSI(ctx->beacon[NUM_APS(ctx) - 1].h.rssi)) /
                 ((float)NUM_APS(ctx) - 1);

    /* if the rssi range is small, throw away middle beacon */

    if (band_range < 0.5) {
        /* search from middle of range looking for uncached beacon */
        for (jump = 0, up_down = -1, i = NUM_APS(ctx) / 2; i >= 0 && i < NUM_APS(ctx);
             jump++, i += up_down * jump, up_down = -up_down) {
            b = &ctx->beacon[i];
            if (!b->ap.property.in_cache) {
                LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Warning: rssi range is small. %s beacon",
                    !jump ? "Remove middle Uncached" : "Found Uncached");
                return remove_beacon(ctx, i) == SKY_SUCCESS;
            }
        }
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Warning: rssi range is small. Removing cached beacon");
        return remove_beacon(ctx, NUM_APS(ctx) / 2) == SKY_SUCCESS;
    }

    /* if beacon with min RSSI is below threshold, */
    /* throw out weak one, not in cache, not Virtual Group or Unused  */
    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Weakest beacon rssi: %d(%d) vs %d threshold",
        EFFECTIVE_RSSI(ctx->beacon[NUM_APS(ctx) - 1].h.rssi), ctx->beacon[NUM_APS(ctx) - 1].h.rssi,
        -CONFIG(ctx->cache, cache_neg_rssi_threshold));
    if (EFFECTIVE_RSSI(ctx->beacon[NUM_APS(ctx) - 1].h.rssi) <
        -CONFIG(ctx->cache, cache_neg_rssi_threshold)) {
        for (i = NUM_APS(ctx) - 1, reject = -1; i > 0 && reject == -1; i--) {
            if (EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) <
                    -CONFIG(ctx->cache, cache_neg_rssi_threshold) &&
                !ctx->beacon[i].ap.property.in_cache)
                reject = i;
        }
        for (i = NUM_APS(ctx) - 1; i > 0 && reject == -1; i--) {
            if (EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) <
                -CONFIG(ctx->cache, cache_neg_rssi_threshold))
                reject = i;
        }
        if (reject == -1)
            reject =
                NUM_APS(ctx) -
                1; // reject lowest rssi value if there is no non-virtual group and no uncached or Unused beacon
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Discarding beacon %d with very weak strength", reject);
        return remove_beacon(ctx, reject) == SKY_SUCCESS;
    }

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "range: %d band range: %d.%02d",
        (int)(band_range * (NUM_APS(ctx) - 1)), (int)band_range,
        (int)fabs(round(100 * (band_range - (int)band_range))));

    /* for each beacon, work out it's ideal rssi value to give an even distribution */
    for (i = 0; i < NUM_APS(ctx); i++)
        ideal_rssi[i] = EFFECTIVE_RSSI(ctx->beacon[0].h.rssi) - (i * band_range);

    /* find AP with poorest fit to ideal rssi */
    /* always keep lowest and highest rssi */
    /* unless all the middle candidates are in the cache or virtual group */
    for (i = 1, reject = -1, worst = 0; i < NUM_APS(ctx) - 1; i++) {
        if (!ctx->beacon[i].ap.property.in_cache &&
            fabs(EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) - ideal_rssi[i]) >= worst) {
            worst = fabs(EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) - ideal_rssi[i]);
            reject = i;
        }
    }
    if (reject == -1) {
        /* haven't found a beacon to remove yet due to matching cached beacons */
        /* Throw away either lowest or highest rssi valued beacons if not cached */
        if (!ctx->beacon[NUM_APS(ctx) - 1].ap.property.in_cache)
            reject = NUM_APS(ctx) - 1;
        else if (!ctx->beacon[0].ap.property.in_cache)
            reject = 0;
        else
            reject = NUM_APS(ctx) / 2; /* remove middle beacon (all beacons are in cache) */
    }
#if SKY_DEBUG
    for (i = 0; i < NUM_APS(ctx); i++) {
        b = &ctx->beacon[i];
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
            "%s: %-2d, %s ideal %d.%02d fit %2d.%02d MAC %02X:%02X:%02X:%02X:%02X:%02X (%d)",
            (reject == i) ? "remove" : "      ", i, b->ap.property.in_cache ? "Cached" : "      ",
            (int)ideal_rssi[i], (int)fabs(round(100 * (ideal_rssi[i] - (int)ideal_rssi[i]))),
            (int)fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i]), ideal_rssi[i],
            (int)fabs(round(100 * (fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i]) -
                                      (int)fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i])))),
            b->ap.mac[0], b->ap.mac[1], b->ap.mac[2], b->ap.mac[3], b->ap.mac[4], b->ap.mac[5],
            b->h.rssi);
    }
#endif
    return remove_beacon(ctx, reject) == SKY_SUCCESS;
}

/*! \brief count number of cached APs in workspace relative to a cacheline
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in, otherwise count in workspace
 *
 *  @return number of used APs or -1 for fatal error
 */
static int count_cached_aps_in_workspace(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int num_aps_cached = 0;
    int j, i;
    if (!ctx || !cl)
        return -1;
    for (j = 0; j < ctx->ap_len; j++) {
        for (i = 0; i < cl->ap_len; i++) {
            num_aps_cached += beacon_equal(ctx, &ctx->beacon[j], &cl->beacon[i], NULL) ? 1 : 0;
        }
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(
        ctx, SKY_LOG_LEVEL_DEBUG, "%d APs in cache %d", num_aps_cached, cl - ctx->cache->cacheline);
#endif
    return num_aps_cached;
}

/*! \brief try to reduce AP by filtering out virtual AP
 *         When similar, remove beacon with highesr mac address
 *         unless it is in cache, then choose to remove the uncached beacon
 *
 *  @param ctx Skyhook request context
 *
 *  @return true if beacon removed or false otherwise
 */
static bool remove_virtual_ap(Sky_ctx_t *ctx)
{
    int i, j;
    int cmp, rm = -1;
#if SKY_DEBUG
    int keep = -1;
    bool cached = false;
#endif

    LOGFMT(
        ctx, SKY_LOG_LEVEL_DEBUG, "ap_len: %d APs of %d beacons", (int)ctx->ap_len, (int)ctx->len);

    if (ctx->ap_len <= CONFIG(ctx->cache, max_ap_beacons)) {
        return false;
    }

    DUMP_WORKSPACE(ctx);

    /* look for any AP beacon that is 'similar' to another */
    if (ctx->beacon[0].h.type != SKY_BEACON_AP) {
        LOGFMT(ctx, SKY_LOG_LEVEL_CRITICAL, "beacon type not WiFi");
        return false;
    }

    /* start search with weakest beacons first */
    for (j = NUM_APS(ctx) - 1; j >= 0; j--) {
        for (i = j - 1; i >= 0; i--) {
            if ((cmp = mac_similar(ctx, ctx->beacon[i].ap.mac, ctx->beacon[j].ap.mac, NULL)) < 0) {
                if (ctx->beacon[j].ap.property.in_cache) {
                    rm = i;
#if SKY_DEBUG
                    keep = j;
                    cached = true;
#endif
                } else {
                    rm = j;
#if SKY_DEBUG
                    keep = i;
#endif
                }
            } else if (cmp > 0) {
                if (ctx->beacon[i].ap.property.in_cache) {
                    rm = j;
#if SKY_DEBUG
                    keep = i;
                    cached = true;
#endif
                } else {
                    rm = i;
#if SKY_DEBUG
                    keep = j;
#endif
                }
            }
            if (rm != -1) {
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: %d similar to %d%s", rm, keep,
                    cached ? " (cached)" : "");
                remove_beacon(ctx, rm);
                return true;
            }
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "no match");
    return false;
}

/*! \brief note newest cache entry
 *
 *  @param ctx Skyhook request context
 *
 *  @return void
 */
static void update_newest_cacheline(Sky_ctx_t *ctx)
{
    int i;
    int newest = 0, idx = 0;

    for (i = 0; i < CACHE_SIZE; i++) {
        if (ctx->cache->cacheline[i].time > newest) {
            newest = ctx->cache->cacheline[i].time;
            idx = i;
        }
    }
    if (newest) {
        ctx->cache->newest = idx;
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cacheline %d is newest", idx);
    }
}

/*! \brief try to reduce AP by filtering out the oldest one
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static bool remove_worst_ap_by_age(Sky_ctx_t *ctx)
{
    int i;
    int oldest_idx = -1;
    uint32_t oldest_age = 0, youngest_age = UINT_MAX; /* age is in seconds, larger means older */

    if (ctx->ap_len <= CONFIG(ctx->cache, max_ap_beacons))
        return false;

    /* Find the youngest and oldest APs search from weakest */
    for (i = ctx->ap_len - 1; i >= 0; i--) {
        if (ctx->beacon[i].h.age < youngest_age) {
            youngest_age = ctx->beacon[i].h.age;
        }
        if (ctx->beacon[i].h.age > oldest_age) {
            oldest_age = ctx->beacon[i].h.age;
            oldest_idx = i;
        }
    }

    /* if the oldest and youngest beacons have the same age,
     * there is nothing to do. Otherwise remove the oldest */
    if (youngest_age != oldest_age && oldest_idx != -1) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: %d oldest", oldest_idx);
        remove_beacon(ctx, oldest_idx);
        return true;
    }
    return false;
}

static Sky_status_t beacon_remove_worst(Sky_ctx_t *ctx)
{
    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates of remove one based on rssi distribution */
    if (!remove_virtual_ap(ctx) && !remove_worst_ap_by_age(ctx) && !remove_worst_ap_by_rssi(ctx)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "failed to remove worst AP, try next plugin?");
        return SKY_ERROR;
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
static Sky_status_t beacon_match(Sky_ctx_t *ctx, int *idx)
{
    int i; /* i iterates through cacheline */
    int err; /* err breaks the seach due to bad value */
    float ratio; /* 0.0 <= ratio <= 1.0 is the degree to which workspace matches cacheline
                    In typical case this is the intersection(workspace, cache) / union(workspace, cache) */
    float bestratio = 0.0;
    float bestputratio = 0.0;
    int score; /* score is number of APs found in cacheline */
    int threshold; /* the threshold determined that ratio should meet */
    int num_aps_cached = 0;
    int bestc = -1, bestput = -1;
    int bestthresh = 0;
    Sky_cacheline_t *cl;
    bool result = false;

    if (!idx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad parameter");
        return SKY_ERROR;
    }

    /* expire old cachelines and note first empty cacheline as best line to save to */
    for (i = 0, err = false; i < CACHE_SIZE; i++) {
        cl = &ctx->cache->cacheline[i];
        /* if cacheline is old, mark it empty */
        if (cl->time != 0 && ((uint32_t)(*ctx->gettime)(NULL)-cl->time) >
                                 (CONFIG(ctx->cache, cache_age_threshold) * SECONDS_IN_HOUR)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache line %d expired", i);
            cl->time = 0;
        }
        /* if line is empty and it is the first one, remember it */
        if (cl->time == 0) {
            if (bestputratio < 1.0) {
                bestput = i;
                bestputratio = 1.0;
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
        cl = &ctx->cache->cacheline[i];
        threshold = ratio = score = 0;
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
                threshold = CONFIG(ctx->cache, cache_match_used_threshold);
                ratio = (float)score / unionAB;
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: score %d (%d/%d) vs %d", i,
                    (int)round(ratio * 100), score, unionAB, threshold);
                result = true;
            }
        }

        if (ratio > bestputratio) {
            bestput = i;
            bestputratio = ratio;
        }
        if (ratio > bestratio) {
            if (bestratio > 0.0)
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "Found better match in cache %d of %d score %d (vs %d)", i, CACHE_SIZE,
                    (int)round(ratio * 100), threshold);
            bestc = i;
            bestratio = ratio;
            bestthresh = threshold;
        }
        if (ratio * 100 > threshold)
            break;
    }
    if (err) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad parameters counting APs");
        return SKY_ERROR;
    }

    /* make a note of the best match used by add_to_cache */
    ctx->save_to = bestput;

    if (bestratio * 100 > bestthresh) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "location in cache, pick cache %d of %d score %d (vs %d)",
            bestc, CACHE_SIZE, (int)round(bestratio * 100), bestthresh);
        *idx = bestc;
        return SKY_SUCCESS;
    }
    if (result) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No Cache match found. Cache %d, best score %d (vs %d)",
            bestc, (int)round(bestratio * 100), bestthresh);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, CACHE_SIZE, (int)round(bestputratio * 100));
        return SKY_FAILURE;
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Unable to compare using APs. No cache match");
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d", bestput,
        CACHE_SIZE, (int)round(bestputratio * 100));
    return SKY_ERROR;
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
static Sky_status_t beacon_to_cache(Sky_ctx_t *ctx, Sky_location_t *loc)
{
    int i = ctx->save_to;
    int j;
    uint32_t now = (*ctx->gettime)(NULL);
    Sky_cacheline_t *cl;

    if (CACHE_SIZE < 1) {
        return SKY_SUCCESS;
    }

    /* compare current time to Mar 1st 2019 */
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Time (now) %d %d", now, time(NULL));
    if (now <= TIMESTAMP_2019_03_01) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Don't have good time of day! %u", now);
        return SKY_ERROR;
    }

    /* if best 'save-to' location was not set by beacon_score, use oldest */
    if (i < 0) {
        i = find_oldest(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "find_oldest chose cache %d of %d", i, CACHE_SIZE);
    }
    cl = &ctx->cache->cacheline[i];
    if (loc->location_status != SKY_LOCATION_STATUS_SUCCESS) {
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Won't add unknown location to cache");
        cl->time = 0; /* clear cacheline */
        update_newest_cacheline(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "clearing cache %d of %d", i, CACHE_SIZE);
        return SKY_ERROR;
    } else if (cl->time == 0)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Saving to empty cache %d of %d", i, CACHE_SIZE);
    else
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Saving to cache %d of %d", i, CACHE_SIZE);

    cl->len = NUM_BEACONS(ctx);
    cl->ap_len = NUM_APS(ctx);
    cl->connected = ctx->connected;
    cl->loc = *loc;
    cl->time = now;
    ctx->cache->newest = i;

    for (j = 0; j < NUM_BEACONS(ctx); j++) {
        cl->beacon[j] = ctx->beacon[j];
        if (cl->beacon[j].h.type == SKY_BEACON_AP) {
            cl->beacon[j].ap.property.in_cache = true;
        }
    }
    DUMP_CACHE(ctx);
    return SKY_SUCCESS;
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
    .equal = beacon_equal, /*Compare two beacons for equality */
    .remove_worst = beacon_remove_worst, /* Remove least desirable beacon from workspace */
    .cache_match = beacon_match, /* Find best match between workspace and cache lines */
    .add_to_cache = beacon_to_cache /* Copy workspace beacons to a cacheline */
};
