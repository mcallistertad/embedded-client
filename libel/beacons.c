/*! \file libel/beacons.c
 *  \brief utilities - Skyhook Embedded Library
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
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>
#define SKY_LIBEL 1
#include "libel.h"

#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define NOMINAL_RSSI(b) ((b) == -1 ? (-90) : (b))
#define PUT_IN_CACHE true
#define GET_FROM_CACHE false

void dump_workspace(Sky_ctx_t *ctx);
void dump_cache(Sky_ctx_t *ctx);
static bool beacon_in_cache(Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl);

/*! \brief test two MAC addresses for being virtual aps
 *
 *  @param macA pointer to the first MAC
 *  @param macB pointer to the second MAC
 *
 *  @return -1, 0 or 1
 *  return 0 when NOT similar, -1 indicates keep A, 1 keep B
 */
static int similar(uint8_t macA[], uint8_t macB[])
{
    /* Return 1 (true) if OUIs are identical and no more than 1 hex digits
     * differ between the two MACs. Else return 0 (false).
     */
    size_t num_diff = 0; // Num hex digits which differ
    size_t i;

    if (memcmp(macA, macB, 3) != 0)
        return 0;

    for (i = 3; i < MAC_SIZE; i++) {
        if (((macA[i] & 0xF0) != (macB[i] & 0xF0) && ++num_diff > 1) ||
            ((macA[i] & 0x0F) != (macB[i] & 0x0F) && ++num_diff > 1))
            return 0;
    }

    /* MACs are similar, choose one to remove */
    return (memcmp(macA + 3, macB + 3, MAC_SIZE - 3) < 0 ? -1 : 1);
}

/*! \brief shuffle list to remove the beacon at index
 *
 *  @param ctx Skyhook request context
 *  @param index 0 based index of AP to remove
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
static Sky_status_t remove_beacon(Sky_ctx_t *ctx, int index)
{
    if (index >= ctx->len)
        return SKY_ERROR;

    if (ctx->beacon[index].h.type == SKY_BEACON_AP)
        ctx->ap_len -= 1;
    if (ctx->connected == index)
        ctx->connected = -1;
    else if (index < ctx->connected)
        // Removed beacon precedes the connected one, so update its index.
        ctx->connected--;

    memmove(
        &ctx->beacon[index], &ctx->beacon[index + 1], sizeof(Beacon_t) * (ctx->len - index - 1));
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "idx:%d", index)
    ctx->len -= 1;
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
static Sky_status_t insert_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b, int *index)
{
    int i;

    /* sanity checks */
    if (!validate_workspace(ctx) || b->h.magic != BEACON_MAGIC || b->h.type >= SKY_BEACON_MAX) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Invalid params. Beacon type %s", sky_pbeacon(b))
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* find correct position to insert based on type */
    for (i = 0; i < ctx->len; i++)
        if (ctx->beacon[i].h.type >= b->h.type)
            break;

    /* add beacon at the end */
    if (i == ctx->len) {
        ctx->beacon[i] = *b;
        ctx->len++;
    } else {
        /* if AP, add in rssi order */
        if (b->h.type == SKY_BEACON_AP) {
            for (; i < ctx->ap_len; i++)
                if (ctx->beacon[i].h.type != SKY_BEACON_AP ||
                    NOMINAL_RSSI(ctx->beacon[i].ap.rssi) > NOMINAL_RSSI(b->ap.rssi))
                    break;
        } else if (is_cell_type(b)) {
            /* if Cell, add in NMR, age, strength order */
            for (; i < ctx->len; i++)
                if (ctx->beacon[i].h.type < b->h.type ||
                    (is_cell_nmr(&ctx->beacon[i]) && !is_cell_nmr(b)) ||
                    get_cell_age(&ctx->beacon[i]) < get_cell_age(b) ||
                    NOMINAL_RSSI(get_cell_type(&ctx->beacon[i])) < NOMINAL_RSSI(get_cell_rssi(b)))
                    break;
        }
        /* shift beacons to make room for the new one */
        memmove(&ctx->beacon[i + 1], &ctx->beacon[i], sizeof(Beacon_t) * (ctx->len - i));
        ctx->beacon[i] = *b;
        ctx->len++;
    }
    /* report back the position beacon was added */
    if (index != NULL)
        *index = i;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Beacon type %s inserted idx: %d", sky_pbeacon(b), i);

    if (i <= ctx->connected)
        // New beacon was inserted before the connected one, so update its index.
        ctx->connected++;

    if (b->h.type == SKY_BEACON_AP)
        ctx->ap_len++;
    return SKY_SUCCESS;
}

