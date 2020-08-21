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

static bool beacon_in_cache(Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl, int *index);
static bool beacon_match(Sky_ctx_t *ctx, Beacon_t *bA, Beacon_t *bB, int *diff);
static Sky_status_t remove_beacon(Sky_ctx_t *ctx, int index);
static Sky_status_t insert_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, Beacon_t *b, int *index);

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
#define NIBBLE_MASK(n) (0xF0 >> (4 * ((n)&1)))
#define LOCAL_ADMIN_MASK(byte) (0x02 & (byte))
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

/*! \brief test two APs in workspace for members of same virtual group
 *
 *  @return 0 if NOT similar, +ve (B parent) or -ve (A parent) if similar
 */
static int ap_similar(Sky_ctx_t *ctx, Beacon_t *apA, Beacon_t *apB, int *pn)
{
    int b, v, n = 0;

    if (!apA || !apB || apA->ap.freq != apB->ap.freq)
        return 0;

    if ((b = mac_similar(ctx, apA->ap.mac, apB->ap.mac, &n)) == 0) {
#if VERBOSE_DEBUG
        dump_ap(ctx, "  Differ A ", apA, __FILE__, __FUNCTION__);
        dump_ap(ctx, "         B ", apB, __FILE__, __FUNCTION__);
#endif
        return 0;
    }
    /* APs have similar mac addresses, but are any members of the virtual groups similar */
    /* Check that children have difference in same nibble */
    for (v = 0; v < NUM_VAPS(apA); v++)
        if (apA->ap.vg[v + VAP_FIRST_DATA].data.nibble_idx != n) {
#if VERBOSE_DEBUG
            dump_ap(ctx, "Mismatch A*", apA, __FILE__, __FUNCTION__);
            dump_ap(ctx, "         B ", apB, __FILE__, __FUNCTION__);
#endif
            return 0;
        }
    for (v = 0; v < NUM_VAPS(apB); v++)
        if (apB->ap.vg[v + VAP_FIRST_DATA].data.nibble_idx != n) {
#if VERBOSE_DEBUG
            dump_ap(ctx, "Mismatch A ", apA, __FILE__, __FUNCTION__);
            dump_ap(ctx, "         B*", apB, __FILE__, __FUNCTION__);
#endif
            return 0;
        }
    if (pn)
        *pn = n;
#if VERBOSE_DEBUG
    dump_ap(ctx, "   Match A ", apA, __FILE__, __FUNCTION__);
    dump_ap(ctx, "         B ", apB, __FILE__, __FUNCTION__);
#endif
    return b;
}

/*! \brief extract nibble from mac
 *
 *  @param mac pointer to mac address
 *  @param d index of nibble
 *
 *  @return value of nibble or 0xff if an error occurred
 */
static uint8_t nibble(uint8_t *mac, int d)
{
    if (d < 0 || d > MAC_SIZE * 2)
        return 0xff;
    if (d & 1)
        return (mac[d / 2] & 0x0F);
    else
        return ((mac[d / 2] >> 4) & 0x0F);
}

/*! \brief add AP to Virtual Group (parent AP), including any associated children of that AP
 *
 *   A list of patches are stored in parent of virtual group, one for each child
 *   These patches describe the how to change parent mac to that of child
 *   The parent rssi is updated based on the weighted average of APs in virtual group.
 *
 *  @param ctx Skyhook request context
 *  @param vg index of parent AP
 *  @param ap index of child AP
 *  @param n index of nibble that differs between parent and child mac
 *
 *  @return bool true if AP is added, false otherwise
 */
static bool add_child_to_VirtualGroup(Sky_ctx_t *ctx, int vg, int ap, int n)
{
    Beacon_t *parent, *child;
    int vg_p, vg_c, dup;
    uint8_t replace; /* value of nibble in child */
    Vap_t patch; /* how to patch parent mac to create child mac */
    Vap_t *pvg; /* the list of patches in parent AP */

    parent = &ctx->beacon[vg];
    child = &ctx->beacon[ap];
#if SKY_DEBUG
    dump_ap(ctx, " Parent", parent, __FILE__, __FUNCTION__);
    dump_ap(ctx, " Child ", child, __FILE__, __FUNCTION__);
#endif

    if (vg >= NUM_APS(ctx) || ap >= NUM_APS(ctx))
        return false;

    if ((replace = nibble(child->ap.mac, n)) == 0xff)
        return false;

    pvg = parent->ap.vg;
    patch.len = 0;
    patch.ap = 0;
    patch.data.nibble_idx = n;
    patch.data.value = replace;

    if (pvg[VAP_LENGTH].len == 0) {
        pvg[VAP_LENGTH].len = 2; /* length of patch bytes */
        pvg[VAP_PARENT].ap = vg; /* index of the parent AP */
    }

    /* ignore child if user has added same AP before */
    for (dup = vg_p = 0; vg_p < NUM_VAPS(parent); vg_p++) {
        if (pvg[vg_p + VAP_FIRST_DATA].data.nibble_idx == patch.data.nibble_idx &&
            pvg[vg_p + VAP_FIRST_DATA].data.value == patch.data.value)
            dup = 1;
    }
    if (!dup && vg_p == CONFIG(ctx->cache, max_vap_per_ap)) /* No room for one more */
        return false;

        /* update parent rssi with proportion of child rssi */
#if SKY_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, " Parent rssi updated from: %d, to: %.1f", parent->h.rssi,
        (EFFECTIVE_RSSI(parent->h.rssi) * (float)(NUM_VAPS(parent) + 1) /
            ((NUM_VAPS(parent) + 1) + (NUM_VAPS(child) + 1))) +
            (EFFECTIVE_RSSI(child->h.rssi) * (float)(NUM_VAPS(child) + 1) /
                ((NUM_VAPS(parent) + 1) + (NUM_VAPS(child) + 1))));
