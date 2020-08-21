/*! \file libel/utilities.c
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
#include <stdarg.h>
#include <stdlib.h>
#define SKY_LIBEL 1
#include "libel.h"
#include "proto.h"

#define MIN(a, b) ((a < b) ? a : b)

#if SKY_DEBUG
static char *cell_params[][6] = { [SKY_BEACON_AP] = { "n/a", "n/a", "n/a", "n/a", "n/a", "n/a" },
    [SKY_BEACON_BLE] = { "n/a", "n/a", "n/a", "n/a", "n/a", "n/a" },
    [SKY_BEACON_NBIOT] = { "mcc", "mnc", "tac", "cellid", "ncid", "earfcn" },
    [SKY_BEACON_LTE] = { "mcc", "mnc", "tac", "eucid", "pci", "earfcn" },
    [SKY_BEACON_NR] = { "mcc", "mnc", "tac", "nci", "pci", "nrarfcn" },
    [SKY_BEACON_UMTS] = { "mcc", "mnc", "lac", "uci", "psc", "uarfcn" },
    [SKY_BEACON_CDMA] = { "n/a", "sid", "nid", "bsid", "pnc", "freq" },
    [SKY_BEACON_GSM] = { "mcc", "mnc", "lac", "ci", "bsic", "arfcn" } };
#endif

/*! \brief set sky_errno and return Sky_status
 *
 *  @param sky_errno sky_errno is the error code
 *  @param code the sky_errno_t code to return
 *
 *  @return Sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_return(Sky_errno_t *sky_errno, Sky_errno_t code)
{
    if (sky_errno != NULL)
        *sky_errno = code;
    return (code == SKY_ERROR_NONE) ? SKY_SUCCESS : SKY_ERROR;
}

/*! \brief validate the workspace buffer
 *
 *  @param ctx workspace buffer
 *
 *  @return true if workspace is valid, else false
 */
int validate_workspace(Sky_ctx_t *ctx)
{
    int i;

    if (ctx == NULL) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "NULL ctx");
        return false;
    }
    if (ctx->len > TOTAL_BEACONS) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Too many beacons");
        return false;
    }
    if (ctx->connected > TOTAL_BEACONS) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad connected value");
        return false;
    }
    if (ctx->header.crc32 == sky_crc32(&ctx->header.magic,
                                 (uint8_t *)&ctx->header.crc32 - (uint8_t *)&ctx->header.magic)) {
        for (i = 0; i < TOTAL_BEACONS; i++) {
            if (ctx->beacon[i].h.magic != BEACON_MAGIC || ctx->beacon[i].h.type > SKY_BEACON_MAX) {
                LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad beacon #%d of %d", i, TOTAL_BEACONS);
                return false;
            }
        }
    } else {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "CRC check failed");
        return false;
    }
    return true;
}

/*! \brief validate the cache buffer - Cant use LOGFMT here
 *
 *  @param c pointer to cache buffer
 *
 *  @return true if cache is valid, else false
 */
int validate_cache(Sky_cache_t *c, Sky_loggerfn_t logf)
{
    int i, j;

    if (c == NULL) {
#if SKY_DEBUG
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Cache validation failed: NULL pointer");
#endif
        return false;
    }

    if (c->len != CACHE_SIZE) {
#if SKY_DEBUG
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Cache validation failed: too big for CACHE_SIZE");
#endif
        return false;
    }
    if (c->newest >= CACHE_SIZE) {
#if SKY_DEBUG
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Cache validation failed: newest too big for CACHE_SIZE");
#endif
        return false;
    }

    if (c->header.magic != SKY_MAGIC) {
#if SKY_DEBUG
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Cache validation failed: bad magic in header");
#endif
        return false;
    }
    if (c->header.crc32 ==
        sky_crc32(&c->header.magic, (uint8_t *)&c->header.crc32 - (uint8_t *)&c->header.magic)) {
        for (i = 0; i < CACHE_SIZE; i++) {
            if (c->cacheline[i].len > TOTAL_BEACONS) {
#if SKY_DEBUG
                if (logf != NULL)
                    (*logf)(SKY_LOG_LEVEL_ERROR,
                        "Cache validation failed: too many beacons for TOTAL_BEACONS");
#endif
                return false;
            }

            for (j = 0; j < TOTAL_BEACONS; j++) {
                if (c->cacheline[i].beacon[j].h.magic != BEACON_MAGIC) {
#if SKY_DEBUG
                    if (logf != NULL)
                        (*logf)(SKY_LOG_LEVEL_ERROR, "Cache validation failed: Bad beacon info");
#endif
                    return false;
                }
                if (c->cacheline[i].beacon[j].h.type > SKY_BEACON_MAX) {
#if SKY_DEBUG
                    if (logf != NULL)
                        (*logf)(SKY_LOG_LEVEL_ERROR, "Cache validation failed: Bad beacon type");
#endif
                    return false;
                }
            }
        }
    } else {
#if SKY_DEBUG
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Cache validation failed: crc mismatch!");
#endif
        return false;
    }
    return true;
}

