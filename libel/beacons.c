/*! \file libel/beacons.c
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
#include <stdio.h>
#include <stdlib.h>
#define SKY_LIBEL
#include "libel.h"

/* set VERBOSE_DEBUG to true to enable extra logging */
#ifndef VERBOSE_DEBUG
#define VERBOSE_DEBUG false
#endif

/*! \brief shuffle list to remove the beacon at index
 *
 *  @param ctx Skyhook request context
 *  @param index 0 based index of AP to remove
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t remove_beacon(Sky_ctx_t *ctx, int index)
{
    if (index >= NUM_BEACONS(ctx))
        return SKY_ERROR;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "type:%s idx:%d", sky_pbeacon(&ctx->beacon[index]), index);
    memmove(&ctx->beacon[index], &ctx->beacon[index + 1],
        sizeof(Beacon_t) * (NUM_BEACONS(ctx) - index - 1));
    if (is_ap_type(&ctx->beacon[index]))
        NUM_APS(ctx) -= 1;
    NUM_BEACONS(ctx) -= 1;
#if VERBOSE_DEBUG
    DUMP_WORKSPACE(ctx);
#endif
    return SKY_SUCCESS;
}

/*! \brief compare beacons for positioning in workspace
 *
 * better beacons are inserted before worse.
 *
 *  @param ctx Skyhook request context
 *  @param a pointer to beacon A
 *  @param B pointer to beacon B
 *
 *  @return  >0 if beacon A is better
 *           <0 if beacon B is better
 */
static int is_beacon_better(Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b)
{
    int diff = 0;

#if VERBOSE_DEBUG
    dump_beacon(ctx, "A: ", a, __FILE__, __FUNCTION__);
    dump_beacon(ctx, "B: ", b, __FILE__, __FUNCTION__);
#endif
    /* sky_plugin_compare compares beacons of the same class and returns SKY_ERROR when they are different classes */
    if ((sky_plugin_compare(ctx, NULL, a, b, &diff)) == SKY_ERROR) {
        /* Beacons are different classes, so compare like this */
        /* If either beacon is not cell, order by type */
        /* If one beacon is nmr cell, order fully qualified first */
        /* If one cell is connected, order connected first */
        /* otherwise order by type */
        if (!is_cell_type(a) || !is_cell_type(b))
            diff = COMPARE_TYPE(a, b) >= 0 ? 1 : -1;
        else if (is_cell_nmr(a) != is_cell_nmr(b))
            /* fully qualified cell is better */
            diff = (!is_cell_nmr(a) ? 1 : -1);
        else if (a->h.connected != b->h.connected)
            diff = COMPARE_CONNECTED(a, b);
        else
            diff = COMPARE_TYPE(a, b);
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Different classes %d (%s)", diff,
            diff < 0 ? "B is better" : "A is better");
#endif
    } else {
        /* otherwise beacons were comparable and plugin set diff appropriately */
        diff = (diff != 0) ? diff : 1; /* choose A if plugin returns 0 */
#if VERBOSE_DEBUG
        if (diff)
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Same types %d (%s)", diff,
                diff < 0 ? "B is better" : "A is better");
#endif
    }
    return diff;
}

/*! \brief insert beacon in list based on type and AP rssi
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno pointer to errno
 *  @param b beacon to add
 *  @param index pointer where to save the insert position
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
static Sky_status_t insert_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b)
{
    int j;

    /* sanity checks */
    if (!validate_workspace(ctx) || b->h.magic != BEACON_MAGIC || b->h.type >= SKY_BEACON_MAX) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Invalid params. Beacon type %s", sky_pbeacon(b));
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* check for duplicate */
    if (is_ap_type(b) || is_cell_type(b)) {
        for (j = 0; j < NUM_BEACONS(ctx); j++) {
            bool equal = false;

            if (sky_plugin_equal(ctx, sky_errno, b, &ctx->beacon[j], NULL, &equal) == SKY_SUCCESS &&
                equal) {
                /* Found duplicate - keep new beacon if it is better */
                if (b->h.age < ctx->beacon[j].h.age || /* Younger */
                    (b->h.age == ctx->beacon[j].h.age &&
                        b->h.connected) || /* same age, but connected */
                    (b->h.age ==
                            ctx->beacon[j].h.age && /* same age and connectedness, but stronger */
                        b->h.connected == ctx->beacon[j].h.connected &&
                        b->h.rssi > ctx->beacon[j].h.rssi)) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Keep new duplicate");
                    break; /* fall through to remove existing duplicate */
                } else {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate");
                    return set_error_status(sky_errno, SKY_ERROR_NONE);
                }
            }
        }
        if (j < NUM_BEACONS(ctx)) {
            /* a better duplicate was found, remove existing beacon */
            remove_beacon(ctx, j);
        }
    } else {
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Unsupported beacon type");
        return set_error_status(sky_errno, SKY_ERROR_INTERNAL);
    }

    /* find initial position to insert based on type */
    for (j = 0; j < NUM_BEACONS(ctx); j++) {
        if (is_beacon_better(ctx, b, &ctx->beacon[j]) > 0)
            break;
    }

    /* add beacon at the end */
    if (j == NUM_BEACONS(ctx)) {
        ctx->beacon[j] = *b;
        NUM_BEACONS(ctx)++;
    } else {
        /* shift beacons to make room for the new one */
        memmove(&ctx->beacon[j + 1], &ctx->beacon[j], sizeof(Beacon_t) * (NUM_BEACONS(ctx) - j));
        ctx->beacon[j] = *b;
        NUM_BEACONS(ctx)++;
    }

    if (is_ap_type(b)) {
        NUM_APS(ctx)++;
    }