/*! \brief try to reduce AP by filtering out based on diversity of rssi
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static Sky_status_t filter_aps_by_rssi(Sky_ctx_t *ctx)
{
    int i, reject, jump, up_down;
    float band_range, worst;
    float ideal_rssi[MAX_AP_BEACONS + 1];

    if (ctx->ap_len <= CONFIG(ctx->cache, max_ap_beacons))
        return SKY_ERROR;

    /* what share of the range of rssi values does each beacon represent */
    band_range = (NOMINAL_RSSI(ctx->beacon[ctx->ap_len - 1].ap.rssi) -
                     NOMINAL_RSSI(ctx->beacon[0].ap.rssi)) /
                 ((float)ctx->ap_len - 1);

    /* if the rssi range is small, throw away middle beacon */

    if (band_range < 0.5) {
        /* search from middle of range looking for beacon not in cache */
        for (jump = 0, up_down = -1, i = ctx->ap_len / 2; i >= 0 && i < ctx->ap_len;
             jump++, i += up_down * jump, up_down = -up_down) {
            if (!ctx->beacon[i].ap.in_cache) {
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Warning: rssi range is small. %s beacon",
                    !jump ? "Remove middle" : "Found non-cached")
                return remove_beacon(ctx, i);
            }
        }
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Warning: rssi range is small. Removing cached beacon")
        return remove_beacon(ctx, ctx->ap_len / 2);
    }

    /* if beacon with min RSSI is below threshold, throw it out */
    if (NOMINAL_RSSI(ctx->beacon[0].ap.rssi) < -CONFIG(ctx->cache, cache_neg_rssi_threshold)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Discarding beacon %d with very weak strength", 0)
        return remove_beacon(ctx, 0);
    }

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "range: %d band range: %d.%02d",
        (NOMINAL_RSSI(ctx->beacon[ctx->ap_len - 1].ap.rssi) - NOMINAL_RSSI(ctx->beacon[0].ap.rssi)),
        (int)band_range, (int)fabs(round(100 * (band_range - (int)band_range))))

    /* for each beacon, work out it's ideal rssi value to give an even distribution */
    for (i = 0; i < ctx->ap_len; i++)
        ideal_rssi[i] = NOMINAL_RSSI(ctx->beacon[0].ap.rssi) + (i * band_range);

    /* find AP with poorest fit to ideal rssi */
    /* always keep lowest and highest rssi */
    /* unless all the middle candidates are in the cache */
    for (i = 1, reject = -1, worst = 0; i < ctx->ap_len - 1; i++) {
        if (!ctx->beacon[i].ap.in_cache &&
            fabs(NOMINAL_RSSI(ctx->beacon[i].ap.rssi) - ideal_rssi[i]) > worst) {
            worst = fabs(NOMINAL_RSSI(ctx->beacon[i].ap.rssi) - ideal_rssi[i]);
            reject = i;
        }
    }
    if (reject == -1) {
        /* haven't found a beacon to remove yet due to matching cached beacons */
        /* Throw away either lowest or highest rssi valued beacons */
        if (!ctx->beacon[ctx->ap_len - 1].ap.in_cache && ctx->beacon[0].ap.in_cache)
            reject = ctx->ap_len - 1;
        else
            reject = 0; /* Throw away lowest rssi value */
    }