#endif
    if (child->h.rssi != -1) { /* don't average children with unknown rssi */
        if (parent->h.rssi == -1) { /* use child rssi if parent rssi is unknown */
            parent->h.rssi = child->h.rssi;
        } else {
            parent->h.rssi = (EFFECTIVE_RSSI(parent->h.rssi) * (float)(NUM_VAPS(parent) + 1) /
                                 ((NUM_VAPS(parent) + 1) + (NUM_VAPS(child) + 1))) +
                             (EFFECTIVE_RSSI(child->h.rssi) * (float)(NUM_VAPS(child) + 1) /
                                 ((NUM_VAPS(parent) + 1) + (NUM_VAPS(child) + 1)));
        }
    }

    /* update parent cache status true if child or parent in cache */
    if ((parent->ap.property.in_cache =
                (parent->ap.property.in_cache || child->ap.property.in_cache)) == true)
        parent->ap.property.used = (parent->ap.property.used || child->ap.property.used);

    /* Add child unless it is already a member in the parent group */
    if (!dup) {
        pvg[vg_p + VAP_FIRST_DATA].data = patch.data;
        pvg[VAP_LENGTH].len = vg_p + VAP_FIRST_DATA;
        NUM_VAPS(parent) = vg_p + 1;
    }

    /* Add any Virtual APs from child */
    for (vg_c = 0; vg_c < NUM_VAPS(child); vg_c++) {
        for (vg_p = 0; vg_p < NUM_VAPS(parent); vg_p++) {
            /* Ignore any duplicates */
            if (pvg[vg_p + VAP_FIRST_DATA].data.nibble_idx ==
                    child->ap.vg[vg_c + VAP_FIRST_DATA].data.nibble_idx &&
                pvg[vg_p + VAP_FIRST_DATA].data.value ==
                    child->ap.vg[vg_c + VAP_FIRST_DATA].data.value) {
                break;
            }
        }
        /* copy child to parent if not already a member */
        if (vg_p == NUM_VAPS(parent)) {
            if (vg_p == CONFIG(ctx->cache, max_vap_per_ap)) {
                LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "No room to keep all Virtual APs");
                return false;
            }
            pvg[vg_p + VAP_FIRST_DATA].data = child->ap.vg[vg_c + VAP_FIRST_DATA].data;
            pvg[VAP_LENGTH].len = vg_p + VAP_FIRST_DATA;
            NUM_VAPS(parent) = vg_p + 1;
        }
    }
    /* Re-insert parent based on new rssi, and remove old child beacon */
    Beacon_t b = *parent;
    if (vg < ap) {
        remove_beacon(ctx, ap); /* remove child first because it is later in the workspace */
        remove_beacon(ctx, vg); /* remove parent before re-inserting */
        insert_beacon(ctx, NULL, &b, NULL);
    } else {
        remove_beacon(ctx, vg); /* remove parent first because it is later in the workspace */
        remove_beacon(ctx, ap); /* remove child before re-inserting parent */
        insert_beacon(ctx, NULL, &b, NULL);
    }
    return true;
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
    DUMP_WORKSPACE(ctx);
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
    int i, diff = 0;

    /* sanity checks */
    if (!validate_workspace(ctx) || b->h.magic != BEACON_MAGIC || b->h.type >= SKY_BEACON_MAX) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Invalid params. Beacon type %s", sky_pbeacon(b));
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* check for duplicate */
    for (i = 0; i < NUM_BEACONS(ctx); i++) {
        if (beacon_match(ctx, b, &ctx->beacon[i], NULL) == true) { // duplicate?
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
        if (beacon_match(ctx, b, &ctx->beacon[i], &diff) == false)
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
    DUMP_WORKSPACE(ctx);
    return SKY_SUCCESS;
}

/*! \brief remove least desirable cell if workspace is full
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static Sky_status_t select_cell(Sky_ctx_t *ctx)
{
    int i = NUM_BEACONS(ctx) - 1; /* index of last cell */

    /* no work to do if workspace not full of max cell */
    if (NUM_BEACONS(ctx) - NUM_APS(ctx) <=
        CONFIG(ctx->cache, total_beacons) - CONFIG(ctx->cache, max_ap_beacons)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No need to remove cell");
        return SKY_SUCCESS;
    }

    /* sanity check, last beacon must be a cell */
    if (ctx->beacon[i].h.type != SKY_BEACON_LTE && ctx->beacon[i].h.type != SKY_BEACON_UMTS &&
        ctx->beacon[i].h.type != SKY_BEACON_CDMA && ctx->beacon[i].h.type != SKY_BEACON_GSM &&
        ctx->beacon[i].h.type != SKY_BEACON_NBIOT) {
        {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Not a cell?");
            return SKY_ERROR;
        }
    }
    return remove_beacon(ctx, i); /* last cell is least desirable */
}