#ifdef SKY_DEBUG
    /* Verify that the beacon we just added now appears in our beacon set. */
    for (j = 0; j < NUM_BEACONS(ctx); j++) {
        bool equal;
        if (sky_plugin_equal(ctx, sky_errno, b, &ctx->beacon[j], NULL, &equal) == SKY_SUCCESS &&
            equal)
            break;
    }
    if (j < NUM_BEACONS(ctx))
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Beacon type %s inserted at idx %d", sky_pbeacon(b), j);
    else
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Beacon NOT found after insert");
#endif
    return SKY_SUCCESS;
}

/*! \brief add beacon to list in workspace context
 *
 *   if beacon is not AP and workspace is full (of non-AP), pick best one
 *   if beacon is AP,
 *    . reject a duplicate
 *    . for duplicates, keep newest and strongest
 *
 *   Insert new beacon in workspace
 *    . Add APs in order based on lowest to highest rssi value
 *    . Add cells after APs
 *
 *   If AP just added is known in cache,
 *    . set cached and copy Used property from cache
 *
 *   If AP just added fills workspace, remove one AP,
 *    . Remove one virtual AP if there is a match
 *    . If haven't removed one AP, remove one based on rssi distribution
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
Sky_status_t add_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b)
{
    int n;

    if (is_ap_type(b)) {
        if (!validate_mac(b->ap.mac, ctx))
            return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* connected flag for nmr beacons always false */
    if (is_cell_nmr(b))
        b->h.connected = false;

#if CACHE_SIZE
    /* Update the AP */
    if (is_ap_type(b)) {
        if (!beacon_in_cache(ctx, b, &b->ap.property)) {
            b->ap.property.in_cache = false;
            b->ap.property.used = false;
        }
    }
#endif

    /* insert the beacon */
    n = NUM_BEACONS(ctx);
    if (insert_beacon(ctx, sky_errno, b) == SKY_ERROR)
        return SKY_ERROR;
    if (n == NUM_BEACONS(ctx)) // no beacon added, must be duplicate because there was no error
        return SKY_SUCCESS;

    /* done if no filtering needed */
    if (NUM_APS(ctx) <= CONFIG(ctx->state, max_ap_beacons) &&
        (NUM_CELLS(ctx) <=
            (CONFIG(ctx->state, total_beacons) - CONFIG(ctx->state, max_ap_beacons)))) {
        return SKY_SUCCESS;
    }

    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates or remove one based on rssi distribution */
    if (sky_plugin_remove_worst(ctx, sky_errno) == SKY_ERROR) {
        if (NUM_BEACONS(ctx) > CONFIG(ctx->state, total_beacons))
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Unexpected failure removing worst beacon");
        return set_error_status(sky_errno, SKY_ERROR_INTERNAL);
    }

    return SKY_SUCCESS;
}

#if CACHE_SIZE
/*! \brief check if a beacon is in cache
 *
 *   Scan all cachelines in the cache. 
 *   If the given beacon is found in the cache true is returned otherwise
 *   false. A beacon may appear in multiple cache lines.
 *   If prop is not NULL, algorithm searches all caches for best match
 *   (beacon with Used == true is best) otherwise, first match is returned
 *
 *  @param ctx Skyhook request context
 *  @param b pointer to new beacon
 *  @param cl pointer to cacheline
 *  @param prop pointer to where the properties of matching beacon is saved
 *
 *  @return true if beacon successfully found or false
 */
