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
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "libel.h"

/* set VERBOSE_DEBUG to true to enable extra logging */
#ifndef VERBOSE_DEBUG
#define VERBOSE_DEBUG false
#endif // VERBOSE_DEBUG

#define IDX(b, rctx) ((b) - (rctx)->beacon)
#define ABS(a) (((a) < 0) ? (-(a)) : (a))
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

#if !SKY_EXCLUDE_WIFI_SUPPORT
static Sky_status_t set_priorities(Sky_rctx_t *rctx);
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

/*! \brief test two APs for equality
 *
 *  @param rctx Skyhook request context
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
    Sky_rctx_t *rctx, Beacon_t *a, Beacon_t *b, Sky_beacon_property_t *prop, bool *equal)
{
#if !SKY_EXCLUDE_WIFI_SUPPORT
    if (!rctx || !a || !b || !equal) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "bad params");
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
#else
    (void)rctx; /* suppress warning unused parameter */
    (void)a; /* suppress warning unused parameter */
    (void)b; /* suppress warning unused parameter */
    (void)prop; /* suppress warning unused parameter */
    (void)equal; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif // !SKY_EXCLUDE_WIFI_SUPPORT
}

/*! \brief compare AP for ordering when adding to context
 *
 *  AP order is primarily based on signal strength
 *  lowest MAC address is used as tie breaker if strengths are the same
 *
 *  @param rctx Skyhook request context
 *  @param a pointer to an AP
 *  @param b pointer to an AP
 *  @param diff result of comparison, positive when a is better
 *
 *  @return
 *  if beacons are comparable, return SKY_SUCCESS and difference
 *  (greater than zero if a should be before b)
 *  if an error occurs during comparison. return SKY_ERROR
 */
static Sky_status_t compare(Sky_rctx_t *rctx, Beacon_t *a, Beacon_t *b, int *diff)
{
#if !SKY_EXCLUDE_WIFI_SUPPORT
    if (!rctx || !a || !b || !diff) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "bad params");
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
#else
    (void)rctx; /* suppress warning unused parameter */
    (void)a; /* suppress warning unused parameter */
    (void)b; /* suppress warning unused parameter */
    (void)diff; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif // !SKY_EXCLUDE_WIFI_SUPPORT
}

#if !SKY_EXCLUDE_WIFI_SUPPORT
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
 *  @return negative, 0 or positive:
 *  - zero when MACs are NOT similar
 *  - negative when parent is A (i.e., A has lower MAC address)
 *  - positive when parent is B (i.e., B has lower MAC address)
 *
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

    /* No more than one nibble is different, so they're similar. Unless their respective
     * local admin bits differ, in which case, they're not.
     */
    if (LOCAL_ADMIN_MASK(macA[0]) != LOCAL_ADMIN_MASK(macB[0])) {
        return 0; /* not similar */
    }

    /* report which nibble is different */
    if (pn)
        *pn = idx_diff;
    return result;
}

#if CACHE_SIZE
/*! \brief count number of cached APs in request rctx relative to a cacheline
 *
 *  @param rctx Skyhook request context
 *  @param cl the cacheline to count in, otherwise count in request rctx
 *
 *  @return number of cached APs or -1 for fatal error
 */
static int count_cached_aps_in_request_ctx(Sky_rctx_t *rctx, Sky_cacheline_t *cl)
{
    int num_aps_cached = 0;
    int j, i;
    for (j = 0; j < NUM_APS(rctx); j++) {
        for (i = 0; i < NUM_APS(cl); i++) {
            bool equivalent = false;
            equal(rctx, &rctx->beacon[j], &cl->beacon[i], NULL, &equivalent);
            num_aps_cached += equivalent;
        }
    }
#if VERBOSE_DEBUG
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d APs in cache %d", num_aps_cached,
        cl - rctx->session->cacheline);
#endif // VERBOSE_DEBUG
    return num_aps_cached;
}
#endif // CACHE_SIZE