/*! \brief try to remove one AP by selecting an AP which leaves best spread of rssi values
 *
 *  @param ctx Skyhook request context
 *
 *  @return sky_status_t SKY_SUCCESS if beacon removed or SKY_ERROR
 */
static Sky_status_t select_ap_by_rssi(Sky_ctx_t *ctx)
{
    int i, reject, jump, up_down;
    float band_range, worst;
    float ideal_rssi[MAX_AP_BEACONS + 1];
    Beacon_t *b;

    if (NUM_APS(ctx) <= CONFIG(ctx->cache, max_ap_beacons))
        return SKY_ERROR;

    if (ctx->beacon[0].h.type != SKY_BEACON_AP)
        return SKY_ERROR;

    /* what share of the range of rssi values does each beacon represent */
    band_range = (EFFECTIVE_RSSI(ctx->beacon[0].h.rssi) -
                     EFFECTIVE_RSSI(ctx->beacon[NUM_APS(ctx) - 1].h.rssi)) /
                 ((float)NUM_APS(ctx) - 1);

    /* if the rssi range is small, throw away middle beacon */

    if (band_range < 0.5) {
        /* search from middle of range looking for uncached beacon or Unused beacon in cache */
        for (jump = 0, up_down = -1, i = NUM_APS(ctx) / 2; i >= 0 && i < NUM_APS(ctx);
             jump++, i += up_down * jump, up_down = -up_down) {
            b = &ctx->beacon[i];
            if (!b->ap.property.in_cache || (b->ap.property.in_cache && !b->ap.property.used)) {
                LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Warning: rssi range is small. %s beacon",
                    !jump ? "Remove middle Unused" : "Found Unused");
                return remove_beacon(ctx, i);
            }
        }
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Warning: rssi range is small. Removing cached beacon");
        return remove_beacon(ctx, NUM_APS(ctx) / 2);
    }

    /* if beacon with min RSSI is below threshold, */
    /* throw out weak one, not in cache, not Virtual Group or Unused  */
    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "rssi: %d(%d) vs %d",
        EFFECTIVE_RSSI(ctx->beacon[NUM_APS(ctx) - 1].h.rssi), ctx->beacon[NUM_APS(ctx) - 1].h.rssi,
        -CONFIG(ctx->cache, cache_neg_rssi_threshold));
    if (EFFECTIVE_RSSI(ctx->beacon[NUM_APS(ctx) - 1].h.rssi) <
        -CONFIG(ctx->cache, cache_neg_rssi_threshold)) {
        for (i = NUM_APS(ctx) - 1, reject = -1; i > 0 && reject == -1; i--) {
            if (EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) <
                    -CONFIG(ctx->cache, cache_neg_rssi_threshold) &&
                !ctx->beacon[i].ap.property.in_cache && !NUM_VAPS(&ctx->beacon[i]))
                reject = i;
        }
        for (i = NUM_APS(ctx) - 1; i > 0 && reject == -1; i--) {
            if (EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) <
                    -CONFIG(ctx->cache, cache_neg_rssi_threshold) &&
                ctx->beacon[i].ap.property.in_cache && !NUM_VAPS(&ctx->beacon[i]) &&
                !ctx->beacon[i].ap.property.used)
                reject = i;
        }
        if (reject == -1)
            reject =
                NUM_APS(ctx) -
                1; // reject lowest rssi value if there is no non-virtual group and no uncached or Unused beacon
        LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Discarding beacon %d with very weak strength", reject);
        return remove_beacon(ctx, reject);
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
        if (!ctx->beacon[i].ap.property.in_cache && !NUM_VAPS(&ctx->beacon[i]) &&
            fabs(EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) - ideal_rssi[i]) > worst) {
            worst = fabs(EFFECTIVE_RSSI(ctx->beacon[i].h.rssi) - ideal_rssi[i]);
            reject = i;
        }
    }
    if (reject == -1) {
        /* haven't found a beacon to remove yet due to matching cached beacons */
        reject = NUM_APS(ctx) - 1;
        /* Throw away either lowest or highest rssi valued beacons if not in cache */
        /* and not in virtual group */
        if (!ctx->beacon[NUM_APS(ctx) - 1].ap.property.in_cache &&
            !NUM_VAPS(&ctx->beacon[NUM_APS(ctx) - 1]))
            reject = NUM_APS(ctx) - 1;
        else if (!ctx->beacon[0].ap.property.in_cache && !NUM_VAPS(&ctx->beacon[0]))
            reject = 0;
    }
    if (reject == -1) {
        /* haven't found a beacon to remove yet due to matching cached beacons */
        /* Throw away Unused beacon with worst fit */
        for (i = 1, worst = 0; i < NUM_APS(ctx) - 1; i++) {
            b = &ctx->beacon[i];
            if (!b->ap.property.used && fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i]) > worst) {
                worst = fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i]);
                reject = i;
            }
        }
    }
    if (reject == -1) {
        /* haven't found a beacon to remove yet due to matching cached beacons and no Unused */
        /* Throw away either lowest or highest rssi valued beacons if not Used */
        if (!ctx->beacon[NUM_APS(ctx) - 1].ap.property.used)
            reject = NUM_APS(ctx) - 1;
        else if (!ctx->beacon[0].ap.property.used)
            reject = 0;
        else
            reject =
                NUM_APS(ctx) / 2; /* remove middle beacon (all beacons are in cache and Used) */
    }
