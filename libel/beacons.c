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
#include <time.h>
#include <math.h>
#include <stdio.h>
#define SKY_LIBEL
#include "libel.h"

/* Uncomment VERBOSE_DEBUG to enable extra logging */
// #define VERBOSE_DEBUG 1

#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define EFFECTIVE_RSSI(b) ((b) == -1 ? (-127) : (b))
#define PUT_IN_CACHE true
#define GET_FROM_CACHE false

static bool beacon_compare(Sky_ctx_t *ctx, Beacon_t *new, Beacon_t *wb, int *diff);
static bool beacon_in_cache(
    Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl, Sky_beacon_property_t *prop);

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

    if (ctx->beacon[index].h.type == SKY_BEACON_AP)
        NUM_APS(ctx) -= 1;
    if (ctx->connected == index)
        ctx->connected = -1;
    else if (index < ctx->connected)
        // Removed beacon precedes the connected one, so update its index.
        ctx->connected--;

    memmove(&ctx->beacon[index], &ctx->beacon[index + 1],
        sizeof(Beacon_t) * (NUM_BEACONS(ctx) - index - 1));
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "idx:%d", index);
    NUM_BEACONS(ctx) -= 1;
#if VERBOSE_DEBUG
    DUMP_WORKSPACE(ctx);
#endif
    return SKY_SUCCESS;
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
Sky_status_t insert_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b, int *index)
{
    int i, diff = 0;

    /* sanity checks */
    if (!validate_workspace(ctx) || b->h.magic != BEACON_MAGIC || b->h.type >= SKY_BEACON_MAX) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Invalid params. Beacon type %s", sky_pbeacon(b));
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* check for duplicate */
    for (i = 0; i < NUM_BEACONS(ctx); i++) {
        if (beacon_compare(ctx, b, &ctx->beacon[i], NULL) == true) { // duplicate?
            if (ctx->beacon[i].ap.vg_len || ctx->beacon[i].h.connected ||
                b->h.age > ctx->beacon[i].h.age ||
                (b->h.age == ctx->beacon[i].h.age &&
                    EFFECTIVE_RSSI(b->h.rssi) <= EFFECTIVE_RSSI(ctx->beacon[i].h.rssi))) {
                LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Reject duplicate beacon");
                return sky_return(sky_errno, SKY_ERROR_NONE);
            }
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Keep new duplicate beacon %s",
                (b->h.age == ctx->beacon[i].h.age) ? "(stronger signal)" : "(younger)");
            remove_beacon(ctx, i);
        }
    }
    /* find correct position to insert based on type */
    for (i = 0; i < NUM_BEACONS(ctx); i++) {
        if (beacon_compare(ctx, b, &ctx->beacon[i], &diff) == false)
            if (diff > 0) // stop if the new beacon is better
                break;
    }

    if (b->h.connected) {
        /* if new beacon is connected, update info about any previously connected */
        if (ctx->connected >= 0)
            ctx->beacon[ctx->connected].h.connected = false;
        ctx->connected = i;
    }

    /* add beacon at the end */
    if (i == NUM_BEACONS(ctx)) {
        ctx->beacon[i] = *b;
        NUM_BEACONS(ctx)++;
    } else {
        /* shift beacons to make room for the new one */
        memmove(&ctx->beacon[i + 1], &ctx->beacon[i], sizeof(Beacon_t) * (NUM_BEACONS(ctx) - i));
        ctx->beacon[i] = *b;
        NUM_BEACONS(ctx)++;
    }
    /* report back the position beacon was added */
    if (index != NULL)
        *index = i;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Beacon type %s inserted idx: %d", sky_pbeacon(b), i);

    if (!b->h.connected && i <= ctx->connected)
        // New beacon was inserted before the connected one, and not connected so update its index.
        ctx->connected++;

    if (b->h.type == SKY_BEACON_AP)
        NUM_APS(ctx)++;
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
    int n, i = -1;
    Beacon_t *w, tmp;

    if (b->h.type == SKY_BEACON_AP) {
        if (!validate_mac(b->ap.mac, ctx))
            return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* if workspace has a connected cell already, re-sort (not connected) it before adding a new connected cell */
    if (ctx->connected != -1 && is_cell_type(&ctx->beacon[ctx->connected]) && b->cell.h.connected) {
        tmp = ctx->beacon[ctx->connected];
        tmp.h.connected = false;
        remove_beacon(ctx, ctx->connected);
        insert_beacon(ctx, sky_errno, &tmp, NULL);
        ctx->connected = -1;
    }

    /* insert the beacon */
    n = ctx->len;
    if (insert_beacon(ctx, sky_errno, b, &i) == SKY_ERROR)
        return SKY_ERROR;
    if (n == ctx->len) // no beacon added, must be duplicate because there was no error
        return SKY_SUCCESS;

    /* Update the AP just added to workspace */
    w = &ctx->beacon[i];
    if (b->h.type == SKY_BEACON_AP) {
        if (!beacon_in_cache(ctx, b, &ctx->cache->cacheline[ctx->cache->newest], &w->ap.property)) {
            w->ap.property.in_cache = false;
            w->ap.property.used = false;
        }
    } else {
        if (sky_plugin_call(ctx, sky_errno, SKY_OP_REMOVE_WORST, NULL) == SKY_ERROR) {
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "failed to filter cell");
            return sky_return(sky_errno, SKY_ERROR_INTERNAL);
        }
        return SKY_SUCCESS;
    }