/*! \brief validate mac address
 *
 *  @param mac pointer to mac address
 *  @param ctx pointer to context
 *
 *  @return true if mac address not all zeros or ones
 */
int validate_mac(uint8_t mac[6], Sky_ctx_t *ctx)
{
    if (mac[0] == 0 || mac[0] == 0xff) {
        if (mac[0] == mac[1] && mac[0] == mac[2] && mac[0] == mac[3] && mac[0] == mac[4] &&
            mac[0] == mac[5]) {
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Invalid mac address");
            return false;
        }
    }

    return true;
}

#if SKY_DEBUG
/*! \brief basename return pointer to the basename of path or path
 *
 *  @param path pathname of file
 *
 *  @return pointer to basename or whole path
 */
const char *sky_basename(const char *path)
{
    const char *p = strrchr(path, '/');

    if (p == NULL)
        return path;
    else
        return p + 1;
}

/*! \brief formatted logging to user provided function
 *
 *  @param ctx workspace buffer
 *  @param level the log level of this msg
 *  @param fmt the msg
 *  @param ... variable arguments
 *
 *  @return 0 for success
 */

int logfmt(
    const char *file, const char *function, Sky_ctx_t *ctx, Sky_log_level_t level, char *fmt, ...)
{
    va_list ap;
    char buf[SKY_LOG_LENGTH];
    int ret, n;
    if (level > ctx->min_level || function == NULL)
        return -1;
    memset(buf, '\0', sizeof(buf));
    // Print log-line prefix ("<source file>:<function name>")
    n = snprintf(buf, sizeof(buf), "%.20s:%.20s() ", sky_basename(file), function);

    va_start(ap, fmt);
    ret = vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    (*ctx->logf)(level, buf);
    va_end(ap);
    return ret;
}
#endif

/*! \brief dump maximum number of bytes of the given buffer in hex on one line
 *
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *  @param ctx workspace buffer
 *  @param level the log level of this msg
 *  @param buffer where to start dumping the next line
 *  @param bufsize remaining size of the buffer in bytes
 *  @param buf_offset byte index of progress through the current buffer
 *
 *  @returns number of bytes dumped, or negitive number on error
 */
int dump_hex16(const char *file, const char *function, Sky_ctx_t *ctx, Sky_log_level_t level,
    void *buffer, uint32_t bufsize, int buf_offset)
{
    int pb = 0;
#if SKY_DEBUG
    char buf[SKY_LOG_LENGTH];
    uint8_t *b = (uint8_t *)buffer;
    int n, N;
    if (level > ctx->min_level || function == NULL || buffer == NULL || bufsize <= 0)
        return -1;
    memset(buf, '\0', sizeof(buf));
    // Print log-line prefix ("<source file>:<function name> <buf offset>:")
    n = snprintf(buf, sizeof(buf), "%.20s:%.20s() %07X:", sky_basename(file), function, buf_offset);

    // Calculate number of characters required to print 16 bytes
    N = n + (16 * 3); /* 16 bytes per line, 3 bytes per byte (' XX') */
    // if width of log line (SKY_LOG_LENGTH) too short 16 bytes, just print those that fit
    for (pb = 0; n < MIN(SKY_LOG_LENGTH - 4, N);) {
        if (pb < bufsize)
            n += sprintf(&buf[n], " %02X", b[pb++]);
        else
            break;
    }
    (*ctx->logf)(level, buf);
#endif
    return pb;
}

/*! \brief dump all bytes of the given buffer in hex
 *
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *  @param ctx workspace pointer
 *  @param buf pointer to the buffer
 *  @param bufsize size of the buffer in bytes
 *
 *  @returns number of bytes dumped
 */
