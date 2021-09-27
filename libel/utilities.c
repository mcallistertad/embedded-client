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
#include <math.h>
#include "libel.h"

#if !SKY_EXCLUDE_SANITY_CHECKS && !SKY_EXCLUDE_WIFI_SUPPORT
static bool validate_mac(const uint8_t mac[6], Sky_rctx_t *rctx);
#endif // !SKY_EXCLUDE_SANITY_CHECKS && !SKY_EXCLUDE_WIFI_SUPPORT

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/*! \brief set sky_errno and return Sky_status
 *
 *  @param sky_errno sky_errno is the error code
 *  @param code the sky_errno_t code to return
 *
 *  @return Sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t set_error_status(Sky_errno_t *sky_errno, Sky_errno_t code)
{
    if (sky_errno != NULL)
        *sky_errno = code;
    return (code == SKY_ERROR_NONE) ? SKY_SUCCESS : SKY_ERROR;
}

/*! \brief validate a beacon
 *
 *  @param b the beacon to be validated
 *  @param rctx request rctx buffer
 *
 *  Some out of range values are forced to unknown
 *
 *  @return true if beacon is valid, else false
 */
bool validate_beacon(Beacon_t *b, Sky_rctx_t *rctx)
{
#if !!SKY_EXCLUDE_SANITY_CHECKS || SKY_EXCLUDE_WIFI_SUPPORT
    (void)rctx;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
    if (b == NULL || b->h.magic != BEACON_MAGIC)
        return false;
    switch (b->h.type) {
    case SKY_BEACON_AP:
        if (b->h.rssi > -10 || b->h.rssi < -127)
            b->h.rssi = -1;
#if !SKY_EXCLUDE_WIFI_SUPPORT
        if (b->ap.freq < 2400 || b->ap.freq > 6000)
            b->ap.freq = 0; /* 0's not sent to server */
#if !SKY_EXCLUDE_SANITY_CHECKS
        return validate_mac(b->ap.mac, rctx);
#else
        return true;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
#endif // !SKY_EXCLUDE_WIFI_SUPPORT
#if !SKY_EXCLUDE_CELL_SUPPORT
    case SKY_BEACON_LTE:
        if (b->h.rssi > -40 || b->h.rssi < -140)
            b->h.rssi = -1;
#if !SKY_EXCLUDE_SANITY_CHECKS
        /* If at least one of the primary IDs is unvalued, then *all* primary IDs must
         * be unvalued (meaning user is attempting to add a neighbor cell). Partial
         * specification of primary IDs is considered an error.
         */
        if ((b->cell.id1 == SKY_UNKNOWN_ID1 || b->cell.id2 == SKY_UNKNOWN_ID2 ||
                b->cell.id4 == SKY_UNKNOWN_ID4) &&
            !(b->cell.id1 == SKY_UNKNOWN_ID1 && b->cell.id2 == SKY_UNKNOWN_ID2 &&
                b->cell.id4 == SKY_UNKNOWN_ID4))
            return false;

        /* range check parameters */
        if ((b->cell.id1 != SKY_UNKNOWN_ID1 &&
                (b->cell.id1 < 200 || b->cell.id1 > 799)) || /* mcc */
            (b->cell.id2 != SKY_UNKNOWN_ID2 && b->cell.id2 > 999) || /* mnc */
            (b->cell.id3 != SKY_UNKNOWN_ID3 &&
                (b->cell.id3 < 1 || b->cell.id3 > 65535)) || /* tac */
            (b->cell.id4 != SKY_UNKNOWN_ID4 &&
                (b->cell.id4 < 0 || b->cell.id4 > 268435455)) || /* e_cellid */
            (b->cell.id5 != SKY_UNKNOWN_ID5 && b->cell.id5 > 503) || /* pci */
            (b->cell.freq != SKY_UNKNOWN_ID6 && b->cell.freq > 262143) || /* earfcn */
            (b->cell.ta != SKY_UNKNOWN_TA && (b->cell.ta < 0 || b->cell.ta > 7690))) /* ta */
            return false;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
        break;
    case SKY_BEACON_NBIOT:
        if (b->h.rssi > -44 || b->h.rssi < -156)
            b->h.rssi = -1;
#if !SKY_EXCLUDE_SANITY_CHECKS
        if ((b->cell.id1 == SKY_UNKNOWN_ID1 || b->cell.id2 == SKY_UNKNOWN_ID2 ||
                b->cell.id4 == SKY_UNKNOWN_ID4) &&
            !(b->cell.id1 == SKY_UNKNOWN_ID1 && b->cell.id2 == SKY_UNKNOWN_ID2 &&
                b->cell.id4 == SKY_UNKNOWN_ID4))
            return false;
        /* range check parameters */
        if ((b->cell.id1 != SKY_UNKNOWN_ID1 &&
                (b->cell.id1 < 200 || b->cell.id1 > 799)) || /* mcc */
            (b->cell.id2 != SKY_UNKNOWN_ID2 && b->cell.id2 > 999) || /* mnc */
            (b->cell.id3 != SKY_UNKNOWN_ID3 &&
                (b->cell.id3 < 1 || b->cell.id3 > 65535)) || /* tac */
            (b->cell.id4 != SKY_UNKNOWN_ID4 &&
                (b->cell.id4 < 0 || b->cell.id4 > 268435455)) || /* e_cellid */
            (b->cell.id5 != SKY_UNKNOWN_ID5 && (b->cell.id5 < 0 || b->cell.id5 > 503)) || /* ncid */
            (b->cell.freq != SKY_UNKNOWN_ID6 &&
                (b->cell.freq < 0 || b->cell.freq > 262143))) /* earfcn */
            return false;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
        break;
    case SKY_BEACON_GSM:
        if (b->h.rssi > -32 || b->h.rssi < -128)
            b->h.rssi = -1;
#if !SKY_EXCLUDE_SANITY_CHECKS
        if (b->cell.id1 == SKY_UNKNOWN_ID1 || b->cell.id2 == SKY_UNKNOWN_ID2 ||
            b->cell.id3 == SKY_UNKNOWN_ID3 || b->cell.id4 == SKY_UNKNOWN_ID4)
            return false;
        /* range check parameters */
        if (b->cell.id1 < 200 || b->cell.id1 > 799 || /* mcc */
            b->cell.id2 > 999 || /* mnc */
            (b->cell.ta != SKY_UNKNOWN_TA && (b->cell.ta < 0 || b->cell.ta > 63))) /* ta */
            return false;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
        break;
    case SKY_BEACON_UMTS:
        if (b->h.rssi > -20 || b->h.rssi < -120)
            b->h.rssi = -1;
#if !SKY_EXCLUDE_SANITY_CHECKS
        if ((b->cell.id1 == SKY_UNKNOWN_ID1 || b->cell.id2 == SKY_UNKNOWN_ID2 ||
                b->cell.id4 == SKY_UNKNOWN_ID4) &&
            !(b->cell.id1 == SKY_UNKNOWN_ID1 && b->cell.id2 == SKY_UNKNOWN_ID2 &&
                b->cell.id4 == SKY_UNKNOWN_ID4))
            return false;
        /* range check parameters */
        if ((b->cell.id1 != SKY_UNKNOWN_ID1 &&
                (b->cell.id1 < 200 || b->cell.id1 > 799)) || /* mcc */
            (b->cell.id2 != SKY_UNKNOWN_ID2 && b->cell.id2 > 999) || /* mnc */
            (b->cell.id4 != SKY_UNKNOWN_ID4 &&
                (b->cell.id4 < 0 || b->cell.id4 > 268435455)) || /* e_cellid */
            (b->cell.id5 != SKY_UNKNOWN_ID5 && b->cell.id5 > 511) || /* psc */
            (b->cell.freq != SKY_UNKNOWN_ID6 &&
                (b->cell.freq < 412 || b->cell.freq > 262143))) /* earfcn */
            return false;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
        break;
    case SKY_BEACON_CDMA:
        if (b->h.rssi > -49 || b->h.rssi < -140)
            b->h.rssi = -1;
#if !SKY_EXCLUDE_SANITY_CHECKS
        if (b->cell.id2 == SKY_UNKNOWN_ID2 || b->cell.id3 == SKY_UNKNOWN_ID3 ||
            b->cell.id4 == SKY_UNKNOWN_ID4)
            return false;
        /* range check parameters */
        if (b->cell.id2 > 32767 || /* sid */
            b->cell.id3 < 0 || b->cell.id3 > 65535 || /* nid */
            b->cell.id4 < 0 || b->cell.id4 > 65535) /* bsid */
            return false;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
        break;
    case SKY_BEACON_NR:
        if (b->h.rssi > -40 || b->h.rssi < -140)
            b->h.rssi = -1;
#if !SKY_EXCLUDE_SANITY_CHECKS
        if ((b->cell.id1 == SKY_UNKNOWN_ID1 || b->cell.id2 == SKY_UNKNOWN_ID2 ||
                b->cell.id4 == SKY_UNKNOWN_ID4) &&
            !(b->cell.id1 == SKY_UNKNOWN_ID1 && b->cell.id2 == SKY_UNKNOWN_ID2 &&
                b->cell.id4 == SKY_UNKNOWN_ID4))
            return false;
        /* range check parameters */
        if ((b->cell.id1 != SKY_UNKNOWN_ID1 &&
                (b->cell.id1 < 200 || b->cell.id1 > 799)) || /* mcc */
            (b->cell.id2 != SKY_UNKNOWN_ID2 && b->cell.id2 > 999) || /* mnc */
            (b->cell.id4 != SKY_UNKNOWN_ID4 &&
                (b->cell.id4 < 0 || b->cell.id4 > 68719476735)) || /* nci */
            (b->cell.id5 != SKY_UNKNOWN_ID5 && (b->cell.id5 < 0 || b->cell.id5 > 107)) || /* pci */
            (b->cell.freq != SKY_UNKNOWN_ID6 &&
                (b->cell.freq < 0 || b->cell.freq > 3279165)) || /* nrarfcn */
            (b->cell.ta != SKY_UNKNOWN_TA && (b->cell.ta < 0 || b->cell.ta > 3846)))
            return false;
#endif // !SKY_EXCLUDE_SANITY_CHECKS
        break;
#endif // !SKY_EXCLUDE_CELL_SUPPORT
    default:
        return false;
    }
#if !SKY_EXCLUDE_CELL_SUPPORT
    if (is_cell_nmr(b)) {
        b->h.connected = false;
        b->cell.ta = SKY_UNKNOWN_TA;
    }
#endif // !SKY_EXCLUDE_CELL_SUPPORT
    return true;
}

