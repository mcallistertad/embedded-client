/*! \file libel/beacons.c
 *  \brief utilities - Skyhook Embedded Library
 *
 * Copyright 2015-present Skyhook Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include "../.submodules/tiny-AES128-C/aes.h"
#define SKY_LIBEL 1
#include "libel.h"

void dump_workspace(Sky_ctx_t *ctx);
void dump_cache(Sky_ctx_t *ctx);

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

    memmove(&ctx->beacon[index], &ctx->beacon[index + 1],
            sizeof(Beacon_t) * (ctx->len - index - 1));
    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: %d", index);
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
static Sky_status_t insert_beacon(
        Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b, int *index)
{
    int i;

    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "Insert_beacon: type %d", b->h.type);
    /* sanity checks */
    if (!validate_workspace(ctx) || b->h.magic != BEACON_MAGIC ||
            b->h.type >= SKY_BEACON_MAX)
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "Insert_beacon: Done type %d", b->h.type);

    /* find correct position to insert based on type */
    for (i = 0; i < ctx->len; i++)
        if (ctx->beacon[i].h.type >= b->h.type)
            break;
    if (b->h.type == SKY_BEACON_AP)
        /* note first AP */
        ctx->ap_low = i;

    /* add beacon at the end */
    if (i == ctx->len) {
        ctx->beacon[i] = *b;
        ctx->len++;
    } else {
        /* if AP, add in rssi order */
        if (b->h.type == SKY_BEACON_AP) {
            for (; i < ctx->ap_len; i++)
                if (ctx->beacon[i].h.type != SKY_BEACON_AP ||
                        ctx->beacon[i].ap.rssi > b->ap.rssi)
                    break;
        }
        /* shift beacons to make room for the new one */
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                "shift beacons to make room for the new one (%d)", i);
        memmove(&ctx->beacon[i + 1], &ctx->beacon[i],
                sizeof(Beacon_t) * (ctx->len - i));
        ctx->beacon[i] = *b;
        ctx->len++;
    }
    /* report back the position beacon was added */
    if (index != NULL)
        *index = i;

    if (b->h.type == SKY_BEACON_AP)
        ctx->ap_len++;
    return SKY_SUCCESS;
}

/*! \brief try to reduce AP by filtering out based on diversity of rssi
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
static Sky_status_t filter_by_rssi(Sky_ctx_t *ctx)
{
    int i, reject;
    float band_range, worst;
    float ideal_rssi[MAX_AP_BEACONS + 1];

    if (ctx->ap_len < MAX_AP_BEACONS)
        return SKY_ERROR;

    /* what share of the range of rssi values does each beacon represent */
    band_range = (ctx->beacon[ctx->ap_low + ctx->ap_len - 1].ap.rssi -
                         ctx->beacon[ctx->ap_low].ap.rssi) /
                 (float)ctx->ap_len;

    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "range: %d band: %.2f",
            (ctx->beacon[ctx->ap_low + ctx->ap_len - 1].ap.rssi -
                    ctx->beacon[ctx->ap_low].ap.rssi),
            band_range);
    /* for each beacon, work out it's ideal rssi value to give an even distribution */
    for (i = 0; i < ctx->ap_len; i++)
        ideal_rssi[i] = ctx->beacon[ctx->ap_low].ap.rssi + i * band_range;

    /* find AP with poorest fit to ideal rssi */
    /* always keep lowest and highest rssi */
    for (i = 1, reject = -1, worst = 0; i < ctx->ap_len - 2; i++) {
        if (fabs(ctx->beacon[i].ap.rssi - ideal_rssi[i]) > worst) {
            worst = fabs(ctx->beacon[i].ap.rssi - ideal_rssi[i]);
            reject = i;
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                    "reject: %d, ideal %.2f worst %.2f", i, ideal_rssi[i],
                    worst);
        }
    }
    return remove_beacon(ctx, reject);
}