/*! \brief determine which of a pair of APs is more valuable
 *
 *  return positive value if i is more valuable, negative value is j is more valuable
 *  otherwise return positive if worse (or 0 when same)
 *
 *  @param rctx Skyhook request context
 *
 */
#define CONNECTED_AND_IN_CACHE_ONLY(priority) ((int16_t)(priority) & (CONNECTED | IN_CACHE))
static int cmp_properties(Sky_rctx_t *rctx, int i, int j)
{
    return (CONNECTED_AND_IN_CACHE_ONLY(rctx->beacon[i].h.priority) -
            CONNECTED_AND_IN_CACHE_ONLY(rctx->beacon[j].h.priority));
}

/*! \brief Remove a single virtual AP
 *
 *  When similar, select beacon with highest mac address
 *  unless it better properties, then choose to select the other beacon
 *  Remove the selected beacon with worst properties
 *
 *  @param rctx Skyhook request context
 *
 *  @return true if beacon removed or false otherwise
 */
static bool remove_virtual_ap(Sky_rctx_t *rctx)
{
    int i, j;
    Beacon_t *vap_a = NULL;
    Beacon_t *vap_b = NULL;
    Beacon_t *worst_vap = NULL;

    if (NUM_APS(rctx) <= CONFIG(rctx->session, max_ap_beacons)) {
        return false;
    }

    if (rctx->beacon[0].h.type != SKY_BEACON_AP) {
        LOGFMT(rctx, SKY_LOG_LEVEL_CRITICAL, "beacon type not WiFi");
        return false;
    }

    /*
     * Iterate over all beacon pairs. For each pair whose members are "similar" to
     * one another (i.e., which are part of the same virtual AP (VAP) group),
     * identify which member of the pair is a candidate for removal. After iterating,
     * remove the worse such candidate. Note: connected APs are ignored.
     */
    for (j = NUM_APS(rctx) - 1; j > 0; j--) {
        /* if connected, ignore this AP */
        if (rctx->beacon[j].h.connected)
            continue;
        for (i = j - 1; i >= 0; i--) {
            /* if connected, ignore this AP */
            if (rctx->beacon[i].h.connected)
                continue;

            int mac_diff = mac_similar(
                rctx->beacon[i].ap.mac, rctx->beacon[j].ap.mac, NULL); // <0 => i is better
            if (mac_diff != 0) {
                /* The MACs are similar (part of the same VAP group). Removal candidate is
                 * the one with worse properties, or, if properties are the same, the one with
                 * the higher MAC address.
                 */
                int prop_diff = cmp_properties(rctx, i, j); // <0 => j is better
                if (prop_diff > 0 || (prop_diff == 0 && mac_diff < 0)) {
                    /* i is better (either because of major properties or mac). j becomes removal candidate */
                    vap_a = &rctx->beacon[j];
                    vap_b = &rctx->beacon[i];
                } else {
                    /* j is better. i becomes removal candidate */
                    vap_a = &rctx->beacon[i];
                    vap_b = &rctx->beacon[j];
                }
#if VERBOSE_DEBUG
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d similar and worse than %d%s",
                    IDX(vap_a, rctx), IDX(vap_b, rctx),
                    vap_b->ap.property.in_cache != vap_a->ap.property.in_cache ?
                        "(cached)" :
                        mac_diff < 0 ? "(mac)" : "");
                dump_ap(rctx, "similar A:  ", vap_a, __FILE__, __FUNCTION__);
                dump_ap(rctx, "similar B:  ", vap_b, __FILE__, __FUNCTION__);
#else
                (void)vap_b;
#endif // VERBOSE_DEBUG
                if (worst_vap != NULL)
                    prop_diff = cmp_properties(rctx, IDX(vap_a, rctx), IDX(worst_vap, rctx));

                if (worst_vap == NULL || prop_diff < 0 ||
                    (prop_diff == 0 && COMPARE_MAC(vap_a, worst_vap) < 0)) {
                    /* This is the first removal candidate or its properties are
                     * worse than the current candidate or its properties are the same
                     * but it has a larger MAC value. */
                    worst_vap = vap_a;
                    dump_ap(rctx, "worst vap:>>", vap_a, __FILE__, __FUNCTION__);
                }
            }
        }
    }
    if (worst_vap != NULL) {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "removing virtual AP idx: %d", IDX(worst_vap, rctx));
        return (remove_beacon(rctx, IDX(worst_vap, rctx)) == SKY_SUCCESS);
    }
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "no match");
    return false;
}