#if !SKY_EXCLUDE_SANITY_CHECKS
/*! \brief validate the request rctx buffer
 *
 *  @param rctx request rctx buffer
 *
 *  @return true if request rctx is valid, else false
 */
bool validate_request_ctx(Sky_rctx_t *rctx)
{
    int i;

    if (rctx == NULL) {
        // Can't use LOGFMT if rctx is bad
        return false;
    }
    if (NUM_BEACONS(rctx) > TOTAL_BEACONS + 1) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Too many beacons");
        return false;
    }
    if (NUM_APS(rctx) > MAX_AP_BEACONS + 1) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Too many AP beacons");
        return false;
    }
    if (rctx->header.magic == SKY_MAGIC &&
        rctx->header.crc32 == sky_crc32(&rctx->header.magic, (uint8_t *)&rctx->header.crc32 -
                                                                 (uint8_t *)&rctx->header.magic)) {
        for (i = 0; i < TOTAL_BEACONS; i++) {
            if (i < NUM_BEACONS(rctx)) {
                if (!validate_beacon(&rctx->beacon[i], rctx)) {
                    LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad beacon #%d of %d", i, TOTAL_BEACONS);
                    return false;
                }
            } else {
                if (rctx->beacon[i].h.magic != BEACON_MAGIC ||
                    rctx->beacon[i].h.type > SKY_BEACON_MAX) {
                    LOGFMT(
                        rctx, SKY_LOG_LEVEL_ERROR, "Bad empty beacon #%d of %d", i, TOTAL_BEACONS);
                    return false;
                }
            }
        }
    } else {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "CRC check failed");
        return false;
    }
    return true;
}
#endif // !SKY_EXCLUDE_SANITY_CHECKS

/*! \brief validate the session context buffer - Cant use LOGFMT here
 *
 *  @param c pointer to csession context buffer
 *
 *  @return true if session context is valid, else false
 */