#if VERBOSE_DEBUG
    dump_beacon(ctx, "new AP: ", w, __FILE__, __FUNCTION__);
#endif
    /* done if no filtering needed */
    if (NUM_APS(ctx) <= CONFIG(ctx->cache, max_ap_beacons))
        return SKY_SUCCESS;

    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates of remove one based on rssi distribution */
    if (sky_plugin_call(ctx, sky_errno, SKY_OP_REMOVE_WORST, NULL) == SKY_ERROR) {
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }
    DUMP_WORKSPACE(ctx);

    return SKY_SUCCESS;
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
static bool beacon_in_cache(
    Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl, Sky_beacon_property_t *prop)
{
    int j;

    if (!cl || !b || !ctx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return false;
    }

    if (cl->time == 0) {
        return false;
    }

    for (j = 0; j < NUM_BEACONS(cl); j++)
        if (sky_plugin_call(ctx, NULL, SKY_OP_EQUAL, b, &cl->beacon[j], prop) == 1)
            return true;
    return false;
}

/*! \brief compare a beacon to one in workspace
 *
 *  if beacon is duplicate, return true
 *
 *  if both are AP, return false and set diff to difference in rssi
 *  if both are cell and same cell type, return false and set diff as below
 *   serving cell
 *   youngest
 *   in cache
 *   strongest
 *
 *  @param ctx Skyhook request context
 *  @param bA pointer to beacon A
 *  @param wb pointer to beacon B
 *
 *  @return true if match or false and set diff
 *   diff is 0 if beacons can't be compared
 *           +ve if beacon A is better
 *           -ve if beacon B is better
 */
static bool beacon_compare(Sky_ctx_t *ctx, Beacon_t *new, Beacon_t *wb, int *diff)
{
    int ret = false;
    int better = 1; // beacon B is better (-ve), or A is better (+ve)

    if (!ctx || !new || !wb) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        if (diff)
            *diff = 0; /* can't compare */
        return false;
    }

    /* if beacons can't be compared, simply order by type */
    if ((ret = sky_plugin_call(ctx, NULL, SKY_OP_EQUAL, new, wb, NULL)) == SKY_ERROR) {
        /* types increase in value as they become lower priority */
        /* so we have to invert the sign of the comparison value */
        /* if the types are different, there is no match */
        better = -(new->h.type - wb->h.type);
        if (diff)
            *diff = better;
#if VERBOSE_DEBUG
        dump_beacon(ctx, "A: ", new, __FILE__, __FUNCTION__);
        dump_beacon(ctx, "B: ", wb, __FILE__, __FUNCTION__);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Different types %d (%s)", better,
            better < 0 ? "B is better" : "A is better");