/*! \brief try to reduce AP by filtering out the oldest one
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static bool remove_oldest_ap(Sky_rctx_t *rctx)
{
    int i;
    int oldest_idx = -1;
    uint32_t oldest_age = 0, youngest_age = UINT_MAX; /* age is in seconds, larger means older */

    /* Find the youngest and oldest APs */
    for (i = 0; i < NUM_APS(rctx); i++) {
        if (rctx->beacon[i].h.age < youngest_age) {
            youngest_age = rctx->beacon[i].h.age;
        }
        if (rctx->beacon[i].h.age > oldest_age) {
            oldest_age = rctx->beacon[i].h.age;
            oldest_idx = i;
        }
    }

    /* if the oldest and youngest beacons have the same age,
     * there is nothing to do. Otherwise remove the oldest */
    if (youngest_age != oldest_age && oldest_idx != -1) {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: %d oldest", oldest_idx);
        remove_beacon(rctx, oldest_idx);
        return true;
    }
    return false;
}
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

/*! \brief try to reduce AP by filtering out the worst one
 *
 *  Request Context AP beacons are stored in decreasing rssi order
 *
 *  @param rctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static Sky_status_t remove_worst(Sky_rctx_t *rctx)
{
#if !SKY_EXCLUDE_WIFI_SUPPORT
    int idx_of_worst;

    idx_of_worst = set_priorities(rctx);
    /* no work to do if request context is not full of max APs */
    if (NUM_APS(rctx) <= CONFIG(rctx->session, max_ap_beacons)) {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "No need to remove AP");
        return SKY_ERROR;
    }

    DUMP_REQUEST_CTX(rctx);
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Overall worst AP idx: %d", idx_of_worst);

    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates or remove one based on age, rssi distribution etc */
    if (!remove_virtual_ap(rctx) && !remove_oldest_ap(rctx)) {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "removing worst AP idx: %d", idx_of_worst);
        return remove_beacon(rctx, idx_of_worst);
    }
    return SKY_SUCCESS;
#else
    (void)rctx; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif // !SKY_EXCLUDE_WIFI_SUPPORT
}

/*! \brief find cache entry with a match to request rctx
 *
 *   Expire any old cachelines
 *   Compare each cacheline with the request rctx beacons:
 *    . If request rctx has enough cached APs, compare them with low threshold
 *    . If just a few APs, compare all APs with higher threshold
 *    . If no APs, compare cells for 100% match
 *
 *   If any cacheline score meets threshold, accept it
 *   setting hit to true and from_cache to cachline index.
 *   While searching, keep track of best cacheline to
 *   save a new server response. An empty cacheline is
 *   best, a good match is next, oldest is the fall back.
 *   Best cacheline to 'save_to' is set to cacheline index for later use.
 *
 *  @param rctx Skyhook request context
 *
 *  @return Sky_status_t SKY_SUCCESS if search produced result, SKY_ERROR otherwise
 */