bool beacon_in_cache(Sky_ctx_t *ctx, Beacon_t *b, Sky_beacon_property_t *prop)
{
    Sky_beacon_property_t best_prop = { false, false };
    Sky_beacon_property_t result = { false, false };

    if (!b || !ctx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return false;
    }

    for (int i = 0; i < CACHE_SIZE; i++) {
        if (beacon_in_cacheline(ctx, b, &ctx->state->cacheline[i], &result)) {
            if (!prop)
                return true; /* don't need to keep looking for used if prop is NULL */
            best_prop.in_cache = true;

            if (result.used) {
                best_prop.used = true;
                break; /* beacon is in cached and used, can stop search */
            }
        }
    }
    if (best_prop.in_cache) {
        *prop = best_prop;
        return true;
    }

    return false;
}

/*! \brief check if a beacon is in a cacheline
 *
 *   Scan all beacons in the cacheline. If the type matches the given beacon, compare
 *   the appropriate attributes. If the given beacon is found in the cacheline
 *   true is returned otherwise false. If index is not NULL, the index of the matching
 *   beacon in the cacheline is saved or -1 if beacon was not found.
 *
 *  @param ctx Skyhook request context
 *  @param b pointer to new beacon
 *  @param cl pointer to cacheline
 *  @param index pointer to where the index of matching beacon is saved
 *
 *  @return true if beacon successfully found or false
 */
bool beacon_in_cacheline(
    Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl, Sky_beacon_property_t *prop)
{
    int j;

    if (!cl || !b || !ctx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return false;
    }

    if (cl->time == CACHE_EMPTY) {
        return false;
    }

    for (j = 0; j < NUM_BEACONS(cl); j++) {
        bool equal = false;

        if (sky_plugin_equal(ctx, NULL, b, &cl->beacon[j], prop, &equal) == SKY_SUCCESS && equal)
            return true;
    }
    return false;
}

/*! \brief find cache entry with oldest entry
 *
 *  @param ctx Skyhook request context
 *
 *  @return index of oldest cache entry, or empty
 */
int find_oldest(Sky_ctx_t *ctx)
{
    int i;
    int oldestc = 0;
    time_t oldest = ctx->header.time;

    for (i = 0; i < CACHE_SIZE; i++) {
        /* if there is only one cache line or
         * if time is unavailable or
         * cacheline is empty,
         * then return index of current cache line 
         */
        if (CACHE_SIZE == 1 || oldest == TIME_UNAVAILABLE ||
            ctx->state->cacheline[i].time == CACHE_EMPTY)
            return i;
        else if (ctx->state->cacheline[i].time < oldest) {
            oldest = ctx->state->cacheline[i].time;
            oldestc = i;
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cacheline %d oldest time %d", oldestc, oldest);
    return oldestc;
}
#endif

/*! \brief test serving cell in request context has changed from that in cache
 *
 *  Cells are in priority order
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in
 *
 *  @return true or false
 *
 *  false if either request context or cache has no cells
 *  false if highest priority cell (which is
 *  assumed to be the serving cell, regardless of whether or
 *  not the user has marked it "connected") matches cache
 *  true otherwise
 */
int serving_cell_changed(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    Beacon_t *w, *c;
    bool equal = false;

    if (!ctx || !cl) {
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
#endif
        return true;
    }

    if (NUM_CELLS(ctx) == 0) {
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "0 cells in workspace");
#endif
        return false;
    }

    if (NUM_CELLS(cl) == 0) {
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "0 cells in cache");
#endif
        return false;
    }

    w = &ctx->beacon[NUM_APS(ctx)];
    c = &cl->beacon[NUM_APS(cl)];
    if (is_cell_nmr(w) || is_cell_nmr(c)) {
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "no significant cell in cache or workspace");
#endif
        return false;
    }

    if (sky_plugin_equal(ctx, NULL, w, c, NULL, &equal) == SKY_SUCCESS && equal)
        return false;
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell mismatch");
    return true;
}

/*! \brief get location from cache
 *
 *  @param ctx Skyhook request context
 *
 *  @return cacheline index or -1
 */
int get_from_cache(Sky_ctx_t *ctx)
{
#if CACHE_SIZE == 0
    /* no match to cacheline */
    return (ctx->get_from = -1);
#else
    int idx;

    /* Avoid using the cache if we have good reason */
    /* to believe that system time is bad */
    if (ctx->header.time <= TIMESTAMP_2019_03_01) {
        /* no match to cacheline */
        return (ctx->get_from = -1);
    }
    return (ctx->get_from =
                (int16_t)(sky_plugin_get_matching_cacheline(ctx, NULL, &idx) == SKY_SUCCESS) ? idx :
                                                                                               -1);
#endif
}

