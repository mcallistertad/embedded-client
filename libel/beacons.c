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
#include "libel.h"

/* set VERBOSE_DEBUG to true to enable extra logging */
#ifndef VERBOSE_DEBUG
#define VERBOSE_DEBUG false
#endif // VERBOSE_DEBUG

/*! \brief compare the connected and used properties of APs
 *         connected has higher value than used when comparing APs
 *
 *  @param a first beacon
 *  @param b second beacon
 *
 *  @return positive if a has better properties
 *          negative if b has better properties
 *          0 if a and b have the same properties
 */
int compare_connected_used(Beacon_t *a, Beacon_t *b)
{
    return ((a->h.connected && !b->h.connected) ?
                1 :
                ((b)->h.connected && !(a)->h.connected) ?
                -1 :
                ((a)->ap.property.used && !(b)->ap.property.used) ?
                1 :
                ((b)->ap.property.used && !(a)->ap.property.used) ? -1 : 0);
}

/*! \brief shuffle list to remove the beacon at index
 *
 *  @param rctx Skyhook request context
 *  @param index 0 based index of AP to remove
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t remove_beacon(Sky_rctx_t *rctx, int index)
{
    if (index >= NUM_BEACONS(rctx))
        return SKY_ERROR;

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "type:%s idx:%d", sky_pbeacon(&rctx->beacon[index]), index);
    if (is_ap_type(&rctx->beacon[index]))
        NUM_APS(rctx) -= 1;
    memmove(&rctx->beacon[index], &rctx->beacon[index + 1],
        sizeof(Beacon_t) * (NUM_BEACONS(rctx) - index - 1));
    NUM_BEACONS(rctx) -= 1;
#if VERBOSE_DEBUG
    DUMP_REQUEST_CTX(rctx);
#endif // VERBOSE_DEBUG
    return SKY_SUCCESS;
}

/*! \brief compare beacons for ordering when inserting in request context
 *
 * better beacons are inserted before worse.
 *
 *  @param rctx Skyhook request context
 *  @param a pointer to beacon A
 *  @param B pointer to beacon B
 *
 *  @return  >0 if beacon A is better
 *           <0 if beacon B is better
 */
static int is_beacon_first(Sky_rctx_t *rctx, Beacon_t *a, Beacon_t *b)
{
    int diff = 0;

#if VERBOSE_DEBUG
    dump_beacon(rctx, "A: ", a, __FILE__, __FUNCTION__);
    dump_beacon(rctx, "B: ", b, __FILE__, __FUNCTION__);
#endif // VERBOSE_DEBUG
    /* sky_plugin_compare compares beacons of the same class and returns SKY_ERROR when they are different classes */
    if ((sky_plugin_compare(rctx, NULL, a, b, &diff)) == SKY_ERROR) {
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
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Different classes %d (%s)", diff,
            diff < 0 ? "B is better" : "A is better");
#endif // VERBOSE_DEBUG
    } else {
        /* otherwise beacons were comparable and plugin set diff appropriately */
        diff = (diff != 0) ? diff : 1; /* choose A if plugin returns 0 */
#if VERBOSE_DEBUG
        if (diff)
            LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Same types %d (%s)", diff,
                diff < 0 ? "B is better" : "A is better");
#endif // VERBOSE_DEBUG
    }
    return diff;
}