bool validate_session_ctx(Sky_sctx_t *sctx, Sky_loggerfn_t logf)
{
    if (sctx == NULL) {
#if SKY_LOGGING
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Session ctx validation failed: NULL pointer");
#else
        (void)logf;
#endif // SKY_LOGGING
        return false;
    }

    if (sctx->header.magic != SKY_MAGIC) {
#if SKY_LOGGING
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Session ctx validation failed: bad magic in header");
#endif // SKY_LOGGING
        return false;
    }
#if !SKY_EXCLUDE_SANITY_CHECKS
    if (sctx->header.crc32 == sky_crc32(&sctx->header.magic, (uint8_t *)&sctx->header.crc32 -
                                                                 (uint8_t *)&sctx->header.magic)) {
#if CACHE_SIZE
        if (sctx->header.size != sizeof(Sky_sctx_t)) {
#if SKY_LOGGING
            if (logf != NULL)
                (*logf)(SKY_LOG_LEVEL_ERROR,
                    "Session ctx validation failed: restored session does not match CACHE_SIZE");
#endif // SKY_LOGGING
            return false;
        }

        for (int i = 0; i < sctx->num_cachelines; i++) {
            int j;

            if (sctx->cacheline[i].num_beacons > TOTAL_BEACONS) {
#if SKY_LOGGING
                if (logf != NULL)
                    (*logf)(SKY_LOG_LEVEL_ERROR,
                        "Session ctx validation failed: too many beacons for TOTAL_BEACONS");
#endif // SKY_LOGGING
                return false;
            }

            for (j = 0; j < TOTAL_BEACONS; j++) {
                if (sctx->cacheline[i].beacon[j].h.magic != BEACON_MAGIC) {
#if SKY_LOGGING
                    if (logf != NULL)
                        (*logf)(
                            SKY_LOG_LEVEL_ERROR, "Session ctx validation failed: Bad beacon info");
#endif // SKY_LOGGING
                    return false;
                }
                if (sctx->cacheline[i].beacon[j].h.type > SKY_BEACON_MAX) {
#if SKY_LOGGING
                    if (logf != NULL)
                        (*logf)(
                            SKY_LOG_LEVEL_ERROR, "Session ctx validation failed: Bad beacon type");
#endif // SKY_LOGGING
                    return false;
                }
            }
        }
#endif // CACHE_SIZE
    } else {
#if SKY_LOGGING
        if (logf != NULL)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Session ctx validation failed: crc mismatch!");
#endif // SKY_LOGGING
        return false;
    }
#endif // !SKY_EXCLUDE_SANITY_CHECKS
    return true;
}

#if !SKY_EXCLUDE_SANITY_CHECKS && !SKY_EXCLUDE_WIFI_SUPPORT
/*! \brief validate mac address
 *
 *  @param mac pointer to mac address
 *  @param rctx pointer to context
 *
 *  @return true if mac address not all zeros or ones
 */
static bool validate_mac(const uint8_t mac[6], Sky_rctx_t *rctx)
{
    if (mac[0] == 0 || mac[0] == 0xff) {
        if (mac[0] == mac[1] && mac[0] == mac[2] && mac[0] == mac[3] && mac[0] == mac[4] &&
            mac[0] == mac[5]) {
            LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Invalid mac address");
            return false;
        }
    }

#if SKY_LOGGING == false
    (void)rctx;
#endif // SKY_LOGGING
    return true;
}
#endif // !SKY_EXCLUDE_SANITY_CHECKS && !SKY_EXCLUDE_WIFI_SUPPORT

/*! \brief return true if library is configured for tbr authentication
 *
 *  @param rctx request rctx buffer
 *
 *  @return is tbr enabled
 */
bool is_tbr_enabled(Sky_rctx_t *rctx)
{
    return (rctx->session->sku[0] != '\0');
}

#if SKY_LOGGING
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
 *  @param rctx request rctx buffer
 *  @param level the log level of this msg
 *  @param fmt the msg
 *  @param ... variable arguments
 *
 *  @return 0 for success
 */

int logfmt(
    const char *file, const char *function, Sky_rctx_t *rctx, Sky_log_level_t level, char *fmt, ...)
{
    va_list ap;
    char buf[SKY_LOG_LENGTH];
    int ret, n;
    if (rctx == NULL || rctx->session == NULL || rctx->session->logf == NULL ||
        level > rctx->session->min_level || function == NULL)
        return -1;
    memset(buf, '\0', sizeof(buf));
    // Print log-line prefix ("<source file>:<function name>")
    n = snprintf(buf, sizeof(buf), "%.18s:%.20s() ", sky_basename(file), function);

    va_start(ap, fmt);
    ret = vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    (*rctx->session->logf)(level, buf);
    va_end(ap);
    return ret;
}
#endif // SKY_LOGGING

/*! \brief dump maximum number of bytes of the given buffer in hex on one line
 *
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *  @param rctx request rctx buffer
 *  @param level the log level of this msg
 *  @param buffer where to start dumping the next line
 *  @param bufsize remaining size of the buffer in bytes
 *  @param buf_offset byte index of progress through the current buffer
 *
 *  @returns number of bytes dumped, or negitive number on error
 */
int dump_hex16(const char *file, const char *function, Sky_rctx_t *rctx, Sky_log_level_t level,
    void *buffer, uint32_t bufsize, uint32_t buf_offset)
{
    uint32_t pb = 0;
#if SKY_LOGGING
    char buf[SKY_LOG_LENGTH];
    uint8_t *b = (uint8_t *)buffer;
    int n, N;
    if (rctx == NULL || rctx->session->logf == NULL || level > rctx->session->min_level ||
        function == NULL || buffer == NULL || bufsize == 0)
        return -1;
    memset(buf, '\0', sizeof(buf));
    // Print log-line prefix ("<source file>:<function name> <buf offset>:")
    n = snprintf(buf, sizeof(buf), "%.20s:%.20s() %07X:", sky_basename(file), function, buf_offset);

    // Calculate number of characters required to print 16 bytes
    N = n + (16 * 3); /* 16 bytes per line, 3 bytes per byte (' XX') */
    // if width of log line (SKY_LOG_LENGTH) too short 16 bytes, just print those that fit
    for (; n < MIN(SKY_LOG_LENGTH - 4, N);) {
        if (pb < bufsize)
            n += sprintf(&buf[n], " %02X", b[pb++]);
        else
            break;
    }
    (*rctx->session->logf)(level, buf);
#else
    (void)file;
    (void)function;
    (void)rctx;
    (void)level;
    (void)buffer;
    (void)bufsize;
    (void)buf_offset;
#endif // SKY_LOGGING
    return (int)pb;
}