int log_buffer(const char *file, const char *function, Sky_ctx_t *ctx, Sky_log_level_t level,
    void *buffer, uint32_t bufsize)
{
    int buf_offset = 0;
#if SKY_DEBUG
    int i, n = bufsize;
    uint8_t *p = buffer;
    /* try to print 16 bytes per line till all dumped */
    while ((i = dump_hex16(
                file, function, ctx, level, (void *)(p + (bufsize - n)), n, buf_offset)) > 0) {
        n -= i;
        buf_offset += i;
    }
#endif
    return buf_offset;
}

/*! \brief dump Virtual APs in group (children not parent)
 *
 *  @param ctx workspace pointer
 *  @param b parent of Virtual Group AP
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns void
 */
void dump_vap(Sky_ctx_t *ctx, char *prefix, Beacon_t *b, const char *file, const char *func)
{
#if SKY_DEBUG
    int j, n, value;
    Vap_t *vap = b->ap.vg;
    uint8_t mac[MAC_SIZE];

    if (!b->ap.vg_len)
        return;

    for (j = 0; j < b->ap.vg_len; j++) {
        memcpy(mac, b->ap.mac, MAC_SIZE);
        n = vap[j + 2].data.nibble_idx;
        value = vap[j + 2].data.value;
        if (n & 1)
            mac[n / 2] = ((mac[n / 2] & 0xF0) | value);
        else
            mac[n / 2] = ((mac[n / 2] & 0x0F) | (value << 4));

        logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG,
            "%s Age: %d %s MAC %02X:%02X:%02X:%02X:%02X:%02X rssi: %-4d %-4d MHz VAP(%d 0x%01X)",
            prefix, b->h.age, " ^^^^ ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], b->h.rssi,
            b->ap.freq, n, value);
    }
#endif
}

/*! \brief dump AP including any VAP
 *
 *  @param ctx workspace pointer
 *  @param str comment
 *  @param b pointer to Beacon_t structure
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns void
 */
void dump_ap(Sky_ctx_t *ctx, char *prefix, Beacon_t *b, const char *file, const char *func)
{
#if SKY_DEBUG
    int cached = 0;

    if (!b || b->h.type != SKY_BEACON_AP) {
        logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG, "%s can't dump non-AP beacon");
        return;
    }

    if (prefix == NULL)
        prefix = "AP:";

    logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG,
        "%s Age: %d %s MAC %02X:%02X:%02X:%02X:%02X:%02X rssi: %-4d %-4d MHz vap: %d", prefix,
        b->h.age,
        (cached || b->ap.property.in_cache) ? (b->ap.property.used ? "Used  " : "Unused") :
                                              "      ",
        b->ap.mac[0], b->ap.mac[1], b->ap.mac[2], b->ap.mac[3], b->ap.mac[4], b->ap.mac[5],
        b->h.rssi, b->ap.freq, b->ap.vg_len);
    dump_vap(ctx, prefix, b, file, func);
#endif
}

/*! \brief dump a beacon
 *
 *  @param ctx workspace pointer
 *  @param b the beacon to dump
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns 0 for success or negative number for error
 */