#if SKY_DEBUG
    for (i = 0; i < ctx->ap_len; i++) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s: %-2d, %s ideal %d.%02d fit %2d.%02d (%d)",
            (reject == i) ? "remove" : "      ", i,
            ctx->beacon[i].ap.in_cache ? "cached" : "      ", (int)ideal_rssi[i],
            (int)fabs(round(100 * (ideal_rssi[i] - (int)ideal_rssi[i]))),
            (int)fabs(NOMINAL_RSSI(ctx->beacon[i].ap.rssi) - ideal_rssi[i]), ideal_rssi[i],
            (int)fabs(
                round(100 * (fabs(NOMINAL_RSSI(ctx->beacon[i].ap.rssi) - ideal_rssi[i]) -
                                (int)fabs(NOMINAL_RSSI(ctx->beacon[i].ap.rssi) - ideal_rssi[i])))),
            ctx->beacon[i].ap.rssi)
    }
#endif
    return remove_beacon(ctx, reject);
}

/*! \brief try to reduce AP by filtering out the oldest one
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static bool filter_aps_by_age(Sky_ctx_t *ctx)
{
    int i;
    int oldest_idx = -1;
    uint32_t oldest_age = 0, youngest_age = UINT_MAX; /* age is in seconds, larger means older */

    if (ctx->ap_len <= CONFIG(ctx->cache, max_ap_beacons))
        return false;

    /* for each beacon, work out it's the oldest or newest found */
    for (i = 0; i < ctx->ap_len; i++) {
        if (ctx->beacon[i].ap.age < youngest_age) {
            youngest_age = ctx->beacon[i].ap.age;
        }
        if (ctx->beacon[i].ap.age > oldest_age) {
            oldest_age = ctx->beacon[i].ap.age;
            oldest_idx = i;
        }
    }

    /* if the oldest and youngest beacons have the same age,
     * there is nothing to do. Otherwise remove the oldest */
    /* NOTE: the oldest and weakest is removed */
    if (youngest_age != oldest_age && oldest_idx != -1) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: %d oldest", oldest_idx);
        remove_beacon(ctx, oldest_idx);
        return true;
    }
    return false;
}

/*! \brief try to reduce AP by filtering out virtual AP
 *         When similar, remove beacon with highesr mac address
 *         unless it is in cache, then choose to remove the uncached beacon
 *
 *  @param ctx Skyhook request context
 *
 *  @return true if beacon removed or false otherwise
 */