static Sky_status_t match(Sky_rctx_t *rctx)
{
#if CACHE_SIZE && !SKY_EXCLUDE_WIFI_SUPPORT
    int i; /* i iterates through cacheline */
    float ratio; /* 0.0 <= ratio <= 1.0 is the degree to which request rctx matches cacheline
                    In typical case this is the intersection(request rctx, cache) / union(request rctx, cache) */
    float bestratio = 0.0f;
    float bestputratio = 0.0f;
    int score; /* score is number of APs found in cacheline */
    int threshold; /* the threshold determined that ratio should meet */
    int num_aps_cached = 0;
    int bestc = -1;
    int16_t bestput = -1;
    int bestthresh = 0;
    Sky_cacheline_t *cl;

    /* expire old cachelines and note first empty cacheline as best line to save to */
    for (i = 0; i < rctx->session->num_cachelines; i++) {
        cl = &rctx->session->cacheline[i];
        /* if cacheline is old, mark it empty */
        if (cl->time != CACHE_EMPTY &&
            (rctx->header.time - cl->time) >
                (CONFIG(rctx->session, cache_age_threshold) * SECONDS_IN_HOUR)) {
            LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Cacheline %d expired", i);
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

    if (NUM_APS(rctx) == 0) {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Unable to compare using APs. No cache match");
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, CACHE_SIZE, (int)round((double)bestputratio * 100));
        return SKY_ERROR;
    }

    DUMP_REQUEST_CTX(rctx);
    DUMP_CACHE(rctx);

    /* score each cacheline wrt beacon match ratio */
    for (i = 0; i < rctx->session->num_cachelines; i++) {
        cl = &rctx->session->cacheline[i];
        threshold = score = 0;
        ratio = 0.0f;
        if (cl->time == CACHE_EMPTY) {
            LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score 0 for empty cacheline", i);
            continue;
#if !SKY_EXCLUDE_CELL_SUPPORT && !SKY_EXCLUDE_GNSS_SUPPORT
        } else if (serving_cell_changed(rctx, cl) == true || cached_gnss_worse(rctx, cl) == true) {
#elif !SKY_EXCLUDE_CELL_SUPPORT && SKY_EXCLUDE_GNSS_SUPPORT
        } else if (serving_cell_changed(rctx, cl) == true) {
#elif SKY_EXCLUDE_CELL_SUPPORT && !SKY_EXCLUDE_GNSS_SUPPORT
        } else if (cached_gnss_worse(rctx, cl) == true) {
#else
        } else if (0) {
#endif // !SKY_EXCLUDE_CELL_SUPPORT && !SKY_EXCLUDE_GNSS_SUPPORT
            /* no support for cell or gnss, so no possibility of forced miss */
            LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                "Cache: %d: Score 0 for cacheline with difference cell or worse gnss", i);
            continue;
        } else {
            /* count number of matching APs in request rctx and cache */
            if ((num_aps_cached = count_cached_aps_in_request_ctx(rctx, cl)) < 0) {
                return SKY_ERROR;
            } else if (NUM_APS(rctx) && NUM_APS(cl)) {
                /* Score based on ALL APs */
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on ALL APs", i);
                score = num_aps_cached;
                int unionAB = NUM_APS(rctx) + NUM_APS(cl) - num_aps_cached;
                if (NUM_APS(rctx) <= CONFIG(rctx->session, cache_beacon_threshold))
                    threshold = 99; /* cache hit requires 100% */
                else
                    threshold = CONFIG(rctx->session, cache_match_all_threshold);
                ratio = (float)score / unionAB;
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: score %d (%d/%d) vs %d", i,
                    (int)round((double)ratio * 100), score, unionAB, threshold);
            }
        }

        if (ratio > bestputratio) {
            bestput = (int16_t)i;
            bestputratio = ratio;
        }
        if (ratio > bestratio) {
            if (bestratio > 0.0)
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                    "Found better match in cache %d of %d score %d (vs %d)", i,
                    rctx->session->num_cachelines, (int)round((double)ratio * 100), threshold);
            bestc = i;
            bestratio = ratio;
            bestthresh = threshold;
        }
        if (ratio * 100 > (float)threshold)
            break;
    }

    /* make a note of the best match used by add_to_cache */
    rctx->save_to = bestput;

    rctx->get_from = bestc;
    if ((bestratio * 100) > (float)bestthresh) {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "location in cache, pick cache %d of %d score %d (vs %d)",
            bestc, rctx->session->num_cachelines, (int)round((double)bestratio * 100), bestthresh);
        rctx->hit = true;
    } else {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "No Cache match found. Cache %d, best score %d (vs %d)",
            bestc, (int)round((double)bestratio * 100), bestthresh);
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of %d score %d",
            bestput, rctx->session->num_cachelines, (int)round((double)bestputratio * 100));
        rctx->get_from = false;
    }
    return SKY_SUCCESS;
