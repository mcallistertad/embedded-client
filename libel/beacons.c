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
#ifdef VERBOSE_DEBUG
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
    int j, diff = 0;

    /* sanity checks */
    if (!validate_workspace(ctx) || b->h.magic != BEACON_MAGIC || b->h.type >= SKY_BEACON_MAX) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Invalid params. Beacon type %s", sky_pbeacon(b));
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* check for duplicate */
    if (!is_cell_type(b)) { /* If new beacon is AP or BLE */
        for (j = 0; j < NUM_APS(ctx); j++) {
            if (sky_plugin_call(ctx, NULL, SKY_OP_EQUAL, b, &ctx->beacon[j], NULL) == SKY_SUCCESS) {
                /* reject new beacon if already have serving AP, or it is older or weaker */
                if (ctx->beacon[j].h.connected) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate AP (not serving)");
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                } else if (b->h.connected && ctx->beacon[j].ap.vg_len) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Keep new duplicate AP (serving)");
                    ctx->beacon[j].h.connected = b->h.connected;
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                } else if (b->h.connected) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Keep new duplicate AP (serving)");
                    break; /* fall through to remove exiting duplicate */
                } else if (b->h.age > ctx->beacon[j].h.age) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate AP (older)");
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                } else if (b->h.age < ctx->beacon[j].h.age) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Keep new duplicate AP (younger)");
                    break; /* fall through to remove exiting duplicate */
                } else if (EFFECTIVE_RSSI(b->h.rssi) <= EFFECTIVE_RSSI(ctx->beacon[j].h.rssi)) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate AP (weaker)");
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                } else {
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Keep new duplicate AP (stronger signal)");
                    break; /* fall through to remove exiting duplicate */
                }
            }
        }
        /* if a better duplicate was found, remove existing worse beacon */
        if (j < NUM_APS(ctx))
            remove_beacon(ctx, j);
    } else { /* If new beacon is one of the cell types */
        for (j = NUM_APS(ctx); j < NUM_BEACONS(ctx); j++) {
            if (sky_plugin_call(ctx, NULL, SKY_OP_EQUAL, b, &ctx->beacon[j], NULL) == SKY_SUCCESS) {
                /* reject new beacon if already have serving cell, or it is older or weaker */
                if (ctx->beacon[j].h.connected) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate cell (not serving)");
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                } else if (b->h.connected) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Keep new duplicate cell (serving)");
                    break; /* fall through to remove exiting duplicate */
                } else if (get_cell_age(b) > get_cell_age(&ctx->beacon[j])) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate cell (older)");
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                } else if (get_cell_age(b) < get_cell_age(&ctx->beacon[j])) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Keep new duplicate cell (younger)");
                    break; /* fall through to remove exiting duplicate */
                } else if (EFFECTIVE_RSSI(get_cell_rssi(b)) <=
                           EFFECTIVE_RSSI(get_cell_rssi(&ctx->beacon[j]))) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate cell (weaker)");
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                } else {
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Keep new duplicate cell (stronger signal)");
                    break; /* fall through to remove exiting duplicate */
                }
            }
        }
        /* if a better duplicate was found, remove existing worse beacon */
        if (j < NUM_BEACONS(ctx))
            remove_beacon(ctx, j);
    }

    /* find correct position to insert based on type */
    for (j = 0; j < NUM_BEACONS(ctx); j++) {
        if (beacon_compare(ctx, b, &ctx->beacon[j], &diff) == false)
            if (diff > 0) // stop if the new beacon is better
                break;
    }

    if (b->h.connected) {
        /* if new beacon is connected, update info about any previously connected */
        if (ctx->connected >= 0)
            ctx->beacon[ctx->connected].h.connected = false;
        ctx->connected = j;
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
    /* report back the position beacon was added */
    if (index != NULL)
        *index = j;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Beacon type %s inserted idx: %d", sky_pbeacon(b), j);

    if (!b->h.connected && j <= ctx->connected)
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

    /* if workspace has a connected beacon already, re-sort (not connected) it before adding a new connected beacon */
    if (ctx->connected != -1 && b->h.connected) {
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
        DUMP_WORKSPACE(ctx);
        if (sky_plugin_call(ctx, sky_errno, SKY_OP_REMOVE_WORST, NULL) == SKY_ERROR) {
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "failed to filter cell");
            return sky_return(sky_errno, SKY_ERROR_INTERNAL);
        }
        return SKY_SUCCESS;
    }

#ifdef VERBOSE_DEBUG
    dump_beacon(ctx, "new AP: ", w, __FILE__, __FUNCTION__);