void dump_beacon(Sky_ctx_t *ctx, char *str, Beacon_t *b, const char *file, const char *func)
{
#if SKY_DEBUG
    char prefixstr[50] = { '\0' };
    int idx_b, idx_c;

    /* Test whether beacon is in cache or workspace */
    if (b >= ctx->beacon && b < ctx->beacon + TOTAL_BEACONS + 1) {
        idx_b = b - ctx->beacon;
        idx_c = 0;
        snprintf(prefixstr, sizeof(prefixstr), "%s     %-2d:%s %6s", str, idx_b,
            b->h.connected ? "*" : " ", sky_pbeacon(b));
    } else if (ctx->cache && b >= ctx->cache->cacheline[0].beacon &&
               b < ctx->cache->cacheline[CACHE_SIZE - 1].beacon +
                       ctx->cache->cacheline[CACHE_SIZE - 1].len) {
        idx_b = b - ctx->cache->cacheline[0].beacon;
        idx_c = idx_b / TOTAL_BEACONS;
        idx_b %= TOTAL_BEACONS;
        snprintf(prefixstr, sizeof(prefixstr), "%s %2d:%-2d:%s %6s", str, idx_c, idx_b,
            b->h.connected ? "*" : " ", sky_pbeacon(b));
    } else {
        idx_b = idx_c = 0;
        snprintf(prefixstr, sizeof(prefixstr), "%s     ? :%s %6s", str, b->h.connected ? "*" : " ",
            sky_pbeacon(b));
    }

    switch (b->h.type) {
    case SKY_BEACON_AP:
        strcat(prefixstr, "    ");
        dump_ap(ctx, prefixstr, b, file, func);
        break;
    case SKY_BEACON_GSM:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_LTE:
    case SKY_BEACON_CDMA:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_NR:
        /* if primary key is UNKNOWN, must be NMR */
        if (b->cell.id2 == SKY_UNKNOWN_ID2) {
            strcat(prefixstr, "-NMR");
            logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG, "%s Age: %d %s: %u, %s: %u, rssi: %d",
                prefixstr, b->h.age, cell_params[b->h.type][4], b->cell.id5,
                cell_params[b->h.type][5], b->cell.freq, b->h.rssi);
        } else {
            strcat(prefixstr, "    ");
            logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG,
                "%s Age: %d %s: %u, %s: %u, %s: %u, %s: %llu, %s: %u, %s: %u, rssi: %d", prefixstr,
                b->h.age, cell_params[b->h.type][0], b->cell.id1, cell_params[b->h.type][1],
                b->cell.id2, cell_params[b->h.type][2], b->cell.id3, cell_params[b->h.type][3],
                b->cell.id4, cell_params[b->h.type][4], b->cell.id5, cell_params[b->h.type][5],
                b->cell.freq, b->h.rssi);
        }
        break;
    default:
        logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG, "Beacon %s: Type: Unknown", prefixstr);
        break;
    }
#endif
}

/*! \brief dump the beacons in the workspace
 *
 *  @param ctx workspace pointer
 *
 *  @returns 0 for success or negative number for error
 */
void dump_workspace(Sky_ctx_t *ctx, const char *file, const char *func)
{
#if SKY_DEBUG
    int i;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "WorkSpace: Got %d beacons, WiFi %d, connected %d", ctx->len,
        ctx->ap_len, ctx->connected);
    for (i = 0; i < ctx->len; i++)
        dump_beacon(ctx, "workspace", &ctx->beacon[i], file, func);

    if (CONFIG(ctx->cache, last_config_time) == 0) {
        logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG,
            "Config: Beacons:%d AP:%d VAP:%d(%d) Thresholds:%d & %d(Match) %d(Age) %d(Beacon) %d(RSSI) Update:Pending",
            CONFIG(ctx->cache, total_beacons), CONFIG(ctx->cache, max_ap_beacons),
            CONFIG(ctx->cache, max_vap_per_ap), CONFIG(ctx->cache, max_vap_per_rq),
            CONFIG(ctx->cache, cache_match_used_threshold),
            CONFIG(ctx->cache, cache_match_all_threshold), CONFIG(ctx->cache, cache_age_threshold),
            CONFIG(ctx->cache, cache_beacon_threshold),
            -CONFIG(ctx->cache, cache_neg_rssi_threshold),
            ctx->header.time - CONFIG(ctx->cache, last_config_time));
    } else {
        logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG,
            "Config: Beacons:%d AP:%d VAP:%d(%d) Thresholds:%d & %d(Match) %d(Age) %d(Beacon) %d(RSSI) Update:%d Sec",
            CONFIG(ctx->cache, total_beacons), CONFIG(ctx->cache, max_ap_beacons),
            CONFIG(ctx->cache, max_vap_per_ap), CONFIG(ctx->cache, max_vap_per_rq),
            CONFIG(ctx->cache, cache_match_used_threshold),
            CONFIG(ctx->cache, cache_match_all_threshold), CONFIG(ctx->cache, cache_age_threshold),
            CONFIG(ctx->cache, cache_beacon_threshold),
            -CONFIG(ctx->cache, cache_neg_rssi_threshold),
            (int)((*ctx->gettime)(NULL)-CONFIG(ctx->cache, last_config_time)));
    }
#endif
}

/*! \brief dump the beacons in the cache
 *
 *  @param ctx workspace pointer
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns 0 for success or negative number for error
 */