/*! \brief insert beacon in list based on type and AP rssi
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno pointer to errno
 *  @param b beacon to add
 *  @param index pointer where to save the insert position
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
static Sky_status_t insert_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, Beacon_t *b)
{
    int j;

    /* check for duplicate */
    if (is_ap_type(b) || is_cell_type(b)) {
        for (j = 0; j < NUM_BEACONS(rctx); j++) {
            bool equal = false;

            if (sky_plugin_equal(rctx, sky_errno, b, &rctx->beacon[j], &equal) == SKY_SUCCESS &&
                equal) {
                /* Found duplicate - keep new beacon if it is better */
                if (b->h.age < rctx->beacon[j].h.age || /* Younger */
                    (b->h.age == rctx->beacon[j].h.age &&
                        b->h.connected) || /* same age, but connected */
                    (b->h.age ==
                            rctx->beacon[j].h.age && /* same age and connectedness, but stronger */
                        b->h.connected == rctx->beacon[j].h.connected &&
                        b->h.rssi > rctx->beacon[j].h.rssi)) {
                    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Keep new duplicate");
                    break; /* break for loop and remove existing duplicate */
                } else {
                    LOGFMT(rctx, SKY_LOG_LEVEL_WARNING, "Reject duplicate");
                    return set_error_status(sky_errno, SKY_ERROR_NONE);
                }
            }
        }
        if (j < NUM_BEACONS(rctx)) {
            /* a better duplicate was found, remove existing beacon */
            remove_beacon(rctx, j);
        }
    } else {
        LOGFMT(rctx, SKY_LOG_LEVEL_WARNING, "Unsupported beacon type");
        return set_error_status(sky_errno, SKY_ERROR_INTERNAL);
    }

    /* find position to insert based on plugin compare operation */
    for (j = 0; j < NUM_BEACONS(rctx); j++) {
        if (is_beacon_first(rctx, b, &rctx->beacon[j]) > 0)
            break;
    }

    /* add beacon at the end */
    if (j == NUM_BEACONS(rctx)) {
        rctx->beacon[j] = *b;
        NUM_BEACONS(rctx)++;
    } else {
        /* shift beacons to make room for the new one */
        memmove(&rctx->beacon[j + 1], &rctx->beacon[j], sizeof(Beacon_t) * (NUM_BEACONS(rctx) - j));
        rctx->beacon[j] = *b;
        NUM_BEACONS(rctx)++;
    }

    if (is_ap_type(b)) {
        NUM_APS(rctx)++;
    }

#ifdef SKY_LOGGING
    /* Verify that the beacon we just added now appears in our beacon set. */
    for (j = 0; j < NUM_BEACONS(rctx); j++) {
        bool equal;
        if (sky_plugin_equal(rctx, sky_errno, b, &rctx->beacon[j], &equal) == SKY_SUCCESS && equal)
            break;
    }
    if (j < NUM_BEACONS(rctx))
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Beacon type %s inserted at idx %d", sky_pbeacon(b), j);
    else
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Beacon NOT found after insert");
#endif // SKY_LOGGING
    return SKY_SUCCESS;
}

/*! \brief add beacon to list in request rctx
 *
 *   if beacon is not AP and request rctx is full (of non-AP), pick best one
 *   if beacon is AP,
 *    . reject a duplicate
 *    . for duplicates, keep newest and strongest
 *
 *   Insert new beacon in request rctx
 *    . Add APs in order based on lowest to highest rssi value
 *    . Add cells after APs
 *
 *   If AP just added is known in cache,
 *    . set cached and copy Used property from cache
 *
 *   If AP just added fills request rctx, remove one AP,
 *    . Remove one virtual AP if there is a match
 *    . If haven't removed one AP, remove one based on rssi distribution
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param b beacon to be added
 *  @param timestamp time that the beacon was scanned
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
Sky_status_t add_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, Beacon_t *b, time_t timestamp)
{
    int n;

#if !SKY_EXCLUDE_SANITY_CHECKS
    if (!validate_request_ctx(rctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
#endif // !SKY_EXCLUDE_SANITY_CHECKS

    if (!rctx->session->open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_beacon(b, rctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* Validate scan was before sky_new_request and since Mar 1st 2019 */
    if (timestamp != TIME_UNAVAILABLE && timestamp < TIMESTAMP_2019_03_01)
        return set_error_status(sky_errno, SKY_ERROR_BAD_TIME);
    else if (rctx->header.time == TIME_UNAVAILABLE || timestamp == TIME_UNAVAILABLE)
        b->h.age = 0;
    else if (difftime(rctx->header.time, timestamp) >= 0)
        b->h.age = difftime(rctx->header.time, timestamp);
    else
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