#else
    rctx->get_from = -1;
    rctx->hit = false;
    (void)rctx; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif // CACHE_SIZE && !SKY_EXCLUDE_WIFI_SUPPORT
}

/*! \brief add location to cache
 *
 *   The location is saved in the cacheline indicated by bestput (set by find_best_match)
 *   unless this is -1, in which case, location is saved in oldest cacheline.
 *
 *  @param rctx Skyhook request context
 *  @param loc pointer to location info
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
static Sky_status_t to_cache(Sky_rctx_t *rctx, Sky_location_t *loc)
{
#if CACHE_SIZE && !SKY_EXCLUDE_WIFI_SUPPORT
    int i = rctx->save_to;
    int j;
    Sky_cacheline_t *cl;

    /* compare current time to Mar 1st 2019 */
    if (loc->time <= TIMESTAMP_2019_03_01) {
        return SKY_ERROR;
    }

    /* if best 'save-to' location was not set by beacon_score, use oldest */
    if (i < 0) {
        i = find_oldest(rctx);
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "find_oldest chose cache %d of %d", i,
            rctx->session->num_cachelines);
    }
    cl = &rctx->session->cacheline[i];
    if (loc->location_status != SKY_LOCATION_STATUS_SUCCESS) {
        LOGFMT(rctx, SKY_LOG_LEVEL_WARNING, "Won't add unknown location to cache");
        cl->time = CACHE_EMPTY; /* clear cacheline */
        LOGFMT(
            rctx, SKY_LOG_LEVEL_DEBUG, "clearing cache %d of %d", i, rctx->session->num_cachelines);
        return SKY_ERROR;
    } else if (cl->time == CACHE_EMPTY)
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Saving to empty cache %d of %d", i,
            rctx->session->num_cachelines);
    else
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Saving to cache %d of %d", i,
            rctx->session->num_cachelines);

    cl->num_beacons = NUM_BEACONS(rctx);
    cl->num_ap = NUM_APS(rctx);
#if !SKY_EXCLUDE_GNSS_SUPPORT
    cl->gnss = rctx->gnss;
#endif // !SKY_EXCLUDE_GNSS_SUPPORT
    cl->loc = *loc;
    cl->time = loc->time;

    for (j = 0; j < NUM_BEACONS(rctx); j++) {
        cl->beacon[j] = rctx->beacon[j];
        if (cl->beacon[j].h.type == SKY_BEACON_AP) {
            cl->beacon[j].ap.property.in_cache = false;
        }
    }
    DUMP_CACHE(rctx);
    return SKY_SUCCESS;
#else
    (void)rctx; /* suppress warning unused parameter */
    (void)loc; /* suppress warning unused parameter */
    return SKY_SUCCESS;
#endif // CACHE_SIZE && !SKY_EXCLUDE_WIFI_SUPPORT
}

#if !SKY_EXCLUDE_WIFI_SUPPORT
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
 *  @param rctx pointer to request context
 *  @param b pointer to AP
 *
 *  @return computed priority
 */
static float get_priority(Sky_rctx_t *rctx, Beacon_t *b)
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
    highest_rssi = EFFECTIVE_RSSI(rctx->beacon[0].h.rssi);
    lowest_rssi = EFFECTIVE_RSSI(rctx->beacon[NUM_APS(rctx) - 1].h.rssi);

    /* Find the deviation of the AP's RSSI from its ideal RSSI. Subtract this number from
     * 128 so that smaller deviations are considered better.
     */
    band_width = (float)(highest_rssi - lowest_rssi) / (float)(NUM_APS(rctx) - 1);
    ideal_rssi = (float)highest_rssi - band_width * (float)(IDX(b, rctx));
    deviation = ABS(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi);
    priority += 128 - deviation;
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d bandwidth:%.1f ideal:%.1f dev:%.1f priority:%.1f",
        IDX(b, rctx), band_width, ideal_rssi, deviation, priority);

