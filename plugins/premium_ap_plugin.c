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
#define VERBOSE_DEBUG 1

#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define EFFECTIVE_RSSI(b) ((b) == -1 ? (-127) : (b))

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
    if (!dup && vg_p == CONFIG(ctx->cache, max_vap_per_ap)) { /* No room for one more */
        remove_beacon(ctx, vg); /* remove parent before re-inserting */
        return false;
    }

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

    /* Add child unless it is already a member in the parent group */
    if (!dup) {
        pvg[vg_p + VAP_FIRST_DATA].data = patch.data;
        pvg[VAP_LENGTH].len = vg_p + VAP_FIRST_DATA;
        /* update cache status of child in group */
        parent->ap.vg_prop[vg_p] = child->ap.property;
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
                break;
            }
            pvg[vg_p + VAP_FIRST_DATA].data = child->ap.vg[vg_c + VAP_FIRST_DATA].data;
            pvg[VAP_LENGTH].len = vg_p + VAP_FIRST_DATA;
            /* update cache status of child in group */
            parent->ap.vg_prop[vg_p] = child->ap.property;
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
static int ap_beacon_in_vg(Sky_ctx_t *ctx, Beacon_t *va, Beacon_t *vb, Sky_beacon_property_t *prop)
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
                    "cmp MAC %02X:%02X:%02X:%02X:%02X:%02X %s with %02X:%02X:%02X:%02X:%02X:%02X %s, match %d %s",
                    mac_va[0], mac_va[1], mac_va[2], mac_va[3], mac_va[4], mac_va[5],
                    w == -1 ? "AP " : "VAP", /* Parent or child */
                    mac_vb[0], mac_vb[1], mac_vb[2], mac_vb[3], mac_vb[4], mac_vb[5],
                    c == -1 ? "AP " : "VAP", /* Parent or child */
                    num_aps, p.used ? "Used" : "Unused");
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

/*! \brief compare beacons fpr equality
 *
 *  if beacons are equivalent, return SKY_SUCCESS otherwise SKY_FAILURE
 *  if an error occurs during comparison. return SKY_ERROR
 */
static Sky_status_t beacon_equal(Sky_ctx_t *ctx, ...)
{
    va_list argp;
    Beacon_t *a;
    Beacon_t *b;
    Sky_beacon_property_t *prop;

    va_start(argp, ctx);
    a = va_arg(argp, Beacon_t *);
    b = va_arg(argp, Beacon_t *);
    prop = va_arg(argp, Sky_beacon_property_t *);

    if (!ctx || !a || !b) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad params");
        return SKY_ERROR;
    }

    /* Two APs can be compared but others are ordered by type */
    if (a->h.type != SKY_BEACON_AP || b->h.type != SKY_BEACON_AP)
        return SKY_ERROR;

    /* test two cells or two APs for equivalence */
    switch (a->h.type) {
    case SKY_BEACON_AP:
#if VERBOSE_DEBUG
        dump_beacon(ctx, "AP a:", a, __FILE__, __FUNCTION__);
        dump_beacon(ctx, "AP b:", b, __FILE__, __FUNCTION__);
#endif
        if (ap_beacon_in_vg(ctx, a, b, prop) > 0) // copy properies from b if equivalent
            return SKY_SUCCESS;
        break;
    default:
        break;
    }
    return SKY_FAILURE;
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
        if (beacon_equal(ctx, b, &cl->beacon[j], prop) == 1)
            return true;
    return false;
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
            num_aps_cached += ap_beacon_in_vg(ctx, &ctx->beacon[j], &cl->beacon[i], NULL);
        }
    }
#ifdef VERBOSE_DEBUG
    LOGFMT(
        ctx, SKY_LOG_LEVEL_DEBUG, "%d APs in cache %d", num_aps_cached, cl - ctx->cache->cacheline);
#endif
    return num_aps_cached;
}