/*! \brief dump all bytes of the given buffer in hex
 *
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *  @param rctx request rctx pointer
 *  @param buf pointer to the buffer
 *  @param bufsize size of the buffer in bytes
 *
 *  @returns number of bytes dumped
 */
int log_buffer(const char *file, const char *function, Sky_rctx_t *rctx, Sky_log_level_t level,
    void *buffer, uint32_t bufsize)
{
    uint32_t buf_offset = 0;
#if SKY_LOGGING
    int i;
    uint32_t n = bufsize;
    uint8_t *p = buffer;
    /* try to print 16 bytes per line till all dumped */
    while ((i = dump_hex16(
                file, function, rctx, level, (void *)(p + (bufsize - n)), n, buf_offset)) > 0) {
        n -= i;
        buf_offset += i;
    }
#else
    (void)file;
    (void)function;
    (void)rctx;
    (void)level;
    (void)buffer;
    (void)bufsize;
#endif // SKY_LOGGING
    return (int)buf_offset;
}

#if !SKY_EXCLUDE_WIFI_SUPPORT
/*! \brief dump Virtual APs in group (children not parent)
 *
 *  @param rctx request rctx pointer
 *  @param b parent of Virtual Group AP
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns void
 */
void dump_vap(Sky_rctx_t *rctx, char *prefix, Beacon_t *b, const char *file, const char *func)
{
#if SKY_LOGGING
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

        logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
            "%s %s %3s %02X:%02X:%02X:%02X:%02X:%02X %-4dMHz rssi:%d age:%d", prefix,
            (b->ap.vg_prop[j].in_cache) ? (b->ap.vg_prop[j].used ? "Used  " : "Cached") : "      ",
            j < b->ap.vg_len - 1 ? "\\ /" : "\\_/", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            b->ap.freq, b->h.rssi, b->h.age);
    }
#else
    (void)rctx;
    (void)prefix;
    (void)b;
    (void)file;
    (void)func;
#endif // SKY_LOGGING
}

/*! \brief dump AP including any VAP
 *
 *  @param rctx request rctx pointer
 *  @param str comment
 *  @param b pointer to Beacon_t structure
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns void
 */
void dump_ap(Sky_rctx_t *rctx, char *prefix, Beacon_t *b, const char *file, const char *func)
{
#if SKY_LOGGING
    if (prefix == NULL)
        prefix = "AP:";

    logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
        "%s %s MAC %02X:%02X:%02X:%02X:%02X:%02X %-4dMHz rssi:%d age:%d pri:%d.%d", prefix,
        (b->ap.property.in_cache) ? (b->ap.property.used ? "Used  " : "Cached") : "      ",
        b->ap.mac[0], b->ap.mac[1], b->ap.mac[2], b->ap.mac[3], b->ap.mac[4], b->ap.mac[5],
        b->ap.freq, b->h.rssi, b->h.age, (int)b->h.priority,
        (int)((b->h.priority - (int)b->h.priority) * 10.0));
    dump_vap(rctx, prefix, b, file, func);
#else
    (void)rctx;
    (void)prefix;
    (void)b;
    (void)file;
    (void)func;
#endif // SKY_LOGGING
}
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

/*! \brief dump a beacon
 *
 *  @param rctx request rctx pointer
 *  @param b the beacon to dump
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns 0 for success or negative number for error
 */
void dump_beacon(Sky_rctx_t *rctx, char *str, Beacon_t *b, const char *file, const char *func)
{
#if SKY_LOGGING
    char prefixstr[50] = { '\0' };
    int idx_b;
#if CACHE_SIZE
    int idx_c;
#endif // CACHE_SIZE

    /* Test whether beacon is in cache or request rctx */
    if (b >= rctx->beacon && b < rctx->beacon + TOTAL_BEACONS + 1) {
        idx_b = (int)(b - rctx->beacon);
        snprintf(prefixstr, sizeof(prefixstr), "%s     %-2d%s %7s", str, idx_b,
            b->h.connected ? "*" : " ", sky_pbeacon(b));
#if CACHE_SIZE
    } else if (rctx->session && b >= rctx->session->cacheline[0].beacon &&
               b < rctx->session->cacheline[CACHE_SIZE - 1].beacon +
                       rctx->session->cacheline[CACHE_SIZE - 1].num_beacons) {
        idx_b = (int)(b - rctx->session->cacheline[0].beacon);
        idx_c = idx_b / TOTAL_BEACONS;
        idx_b %= TOTAL_BEACONS;
        snprintf(prefixstr, sizeof(prefixstr), "%s %2d:%-2d%s %7s", str, idx_c, idx_b,
            b->h.connected ? "*" : " ", sky_pbeacon(b));
#endif // CACHE_SIZE
    } else {
        snprintf(prefixstr, sizeof(prefixstr), "%s     ? %s %7s", str, b->h.connected ? "*" : " ",
            sky_pbeacon(b));
    }

    switch (b->h.type) {
#if !SKY_EXCLUDE_WIFI_SUPPORT
    case SKY_BEACON_AP:
        strcat(prefixstr, "    ");
        dump_ap(rctx, prefixstr, b, file, func);
        break;
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

#if !SKY_EXCLUDE_CELL_SUPPORT
    case SKY_BEACON_GSM:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_LTE:
    case SKY_BEACON_CDMA:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_NR:
        /* if primary key is UNKNOWN, must be NMR */
        strcat(prefixstr, "    ");
        if (b->cell.id2 == SKY_UNKNOWN_ID2) {
            logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG, "%9s %d %dMHz rssi:%d age:%d pri:%d.%d",
                prefixstr, b->cell.id5, b->cell.freq, b->h.rssi, b->h.age, (int)b->h.priority,
                (int)((b->h.priority - (int)b->h.priority) * 10.0));
        } else {
            logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
                "%9s %u,%u,%u,%llu,%d %dMHz rssi:%d ta:%d age:%d pri:%d.%d", prefixstr, b->cell.id1,
                b->cell.id2, b->cell.id3, b->cell.id4, b->cell.id5, b->cell.freq, b->h.rssi,
                b->cell.ta, b->h.age, (int)b->h.priority,
                (int)((b->h.priority - (int)b->h.priority) * 10.0));
        }
        break;