#endif
    /* done if no filtering needed */
    if (NUM_APS(ctx) <= CONFIG(ctx->cache, max_ap_beacons))
        return SKY_SUCCESS;

    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates of remove one based on rssi distribution */
    DUMP_WORKSPACE(ctx);
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
bool beacon_in_cache(Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl, Sky_beacon_property_t *prop)
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
    Sky_status_t equality = SKY_ERROR;
    bool ret = false;
    int better = 1; // beacon B is better (-ve), or A is better (+ve)

    if (!ctx || !new || !wb) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        if (diff)
            *diff = 0; /* can't compare */
        return false;
    }

    /* if beacons can't be compared for equality, order like this */
    if ((equality = sky_plugin_call(ctx, NULL, SKY_OP_EQUAL, new, wb, NULL)) == SKY_ERROR) {
        /* types increase in value as they become lower priority */
        /* so we have to invert the sign of the comparison value */
        better = -(new->h.type - wb->h.type);
#ifdef VERBOSE_DEBUG
        dump_beacon(ctx, "A: ", new, __FILE__, __FUNCTION__);
        dump_beacon(ctx, "B: ", wb, __FILE__, __FUNCTION__);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Different types %d (%s)", better,
            better < 0 ? "B is better" : "A is better");
#endif
    } else if (equality == SKY_SUCCESS) // if beacons are equivalent, return true
        ret = true;
    else {
        /* if the beacons can be compared and are not equivalent, determine which is better */
        if (new->h.type == SKY_BEACON_AP || new->h.type == SKY_BEACON_BLE) {
            if (EFFECTIVE_RSSI(new->h.rssi) != EFFECTIVE_RSSI(wb->h.rssi)) {
                /* Compare APs by rssi */
                better = EFFECTIVE_RSSI(new->h.rssi) - EFFECTIVE_RSSI(wb->h.rssi);
#ifdef VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "WiFi rssi score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else
                /* vg with most members is better */
                better = new->ap.vg_len - wb->ap.vg_len;
        } else {
#ifdef VERBOSE_DEBUG
            dump_beacon(ctx, "A: ", new, __FILE__, __FUNCTION__);
            dump_beacon(ctx, "B: ", wb, __FILE__, __FUNCTION__);
#endif
            /* cell comparison is type, connected, or youngest, or type or stongest */
            if (new->h.connected || wb->h.connected) {
                better = (new->h.connected ? 1 : -1);
#ifdef VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell connected score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (is_cell_nmr(new) != is_cell_nmr(wb)) {
                /* fully qualified is best */
                better = (!is_cell_nmr(new) ? 1 : -1);
#ifdef VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell nmr score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (new->h.age != wb->h.age) {
                /* youngest is best */
                better = -(new->h.age - wb->h.age);
#ifdef VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell age score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (new->h.type != wb->h.type) {
                /* by type order */
                better = -(new->h.type - wb->h.type);
#ifdef VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell type score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (EFFECTIVE_RSSI(new->h.rssi) != EFFECTIVE_RSSI(wb->h.rssi)) {
                /* highest signal strength is best */
                better = EFFECTIVE_RSSI(new->h.rssi) - EFFECTIVE_RSSI(wb->h.rssi);
#ifdef VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell signal strength score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else {
                better = 1;
#ifdef VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell similar, pick one (%s)",
                    better < 0 ? "B is better" : "A is better");
#endif
            }
        }
    }

    if (!ret && diff)
        *diff = better;

#ifdef VERBOSE_DEBUG
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
            for (v = 0; v < NUM_VAPS(&cl->beacon[j]); v++)
                cl->beacon[j].ap.vg_prop[v].in_cache = true;
        }
    }
    DUMP_CACHE(ctx);
    return SKY_SUCCESS;
}

/*! \brief test cell in workspace has changed from that in cache
 *
 *  false if either workspace or cache has no cells
 *  false if serving cell matches cache
 *  true otherwise
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in
 *
 *  @return true or false
 */
int cell_changed(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int j;
    if (!ctx || !cl) {
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
#endif
        return true;
    }

    if ((NUM_BEACONS(ctx) - NUM_APS(ctx)) == 0 || (NUM_BEACONS(cl) - NUM_APS(cl)) == 0) {
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "0 cells in cache or workspace");
#endif
        return false;
    }

    if (ctx->connected == -1) {
#ifdef VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "no serving cell in workspace");
#endif
        return false;
    }

    /* for each cell in workspace, compare with cacheline */
    for (j = NUM_APS(ctx); j < NUM_BEACONS(ctx); j++) {
        if (ctx->beacon[j].h.connected && beacon_in_cache(ctx, &ctx->beacon[j], cl, NULL)) {
#ifdef VERBOSE_DEBUG
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d - serving cells match",
                cl - ctx->cache->cacheline);
#endif
            return false;
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d - cell mismatch", cl - ctx->cache->cacheline);
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
    uint32_t now = (*ctx->gettime)(NULL);
    int idx;

    if (CACHE_SIZE < 1) {
        return SKY_ERROR;
    }

    /* compare current time to Mar 1st 2019 */
    if (now <= TIMESTAMP_2019_03_01) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Don't have good time of day!");
        return SKY_ERROR;
    }
    return sky_plugin_call(ctx, NULL, SKY_OP_CACHE_MATCH, &idx) == SKY_SUCCESS ? idx : -1;
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