void dump_cache(Sky_ctx_t *ctx, const char *file, const char *func)
{
#if SKY_DEBUG
    int i, j;
    Sky_cacheline_t *c;

    for (i = 0; i < CACHE_SIZE; i++) {
        c = &ctx->cache->cacheline[i];
        if (c->len == 0 || c->time == 0) {
            logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG,
                "cache: %d of %d - empty len:%d ap_len:%d time:%u", i, ctx->cache->len, c->len,
                c->ap_len, c->time);
        } else {
            logfmt(file, func, ctx, SKY_LOG_LEVEL_DEBUG,
                "cache: %d of %d%s GPS:%d.%06d,%d.%06d,%d  %d beacons", i, ctx->cache->len,
                ctx->cache->newest == i ? "<-newest" : "", (int)c->loc.lat,
                (int)fabs(round(1000000 * (c->loc.lat - (int)c->loc.lat))), (int)c->loc.lon,
                (int)fabs(round(1000000 * (c->loc.lon - (int)c->loc.lon))), c->loc.hpe, c->len);
            for (j = 0; j < c->len; j++) {
                dump_beacon(ctx, "cache", &c->beacon[j], file, func);
            }
        }
    }
#endif
}

/*! \brief set dynamic config parameter defaults
 *
 *  @param cache buffer
 *
 *  @return void
 */
void config_defaults(Sky_cache_t *c)
{
    if (CONFIG(c, total_beacons) == 0)
        CONFIG(c, total_beacons) = TOTAL_BEACONS;
    if (CONFIG(c, max_ap_beacons) == 0)
        CONFIG(c, max_ap_beacons) = MAX_AP_BEACONS;
    if (CONFIG(c, cache_match_used_threshold) == 0)
        CONFIG(c, cache_match_used_threshold) = CACHE_MATCH_THRESHOLD_USED;
    if (CONFIG(c, cache_match_all_threshold) == 0)
        CONFIG(c, cache_match_all_threshold) = CACHE_MATCH_THRESHOLD_ALL;
    if (CONFIG(c, cache_age_threshold) == 0)
        CONFIG(c, cache_age_threshold) = CACHE_AGE_THRESHOLD;
    if (CONFIG(c, cache_beacon_threshold) == 0)
        CONFIG(c, cache_beacon_threshold) = CACHE_BEACON_THRESHOLD;
    if (CONFIG(c, cache_neg_rssi_threshold) == 0)
        CONFIG(c, cache_neg_rssi_threshold) = CACHE_RSSI_THRESHOLD;
    if (CONFIG(c, max_vap_per_ap) == 0)
        CONFIG(c, max_vap_per_ap) = MAX_VAP_PER_AP;
    if (CONFIG(c, max_vap_per_rq) == 0)
        CONFIG(c, max_vap_per_rq) = MAX_VAP_PER_RQ;
    /* Add new config parameters here */
}

/*! \brief field extraction for dynamic use of Nanopb (ctx partner_id)
 *
 *  @param ctx workspace buffer
 *
 *  @return partner_id
 */
uint32_t get_ctx_partner_id(Sky_ctx_t *ctx)
{
    return ctx->cache->sky_partner_id;
}

/*! \brief field extraction for dynamic use of Nanopb (ctx sky_aes_key)
 *
 *  @param ctx workspace buffer
 *
 *  @return sky_aes_key
 */
uint8_t *get_ctx_aes_key(Sky_ctx_t *ctx)
{
    return ctx->cache->sky_aes_key;
}

/*! \brief field extraction for dynamic use of Nanopb (ctx sky_device_id)
 *
 *  @param ctx workspace buffer
 *
 *  @return sky_device_id
 */
uint8_t *get_ctx_device_id(Sky_ctx_t *ctx)
{
    return ctx->cache->sky_device_id;
}

/*! \brief field extraction for dynamic use of Nanopb (ctx sky_id_len)
 *
 *  @param ctx workspace buffer
 *
 *  @return sky_id_len
 */
uint32_t get_ctx_id_length(Sky_ctx_t *ctx)
{
    return ctx->cache->sky_id_len;
}

/*! \brief field extraction for dynamic use of Nanopb (ctx logf)
 *
 *  @param ctx workspace buffer
 *
 *  @return logf
 */
Sky_loggerfn_t get_ctx_logf(Sky_ctx_t *ctx)
{
    return ctx->logf;
}

/*! \brief field extraction for dynamic use of Nanopb (ctx sky_id_len)
 *
 *  @param ctx workspace buffer
 *
 *  @return sky_id_len
 */
Sky_randfn_t get_ctx_rand_bytes(Sky_ctx_t *ctx)
{
    return ctx->rand_bytes;
}

/*! \brief field extraction for dynamic use of Nanopb (count beacons)
 *
 *  @param ctx workspace buffer
 *  @param t type of beacon to count
 *
 *  @return number of beacons of the specified type
 */