#if CACHE_SIZE && !SKY_EXCLUDE_WIFI_SUPPORT
    /* Update the AP with any used info from cache */
    if (is_ap_type(b))
        beacon_in_cache(rctx, b); /* b is updated with Used info if the beacon is found */
#endif // CACHE_SIZE && !SKY_EXCLUDE_WIFI_SUPPORT

    /* insert the beacon */
    n = NUM_BEACONS(rctx);
    if (insert_beacon(rctx, sky_errno, b) == SKY_ERROR)
        return SKY_ERROR;
    if (n == NUM_BEACONS(rctx)) // no beacon added, must be duplicate because there was no error
        return SKY_SUCCESS;

    /* done if no filtering needed */
    if (NUM_APS(rctx) <= CONFIG(rctx->session, max_ap_beacons) &&
        (NUM_CELLS(rctx) <=
            (CONFIG(rctx->session, total_beacons) - CONFIG(rctx->session, max_ap_beacons)))) {
        return SKY_SUCCESS;
    }

    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates or remove one based on rssi distribution */
    if (sky_plugin_remove_worst(rctx, sky_errno) == SKY_ERROR) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Unexpected failure removing worst beacon");
        DUMP_REQUEST_CTX(rctx);
        return set_error_status(sky_errno, SKY_ERROR_INTERNAL);
    }

    return SKY_SUCCESS;
}

#if CACHE_SIZE
/*! \brief check if a beacon is in cache
 *
 *   Scan all cachelines in the cache.
 *   If the given beacon is found in the cache true is returned otherwise
 *   false. A beacon may appear in multiple cachelines.
 *   If beacon is found and the cached beacon is marked 'used', the given
 *   beacon is marked as 'used' also.
 *   Algorithm stops searching if beacon is found and that beacon is marked used.
 *   Otherwise the entire cache is scanned.
 *
 *  @param rctx Skyhook request context
 *  @param b pointer to new beacon
 *  @param cl pointer to cacheline
 *  @param prop pointer to where the properties of matching beacon is saved
 *
 *  @return true if beacon successfully found or false
 */
bool beacon_in_cache(Sky_rctx_t *rctx, Beacon_t *b)
{
    bool result = false;

    for (int i = 0; i < rctx->session->num_cachelines; i++) {
        if (beacon_in_cacheline(rctx, b, &rctx->session->cacheline[i])) {
            result = true; /* beacon is in cache */
            if (is_ap_type(b) && b->ap.property.used)
                break; /* beacon is in cached and used, can stop search */
        }
    }
    return result;
}

/*! \brief check if a beacon is in a cacheline
 *
 *   Scan all beacons in the cacheline. If the type matches the given beacon, compare
 *   the appropriate attributes. If the given beacon is found in the cacheline
 *   true is returned otherwise false. Also if cached beacon has the Used property,
 *   set it in the beacon we searched for.
 *
 *  @param rctx Skyhook request context
 *  @param b pointer to new beacon
 *  @param cl pointer to cacheline
 *
 *  @return true if beacon successfully found or false
 */
bool beacon_in_cacheline(Sky_rctx_t *rctx, Beacon_t *b, Sky_cacheline_t *cl)
{
    int j;

    if (cl->time == CACHE_EMPTY) {
        return false;
    }

    for (j = 0; j < NUM_BEACONS(cl); j++) {
        bool equal = false;

        if (sky_plugin_equal(rctx, NULL, b, &cl->beacon[j], &equal) == SKY_SUCCESS && equal) {
            if (is_ap_type(&cl->beacon[j]) && cl->beacon[j].ap.property.used)
                b->ap.property.used = true;
            return true;
        }
    }
    return false;
}

/*! \brief find cache entry with oldest entry
 *
 *  @param rctx Skyhook request context
 *
 *  @return index of oldest cache entry, or empty
 */