#endif // !SKY_EXCLUDE_CELL_SUPPORT
    default:
        logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG, "%s: Type: Unknown", prefixstr);
        break;
    }
#else
    (void)rctx;
    (void)str;
    (void)b;
    (void)file;
    (void)func;
#endif // SKY_LOGGING
}

#if !SKY_EXCLUDE_GNSS_SUPPORT
/*! \brief dump gnss info, if present
 *
 *  @param rctx workspace pointer
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *  @param gnss gnss pointer
 *  @returns void
 */
void dump_gnss(Sky_rctx_t *rctx, const char *file, const char *func, Gnss_t *gnss)
{
#if SKY_LOGGING
    if (!isnan(gnss->lat))
        logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG, "gnss: %d.%06d, %d.%6d hpe: %d",
            (int)gnss->lat, (int)(fabs(round(1000000.0 * (gnss->lat - (int)gnss->lat)))),
            (int)gnss->lon, (int)(fabs(round(1000000.0 * (gnss->lon - (int)gnss->lon)))),
            gnss->hpe);
#else
    (void)rctx;
    (void)file;
    (void)func;
    (void)gnss;
#endif // SKY_LOGGING
}
#endif // !SKY_EXCLUDE_GNSS_SUPPORT

/*! \brief dump the beacons in the request rctx
 *
 *  @param rctx request rctx pointer
 *
 *  @returns 0 for success or negative number for error
 */
void dump_request_ctx(Sky_rctx_t *rctx, const char *file, const char *func)
{
#if SKY_LOGGING
    int i;

    logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
        "Dump Request Context: Got %d beacons, WiFi %d%s%s", NUM_BEACONS(rctx), NUM_APS(rctx),
        is_tbr_enabled(rctx) ? ", TBR" : "", rctx->hit ? ", Cache Hit" : "");
#if !SKY_EXCLUDE_GNSS_SUPPORT
    dump_gnss(rctx, __FILE__, __FUNCTION__, &rctx->gnss);
#endif // !SKY_EXCLUDE_GNSS_SUPPORT

    for (i = 0; i < NUM_BEACONS(rctx); i++)
        dump_beacon(rctx, "req", &rctx->beacon[i], file, func);

    if (CONFIG(rctx->session, last_config_time) == CONFIG_UPDATE_DUE) {
        logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
            "Config: Total:%d AP:%d VAP:%d(%d) Update:Pending",
            CONFIG(rctx->session, total_beacons), CONFIG(rctx->session, max_ap_beacons),
            CONFIG(rctx->session, max_vap_per_ap), CONFIG(rctx->session, max_vap_per_rq));
    } else {
        logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
            "Config: Total:%d AP:%d VAP:%d(%d) Update:%d Sec", CONFIG(rctx->session, total_beacons),
            CONFIG(rctx->session, max_ap_beacons), CONFIG(rctx->session, max_vap_per_ap),
            CONFIG(rctx->session, max_vap_per_rq),
            rctx->header.time - CONFIG(rctx->session, last_config_time));
    }
    logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
        "Config: Threshold:%d(Used) %d(All) %d(Age) %d(Beacon) %d(RSSI)",
        CONFIG(rctx->session, cache_match_used_threshold),
        CONFIG(rctx->session, cache_match_all_threshold),
        CONFIG(rctx->session, cache_age_threshold), CONFIG(rctx->session, cache_beacon_threshold),
        -CONFIG(rctx->session, cache_neg_rssi_threshold));
#else
    (void)rctx;
    (void)file;
    (void)func;
#endif // SKY_LOGGING
}

/*! \brief dump the beacons in the cache
 *
 *  @param rctx request rctx pointer
 *  @param file the file name where LOG_BUFFER was invoked
 *  @param function the function name where LOG_BUFFER was invoked
 *
 *  @returns 0 for success or negative number for error
 */
void dump_cache(Sky_rctx_t *rctx, const char *file, const char *func)
{
#if SKY_LOGGING
#if CACHE_SIZE
    int i, j;
    Sky_cacheline_t *cl;

    for (i = 0; i < rctx->session->num_cachelines; i++) {
        cl = &rctx->session->cacheline[i];
        if (cl->num_beacons == 0 || cl->time == CACHE_EMPTY) {
            logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
                "cache: %d of %d - empty num_beacons:%d num_ap:%d time:%u", i,
                rctx->session->num_cachelines, cl->num_beacons, cl->num_ap, cl->time);
        } else {
            logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG,
                "cache: %d of %d loc:%d.%06d,%d.%06d, hpe: %d  %d beacons%s", i,
                rctx->session->num_cachelines, (int)cl->loc.lat,
                (int)fabs(round(1000000.0 * (cl->loc.lat - (int)cl->loc.lat))), (int)cl->loc.lon,
                (int)fabs(round(1000000.0 * (cl->loc.lon - (int)cl->loc.lon))), cl->loc.hpe,
                cl->num_beacons, (rctx->hit && rctx->get_from == i) ? ", <--Cache Hit" : "");
#if !SKY_EXCLUDE_GNSS_SUPPORT
            dump_gnss(rctx, __FILE__, __FUNCTION__, &cl->gnss);
#endif // !SKY_EXCLUDE_GNSS_SUPPORT
            for (j = 0; j < cl->num_beacons; j++) {
                dump_beacon(rctx, "cache", &cl->beacon[j], file, func);
            }
        }
    }
#else
    (void)rctx;
    logfmt(file, func, rctx, SKY_LOG_LEVEL_DEBUG, "cache: Disabled");
#endif // CACHE_SIZE
#else
    (void)rctx;
    (void)file;
    (void)func;
#endif // SKY_LOGGING
}

/*! \brief set dynamic config parameter defaults
 *
 *  @param cache buffer
 *
 *  @return void
 */