int32_t get_num_beacons(Sky_ctx_t *ctx, Sky_beacon_type_t t)
{
    int i, b = 0;

    if (ctx == NULL || t > SKY_BEACON_MAX) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    if (t == SKY_BEACON_AP) {
        return ctx->ap_len;
    } else {
        for (i = ctx->ap_len, b = 0; i < ctx->len; i++) {
            if (ctx->beacon[i].h.type == t)
                b++;
            if (b && ctx->beacon[i].h.type != t)
                break; /* End of beacons of this type */
        }
    }
    return b;
}

/*! \brief Return the total number of scanned cells (serving, neighbor, or otherwise)
 *
 *  @param ctx workspace buffer
 *
 *  @return number of cells
 */
int32_t get_num_cells(Sky_ctx_t *ctx)
{
    int i, b = 0;

    if (ctx == NULL) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param")
        return 0;
    }

    for (i = ctx->ap_len, b = 0; i < ctx->len; i++) {
        if (ctx->beacon[i].h.type >= SKY_BEACON_FIRST_CELL_TYPE &&
            ctx->beacon[i].h.type <= SKY_BEACON_LAST_CELL_TYPE)
            b++;
    }

    return b;
}

/*! \brief field extraction for dynamic use of Nanopb (base of beacon type)
 *
 *  @param ctx workspace buffer
 *  @param t type of beacon to find
 *
 *  @return first beacon of the specified type
 */
int get_base_beacons(Sky_ctx_t *ctx, Sky_beacon_type_t t)
{
    int i = 0;

    if (ctx == NULL || t > SKY_BEACON_MAX) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    if (t == SKY_BEACON_AP) {
        if (ctx->beacon[0].h.type == t)
            return i;
    } else {
        for (i = ctx->ap_len; i < ctx->len; i++) {
            if (ctx->beacon[i].h.type == t)
                return i;
        }
    }
    return -1;
}

/*! \brief field extraction for dynamic use of Nanopb (num AP)
 *
 *  @param ctx workspace buffer
 *
 *  @return number of AP beacons
 */
int32_t get_num_aps(Sky_ctx_t *ctx)
{
    if (ctx == NULL) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return ctx->ap_len;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/MAC)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon mac info
 */
uint8_t *get_ap_mac(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > ctx->ap_len) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return ctx->beacon[idx].ap.mac;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/freq)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon channel info
 */
int64_t get_ap_freq(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > ctx->ap_len) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return ctx->beacon[idx].ap.freq;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/rssi)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon rssi info
 */
int64_t get_ap_rssi(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > ctx->ap_len) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return ctx->beacon[idx].h.rssi;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/is_connected)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon is_connected info
 */
bool get_ap_is_connected(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > ctx->ap_len) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return idx == ctx->connected;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/timestamp)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon timestamp info
 */
int64_t get_ap_age(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > ctx->ap_len) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return ctx->beacon[idx].h.age;
}

/*! \brief Get a cell
 *
 *  @param ctx workspace buffer
 *  @param idx index into cells
 *
 *  @return Pointer to cell
 */
Beacon_t *get_cell(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return 0;
    }

    return &ctx->beacon[ctx->ap_len + idx];
}

/*! \brief Get cell type
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell type
 */
int16_t get_cell_type(Beacon_t *cell)
{
    uint16_t map[] = { [SKY_BEACON_NR] = Cell_Type_NR,
        [SKY_BEACON_LTE] = Cell_Type_LTE,
        [SKY_BEACON_UMTS] = Cell_Type_UMTS,
        [SKY_BEACON_NBIOT] = Cell_Type_NBIOT,
        [SKY_BEACON_CDMA] = Cell_Type_CDMA,
        [SKY_BEACON_GSM] = Cell_Type_GSM,
        [SKY_BEACON_MAX] = Cell_Type_UNKNOWN };
    if (cell->h.type < SKY_BEACON_FIRST_CELL_TYPE || cell->h.type > SKY_BEACON_LAST_CELL_TYPE)
        return Cell_Type_UNKNOWN;
    else
        return map[cell->h.type];
}

/*! \brief Get cell id1
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell id1, -1 if not available
 */
int64_t get_cell_id1(Beacon_t *cell)
{
    uint16_t type = get_cell_type(cell);

    switch (type) {
    case SKY_BEACON_CDMA:
        return SKY_UNKNOWN_ID1; // ID1 irrelevant for CDMA.
    case SKY_BEACON_GSM:
    case SKY_BEACON_LTE:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_NR:
        return cell->cell.id1;
    }

    return 0;
}