static bool filter_virtual_aps(Sky_ctx_t *ctx)
{
    int i, j;
    int cmp, rm = -1;
#if SKY_DEBUG
    int keep = -1;
    bool cached = false;
#endif

    LOGFMT(
        ctx, SKY_LOG_LEVEL_DEBUG, "ap_len: %d APs of %d beacons", (int)ctx->ap_len, (int)ctx->len)

    dump_workspace(ctx);

    if (ctx->ap_len <= CONFIG(ctx->cache, max_ap_beacons)) {
        return false;
    }

    /* look for any AP beacon that is 'similar' to another */
    if (ctx->beacon[0].h.type != SKY_BEACON_AP) {
        LOGFMT(ctx, SKY_LOG_LEVEL_CRITICAL, "beacon type not WiFi")
        return false;
    }

    for (j = 0; j < ctx->ap_len; j++) {
        for (i = j + 1; i < ctx->ap_len; i++) {
            if ((cmp = similar(ctx->beacon[i].ap.mac, ctx->beacon[j].ap.mac)) < 0) {
                if (ctx->beacon[j].ap.in_cache) {
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
                if (ctx->beacon[i].ap.in_cache) {
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
                    cached ? " (cached)" : "")
                remove_beacon(ctx, rm);
                return true;
            }
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "no match")
    return false;
}

/*! \brief compare beacons fpr equality
 *
 *  if beacons are equivalent, return 1 otherwise 0
 *  if an error occurs during comparison. return -1
 */
static int beacon_is_same(Sky_ctx_t *ctx, Beacon_t *a, Beacon_t *b)
{
    int ret = -1;

    if (!a || !b || !ctx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return -1;
    }

    if (a->h.type == b->h.type) {
        switch (a->h.type) {
        case SKY_BEACON_AP:
            if (memcmp(a->ap.mac, b->ap.mac, MAC_SIZE) == 0)
                ret = 1;
            break;
        case SKY_BEACON_BLE:
            if ((memcmp(a->ble.mac, b->ble.mac, MAC_SIZE) == 0) && (a->ble.major == b->ble.major) &&
                (a->ble.minor == b->ble.minor) && (memcmp(a->ble.uuid, b->ble.uuid, 16) == 0))
                ret = 1;
            break;
        case SKY_BEACON_CDMA:
            if ((a->cdma.sid == b->cdma.sid) && (a->cdma.nid == b->cdma.nid) &&
                (a->cdma.bsid == b->cdma.bsid)) {
                if (!(a->cdma.sid == SKY_UNKNOWN_ID2 || a->cdma.nid == SKY_UNKNOWN_ID3 ||
                        a->cdma.bsid == SKY_UNKNOWN_ID4))
                    ret = 1;
            }
            break;
        case SKY_BEACON_GSM:
            if ((a->gsm.ci == b->gsm.ci) && (a->gsm.mcc == b->gsm.mcc) &&
                (a->gsm.mnc == b->gsm.mnc) && (a->gsm.lac == b->gsm.lac))
                if (!(a->gsm.ci == SKY_UNKNOWN_ID4 || a->gsm.mcc == SKY_UNKNOWN_ID1 ||
                        a->gsm.mnc == SKY_UNKNOWN_ID2 || a->gsm.lac == SKY_UNKNOWN_ID3)) {
                    ret = 1;
                }
            break;
        case SKY_BEACON_LTE:
            if ((a->lte.mcc == b->lte.mcc) && (a->lte.mnc == b->lte.mnc) &&
                (a->lte.e_cellid == b->lte.e_cellid)) {
                if (a->lte.mcc == SKY_UNKNOWN_ID2) { /* this is an NMR if ID2 is unknown */
                    if ((a->lte.pci == b->lte.pci) && (a->lte.earfcn == b->lte.earfcn))
                        ret = 1;
                } else
                    ret = 1;
            }
            break;
        case SKY_BEACON_NBIOT:
            if ((a->nbiot.mcc == b->nbiot.mcc) && (a->nbiot.mnc == b->nbiot.mnc) &&
                (a->nbiot.e_cellid == b->nbiot.e_cellid)) {
                if (a->nbiot.mcc == SKY_UNKNOWN_ID2) { /* this is an NMR if ID2 is unknown */
                    if ((a->nbiot.ncid == b->nbiot.ncid) && (a->nbiot.earfcn == b->nbiot.earfcn))
                        ret = true;
                } else
                    ret = 1;
            }
            break;
        case SKY_BEACON_UMTS:
            if ((a->umts.ucid == b->umts.ucid) && (a->umts.mcc == b->umts.mcc) &&
                (a->umts.mnc == b->umts.mnc)) {
                if (a->umts.mcc == SKY_UNKNOWN_ID2) { /* this is an NMR if ID2 is unknown */
                    if ((a->umts.psc == b->umts.psc) && (a->umts.uarfcn == b->umts.uarfcn))
                        ret = true;
                } else
                    ret = 1;
            }
            break;
        case SKY_BEACON_NR:
            if ((a->nr.mcc == b->nr.mcc) && (a->nr.mnc == b->nr.mnc) && (a->nr.nci == b->nr.nci)) {
                if (a->nr.mcc == SKY_UNKNOWN_ID2) { /* this is an NMR if ID2 is unknown */
                    if ((a->nr.pci == b->nr.pci) && (a->nr.nrarfcn == b->nr.nrarfcn))
                        ret = true;
                } else
                    ret = 1;
            }
            break;
        default:
            ret = 0;
        }
    }
    return ret;
}

/*! \brief add beacon to list
 *  if beacon is AP, filter
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param b pointer to new beacon
 *  @param is_connected This beacon is currently connected
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
Sky_status_t add_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b, bool is_connected)
{
    int j, idx = -1;

    if (b->h.type == SKY_BEACON_AP) {
        if (!validate_mac(b->ap.mac, ctx))
            return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
        /* see if this mac already added (duplicate beacon) */
        for (j = 0; j < ctx->ap_len; j++) {
            if (memcmp(b->ap.mac, ctx->beacon[j].ap.mac, MAC_SIZE) == 0) {
                /* reject new beacon if older or weaker */
                if (b->ap.age > ctx->beacon[j].ap.age ||
                    (b->ap.age == ctx->beacon[j].ap.age &&
                        NOMINAL_RSSI(b->ap.rssi) <= NOMINAL_RSSI(ctx->beacon[j].ap.rssi))) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Reject duplicate beacon")
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                }
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Keep new duplicate beacon %s",
                    (b->ap.age == ctx->beacon[j].ap.age) ? "(stronger signal)" : "(younger)")
                remove_beacon(ctx, j);
                break;
            }
        }
    } else if (is_cell_type(b)) {
        for (j = ctx->ap_len; j < ctx->len; j++) {
            if (beacon_is_same(ctx, b, &ctx->beacon[j]) == 1) {
                /* reject new beacon if older or weaker */
                if (get_cell_age(b) > get_cell_age(&ctx->beacon[j]) ||
                    (get_cell_age(b) == get_cell_age(&ctx->beacon[j]) &&
                        NOMINAL_RSSI(get_cell_rssi(b)) <=
                            NOMINAL_RSSI(get_cell_rssi(&ctx->beacon[j])))) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate cell beacon")
                    return sky_return(sky_errno, SKY_ERROR_NONE);
                }
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Keep new duplicate cell beacon %s",
                    (get_cell_age(b) == get_cell_age(&ctx->beacon[j])) ? "(stronger signal)" :
                                                                         "(younger)")
                remove_beacon(ctx, j);
                break;
            }
        }
    }

    /* insert the beacon */
    if (insert_beacon(ctx, sky_errno, b, &idx) == SKY_ERROR)
        return SKY_ERROR;
    if (is_connected)
        ctx->connected = idx;

    dump_workspace(ctx);

    if (b->h.type == SKY_BEACON_AP) {
        ctx->beacon[idx].ap.in_cache =
            beacon_in_cache(ctx, b, &ctx->cache->cacheline[ctx->cache->newest]);

        /* done if no filtering needed */
        if (ctx->ap_len <= CONFIG(ctx->cache, max_ap_beacons))
            return sky_return(sky_errno, SKY_ERROR_NONE);

        /* beacon is AP and is subject to filtering */
        if (!filter_virtual_aps(ctx) && !filter_aps_by_age(ctx))
            if (filter_aps_by_rssi(ctx) == SKY_ERROR) {
                LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "failed to filter")
                return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
            }
    } else if (is_cell_type(b)) {
        /* done if no filtering needed */
        if ((ctx->len - ctx->ap_len) <=
            (CONFIG(ctx->cache, total_beacons) - CONFIG(ctx->cache, max_ap_beacons)))
            return sky_return(sky_errno, SKY_ERROR_NONE);

        /* cells added in priority order, remove lowest priority beacon */
        remove_beacon(ctx, ctx->len - 1);
    }

    dump_workspace(ctx);
    return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief check if a beacon is in a cacheline
 *
 *  @param ctx Skyhook request context
 *  @param b pointer to new beacon
 *  @param cl pointer to cacheline
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
static bool beacon_in_cache(Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl)
{
    int j;

    if (!cl || !b || !ctx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return false;
    }

    if (cl->time == 0) {
        return false;
    }

    for (j = 0; j < cl->len; j++)
        if (beacon_is_same(ctx, b, &cl->beacon[j]) == 1) {
            return true;
        }
    return false;
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
static bool cell_changed(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int j;
    if (!ctx || !cl) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return true;
    }

    if ((ctx->len - ctx->ap_len) == 0 || (cl->len - cl->ap_len) == 0) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "0 cells in cache or workspace");
        return false;
    }

    /* for each cell in workspace, compare with cacheline */
    for (j = ctx->ap_len; j < ctx->len; j++) {
        if (ctx->connected == j && beacon_in_cache(ctx, &ctx->beacon[j], cl)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "serving cells match");
            return false;
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d - cell mismatch", cl - ctx->cache->cacheline);

    return true;
}