#endif
        return false;
    }
    if (ret == SKY_SUCCESS) // if beacons are equivalent, return true
        ret = true;

    /* if the beacons can be compared and are not equivalent, determine which is better */
    if (ret == false) {
        if (new->h.type == SKY_BEACON_AP) {
            /* Compare APs by rssi */
            better = EFFECTIVE_RSSI(new->h.rssi) - EFFECTIVE_RSSI(wb->h.rssi);
#if VERBOSE_DEBUG
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "WiFi rssi score %d (%s)", better,
                better < 0 ? "B is better" : "A is better");
#endif
        } else {
#if VERBOSE_DEBUG
            dump_beacon(ctx, "A: ", new, __FILE__, __FUNCTION__);
            dump_beacon(ctx, "B: ", wb, __FILE__, __FUNCTION__);
#endif
            /* cell comparison is type, connected, or youngest, or type or stongest */
            if (new->h.connected || wb->h.connected) {
                better = (new->h.connected ? 1 : -1);
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell connected score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (new->h.age != wb->h.age) {
                /* youngest is best */
                better = -(new->h.age - wb->h.age);
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell age score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (new->h.type != wb->h.type) {
                better = -(new->h.type - wb->h.type);
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell type score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (EFFECTIVE_RSSI(new->h.rssi) != EFFECTIVE_RSSI(wb->h.rssi)) {
                better = (EFFECTIVE_RSSI(new->h.rssi) - EFFECTIVE_RSSI(wb->h.rssi));
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell signal strength score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else {
                better = 1;
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell similar, pick one (%s)",
                    better < 0 ? "B is better" : "A is better");
#endif
            }
        }
    }

    if (!ret && diff)
        *diff = better;

#if VERBOSE_DEBUG
    if (ret)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Beacons match");
#endif
    return ret;
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
    uint32_t oldestc = 0;
    int oldest = (*ctx->gettime)(NULL);

    for (i = 0; i < CACHE_SIZE; i++) {
        if (ctx->cache->cacheline[i].time == 0)
            return i;
        else if (ctx->cache->cacheline[i].time < oldest) {
            oldest = ctx->cache->cacheline[i].time;
            oldestc = i;
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cacheline %d oldest time %d", oldestc, oldest);
    return oldestc;
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
Sky_status_t add_to_cache(Sky_ctx_t *ctx, Sky_location_t *loc)
{
    int i = ctx->save_to;
    int j, v;
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

    /* Find best match in cache */
    /*    yes - add entry here */
    /* else find oldest cache entry */
    /*    yes - add entry here */
    if (i < 0) {
        i = find_oldest(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "find_oldest chose cache %d of 0..%d", i, CACHE_SIZE - 1);
    }
    cl = &ctx->cache->cacheline[i];
    if (loc->location_status != SKY_LOCATION_STATUS_SUCCESS) {
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Won't add unknown location to cache");
        cl->time = 0; /* clear cacheline */
        update_newest_cacheline(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "clearing cache %d of 0..%d", i, CACHE_SIZE - 1);
        return SKY_ERROR;
    } else if (cl->time == 0)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Saving to empty cache %d of 0..%d", i, CACHE_SIZE - 1);
    else
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Saving to cache %d of 0..%d", i, CACHE_SIZE - 1);

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
            for (v = 0; v < NUM_VAPS(&cl->beacon[j]); v++)
                cl->beacon[j].ap.vg_prop[v].in_cache = true;
        }
    }
    DUMP_CACHE(ctx);
    return SKY_SUCCESS;
}

/*! \brief get location from cache
 *
 *  @param ctx Skyhook request context
 *
 *  @return cacheline index or -1
 */
int get_from_cache(Sky_ctx_t *ctx)
{
    uint32_t now = (*ctx->gettime)(NULL);

    if (CACHE_SIZE < 1) {
        return SKY_ERROR;
    }

    /* compare current time to Mar 1st 2019 */
    if (now <= TIMESTAMP_2019_03_01) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Don't have good time of day!");
        return SKY_ERROR;
    }
    return sky_plugin_call(ctx, NULL, SKY_OP_SCORE_CACHELINE);
}