/*! \brief try to reduce AP by filtering out virtual AP
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
static Sky_status_t filter_virtual_aps(Sky_ctx_t *ctx)
{
    int i, j;
    int cmp;

    logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
            "filter_virtual_aps: ap_low: %d, ap_len: %d of %d APs", ctx->ap_low,
            (int)ctx->ap_len, (int)ctx->len);
    dump_workspace(ctx);

    if (ctx->ap_len < MAX_AP_BEACONS) {
        logfmt(ctx, SKY_LOG_LEVEL_CRITICAL, "%s: too many AP beacons",
                __FUNCTION__);
        return SKY_ERROR;
    }

    /* look for any AP beacon that is 'similar' to another */
    if (ctx->beacon[ctx->ap_low].h.type != SKY_BEACON_AP) {
        logfmt(ctx, SKY_LOG_LEVEL_CRITICAL, "%s: beacon type not AP",
                __FUNCTION__);
        return SKY_ERROR;
    }

    for (j = ctx->ap_low; j <= ctx->ap_low + ctx->ap_len; j++) {
        for (i = j + 1; i <= ctx->ap_low + ctx->ap_len; i++) {
            if ((cmp = similar(ctx->beacon[i].ap.mac, ctx->beacon[j].ap.mac)) <
                    0) {
                logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                        "remove_beacon: %d similar to %d", j, i);
                dump_workspace(ctx);
                remove_beacon(ctx, j);
                return SKY_SUCCESS;
            } else if (cmp > 0) {
                logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                        "remove_beacon: %d similar to %d", i, j);
                dump_workspace(ctx);
                remove_beacon(ctx, i);
                return SKY_SUCCESS;
            }
        }
    }
    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "filter_virtual_aps: no match");
    return SKY_ERROR;
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
Sky_status_t add_beacon(
        Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b, bool is_connected)
{
    int i = -1;

    /* check if maximum number of non-AP beacons already added */
    if (b->h.type != SKY_BEACON_AP &&
            ctx->len - ctx->ap_len > (TOTAL_BEACONS - MAX_AP_BEACONS)) {
        logfmt(ctx, SKY_LOG_LEVEL_WARNING,
                "%s: too many (b->h.type: %d) (ctx->len - ctx->ap_len: %d)",
                __FUNCTION__, b->h.type, ctx->len - ctx->ap_len);
        return sky_return(sky_errno, SKY_ERROR_TOO_MANY);
    }

    /* insert the beacon */
    if (insert_beacon(ctx, sky_errno, b, &i) == SKY_ERROR)
        return SKY_ERROR;
    if (is_connected)
        ctx->connected = i;

    /* done if no filtering needed */
    if (b->h.type != SKY_BEACON_AP || ctx->ap_len <= MAX_AP_BEACONS)
        return sky_return(sky_errno, SKY_ERROR_NONE);

    /* beacon is AP and need filter */
    if (filter_virtual_aps(ctx) == SKY_ERROR)
        if (filter_by_rssi(ctx) == SKY_ERROR) {
            logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: failed to filter",
                    __FUNCTION__);
            return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
        }

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
    int j, ret = 0;

    /* score each cache line wrt beacon match ratio */
    for (j = 0; j < TOTAL_BEACONS; j++)
        if (b->h.type == cl->beacon[j].h.type) {
            switch (b->h.type) {
            case SKY_BEACON_AP:
                if ((memcmp(b->ap.mac, cl->beacon[j].ap.mac, MAC_SIZE) == 0) &&
                        (b->ap.channel == cl->beacon[j].ap.channel)) {
                    ret = 1;
                }
                break;
            case SKY_BEACON_BLE:
                if ((memcmp(b->ble.mac, cl->beacon[j].ble.mac, MAC_SIZE) ==
                            0) &&
                        (b->ble.major == cl->beacon[j].ble.major) &&
                        (b->ble.minor == cl->beacon[j].ble.minor) &&
                        (memcmp(b->ble.uuid, cl->beacon[j].ble.uuid, 16) == 0))
                    ret = 1;
                break;
            case SKY_BEACON_CDMA:
                if ((b->cdma.sid == cl->beacon[j].cdma.sid) &&
                        (b->cdma.nid == cl->beacon[j].cdma.nid) &&
                        (b->cdma.bsid == cl->beacon[j].cdma.bsid))
                    ret = 1;
                break;
            case SKY_BEACON_GSM:
                if ((b->gsm.ci == cl->beacon[j].gsm.ci) &&
                        (b->gsm.mcc == cl->beacon[j].gsm.mcc) &&
                        (b->gsm.mnc == cl->beacon[j].gsm.mnc) &&
                        (b->gsm.lac == cl->beacon[j].gsm.lac))
                    ret = 1;
                break;
            case SKY_BEACON_LTE:
                if ((b->lte.eucid == cl->beacon[j].lte.eucid) &&
                        (b->lte.mcc == cl->beacon[j].lte.mcc) &&
                        (b->lte.mnc == cl->beacon[j].lte.mnc))
                    ret = 1;
                break;
            case SKY_BEACON_NBIOT:
                if ((b->nbiot.mcc == cl->beacon[j].nbiot.mcc) &&
                        (b->nbiot.mnc == cl->beacon[j].nbiot.mnc) &&
                        (b->nbiot.e_cellid == cl->beacon[j].nbiot.e_cellid) &&
                        (b->nbiot.tac == cl->beacon[j].nbiot.tac))
                    ret = 1;
                break;
            case SKY_BEACON_UMTS:
                if ((b->umts.ci == cl->beacon[j].umts.ci) &&
                        (b->umts.mcc == cl->beacon[j].umts.mcc) &&
                        (b->umts.mnc == cl->beacon[j].umts.mnc) &&
                        (b->umts.lac == cl->beacon[j].umts.lac))
                    ret = 1;
                break;
            default:
                ret = 0;
            }
        }
    return ret;
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
    int i, j;
    float score[CACHE_SIZE];
    float best = 0;
    int bestc = -1;

    /* score each cache line wrt beacon match ratio */
    for (i = 0; i < CACHE_SIZE; i++) {
        score[i] = 0;
        /* Discard old cachelines */
        if ((uint32_t)time(NULL) - ctx->cache->cacheline[i].time >
                (CACHE_AGE_THRESHOLD * 60 * 60))
            ctx->cache->cacheline[i].time = 0;
        if (put && ctx->cache->cacheline[i].time == 0)
            score[i] =
                    ctx->len; /* looking for match for put, fill empty cache first */
        else if (ctx->cache->cacheline[i].time == 0)
            continue; /* looking for match for get, ignore empty cache */
        else
            for (j = 0; j < ctx->cache->cacheline[i].len; j++) {
                if (beacon_in_cache(
                            ctx, &ctx->beacon[j], &ctx->cache->cacheline[i])) {
                    score[i] = score[i] + 1.0;
                }
            }
    }

    for (i = 0; i < CACHE_SIZE; i++) {
        score[i] /= ctx->len;
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "%s: cache: %d: score %.2f",
                __FUNCTION__, i, score[i] * 100);
        if (score[i] > best) {
            best = score[i];
            bestc = i;
        }
    }

    /* if match is for get, must meet threshold */
    if (!put) {
        if (ctx->len <= CACHE_BEACON_THRESHOLD && best * 100 == 100) {
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                    "%s: Only %d beacons; pick cache %d of 0..%d score %.2f",
                    __FUNCTION__, ctx->len, bestc, CACHE_SIZE - 1,
                    score[bestc] * 100);
            return bestc;
        } else if (ctx->len > CACHE_BEACON_THRESHOLD &&
                   best * 100 > CACHE_MATCH_THRESHOLD) {
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                    "%s: location in cache, pick cache %d of 0..%d score %.2f",
                    __FUNCTION__, bestc, CACHE_SIZE - 1, score[bestc] * 100);
            return bestc;
        }
    } else if (bestc >= 0) { /* match is for put */
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                "%s: save location in best cache, %d of 0..%d score %.2f",
                __FUNCTION__, bestc, CACHE_SIZE - 1, score[bestc] * 100);
        return bestc;
    }
    return -1;
}