void config_defaults(Sky_sctx_t *sctx)
{
    if (CONFIG(sctx, total_beacons) == 0)
        CONFIG(sctx, total_beacons) = TOTAL_BEACONS;
    if (CONFIG(sctx, max_ap_beacons) == 0)
        CONFIG(sctx, max_ap_beacons) = MAX_AP_BEACONS;
    if (CONFIG(sctx, cache_match_used_threshold) == 0)
        CONFIG(sctx, cache_match_used_threshold) = CACHE_MATCH_THRESHOLD_USED;
    if (CONFIG(sctx, cache_match_all_threshold) == 0)
        CONFIG(sctx, cache_match_all_threshold) = CACHE_MATCH_THRESHOLD_ALL;
    if (CONFIG(sctx, cache_age_threshold) == 0)
        CONFIG(sctx, cache_age_threshold) = CACHE_AGE_THRESHOLD;
    if (CONFIG(sctx, cache_beacon_threshold) == 0)
        CONFIG(sctx, cache_beacon_threshold) = CACHE_BEACON_THRESHOLD;
    if (CONFIG(sctx, cache_neg_rssi_threshold) == 0)
        CONFIG(sctx, cache_neg_rssi_threshold) = CACHE_RSSI_THRESHOLD;
    if (CONFIG(sctx, max_vap_per_ap) == 0)
        CONFIG(sctx, max_vap_per_ap) = MAX_VAP_PER_AP;
    if (CONFIG(sctx, max_vap_per_rq) == 0)
        CONFIG(sctx, max_vap_per_rq) = MAX_VAP_PER_RQ;
    /* Add new config parameters here */
}

/*! \brief field extraction for dynamic use of Nanopb (rctx partner_id)
 *
 *  @param rctx request rctx buffer
 *
 *  @return partner_id
 */
uint32_t get_ctx_partner_id(Sky_rctx_t *rctx)
{
    return rctx->session->partner_id;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx aes_key)
 *
 *  @param rctx request rctx buffer
 *
 *  @return aes_key
 */
uint8_t *get_ctx_aes_key(Sky_rctx_t *rctx)
{
    return rctx->session->aes_key;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx device_id)
 *
 *  @param rctx request rctx buffer
 *
 *  @return device_id
 */
uint8_t *get_ctx_device_id(Sky_rctx_t *rctx)
{
    return rctx->session->device_id;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx id_len)
 *
 *  @param rctx request rctx buffer
 *
 *  @return id_len
 */
uint32_t get_ctx_id_length(Sky_rctx_t *rctx)
{
    return rctx->session->id_len;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx device_id)
 *
 *  @param rctx request rctx buffer
 *
 *  @return device_id
 */
uint8_t *get_ctx_ul_app_data(Sky_rctx_t *rctx)
{
    return rctx->session->ul_app_data;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx id_len)
 *
 *  @param rctx request rctx buffer
 *
 *  @return id_len
 */
uint32_t get_ctx_ul_app_data_length(Sky_rctx_t *rctx)
{
    return rctx->session->ul_app_data_len;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx sku)
 *
 *  @param rctx request rctx buffer
 *
 *  @return token_id
 */
uint32_t get_ctx_token_id(Sky_rctx_t *rctx)
{
    return rctx->session->token_id;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx sku)
 *
 *  @param rctx request rctx buffer
 *
 *  @return sku
 */
char *get_ctx_sku(Sky_rctx_t *rctx)
{
    return rctx->session->sku;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx cc)
 *
 *  @param rctx request rctx buffer
 *
 *  @return cc
 */
uint32_t get_ctx_cc(Sky_rctx_t *rctx)
{
    return rctx->session->cc;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx logf)
 *
 *  @param rctx request rctx buffer
 *
 *  @return logf
 */
Sky_loggerfn_t get_ctx_logf(Sky_rctx_t *rctx)
{
    return rctx->session->logf;
}

/*! \brief field extraction for dynamic use of Nanopb (rctx id_len)
 *
 *  @param rctx request rctx buffer
 *
 *  @return id_len
 */
Sky_randfn_t get_ctx_rand_bytes(Sky_rctx_t *rctx)
{
    return rctx->session->rand_bytes;
}

/*! \brief field extraction for dynamic use of Nanopb (count beacons)
 *
 *  @param rctx request rctx buffer
 *  @param t type of beacon to count
 *
 *  @return number of beacons of the specified type
 */
int32_t get_num_beacons(Sky_rctx_t *rctx, Sky_beacon_type_t t)
{
    int i, b;

    if (rctx == NULL || t > SKY_BEACON_MAX) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    if (t == SKY_BEACON_AP) {
        return NUM_APS(rctx);
    } else {
        for (i = NUM_APS(rctx), b = 0; i < NUM_BEACONS(rctx); i++) {
            if (rctx->beacon[i].h.type == t)
                b++;
            if (b && rctx->beacon[i].h.type != t)
                break; /* End of beacons of this type */
        }
    }
    return b;
}

/*! \brief Return the total number of scanned cells (serving, neighbor, or otherwise)
 *
 *  @param rctx request rctx buffer
 *
 *  @return number of cells
 */
int32_t get_num_cells(Sky_rctx_t *rctx)
{
    int i, b;

    if (rctx == NULL) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param")
        return 0;
    }

    for (i = NUM_APS(rctx), b = 0; i < NUM_BEACONS(rctx); i++) {
        if (is_cell_type(&rctx->beacon[i]))
            b++;
    }

    return b;
}

/*! \brief field extraction for dynamic use of Nanopb (base of beacon type)
 *
 *  @param rctx request rctx buffer
 *  @param t type of beacon to find
 *
 *  @return first beacon of the specified type
 */
int get_base_beacons(Sky_rctx_t *rctx, Sky_beacon_type_t t)
{
    int i = 0;

    if (rctx == NULL || t > SKY_BEACON_MAX) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    if (t == SKY_BEACON_AP) {
        if (rctx->beacon[0].h.type == t)
            return i;
    } else {
        for (i = NUM_APS(rctx); i < NUM_BEACONS(rctx); i++) {
            if (rctx->beacon[i].h.type == t)
                return i;
        }
    }
    return -1;
}

/*! \brief field extraction for dynamic use of Nanopb (num AP)
 *
 *  @param rctx request rctx buffer
 *
 *  @return number of AP beacons
 */
int32_t get_num_aps(Sky_rctx_t *rctx)
{
    if (rctx == NULL) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return NUM_APS(rctx);
}