/*! \brief find cache entry with best match
 *  if beacon is AP, filter
 *
 *  @param ctx Skyhook request context
 *  @param put boolean which reflects put or get from cache
 *
 *  @return index of best match or empty cacheline or -1
 */
int find_best_match(Sky_ctx_t *ctx, bool put)
{
    int i, j, start, end;
    float ratio[CACHE_SIZE];
    int score[CACHE_SIZE];
    float bestratio = 0;
    int bestscore = 0;
    int bestc = -1;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s", put ? "for save to cache" : "for get from cache")
    dump_workspace(ctx);

    /* score each cache line wrt beacon match ratio */
    for (i = 0; i < CACHE_SIZE; i++) {
        ratio[i] = score[i] = 0;
        if (ctx->cache->cacheline[i].time != 0 &&
            ((uint32_t)(*ctx->gettime)(NULL)-ctx->cache->cacheline[i].time) >
                (CONFIG(ctx->cache, cache_age_threshold) * SECONDS_IN_HOUR)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache line %d expired", i);
            ctx->cache->cacheline[i].time = 0;
        }
        if (put && (ctx->cache->cacheline[i].time == 0)) {
            /* looking for match for put, empty cacheline = 1st choice */
            score[i] = CONFIG(ctx->cache, total_beacons) * 2;
        } else if (!put && ctx->cache->cacheline[i].time == 0) {
            /* looking for match for get, ignore empty cache */
            continue;
        } else if (!put && cell_changed(ctx, &ctx->cache->cacheline[i])) {
            /* looking for match for get, ignore cache if serving cell does not match */
            continue;
        } else {
            /* Non empty cacheline - count matching beacons */
            if (ctx->ap_len) {
                start = 0;
                end = ctx->ap_len;
            } else {
                start = ctx->ap_len - 1;
                end = ctx->len;
            }

            for (j = start; j < end; j++) {
                if (beacon_in_cache(ctx, &ctx->beacon[j], &ctx->cache->cacheline[i])) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Beacon %d type %s matches cache %d of 0..%d",
                        j, sky_pbeacon(&ctx->beacon[j]), i, CACHE_SIZE)
                    score[i] = score[i] + 1.0;
                }
            }
        }
    }

    for (i = 0; i < CACHE_SIZE; i++) {
        if (score[i] == CONFIG(ctx->cache, total_beacons) * 2) {
            ratio[i] = 1.0;
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache: %d: fill empty cacheline", i)
        } else if (ctx->ap_len && ctx->cache->cacheline[i].ap_len) {
            // score = intersection(A, B) / union(A, B)
            int unionAB =
                (ctx->ap_len +
                    MIN(ctx->cache->cacheline[i].ap_len, CONFIG(ctx->cache, max_ap_beacons)) -
                    score[i]);
            ratio[i] = (float)score[i] / unionAB;
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache: %d: score %d (%d/%d)", i,
                (int)round(ratio[i] * 100), score[i], unionAB)

        } else if (ctx->len - ctx->ap_len &&
                   ctx->cache->cacheline[i].len - ctx->cache->cacheline[i].ap_len) {
            // if all cell beacons match
            ratio[i] = (score[i] == ctx->len - ctx->ap_len) ? 100.0 : 0.0;
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache: %d: all %d cell beacons match",
                ctx->len - ctx->ap_len)
        }

        if (ratio[i] > bestratio) {
            bestratio = ratio[i];
            bestscore = score[i];
            bestc = i;
        }
    }

    /* if match is for get, must meet threshold */
    if (!put) {
        if (ctx->len <= CONFIG(ctx->cache, cache_beacon_threshold) && bestscore == ctx->len) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "Only %d beacons; pick cache %d of 0..%d score %d (vs %d)", ctx->len, bestc,
                CACHE_SIZE, (int)round(ratio[bestc] * 100),
                CONFIG(ctx->cache, cache_beacon_threshold))
            return bestc;
        } else if (ctx->len > CONFIG(ctx->cache, cache_beacon_threshold) &&
                   bestratio * 100 > CONFIG(ctx->cache, cache_match_threshold)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "location in cache, pick cache %d of 0..%d score %d (vs %d)", bestc, CACHE_SIZE - 1,
                (int)round(ratio[bestc] * 100), CONFIG(ctx->cache, cache_match_threshold))
            return bestc;
        }
    } else if (bestc >= 0) { /* match is for put */
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
            "save location in best cache, %d of 0..%d score %d (vs %d)", bestc, CACHE_SIZE - 1,
            (int)round(ratio[bestc] * 100), CONFIG(ctx->cache, cache_match_threshold))
        return bestc;
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache match failed. Best score %d (vs %d)",
        (int)round(bestratio * 100), CONFIG(ctx->cache, cache_match_threshold))
    return -1;
}