#if VERBOSE_DEBUGC
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d rssi:%d ideal:%.1f priority:%.1f", IDX(b, rctx),
        EFFECTIVE_RSSI(b->h.rssi), deviation, priority);
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d rssi:%d ideal:%d.%d priority:%d.%d", IDX(b, rctx),
        EFFECTIVE_RSSI(b->h.rssi), deviation, (int)(deviation * 10), (int)priority,
        (int)((priority - (int)priority) * 10));
#endif // VERBOSE_DEBUGC
    return priority;
}

/*! \brief assign priority value to all beacons
 *
 * use get_priority to assign a priority to each beacon in request context
 * process APs from the middle and remember the worst AP found
 * if the weakest AP is below threshold, find the worst weak AP
 *
 *  @param rctx Skyhook request context
 *
 *  @return idx of beacon with lowest priority
 */
static int set_priorities(Sky_rctx_t *rctx)
{
    int j, jump, up_down;
    int idx_of_worst = NUM_APS(rctx) / 2;
    float priority_of_worst = HIGHEST_PRIORITY;
    bool weak_only;

    /* if weakest AP is below threshold
     * look for lowest priority weak beacon */
    weak_only = (AP_BELOW_RSSI_THRESHOLD(rctx, NUM_APS(rctx) - 1));

    /* search to the middle of range looking for worst AP */
    for (jump = NUM_APS(rctx), up_down = 1, j = 0; j >= 0 && j < NUM_APS(rctx) && jump > 0;
         jump--, j += up_down * jump, up_down = -up_down) {
        rctx->beacon[j].h.priority = get_priority(rctx, &rctx->beacon[j]);
        if ((weak_only && AP_BELOW_RSSI_THRESHOLD(rctx, j)) ||
            rctx->beacon[j].h.priority <= priority_of_worst) {
            /* break a priority tie with mac */
            if (rctx->beacon[j].h.priority != priority_of_worst ||
                COMPARE_MAC(&rctx->beacon[j], &rctx->beacon[idx_of_worst]) < 0) {
                idx_of_worst = j;
                priority_of_worst = rctx->beacon[j].h.priority;
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "idx_of_worst %d", idx_of_worst);
            }
        }
    }

    return idx_of_worst;
}
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

#ifdef UNITTESTS