/*! \brief Get cell id2
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell id2, -1 if not available
 */
int64_t get_cell_id2(Beacon_t *cell)
{
    return cell->cell.id2;
}

/*! \brief Get cell id3
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell id3, -1 if not available
 */
int64_t get_cell_id3(Beacon_t *cell)
{
    return cell->cell.id3;
}

/*! \brief Get cell id4
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell id4, -1 if not available
 */
int64_t get_cell_id4(Beacon_t *cell)
{
    return cell->cell.id4;
}

/*! \brief Get cell id5
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell id5, -1 if not available
 */
int64_t get_cell_id5(Beacon_t *cell)
{
    uint16_t type = get_cell_type(cell);

    switch (type) {
    case SKY_BEACON_CDMA:
        return SKY_UNKNOWN_ID5; // Reporting ID5 value not supported for CDMA
    case SKY_BEACON_GSM:
        return SKY_UNKNOWN_ID5; // Reporting ID5 value not supported for GSM
    case SKY_BEACON_LTE:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_NR:
        return cell->cell.id5;
    }

    return 0;
}

/*! \brief Get cell id6
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell id6, -1 if not available
 */
int64_t get_cell_id6(Beacon_t *cell)
{
    uint16_t type = get_cell_type(cell);

    switch (type) {
    case SKY_BEACON_CDMA:
        return SKY_UNKNOWN_ID6; // Reporting ID6 value not supported for CDMA
    case SKY_BEACON_GSM:
        return SKY_UNKNOWN_ID6; // Reporting ID6 value not supported for GSM
    case SKY_BEACON_LTE:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_NR:
        return cell->cell.freq;
    }

    return 0;
}

/*! \brief Get cell connected flag
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell connected flag
 */
bool get_cell_connected_flag(Sky_ctx_t *ctx, Beacon_t *cell)
{
    return ctx->connected >= 0 && &ctx->beacon[ctx->connected] == cell;
}

/*! \brief Return cell RSSI value
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return beacon rssi
 */
int64_t get_cell_rssi(Beacon_t *cell)
{
    return cell->h.rssi;
}

/*! \brief Return cell age value
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return beacon age
 */
int64_t get_cell_age(Beacon_t *cell)
{
    return cell->h.age;
}

/*! \brief field extraction for dynamic use of Nanopb (num gnss)
 *
 *  @param ctx workspace buffer
 *
 *  @return number of gnss
 */
int32_t get_num_gnss(Sky_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    return isnan(ctx->gps.lat) ? 0 : 1;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/lat)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon is_connected info
 */
float get_gnss_lat(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        return NAN;
    }
    return ctx->gps.lat;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/lon)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon is_connected info
 */
float get_gnss_lon(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return NAN;
    }
    return ctx->gps.lon;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/hpe)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon hpe info
 */
int64_t get_gnss_hpe(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return 0;
    }
    return ctx->gps.hpe;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/alt)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon is_connected info
 */
float get_gnss_alt(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return NAN;
    }
    return ctx->gps.alt;
}
/*! \brief field extraction for dynamic use of Nanopb (gnss/vpe)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon vpe info
 */
int64_t get_gnss_vpe(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return 0;
    }
    return ctx->gps.vpe;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/speed)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon is_connected info
 */
float get_gnss_speed(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return NAN;
    }
    return ctx->gps.speed;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/bearing)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon bearing info
 */
int64_t get_gnss_bearing(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return 0;
    }
    return ctx->gps.bearing;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/nsat)
 *
 *  @param ctx workspace buffer
 *  @param idx index (unused)
 *
 *  @return beacon nsat info
 */
int64_t get_gnss_nsat(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || isnan(ctx->gps.lat)) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return 0;
    }
    return ctx->gps.nsat;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/timestamp)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return timestamp info
 */
int64_t get_gnss_age(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return 0;
    }
    return ctx->gps.age;
}

/*! \brief field extraction for dynamic use of Nanopb (num vaps)
 *
 *  @param ctx workspace buffer
 *
 *  @return number of bytes of compressed Virtual AP groups
 */