#if !SKY_EXCLUDE_WIFI_SUPPORT
/*! \brief field extraction for dynamic use of Nanopb (AP/MAC)
 *
 *  @param rctx request rctx buffer
 *  @param idx index into beacons
 *
 *  @return beacon mac info
 */
uint8_t *get_ap_mac(Sky_rctx_t *rctx, uint32_t idx)
{
    if (rctx == NULL || idx > NUM_APS(rctx)) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return rctx->beacon[idx].ap.mac;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/freq)
 *
 *  @param rctx request rctx buffer
 *  @param idx index into beacons
 *
 *  @return beacon channel info
 */
int64_t get_ap_freq(Sky_rctx_t *rctx, uint32_t idx)
{
    if (rctx == NULL || idx > NUM_APS(rctx)) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return rctx->beacon[idx].ap.freq;
}
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

/*! \brief field extraction for dynamic use of Nanopb (AP/rssi)
 *
 *  @param rctx request rctx buffer
 *  @param idx index into beacons
 *
 *  @return beacon rssi info
 */
int64_t get_ap_rssi(Sky_rctx_t *rctx, uint32_t idx)
{
    if (rctx == NULL || idx > NUM_APS(rctx)) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return rctx->beacon[idx].h.rssi;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/is_connected)
 *
 *  @param rctx request rctx buffer
 *  @param idx index into beacons
 *
 *  @return beacon is_connected info
 */
bool get_ap_is_connected(Sky_rctx_t *rctx, uint32_t idx)
{
    if (rctx == NULL || idx > NUM_APS(rctx)) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return false;
    }
    return rctx->beacon[idx].h.connected;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/timestamp)
 *
 *  @param rctx request rctx buffer
 *  @param idx index into beacons
 *
 *  @return beacon timestamp info
 */
int64_t get_ap_age(Sky_rctx_t *rctx, uint32_t idx)
{
    if (rctx == NULL || idx > NUM_APS(rctx)) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    return rctx->beacon[idx].h.age;
}

#if !SKY_EXCLUDE_CELL_SUPPORT
/*! \brief Get a cell
 *
 *  @param rctx request rctx buffer
 *  @param idx index into cells
 *
 *  @return Pointer to cell
 */
Beacon_t *get_cell(Sky_rctx_t *rctx, uint32_t idx)
{
    if (rctx == NULL) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "bad param");
        return 0;
    }

    return &rctx->beacon[NUM_APS(rctx) + idx];
}

/*! \brief Get cell type
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell type
 */
int16_t get_cell_type(Beacon_t *cell)
{
    if (!cell || !is_cell_type(cell))
        return SKY_BEACON_MAX;
    else
        return (int16_t)cell->h.type;
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
    default:
        break;
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
    if (!cell)
        return -1;
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
    if (!cell)
        return -1;
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
    if (!cell)
        return -1;
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
    case SKY_BEACON_GSM:
        return SKY_UNKNOWN_ID5; // Reporting ID5 value not supported for GSM

    case SKY_BEACON_LTE:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_NR:
        return cell->cell.id5;
    default:
        break;
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
    case SKY_BEACON_GSM:
        return SKY_UNKNOWN_ID6; // Reporting ID6 value not supported for GSM

    case SKY_BEACON_LTE:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_UMTS:
    case SKY_BEACON_NR:
        return cell->cell.freq;
    default:
        break;
    }

    return 0;
}

/*! \brief Get cell connected flag
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell connected flag
 */
bool get_cell_connected_flag(Sky_rctx_t *rctx, Beacon_t *cell)
{
    if (!rctx || !cell)
        return -1;
    return cell->h.connected;
}

/*! \brief Return cell RSSI value
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return beacon rssi
 */
int64_t get_cell_rssi(Beacon_t *cell)
{
    if (!cell)
        return -1;
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
    if (!cell)
        return -1;
    return cell->h.age;
}

/*! \brief Return cell ta value
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return beacon ta
 */
int64_t get_cell_ta(Beacon_t *cell)
{
    uint16_t type = get_cell_type(cell);

    if (!cell)
        return -1;
    switch (type) {
    case SKY_BEACON_GSM:
    case SKY_BEACON_LTE:
    case SKY_BEACON_NR:
        return cell->cell.ta;
    case SKY_BEACON_CDMA:
    case SKY_BEACON_NBIOT:
    case SKY_BEACON_UMTS:
    default:
        return SKY_UNKNOWN_TA;
    }
}
#endif // !SKY_EXCLUDE_CELL_SUPPORT

#if !SKY_EXCLUDE_GNSS_SUPPORT
/*! \brief field extraction for dynamic use of Nanopb (num gnss)
 *
 *  @param rctx request rctx buffer
 *
 *  @return number of gnss
 */
int32_t get_num_gnss(Sky_rctx_t *rctx)
{
    return has_gnss(rctx) ? 1 : 0;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/lat)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss lat info
 */
float get_gnss_lat(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.lat : NAN;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/lon)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss lon info
 */
float get_gnss_lon(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.lon : NAN;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/hpe)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss hpe info
 */
int64_t get_gnss_hpe(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.hpe : 0;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/alt)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss alt info
 */
float get_gnss_alt(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.alt : NAN;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/vpe)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss vpe info
 */
int64_t get_gnss_vpe(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.vpe : 0;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/speed)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss gnss speed info
 */
float get_gnss_speed(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.speed : NAN;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/bearing)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss bearing info
 */
int64_t get_gnss_bearing(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.bearing : 0;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/nsat)
 *
 *  @param rctx request rctx buffer
 *  @param idx index (unused)
 *
 *  @return gnss nsat info
 */
int64_t get_gnss_nsat(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.nsat : 0;
}

/*! \brief field extraction for dynamic use of Nanopb (gnss/timestamp)
 *
 *  @param rctx request rctx buffer
 *  @param idx index into beacons
 *
 *  @return gnss timestamp info
 */
int64_t get_gnss_age(Sky_rctx_t *rctx, uint32_t idx)
{
    (void)idx; /* suppress warning of unused parameter */
    return has_gnss(rctx) ? rctx->gnss.age : 0;
}
#endif // !SKY_EXCLUDE_GNSS_SUPPORT