TEST_FUNC(test_ap_plugin)
{
    GROUP("remove worst");
    TEST("remove_worst removes ap with worst fit to ideal rssi", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x3B, 0x5E, 0x0C, 0xB0, 0x17, 0x4D };
        uint8_t mac3[] = { 0x2A, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        uint8_t mac4[] = { 0x19, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint32_t value;

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        /* Add in rssi order */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, -50, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, -90, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, -76, freq, false));
        /* add one more AP than MAX AP Beacons allows with rssi value away from ideal*/
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, -60, freq, false));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        /*
         *  >>> beacons.c:remove_beacon() req     0     Wi-Fi            MAC 4C:5E:0C:B0:17:4B 3660MHz rssi:-50 age:0 pri:128.0
         *  >>> beacons.c:remove_beacon() req     1     Wi-Fi            MAC 2A:5E:0C:B0:17:4C 3660MHz rssi:-76 age:0 pri:127.3
         *  >>> beacons.c:remove_beacon() req     2     Wi-Fi            MAC 3B:5E:0C:B0:17:4D 3660MHz rssi:-90 age:0 pri:128.0

         */
        ASSERT(ctx->beacon[0].ap.mac[5] == 0x4B);
        ASSERT(ctx->beacon[1].ap.mac[5] == 0x4C);
        ASSERT(ctx->beacon[2].ap.mac[5] == 0x4D);
    });
    TEST("remove_worst removes ap with higher mac if same rssi", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x3B, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        uint8_t mac3[] = { 0x2A, 0x5E, 0x0C, 0xB0, 0x17, 0x4A }; /* remove */
        uint8_t mac4[] = { 0x19, 0x5E, 0x0C, 0xB0, 0x17, 0x49 }; /* keep */
        uint32_t value;

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        /* Add in rssi order */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, -50, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, -83, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, -60, freq, false));
        /* add one more AP than MAX AP Beacons allows with rssi value away from ideal*/
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, -73, freq, false));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        ASSERT(ctx->beacon[0].ap.mac[5] == 0x4C);
        ASSERT(ctx->beacon[1].ap.mac[5] == 0x49);
        ASSERT(ctx->beacon[2].ap.mac[5] == 0x4B);
    });
    TEST("remove_worst removes ap with higher mac if same rssi unless connected", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x3B, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        uint8_t mac3[] = { 0x2A, 0x5E, 0x0C, 0xB0, 0x17, 0x4A }; /* connected */
        uint8_t mac4[] = { 0x19, 0x5E, 0x0C, 0xB0, 0x17, 0x49 };
        uint32_t value;

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        /* Add in rssi order */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, -50, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, -83, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, -60, freq, true));
        /* add one more AP than MAX AP Beacons allows with rssi value away from ideal*/
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, -73, freq, false));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        ASSERT(ctx->beacon[0].ap.mac[5] == 0x4C);
        ASSERT(ctx->beacon[1].ap.mac[5] == 0x4A);
        ASSERT(ctx->beacon[2].ap.mac[5] == 0x4B);
    });
    TEST("remove_worst removes highest mac VAP", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4D };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint32_t value;

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        /* Add in rssi order */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi--, freq, false));
        /* add one more VAP than MAX AP Beacons allows */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        ASSERT(ctx->beacon[0].ap.mac[5] == 0x4B);
        ASSERT(ctx->beacon[1].ap.mac[5] == 0x4C);
        ASSERT(ctx->beacon[2].ap.mac[5] == 0x4A);
    });
    TEST("remove_worst respects connected properties removing VAP", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4D };
        uint32_t value;

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        /* Add in rssi order */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi--, freq, false));
        /* add one more VAP than MAX AP Beacons allows */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi--, freq, true));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        ASSERT(ctx->beacon[0].ap.mac[5] == 0x4B);
        ASSERT(ctx->beacon[1].ap.mac[5] == 0x4A);
        ASSERT(ctx->beacon[2].ap.mac[5] == 0x4D);
    });
    TEST("remove_worst removes VAP with highest mac", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0xAC };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0xAD }; /* remove */
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint32_t value;

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        /* Add in rssi order */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi--, freq, false));
        /* add one more VAP than MAX AP Beacons allows */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        ASSERT(ctx->beacon[0].ap.mac[5] == 0x4B);
        ASSERT(ctx->beacon[1].ap.mac[5] == 0xAC);
        ASSERT(ctx->beacon[2].ap.mac[5] == 0x4A);
    });
    TEST("remove_worst removes VAP with highest mac unless cached", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0xAD };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0xAC }; /* remove */
        uint32_t value;

        ASSERT(sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3) == SKY_SUCCESS);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
        /* Add in rssi order */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi--, freq, false));
        ctx->beacon[0].ap.property.in_cache = true;
        ctx->beacon[1].ap.property.in_cache = true;
        ctx->beacon[2].ap.property.in_cache = true;
        /* add one more VAP than MAX AP Beacons allows */
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi--, freq, false));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        ASSERT(ctx->beacon[0].ap.mac[5] == 0x4B);
        ASSERT(ctx->beacon[1].ap.mac[5] == 0xAD);
        ASSERT(ctx->beacon[2].ap.mac[5] == 0x4A);
    });
}

static Sky_status_t unit_tests(void *_ctx)
{
    GROUP_CALL("Remove Worst", test_ap_plugin);
    return SKY_SUCCESS;
}

#endif // UNITTESTS
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
    .cache_match = match, /* Find best match between request context and cachelines */
    .add_to_cache = to_cache, /* Copy request context beacons to a cacheline */
#ifdef UNITTESTS
    .unit_tests = unit_tests, /* Unit Tests */
#endif // UNITTESTS
};