int32_t get_num_vaps(Sky_ctx_t *ctx)
{
    int j, nv = 0;
#if SKY_DEBUG
    int total_vap = 0;
#endif
    Beacon_t *w;

    if (ctx == NULL) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    for (j = 0; j < NUM_APS(ctx); j++) {
        w = &ctx->beacon[j];
        nv += (w->ap.vg_len ? 1 : 0);
#if SKY_DEBUG
        total_vap += w->ap.vg_len;
#endif
    }

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Groups: %d, vaps: %d", nv, total_vap);
    return nv;
}

/*! \brief field extraction for dynamic use of Nanopb (vap_data)
 *
 *  @param ctx workspace buffer
 *  @param idx index into Virtual Groups
 *
 *  @return vaps data i.e len, AP, patch1, patch2...
 */
uint8_t *get_vap_data(Sky_ctx_t *ctx, uint32_t idx)
{
    int j, nvg = 0;
    Beacon_t *w;

    if (ctx == NULL) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    /* Walk through APs counting vap, when the idx is the current Virtual Group */
    /* return the Virtual AP data */
    for (j = 0; j < NUM_APS(ctx); j++) {
        w = &ctx->beacon[j];
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "AP: %d Group #: %d len: %d nvg: %d", j, idx, w->ap.vg_len,
            nvg);
        if (w->ap.vg_len && nvg == idx) {
#if 1
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Group: %d AP: %d idx: %d len: %d ap: %d", idx, j, idx,
                w->ap.vg[VAP_LENGTH].len, w->ap.vg[VAP_PARENT].ap);
            dump_hex16(__FILE__, __FUNCTION__, ctx, SKY_LOG_LEVEL_DEBUG, w->ap.vg + 1,
                w->ap.vg[VAP_LENGTH].len, 0);
#endif
            return (uint8_t *)w->ap.vg;
        } else {
            nvg += (w->ap.vg_len ? 1 : 0);
        }
    }
    return 0;
}

/*! \brief trim VAP children to meet max_vap_per_rq config
 *
 *  this routine alters the vap patch data, reducing where necessary
 *  the number of children in a virtual group so that as many groups
 *  as possible are retained, but the max_vap_per_rq is not exceeded
 *
 *  @param ctx workspace buffer
 *
 *  @return vaps data i.e len, AP, patch1, patch2...
 */
uint8_t *select_vap(Sky_ctx_t *ctx)
{
    int j, nvap = 0, no_more = false;
    Beacon_t *w;
    uint8_t cap_vap[MAX_AP_BEACONS] = {
        0
    }; /* fill request with as many virtual groups as possible */

    if (ctx == NULL) {
        // LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    for (; !no_more && nvap < CONFIG(ctx->cache, max_vap_per_rq);) {
        /* Walk through APs counting vap, when the idx is the current Virtual Group */
        /* return the Virtual AP data */
        no_more = true;
        for (j = 0; j < NUM_APS(ctx); j++) {
            w = &ctx->beacon[j];
            if (w->ap.vg_len > cap_vap[j]) {
                cap_vap[j]++;
                nvap++;
                if (nvap == CONFIG(ctx->cache, max_vap_per_rq))
                    break;
                if (w->ap.vg_len > cap_vap[j])
                    no_more = false;
            }
        }
    }
    /* Complete the virtual group patch bytes with index of parent and update length */
    for (j = 0; j < NUM_APS(ctx); j++) {
        w = &ctx->beacon[j];
        w->ap.vg[VAP_PARENT].ap = j;
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "AP: %d len: %d -> %d", w->ap.vg[VAP_PARENT].ap,
            w->ap.vg[VAP_LENGTH].len, cap_vap[j] + VAP_PARENT);
        w->ap.vg[VAP_LENGTH].len = cap_vap[j] ? cap_vap[j] + VAP_PARENT : 0;
        dump_hex16(__FILE__, __FUNCTION__, ctx, SKY_LOG_LEVEL_DEBUG, w->ap.vg + 1,
            w->ap.vg[VAP_LENGTH].len, 0);
    }
    return 0;
}

/*! \brief generate random byte sequence
 *
 *  @param rand_buf pointer to buffer where rand bytes are put
 *  @param bufsize length of rand bytes
 *
 *  @returns 0 for failure, length of rand sequence for success
 */
int sky_rand_fn(uint8_t *rand_buf, uint32_t bufsize)
{
    int i;

    if (!rand_buf)
        return 0;

    for (i = 0; i < bufsize; i++)
        rand_buf[i] = rand() % 256;
    return bufsize;
}