/*! \brief check if an AP beacon is in a virtual group
 *
 *  Both the b (in workspace) and vg in cache may be virtual groups
 *  if the two macs are similar and difference is same nibble as child, then
 *  if any of the children have matching macs, then match
 *
 *  @param ctx Skyhook request context
 *  @param b pointer to new beacon
 *  @param vg pointer to beacon in cacheline
 *
 *  @return 0 if no matches otherwise return number of matching APs
 */
int ap_beacon_in_vg(Sky_ctx_t *ctx, Beacon_t *va, Beacon_t *vb, Sky_beacon_property_t *prop)
{
    int w, c, num_aps = 0;
    uint8_t mac_va[MAC_SIZE] = { 0 };
    uint8_t mac_vb[MAC_SIZE] = { 0 };
    Sky_beacon_property_t p;

    if (!ctx || !va || !vb || va->h.type != SKY_BEACON_AP || vb->h.type != SKY_BEACON_AP) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return false;
    }
#if VERBOSE_DEBUG
    dump_beacon(ctx, "A: ", va, __FILE__, __FUNCTION__);
    dump_beacon(ctx, "B: ", vb, __FILE__, __FUNCTION__);
#endif

    /* Compare every member of any virtual group with every other */
    /* index -1 is used to reference the parent mac */
    for (w = -1; w < NUM_VAPS(va); w++) {
        for (c = -1; c < NUM_VAPS(vb); c++) {
            int value, idx;

            if (w == -1)
                memcpy(mac_va, va->ap.mac, MAC_SIZE);
            else {
                idx = va->ap.vg[VAP_FIRST_DATA + w].data.nibble_idx;
                value = va->ap.vg[VAP_FIRST_DATA + w].data.value << (4 * ((~idx) & 1));
                mac_va[va->ap.vg[VAP_FIRST_DATA + w].data.nibble_idx / 2] =
                    (mac_va[va->ap.vg[VAP_FIRST_DATA + w].data.nibble_idx / 2] &
                        ~NIBBLE_MASK(idx)) |
                    value;
            }
            if (c == -1)
                memcpy(mac_vb, vb->ap.mac, MAC_SIZE);
            else {
                idx = vb->ap.vg[VAP_FIRST_DATA + c].data.nibble_idx;
                value = vb->ap.vg[VAP_FIRST_DATA + c].data.value << (4 * ((~idx) & 1));
                mac_vb[vb->ap.vg[VAP_FIRST_DATA + c].data.nibble_idx / 2] =
                    (mac_vb[vb->ap.vg[VAP_FIRST_DATA + c].data.nibble_idx / 2] &
                        ~NIBBLE_MASK(idx)) |
                    value;
            }
            if (memcmp(mac_va, mac_vb, MAC_SIZE) == 0) {
                num_aps++;
                p = (c == -1) ? vb->ap.property : vb->ap.vg_prop[c];
                if (prop)
                    *prop = p;
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "cmp MAC %02X:%02X:%02X:%02X:%02X:%02X %s with "
                    "%02X:%02X:%02X:%02X:%02X:%02X %s, match %d %s",
                    mac_va[0], mac_va[1], mac_va[2], mac_va[3], mac_va[4], mac_va[5],
                    w == -1 ? "AP " : "VAP", /* Parent or child */
                    mac_vb[0], mac_vb[1], mac_vb[2], mac_vb[3], mac_vb[4], mac_vb[5],
                    c == -1 ? "AP " : "VAP", /* Parent or child */
                    num_aps, p.used ? "Used" : "Unused");
#endif
            } else {
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "cmp MAC %02X:%02X:%02X:%02X:%02X:%02X %s with "
                    "%02X:%02X:%02X:%02X:%02X:%02X %s",
                    mac_va[0], mac_va[1], mac_va[2], mac_va[3], mac_va[4], mac_va[5],
                    w == -1 ? "AP " : "VAP", /* Parent or child */
                    mac_vb[0], mac_vb[1], mac_vb[2], mac_vb[3], mac_vb[4], mac_vb[5],
                    c == -1 ? "AP " : "VAP"); /* Parent or child */
#endif
            }
        }
    }
    return num_aps;
}

#ifdef UNITTESTS

#include "beacons.ut.c"

#endif