int find_oldest(Sky_rctx_t *rctx)
{
#if CACHE_SIZE == 1
    /* if there is only one cacheline */
    (void)rctx;
    return 0;
#else
    int i;
    int oldestc = 0;
    time_t oldest = rctx->header.time;

    for (i = 0; i < CACHE_SIZE; i++) {
        /* if time is unavailable or
         * cacheline is empty,
         * then return index of current cacheline */
        if (oldest == TIME_UNAVAILABLE || rctx->session->cacheline[i].time == CACHE_EMPTY)
            return i;
        else if (difftime(rctx->session->cacheline[i].time, oldest) < 0) {
            oldest = rctx->session->cacheline[i].time;
            oldestc = i;
        }
    }
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "cacheline %d oldest time %d", oldestc, oldest);
    return oldestc;
#endif // CACHE_SIZE == 1
}

#if !SKY_EXCLUDE_GNSS_SUPPORT
/*! \brief test whether gnss in new scan is preferable to that in cache
 *
 *  if new scan has better gnss that that in cache, it is better to update cache
 *  by sending new scan to server.
 *
 *  true only if gnss fix in cache is worse than new scan
 *  false if cached gnss is better
 *
 *  @param rctx Skyhook request context
 *  @param cl the cacheline to count in
 *
 *  @return true or false
 */
int cached_gnss_worse(Sky_rctx_t *rctx, Sky_cacheline_t *cl)
{
    if (!has_gnss(rctx))
        /* new scan doesn't include gnss */
        return false;

    if (!has_gnss(cl)) {
#ifdef VERBOSE_DEBUG
        /* new scan includes gnss, but cached scan does not */
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "cache miss! Cacheline has no gnss!");
#endif // VERBOSE_DEBUG
        return true;
    }

    /* at this point both new and cached scans include GNSS */
    if (rctx->gnss.hpe < cl->gnss.hpe) {
#ifdef VERBOSE_DEBUG
        /* New gnss is more accurate than cached gnss */
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "cache miss! Cacheline has worse gnss hpe!");
#endif // VERBOSE_DEBUG
        return true;
    }

    if (distance_A_to_B(rctx->gnss.lat, rctx->gnss.lon, cl->gnss.lat, cl->gnss.lon) >=
        (float)rctx->gnss.hpe) {
#ifdef VERBOSE_DEBUG
        /* Cached gnss location is outside radius of uncertainty at 68% of new gnss */
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
            "cache miss! Distance to cacheline gnss fix (%dm) is larger than HPE of new gnss fix (%dm)",
            (int)distance_A_to_B(rctx->gnss.lat, rctx->gnss.lon, cl->gnss.lat, cl->gnss.lon),
            rctx->gnss.hpe);
#endif // VERBOSE_DEBUG
        return true;
    }

    return false;
}
#endif // !SKY_EXCLUDE_GNSS_SUPPORT

#if !SKY_EXCLUDE_CELL_SUPPORT
/*! \brief test serving cell in workspace has changed from that in cache
 *
 *  @return true or false
 *
 *  false if either request context or cache has no cells
 *  false if highest priority cell (which is
 *  assumed to be the serving cell, regardless of whether or
 *  not the user has marked it "connected") matches cache
 *  true otherwise
 */
int serving_cell_changed(Sky_rctx_t *rctx, Sky_cacheline_t *cl)
{
    Beacon_t *w, *c;
    bool equal = false;

    if (NUM_CELLS(rctx) == 0) {
#if VERBOSE_DEBUG
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "0 cells in request rctx");
#endif // VERBOSE_DEBUG
        return false;
    }

    if (NUM_CELLS(cl) == 0) {
#if VERBOSE_DEBUG
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "0 cells in cache");
#endif // VERBOSE_DEBUG
        return false;
    }

    w = &rctx->beacon[NUM_APS(rctx)];
    c = &cl->beacon[NUM_APS(cl)];
    if (is_cell_nmr(w) || is_cell_nmr(c)) {
#if VERBOSE_DEBUG
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "no significant cell in cache or request rctx");
#endif // VERBOSE_DEBUG
        return false;
    }

    if (sky_plugin_equal(rctx, NULL, w, c, &equal) == SKY_SUCCESS && equal)
        return false;
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "cell mismatch");
    return true;
}
#endif // !SKY_EXCLUDE_CELL_SUPPORT
#endif // CACHE_SIZE