/*! \brief find cache entry with oldest entry
 *  if beacon is AP, filter
 *
 *  @param ctx Skyhook request context
 *
 *  @return index of oldest cache entry, or empty
 */
static int find_oldest(Sky_ctx_t *ctx)
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
 *  @param ctx Skyhook request context
 *  @param loc pointer to location info
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
Sky_status_t add_to_cache(Sky_ctx_t *ctx, Sky_location_t *loc)
{
    int i = -1;
    int j;
    uint32_t now = (*ctx->gettime)(NULL);

    if (CACHE_SIZE < 1) {
        return SKY_SUCCESS;
    }

    /* compare current time to Mar 1st 2019 */
    if (now <= TIMESTAMP_2019_03_01) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Don't have good time of day!")
        return SKY_ERROR;
    }

    /* Find best match in cache */
    /*    yes - add entry here */
    /* else find oldest cache entry */
    /*    yes - add entryu here */
    if ((i = find_best_match(ctx, PUT_IN_CACHE)) < 0) {
        i = find_oldest(ctx);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "find_oldest chose cache %d of 0..%d", i, CACHE_SIZE)
    } else {
        if (loc->location_status != SKY_LOCATION_STATUS_SUCCESS) {
            ctx->cache->cacheline[i].time = 0; /* clear cacheline */
            update_newest_cacheline(ctx);
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "find_best_match found cache match %d of 0..%d, but cleared", i, CACHE_SIZE);
        } else
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "find_best_match found cache match %d of 0..%d", i,
                CACHE_SIZE);
    }
    if (loc->location_status != SKY_LOCATION_STATUS_SUCCESS) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Won't add unknown location to cache")
        return SKY_ERROR;
    }
    ctx->cache->cacheline[i].len = ctx->len;
    ctx->cache->cacheline[i].ap_len = ctx->ap_len;
    ctx->cache->cacheline[i].loc = *loc;
    ctx->cache->cacheline[i].time = now;
    ctx->cache->newest = i;
    for (j = 0; j < ctx->len; j++)
        ctx->cache->cacheline[i].beacon[j] = ctx->beacon[j];
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
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Don't have good time of day!")
        return SKY_ERROR;
    }
    return find_best_match(ctx, GET_FROM_CACHE);
}