/*! \brief find cache entry with oldest entry
 *  if beacon is AP, filter
 *
 *  @param ctx Skyhook request context
 *
 *  @return index of oldest cache entry, or empty
 */
int find_oldest(Sky_ctx_t *ctx)
{
    int i;
    uint32_t oldestc = 0;
    int oldest = time(NULL);

    for (i = 0; i < CACHE_SIZE; i++) {
        /* Discard old cachelines */
        if ((uint32_t)time(NULL) - ctx->cache->cacheline[i].time >
                (CACHE_AGE_THRESHOLD * 60 * 60)) {
            ctx->cache->cacheline[i].time = 0;
            return i;
        }
        if (ctx->cache->cacheline[i].time == 0)
            return i;
        else if (ctx->cache->cacheline[i].time < oldest) {
            oldest = ctx->cache->cacheline[i].time;
            oldestc = i;
        }
    }
    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "%s: cacheline %d oldest time %d",
            __FUNCTION__, oldestc, oldest);
    return oldestc;
}

/*! \brief add location to cache
 *
 *  @param ctx Skyhook request context
 *  @param loc pointer to location info
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
Sky_status_t add_cache(Sky_ctx_t *ctx, Sky_location_t *loc)
{
    int i = -1;
    int j;
    uint32_t now = time(NULL);

    /* compare current time to Mar 1st 2019 */
    if (now <= 1551398400) {
        logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: Don't have good time of day!",
                __FUNCTION__);
        return SKY_ERROR;
    }

    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "%s:", __FUNCTION__);
    dump_workspace(ctx);
    dump_cache(ctx);

    /* Find best match in cache */
    /*    yes - add entry here */
    /* else find oldest cache entry */
    /*    yes - add entryu here */
    if ((i = find_best_match(ctx, 1)) < 0) {
        i = find_oldest(ctx);
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                "%s: find_oldest chose cache %d of 0..%d", __FUNCTION__, i,
                CACHE_SIZE - 1);
    } else
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                "%s: find_best_match found cache match %d of 0..%d",
                __FUNCTION__, i, CACHE_SIZE - 1);
    for (j = 0; j < TOTAL_BEACONS; j++) {
        ctx->cache->cacheline[i].len = ctx->len;
        ctx->cache->cacheline[i].beacon[j] = ctx->beacon[j];
        ctx->cache->cacheline[i].loc = *loc;
        ctx->cache->cacheline[i].time = now;
    }
    return SKY_SUCCESS;
}

/*! \brief get location from cache
 *
 *  @param ctx Skyhook request context
 *
 *  @return cacheline index or -1
 */
int get_cache(Sky_ctx_t *ctx)
{
    uint32_t now = time(NULL);

    /* compare current time to Mar 1st 2019 */
    if (now <= 1551398400) {
        logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: Don't have good time of day!",
                __FUNCTION__);
        return SKY_ERROR;
    }
    return find_best_match(ctx, 0);
}