#if !SKY_EXCLUDE_WIFI_SUPPORT
/*! \brief field extraction for dynamic use of Nanopb (num vaps)
 *
 *  @param rctx request rctx buffer
 *
 *  @return number of Virtual AP groups
 */
int32_t get_num_vaps(Sky_rctx_t *rctx)
{
    int j, nv = 0;
#if SKY_LOGGING
    int total_vap = 0;
#endif // SKY_LOGGING
    Beacon_t *w;

    if (rctx == NULL) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    for (j = 0; j < NUM_APS(rctx); j++) {
        w = &rctx->beacon[j];
        nv += (w->ap.vg[VAP_LENGTH].len ? 1 : 0);
#if SKY_LOGGING
        total_vap += w->ap.vg[VAP_LENGTH].len;
#endif // SKY_LOGGING
    }

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Groups: %d, vaps: %d", nv, total_vap);
    return nv;
}

/*! \brief field extraction for dynamic use of Nanopb (vap_data)
 *
 *  @param rctx request rctx buffer
 *  @param idx index into Virtual Groups
 *
 *  @return vaps data i.e num_beacons, AP, patch1, patch2...
 */
uint8_t *get_vap_data(Sky_rctx_t *rctx, uint32_t idx)
{
    uint32_t j, nvg = 0;
    Beacon_t *w;

    if (rctx == NULL) {
        // LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad param");
        return 0;
    }
    /* Walk through APs counting vap, when the idx is the current Virtual Group */
    /* return the Virtual AP data */
    for (j = 0; j < NUM_APS(rctx); j++) {
        w = &rctx->beacon[j];
        // LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "AP: %d Group #: %d num_beacons: %d nvg: %d", j, idx, w->ap.vg_len, nvg);
        if (w->ap.vg[VAP_LENGTH].len && nvg == idx) {
            // LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Group: %d AP: %d idx: %d num_beacons: %d ap: %d", idx, j, idx,
            //     w->ap.vg[VAP_LENGTH].num_beacons, w->ap.vg[VAP_PARENT].ap);
            // dump_hex16(__FILE__, __FUNCTION__, rctx, SKY_LOG_LEVEL_DEBUG, w->ap.vg + 1,
            //     w->ap.vg[VAP_LENGTH].num_beacons, 0);
            return (uint8_t *)w->ap.vg;
        } else {
            nvg += (w->ap.vg[VAP_LENGTH].len ? 1 : 0);
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
 *  @param rctx request rctx buffer
 *
 *  @return void
 */
void select_vap(Sky_rctx_t *rctx)
{
    uint32_t j, nvap = 0, no_more = false;
    Beacon_t *w;
    uint8_t cap_vap[MAX_AP_BEACONS] = {
        0
    }; /* fill request with as many virtual groups as possible */

    for (; !no_more && nvap < CONFIG(rctx->session, max_vap_per_rq);) {
        /* Walk through APs counting vap, when max_vap_per_rq is reached */
        /* then walk through again, truncating the compressed bytes */
        no_more = true;
        for (j = 0; j < NUM_APS(rctx); j++) {
            w = &rctx->beacon[j];
            if (w->ap.vg_len > cap_vap[j]) {
                cap_vap[j]++;
                nvap++;
                if (nvap == CONFIG(rctx->session, max_vap_per_rq))
                    break;
                if (w->ap.vg_len > cap_vap[j])
                    no_more = false;
            }
        }
    }
    /* Complete the virtual group patch bytes with index of parent and update length */
    for (j = 0; j < NUM_APS(rctx); j++) {
        w = &rctx->beacon[j];
        w->ap.vg[VAP_PARENT].ap = j;
#if VERBOSE_DEBUG
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "AP: %d num_beacons: %d -> %d", w->ap.vg[VAP_PARENT].ap,
            w->ap.vg[VAP_LENGTH].len, cap_vap[j] ? cap_vap[j] + VAP_PARENT : 0);
#endif // VERBOSE_DEBUG
        w->ap.vg[VAP_LENGTH].len = cap_vap[j] ? cap_vap[j] + VAP_PARENT : 0;
        dump_hex16(__FILE__, __FUNCTION__, rctx, SKY_LOG_LEVEL_DEBUG, w->ap.vg + 1,
            w->ap.vg[VAP_LENGTH].len, 0);
    }
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "select_vap completed!");
}
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

/*! \brief generate random byte sequence
 *
 *  @param rand_buf pointer to buffer where rand bytes are put
 *  @param bufsize length of rand bytes
 *
 *  @returns 0 for failure, length of rand sequence for success
 */
int sky_rand_fn(uint8_t *rand_buf, uint32_t bufsize)
{
    uint32_t i;

    if (!rand_buf)
        return 0;

    for (i = 0; i < bufsize; i++)
        rand_buf[i] = rand() % 256;
    return (int)bufsize;
}

/*! \brief Calculate distance between two gps coordinates using Haversine formula
 *
 *  ref: https://www.geeksforgeeks.org/program-distance-two-points-earth/
 *
 *  @param a_lat latitude of position A in degrees
 *  @param a_lon longitude of position A in degrees
 *  @param a_lat latitude of position B in degrees
 *  @param a_lon longitude of position B in degrees
 *  @returns distance distance in meters
 */
#define PI (acos(-1.0))
#define RADIANS(d) (PI / 180.0f * (d))
float distance_A_to_B(float lat_a, float lon_a, float lat_b, float lon_b)
{
    return (float)(1000 * 6371 *
                   acos(cos(RADIANS(90 - lat_a)) * cos(RADIANS(90 - lat_b)) +
                        sin(RADIANS(90 - lat_a)) * sin(RADIANS(90 - lat_b)) *
                            cos(RADIANS(lon_a - lon_b))));
}

#ifdef UNITTESTS

BEGIN_TESTS(test_utilities)

GROUP("get_cell_type");

TEST("should return SKY_BEACON_MAX for non-cell", rctx, {
    Beacon_t b;
    b.h.type = SKY_BEACON_AP;
    ASSERT(SKY_BEACON_MAX == get_cell_type(&b));
});

TEST("should return SKY_BEACON_MAX with bad args", rctx,
    { ASSERT(SKY_BEACON_MAX == get_cell_type(NULL)); });

END_TESTS();

#endif // UNITTESTS