#if SKY_DEBUG
    for (i = 0; i < NUM_APS(ctx); i++) {
        b = &ctx->beacon[i];
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%s: %-2d, %s ideal %d.%02d fit %2d.%02d (%d)",
            (reject == i) ? "remove" : "      ", i,
            b->ap.property.in_cache ? b->ap.property.used ? "Used  " : "Unused" : "      ",
            (int)ideal_rssi[i], (int)fabs(round(100 * (ideal_rssi[i] - (int)ideal_rssi[i]))),
            (int)fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i]), ideal_rssi[i],
            (int)fabs(round(100 * (fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i]) -
                                      (int)fabs(EFFECTIVE_RSSI(b->h.rssi) - ideal_rssi[i])))),
            b->h.rssi, NUM_VAPS(b));
    }
#endif
    return remove_beacon(ctx, reject);
}

/*! \brief try to make space in workspace by compressing a virtual AP
 *         When similar, beacon with lowest mac address becomes Group parent
 *         remove the other AP and add it as child of parent
 *
 *  @param ctx Skyhook request context
 *
 *  @return SKY_SUCCESS if beacon removed or SKY_ERROR otherwise
 */
static Sky_status_t compress_virtual_ap(Sky_ctx_t *ctx)
{
    int i, j, n = -1;
    int cmp = 0, rm = -1;
    int keep = -1;
#if SKY_DEBUG

    bool cached = false;
#endif

    if (NUM_APS(ctx) <= CONFIG(ctx->cache, max_ap_beacons)) {
        return SKY_ERROR;
    }

    if (ctx->beacon[0].h.type != SKY_BEACON_AP) {
        return SKY_ERROR;
    }

    /* look for any AP beacon that is 'similar' to another */
    for (j = 0; j < NUM_APS(ctx) - 1; j++) {
#if VERBOSE_DEBUG
        dump_ap(ctx, "cmp A", &ctx->beacon[j], __FILE__, __FUNCTION__);
#endif
        for (i = j + 1; i < NUM_APS(ctx); i++) {
#if VERBOSE_DEBUG
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "compare %d to %d", j, i);
            dump_ap(ctx, "cmp B", &ctx->beacon[i], __FILE__, __FUNCTION__);
#endif
            if ((cmp = ap_similar(ctx, &ctx->beacon[i], &ctx->beacon[j], &n)) < 0) {
                rm = j;
                keep = i;
            } else if (cmp > 0) {
                rm = i;
                keep = j;
            }
            /* if similar, remove and save child virtual AP */
            if (rm != -1) {
#if SKY_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "remove_beacon: %d similar to %d%s at nibble %d",
                    rm, keep, cached ? " (cached)" : "", n);
#endif
                if (!add_child_to_VirtualGroup(ctx, keep, rm, n)) {
                    LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "Didn't save Virtual AP");
                }
                return SKY_SUCCESS;
            }
        }
    }