/*! \brief get location from cache
 *
 *  The request context is updated with the index of cacheline with best match
 *  and the status of whether that cacheline is a good enough match to be considered
 *  a cache hit.
 *
 *  @param rctx Skyhook request context
 *
 *  @return true or false based on match of new scan to cachelines
 */
int search_cache(Sky_rctx_t *rctx)
{
#if CACHE_SIZE == 0
    /* no match to cacheline */
    rctx->hit = false;
    return (rctx->get_from = -1);
#else
    /* Avoid using the cache if we have good reason */
    /* to believe that system time is bad or no cache */
    if (rctx->session->num_cachelines < 1 ||
        difftime(rctx->header.time, TIMESTAMP_2019_03_01) < 0 ||
        sky_plugin_match_cache(rctx, NULL) != SKY_SUCCESS) {
        /* no match to cacheline */
        rctx->get_from = -1;
        return (rctx->hit = false);
    }

    return (rctx->hit);
#endif // CACHE_SIZE == 0
}

#if !SKY_EXCLUDE_WIFI_SUPPORT
/*! \brief check if an AP beacon is in a virtual group
 *
 *  Both the b (in request rctx) and vg in cache may be virtual groups
 *  if the two macs are similar and difference is same nibble as child, then
 *  if any of the children have matching macs, then match
 *
 *  @param rctx Skyhook request context
 *  @param b pointer to new beacon
 *  @param vg pointer to beacon in cacheline
 *
 *  @return 0 if no matches otherwise return number of matching APs
 */
int ap_beacon_in_vg(Sky_rctx_t *rctx, Beacon_t *va, Beacon_t *vb, Sky_beacon_property_t *prop)
{
    int w, c, num_aps = 0;
    uint8_t mac_va[MAC_SIZE] = { 0 };
    uint8_t mac_vb[MAC_SIZE] = { 0 };
    Sky_beacon_property_t p;

#if !SKY_LOGGING
    (void)rctx;
#endif // !SKY_LOGGING

    if (!va || !vb || va->h.type != SKY_BEACON_AP || vb->h.type != SKY_BEACON_AP) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return false;
    }
#if VERBOSE_DEBUG
    dump_beacon(rctx, "A: ", va, __FILE__, __FUNCTION__);
    dump_beacon(rctx, "B: ", vb, __FILE__, __FUNCTION__);
#endif // VERBOSE_DEBUG

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
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                    "cmp MAC %02X:%02X:%02X:%02X:%02X:%02X %s with "
                    "%02X:%02X:%02X:%02X:%02X:%02X %s, match %d %s",
                    mac_va[0], mac_va[1], mac_va[2], mac_va[3], mac_va[4], mac_va[5],
                    w == -1 ? "AP " : "VAP", /* Parent or child */
                    mac_vb[0], mac_vb[1], mac_vb[2], mac_vb[3], mac_vb[4], mac_vb[5],
                    c == -1 ? "AP " : "VAP", /* Parent or child */
                    num_aps, p.used ? "Used" : "Unused");
#endif // VERBOSE_DEBUG
            } else {
#if VERBOSE_DEBUG
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                    "cmp MAC %02X:%02X:%02X:%02X:%02X:%02X %s with "
                    "%02X:%02X:%02X:%02X:%02X:%02X %s",
                    mac_va[0], mac_va[1], mac_va[2], mac_va[3], mac_va[4], mac_va[5],
                    w == -1 ? "AP " : "VAP", /* Parent or child */
                    mac_vb[0], mac_vb[1], mac_vb[2], mac_vb[3], mac_vb[4], mac_vb[5],
                    c == -1 ? "AP " : "VAP"); /* Parent or child */
#endif // VERBOSE_DEBUG
            }
        }
    }
    return num_aps;
}
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

#ifdef UNITTESTS

#include "beacons.ut.c"

#endif // UNITTESTS