/*! \brief count number of APs in workspace including compressed vap
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

/*! \brief count number of APs in cacheline including vap
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
            if ((match = ap_beacon_in_vg(ctx, &ctx->beacon[j], &cl->beacon[i], NULL)) &&
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
static int cell_changed(Sky_ctx_t *ctx, Sky_cacheline_t *cl)
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

    /* for each cell in workspace, compare with cacheline */
    for (j = NUM_APS(ctx); j < NUM_BEACONS(ctx); j++) {
        if (ctx->beacon[j].h.connected && beacon_in_cache(ctx, &ctx->beacon[j], cl, NULL)) {
#ifdef VERBOSE_DEBUG
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "serving cells match");
#endif
            return false;
        }
    }
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d - cell mismatch", cl - ctx->cache->cacheline);
    return true;
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
static Sky_status_t beacon_score(Sky_ctx_t *ctx, ...)
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
        if (cl->time == 0 || cell_changed(ctx, cl) == true) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "Cache: %d: Score 0 for empty cacheline or cell change", i);
            continue;
        } else {
            /* count number of matching APs in workspace and cache */
            if ((num_aps_cached = count_cached_aps_in_workspace(ctx, cl)) < 0) {
                err = true;
                break;
            } else if ((num_aps_used = count_used_aps_in_workspace(ctx, cl)) < 0) {
                /* count number of used APs in workspace */
                err = true;
                break;
            } else if (num_aps_cached) {
                /* there are some significant APs */
                if (num_aps_used < CONFIG(ctx->cache, cache_beacon_threshold)) {
                    /* if there are only a few significant APs, Score based on ALL APs */
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on ALL APs", i);
                    score = num_aps_cached;
                    int unionAB = count_aps_in_workspace(ctx) + count_aps_in_cacheline(ctx, cl) -
                                  num_aps_cached;
                    threshold = CONFIG(ctx->cache, cache_match_all_threshold);
                    ratio = (float)score / unionAB;
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: score %d (%d/%d) vs %d", i,
                        (int)round(ratio * 100), score, unionAB, threshold);
                } else {
                    /* there are are enough significant APs, Score based on just Used APs */
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on just Used APs", i);

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
            } else {
                /* Compare cell beacons because there are no APs */
                /* count number of matching cells */
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Cache: %d: Score based on cell beacons", i);
                threshold = CONFIG(ctx->cache, cache_match_used_threshold);
                score = 0.0;
                for (int j = NUM_APS(ctx) - 1; j < NUM_BEACONS(ctx); j++) {
                    if (beacon_in_cache(ctx, &ctx->beacon[j], &ctx->cache->cacheline[i], NULL)) {
#if VERBOSE_DEBUG
                        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                            "Cell Beacon %d type %s matches cache %d of 0..%d Score %d", j,
                            sky_pbeacon(&ctx->beacon[j]), i, CACHE_SIZE, (int)score);
#endif
                        score = score + 1.0;
                    }
                }
                /* cell score = number of matching cells / union( cells in workspace + cache) */
                ratio = (float)score / ((NUM_BEACONS(ctx) - NUM_APS(ctx)) +
                                           (NUM_BEACONS(&ctx->cache->cacheline[i]) -
                                               NUM_APS(&ctx->cache->cacheline[i])) -
                                           score);
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache: %d: score %d (%d/%d) vs %d", i,
                    (int)round(ratio * 100), score,
                    (NUM_BEACONS(ctx) - NUM_APS(ctx)) +
                        (NUM_BEACONS(&ctx->cache->cacheline[i]) -
                            NUM_APS(&ctx->cache->cacheline[i])) -
                        score,
                    threshold);
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
static Sky_status_t beacon_to_cache(Sky_ctx_t *ctx, ...)
{
    va_list argp;
    Sky_location_t *loc;

    va_start(argp, ctx);
    loc = va_arg(argp, Sky_location_t *);

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

/*! \brief return name of plugin
 *
 *  @param ctx Skyhook request context
 *  @param s the location where the plugin name is to be stored
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
static Sky_status_t plugin_name(Sky_ctx_t *ctx, ...)
{
    va_list argp;
    char **s;
    char *r, *p = __FILE__;

    va_start(argp, ctx);
    s = va_arg(argp, char **);

    if (s == NULL)
        return SKY_ERROR;

    r = strrchr(p, '/');
    if (r == NULL)
        *s = p;
    else
        *s = r;
    return SKY_SUCCESS;
}

static Sky_status_t beacon_remove_worst(Sky_ctx_t *ctx, ...)
{
    /* beacon is AP and is subject to filtering */
    /* discard virtual duplicates of remove one based on rssi distribution */
    if (compress_virtual_ap(ctx) == SKY_ERROR) {
#if VERBOSE_DEBUG
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "failed to compress AP");
#endif
        if (select_ap_by_rssi(ctx) == SKY_ERROR) {
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "failed to filter AP");
            return SKY_ERROR;
        }
    }
    return SKY_SUCCESS;
}

Sky_plugin_op_t SKY_PLUGIN_TABLE(premium_ap_plugin)[SKY_OP_MAX] = {
    [SKY_OP_NEXT] = NULL, /* Pointer to next plugin table */
    [SKY_OP_NAME] = plugin_name,
    /* Entry points */
    [SKY_OP_EQUAL] = beacon_equal, /* Conpare two beacons for duplicate and which is better */
    [SKY_OP_REMOVE_WORST] =
        beacon_remove_worst, /* Conpare two beacons for duplicate and which is better */
    [SKY_OP_SCORE_CACHELINE] = beacon_score, /* Score the match between workspace and a cache line */
    [SKY_OP_ADD_TO_CACHE] = beacon_to_cache /* copy workspace beacons to a cacheline */
};