#if VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "no match");
#endif
    return SKY_ERROR;
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
    int n, i = -1, j = 0;
    Beacon_t *w, *c, tmp;

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
        w->ap.property.in_cache =
            beacon_in_cache(ctx, b, &ctx->cache->cacheline[ctx->cache->newest], &j);
        /* If the added AP is in cache, copy properties */
        c = &ctx->cache->cacheline[ctx->cache->newest].beacon[j];
        if (w->ap.property.in_cache) {
            w->ap.property.used = c->ap.property.used;
#if SKY_DEBUG
            dump_ap(ctx, "() Worksp", w, __FILE__, __FUNCTION__);
            dump_ap(ctx, "() Cache ", c, __FILE__, __FUNCTION__);
#endif
        } else
            w->ap.property.used = false;

    } else {
        if (select_cell(ctx) == SKY_ERROR) {
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "failed to filter cell");
            return sky_return(sky_errno, SKY_ERROR_INTERNAL);
        }
        return SKY_SUCCESS;
    }

    /* done if no filtering needed */
    if (NUM_APS(ctx) <= CONFIG(ctx->cache, max_ap_beacons))
        return SKY_SUCCESS;

    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates of remove one based on rssi distribution */
    if (compress_virtual_ap(ctx) == SKY_ERROR) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "failed to compress AP");
        if (select_ap_by_rssi(ctx) == SKY_ERROR) {
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "failed to filter AP");
            return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
        }
    }

    return SKY_SUCCESS;
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
static int ap_beacon_in_vg(Sky_ctx_t *ctx, Beacon_t *va, Beacon_t *vb)
{
    int w, c, num_aps = 0;
    uint8_t mac_va[MAC_SIZE] = { 0 };
    uint8_t mac_vb[MAC_SIZE] = { 0 };

    if (!ctx || !va || !vb) {
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
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "cmp MAC %02X:%02X:%02X:%02X:%02X:%02X %s with %02X:%02X:%02X:%02X:%02X:%02X %s, match %d",
                    mac_va[0], mac_va[1], mac_va[2], mac_va[3], mac_va[4], mac_va[5],
                    w == -1 ? "AP " : "VAP", /* Parent or child */
                    mac_vb[0], mac_vb[1], mac_vb[2], mac_vb[3], mac_vb[4], mac_vb[5],
                    c == -1 ? "AP " : "VAP", /* Parent or child */
                    num_aps);
#endif
            } else {
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "cmp MAC %02X:%02X:%02X:%02X:%02X:%02X %s with %02X:%02X:%02X:%02X:%02X:%02X %s",
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

/*! \brief compare two beacons
 *
 *  if both are AP, return false and set diff to difference in rssi
 *  if both are cell same cell type, return false and set diff
 *  use the following prioritized list to report A or B best
 *   serving cell
 *   youngest
 *   in cache
 *   strongest
 *
 *  @param ctx Skyhook request context
 *  @param bA pointer to beacon A
 *  @param bB pointer to beacon B
 *
 *  @return true if match or false and set diff
 *   diff is 0 if beacons can't be compared
 *           +ve if beacon A is better
 *           -ve if beacon B is better
 */
static bool beacon_match(Sky_ctx_t *ctx, Beacon_t *bA, Beacon_t *bB, int *diff)
{
    int ret = false;
    int better = 0; // beacon B is better (-ve), or A is better (+ve)

    if (!ctx || !bA || !bB) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        if (diff)
            *diff = 0; /* can't compare */
        return false;
    }

    /* if beacons are not both APs or cells */
    if (bA->h.type != bB->h.type &&
        (bA->h.type == SKY_BEACON_AP || bB->h.type == SKY_BEACON_AP ||
            bA->h.type == SKY_BEACON_BLE || bB->h.type == SKY_BEACON_BLE)) {
        /* types increase in value as they become lower priority */
        /* so we have to invert the sign of the comparison value */
        better = -(bA->h.type - bB->h.type);
        if (diff)
            *diff = better;
#if VERBOSE_DEBUG
        dump_beacon(ctx, "A: ", bA, __FILE__, __FUNCTION__);
        dump_beacon(ctx, "B: ", bB, __FILE__, __FUNCTION__);
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Different types %d (%s)", better,
            better < 0 ? "B is better" : "A is better");
#endif
        return false;
    } else {
        if (bA->h.type == bB->h.type) {
            switch (bA->h.type) {
            case SKY_BEACON_AP:
                if ((ret = ap_beacon_in_vg(ctx, bA, bB)) > 0)
                    ret = true;
                break;
            case SKY_BEACON_BLE:
                if (((better = memcmp(bA->ble.mac, bB->ble.mac, MAC_SIZE)) == 0) &&
                    (bA->ble.major == bB->ble.major) && (bA->ble.minor == bB->ble.minor) &&
                    (memcmp(bA->ble.uuid, bB->ble.uuid, 16) == 0))
                    ret = true;
                break;
            case SKY_BEACON_CDMA:
                if ((bA->cell.id2 == bB->cell.id2) && (bA->cell.id3 == bB->cell.id3) &&
                    (bA->cell.id4 == bB->cell.id4))
                    ret = true;
                break;
            case SKY_BEACON_GSM:
                if ((bA->cell.id4 == bB->cell.id4) && (bA->cell.id1 == bB->cell.id1) &&
                    (bA->cell.id2 == bB->cell.id2) && (bA->cell.id3 == bB->cell.id3))
                    ret = true;
                break;
            case SKY_BEACON_LTE:
            case SKY_BEACON_NBIOT:
            case SKY_BEACON_UMTS:
            case SKY_BEACON_NR:
                if ((bA->cell.id1 == bB->cell.id1) && (bA->cell.id2 == bB->cell.id2) &&
                    (bA->cell.id4 == bB->cell.id4))
                    ret = true;
                break;
            default:
                ret = false;
            }
        } else {
            /* if the cell types are different, there is no match */
            ret = false;
        }
    }

    /* if the beacons can be compared and are not a match, determine which is better */
    if (!ret) {
        if (bA->h.type == SKY_BEACON_AP) {
            /* Compare APs by rssi */
            better = EFFECTIVE_RSSI(bA->h.rssi) - EFFECTIVE_RSSI(bB->h.rssi);
#if VERBOSE_DEBUG
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "WiFi rssi score %d (%s)", better,
                better < 0 ? "B is better" : "A is better");
#endif
        } else {
#if VERBOSE_DEBUG
            dump_beacon(ctx, "A: ", bA, __FILE__, __FUNCTION__);
            dump_beacon(ctx, "B: ", bB, __FILE__, __FUNCTION__);
#endif
            /* cell comparison is type, connected, or youngest, or type or stongest */
            if (bA->h.connected || bB->h.connected) {
                better = (bA->h.connected ? 1 : -1);
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell connected score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (bA->h.age != bB->h.age) {
                /* youngest is best */
                better = -(bA->h.age - bB->h.age);
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell age score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (bA->h.type != bB->h.type) {
                better = -(bA->h.type - bB->h.type);
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell type score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else if (EFFECTIVE_RSSI(bA->h.rssi) != EFFECTIVE_RSSI(bB->h.rssi)) {
                better = (EFFECTIVE_RSSI(bA->h.rssi) - EFFECTIVE_RSSI(bB->h.rssi));
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell signal strength score %d (%s)", better,
                    better < 0 ? "B is better" : "A is better");
#endif
            } else {
                better = 1;
#if VERBOSE_DEBUG
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cell similar, pick one (%s)", better,
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
static bool beacon_in_cache(Sky_ctx_t *ctx, Beacon_t *b, Sky_cacheline_t *cl, int *index)
{
    int j;
    bool ret = false;

    if (!cl || !b || !ctx) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return false;
    }

    if (cl->time == 0) {
        return false;
    }

    for (j = 0; ret == false && j < NUM_BEACONS(cl); j++)
        if (b->h.type == cl->beacon[j].h.type) {
            switch (b->h.type) {
            case SKY_BEACON_AP:
                if (ap_beacon_in_vg(ctx, b, &cl->beacon[j]))
                    ret = true;
                break;
            case SKY_BEACON_BLE:
                if ((memcmp(b->ble.mac, cl->beacon[j].ble.mac, MAC_SIZE) == 0) &&
                    (b->ble.major == cl->beacon[j].ble.major) &&
                    (b->ble.minor == cl->beacon[j].ble.minor) &&
                    (memcmp(b->ble.uuid, cl->beacon[j].ble.uuid, 16) == 0))
                    ret = true;
                break;
            case SKY_BEACON_CDMA:
                if ((b->cell.id2 == cl->beacon[j].cell.id2) &&
                    (b->cell.id3 == cl->beacon[j].cell.id3) &&
                    (b->cell.id4 == cl->beacon[j].cell.id4))
                    ret = true;
                break;
            case SKY_BEACON_GSM:
                if ((b->cell.id4 == cl->beacon[j].cell.id4) &&
                    (b->cell.id1 == cl->beacon[j].cell.id1) &&
                    (b->cell.id2 == cl->beacon[j].cell.id2) &&
                    (b->cell.id3 == cl->beacon[j].cell.id3))
                    ret = true;
                break;
            case SKY_BEACON_LTE:
            case SKY_BEACON_NBIOT:
            case SKY_BEACON_UMTS:
            case SKY_BEACON_NR:
                if ((b->cell.id1 == cl->beacon[j].cell.id1) &&
                    (b->cell.id2 == cl->beacon[j].cell.id2) &&
                    (b->cell.id4 == cl->beacon[j].cell.id4))
                    ret = true;
                break;
            default:
                ret = false;
            }
            if (ret == true)
                break; // for loop without incrementing j
        }
    if (index)
        *index = (ret == true) ? j : -1;
    return ret;
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
            num_aps_cached += ap_beacon_in_vg(ctx, &ctx->beacon[j], &cl->beacon[i]);
        }
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(
        ctx, SKY_LOG_LEVEL_DEBUG, "%d APs in cache %d", num_aps_cached, cl - ctx->cache->cacheline);
#endif
    return num_aps_cached;
}

/*! \brief count number of APs in workspace
 *
 *  @param ctx Skyhook request context
 *
 *  @return number of APs or -1 for fatal error
 */
static int count_aps_in_workspace(Sky_ctx_t *ctx)
{
    int num_aps = 0;
    int j;
    if (!ctx)
        return -1;
    for (j = 0; j < ctx->ap_len; j++) {
        num_aps += (ctx->beacon[j].ap.vg_len + 1);
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d APs", num_aps);
#endif
    return num_aps;
}

/*! \brief count number of APs in cacheline
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in, otherwise count in workspace
 *
 *  @return number of used APs or -1 for fatal error
 */
static int count_aps_in_cacheline(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int num_aps_in_cacheline = 0;
    int j;

    if (!ctx)
        return -1;
    for (j = 0; j < cl->ap_len; j++) {
        num_aps_in_cacheline += (cl->beacon[j].ap.vg_len + 1);
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d APs in cache %d", num_aps_in_cacheline,
        cl - ctx->cache->cacheline);
#endif
    return num_aps_in_cacheline;
}

/*! \brief count number of used APs in workspace relative to a cacheline
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in
 *
 *  @return number of used APs or -1 for fatal error
 */
static int count_used_aps_in_workspace(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int num_aps_used = 0;
    int j, i, match;

    if (!ctx || !cl)
        return -1;
    for (j = 0; j < ctx->ap_len; j++) {
        for (i = 0; i < cl->ap_len; i++) {
            if ((match = ap_beacon_in_vg(ctx, &ctx->beacon[j], &cl->beacon[i])) &&
                cl->beacon[i].ap.property.used)
                num_aps_used += match;
        }
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d used APs in workspace", num_aps_used);
#endif
    return num_aps_used;
}

/*! \brief count number of APs in cacheline
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in, otherwise count in workspace
 *
 *  @return number of used APs or -1 for fatal error
 */
static int count_used_aps_in_cacheline(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int num_aps_used = 0;
    int j;
    if (!ctx || !cl)
        return -1;
    for (j = 0; j < cl->ap_len; j++) {
        if (cl->beacon[j].ap.property.used) {
            num_aps_used += (cl->beacon[j].ap.vg_len + 1);
        }
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d used APs in cache", num_aps_used);
#endif
    return num_aps_used;
}

/*! \brief test cells in workspace for match to cacheline
 *
 *  True if either all cells in workspace are present in cache or
 *  if serving cell matches cache or
 *  if there are no cell in workspace
 *
 *  @param ctx Skyhook request context
 *  @param cl the cacheline to count in
 *
 *  @return true or false
 */
static int test_cells_in_cacheline(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
{
    int num_cells = 0;
    int serving_cell = 0;
    int j;
    if (!ctx || !cl)
        return -1;
    /* for each cell in workspace, compare with cacheline */
    for (j = NUM_APS(ctx); j < NUM_BEACONS(ctx); j++) {
        if (beacon_in_cache(ctx, &ctx->beacon[j], cl, NULL)) {
            num_cells++;
            serving_cell = serving_cell || (ctx->connected == j);
        }
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%.1f%% cells in cache, serving cell: %s",
        ((float)num_cells / (NUM_BEACONS(ctx) - NUM_APS(ctx))) * 100.0,
        serving_cell ? "true" : "false");
#endif
    return num_cells;
}

/*! \brief find cache entry with a match to workspace
 *
 *   Expire any old cachelines
 *   Compare each cacheline with the workspace beacons:
 *    . If workspace has enough Used APs, compare them with low threshold
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
  *
  *  @return index of best match or empty cacheline or -1
  */
int find_best_match(Sky_ctx_t *ctx)
{
    int i; /* i iterates through cacheline */
    int err; /* err breaks the seach due to bad value */
    float ratio; /* 0.0 <= ratio <= 1.0 is the degree to which workspace matches cacheline
                    In typical case this is the intersection(workspace, cache) / union(workspace, cache) */
    float bestratio = -1.0;
    float bestputratio = -1.0;
    int score; /* score is number of APs found in cacheline */
    int threshold; /* the threshold determined that ratio should meet */
    int num_aps_used = 0;
    int bestc = -1, bestput = -1;
    int bestthresh = 0;
    Sky_cacheline_t *cl;

    DUMP_WORKSPACE(ctx);
    DUMP_CACHE(ctx);

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

    /* score each cache line wrt beacon match ratio */
    for (i = 0, err = false; i < CACHE_SIZE; i++) {
        cl = &ctx->cache->cacheline[i];
        threshold = ratio = score = 0;
        if (cl->time == 0) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score 0 for empty cacheline", i);
            continue;
        } else {
            /* count number of used APs */
            if ((num_aps_used = count_used_aps_in_workspace(ctx, cl)) < 0) {
                err = true;
                break;
            } else if (num_aps_used) {
                /* there are some significant APs */
                if (num_aps_used < CONFIG(ctx->cache, cache_beacon_threshold)) {
                    /* if there are only a few significant APs, Score based on ALL APs */
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on ALL APs", i);

                    if (!(score = test_cells_in_cacheline(ctx, cl))) {
                        threshold = CONFIG(ctx->cache, cache_match_all_threshold);
                        ratio = 0.0;
                        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                            "Cache: %d: score %d vs %d - cell mismatch", i, (int)round(ratio * 100),
                            score, threshold);
                    } else {
                        if ((score = count_cached_aps_in_workspace(ctx, cl)) < 0) {
                            threshold = CONFIG(ctx->cache, cache_match_all_threshold);
                            err = true;
                            break;
                        }
                        int unionAB = count_aps_in_workspace(ctx) +
                                      count_aps_in_cacheline(ctx, cl) -
                                      count_cached_aps_in_workspace(ctx, cl);
                        threshold = CONFIG(ctx->cache, cache_match_all_threshold);
                        ratio = (float)score / unionAB;
                        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: score %d (%d/%d) vs %d", i,
                            (int)round(ratio * 100), score, unionAB, threshold);
                    }
                } else {
                    /* there are are enough significant APs, Score based on just Used APs */
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on just Used APs", i);

                    /* if 100% cell match or a matching cell is the serving cell */
                    if ((score = test_cells_in_cacheline(ctx, cl)) !=
                        (NUM_BEACONS(ctx) - NUM_APS(ctx))) {
                        threshold = CONFIG(ctx->cache, cache_match_all_threshold);
                        ratio = 0.0;
                        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                            "Cache: %d: score %d vs %d - cell mismatch", i, (int)round(ratio * 100),
                            score, threshold);
                    } else {
                        int unionAB = count_used_aps_in_cacheline(ctx, cl);
                        if (unionAB < 0) {
                            err = true;
                            break;
                        }
                        ratio = (float)num_aps_used / unionAB;
                        threshold = CONFIG(ctx->cache, cache_match_used_threshold);
                        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache: %d: score %d (%d/%d) vs %d", i,
                            (int)round(ratio * 100), num_aps_used, unionAB, threshold);
                    }
                }
            }
        }

        if (ratio > bestputratio) {
            bestput = i;
            bestputratio = ratio;
        }
        if (ratio > bestratio) {
            if (bestratio > 0.0)
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                    "Found better match in cache %d of 0..%d score %d (vs %d)", i, CACHE_SIZE - 1,
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
        return -1;
    }

    /* make a note of the best match used by add_to_cache */
    ctx->save_to = bestput;

    if (bestratio * 100 > bestthresh) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
            "location in cache, pick cache %d of 0..%d score %d (vs %d)", bestc, CACHE_SIZE - 1,
            (int)round(bestratio * 100), bestthresh);
        return bestc;
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache match failed. Cache %d, best score %d (vs %d)", bestc,
        (int)round(bestratio * 100), bestthresh);
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Best cacheline to save location: %d of 0..%d score %d",
        bestput, CACHE_SIZE - 1, (int)round(bestputratio * 100));
    return -1;
}

/*! \brief find cache entry with oldest entry
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

    /* Find best match in cache */
    /*    yes - add entry here */
    /* else find oldest cache entry */
    /*    yes - add entryu here */
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

    for (j = 0; j < CONFIG(ctx->cache, total_beacons); j++)
        cl->beacon[j] = ctx->beacon[j];
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
    return find_best_match(ctx);
}
