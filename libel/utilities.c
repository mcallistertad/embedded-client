/*! \file libel/utilities.c
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../.submodules/tiny-AES128-C/aes.h"
#define SKY_LIBEL 1
#include "libel.h"

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

    if (ctx != NULL && ctx->header.crc32 ==
                               sky_crc32(&ctx->header.magic,
                                       (uint8_t *)&ctx->header.crc32 -
                                               (uint8_t *)&ctx->header.magic)) {
        for (i = 0; i < TOTAL_BEACONS; i++) {
            if (ctx->beacon[i].h.magic != BEACON_MAGIC ||
                    ctx->beacon[i].h.type > SKY_BEACON_MAX)
                return false;
        }
    }
    if (ctx == NULL || ctx->len > TOTAL_BEACONS ||
            ctx->connected > TOTAL_BEACONS)
        return false;
    return true;
}

/*! \brief validate the cache buffer
 *
 *  @param c pointer to cache buffer
 *
 *  @return true if cache is valid, else false
 */
int validate_cache(Sky_cache_t *c)
{
    int i, j;

    if (c == NULL)
        return false;

    if (c->len != CACHE_SIZE) {
        printf("%s: CACHE_SIZE %d vs len %d", __FUNCTION__, CACHE_SIZE, c->len);
        return false;
    }

    if (c->header.crc32 ==
            sky_crc32(&c->header.magic, (uint8_t *)&c->header.crc32 -
                                                (uint8_t *)&c->header.magic)) {
        for (i = 0; i < CACHE_SIZE; i++) {
            if (c->cacheline[i].len > TOTAL_BEACONS)
                return false;

            for (j = 0; j < TOTAL_BEACONS; j++) {
                if (c->cacheline[i].beacon[j].h.magic != BEACON_MAGIC)
                    return false;
                if (c->cacheline[i].beacon[j].h.type > SKY_BEACON_MAX)
                    return false;
            }
        }
    } else
        return false;
    return true;
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
int logfmt(Sky_ctx_t *ctx, Sky_log_level_t level, const char *fmt, ...)
{
#if SKY_DEBUG
    va_list ap;
    char buf[96];
    int ret;
    if (level > ctx->min_level)
        return -1;
    va_start(ap, fmt);
    ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    (*ctx->logf)(level, buf);
    va_end(ap);
    return ret;
#else
    return 0;
#endif
}

/*! \brief dump the beacons in the workspace
 *
 *  @param ctx workspace pointer
 *
 *  @returns 0 for success or negative number for error
 */
void dump_workspace(Sky_ctx_t *ctx)
{
    int i;

    logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
            "WorkSpace: Expect %d, got %d, AP %d starting at %d, connected %d",
            ctx->expect, ctx->len, ctx->ap_len, ctx->ap_low, ctx->connected);
    for (i = 0; i < ctx->len; i++) {
        switch (ctx->beacon[i].h.type) {
        case SKY_BEACON_AP:
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                    "Beacon % 2d: Type: AP, MAC %02X:%02X:%02X:%02X:%02X:%02X rssi: %d",
                    i, ctx->beacon[i].ap.mac[0], ctx->beacon[i].ap.mac[1],
                    ctx->beacon[i].ap.mac[2], ctx->beacon[i].ap.mac[3],
                    ctx->beacon[i].ap.mac[4], ctx->beacon[i].ap.mac[5],
                    ctx->beacon[i].ap.rssi);
            break;
        case SKY_BEACON_GSM:
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                    "Beacon % 2d: Type: GSM, lac: %d, ui: %d, mcc: %d, mnc: %d, rssi: %d",
                    i, ctx->beacon[i].gsm.lac, ctx->beacon[i].gsm.ci,
                    ctx->beacon[i].gsm.mcc, ctx->beacon[i].gsm.mnc,
                    ctx->beacon[i].gsm.rssi);
            break;
        case SKY_BEACON_NBIOT:
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                    "Beacon % 2d: Type: nb IoT, mcc: %d, mnc: %d, e_cellid: %d, tac: %d, rssi: %d",
                    i, ctx->beacon[i].nbiot.mcc, ctx->beacon[i].nbiot.mnc,
                    ctx->beacon[i].nbiot.e_cellid, ctx->beacon[i].nbiot.tac,
                    ctx->beacon[i].nbiot.rssi);
            break;
        }
    }
}

/*! \brief dump the beacons in the cache
 *
 *  @param ctx workspace pointer
 *
 *  @returns 0 for success or negative number for error
 */
void dump_cache(Sky_ctx_t *ctx)
{
    int i, j;
    Sky_cacheline_t *c;
    Beacon_t *b;

    for (i = 0; i < CACHE_SIZE; i++) {
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "cache: %d of %d", i, CACHE_SIZE);
        c = &ctx->cache->cacheline[i];
        for (j = 0; j < c->len; j++) {
            b = &c->beacon[j];
            switch (b->h.type) {
            case SKY_BEACON_AP:
                logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                        "cache % 2d: Type: AP, MAC %02X:%02X:%02X:%02X:%02X:%02X rssi: %d, %.2f,%.2f,%d",
                        i, b->ap.mac[0], b->ap.mac[1], b->ap.mac[2],
                        b->ap.mac[3], b->ap.mac[4], b->ap.mac[5], b->ap.rssi,
                        c->loc.lat, c->loc.lon, c->loc.hpe);
                break;
            case SKY_BEACON_GSM:
                logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                        "cache % 2d: Type: GSM, lac: %d, ui: %d, mcc: %d, mnc: %d, rssi: %d: %d, %.2f,%.2f,%d",
                        i, b->gsm.lac, b->gsm.ci, b->gsm.mcc, b->gsm.mnc,
                        b->gsm.rssi, c->loc.lat, c->loc.lon, c->loc.hpe);
                break;
            case SKY_BEACON_NBIOT:
                logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                        "cache % 2d: Type: nb IoT, mcc: %d, mnc: %d, e_cellid: %d, tac: %d, rssi: %d: %d, %.2f,%.2f,%d",
                        i, b->nbiot.mcc, b->nbiot.mnc, b->nbiot.e_cellid,
                        b->nbiot.tac, b->nbiot.rssi, c->loc.lat, c->loc.lon,
                        c->loc.hpe);
                break;
            }
        }
    }
}

/*! \brief field extraction for dynamic use of Nanopb (ctx request)
 *
 *  @param ctx workspace buffer
 *
 *  @return pointer to request buffer
 */
uint8_t *get_ctx_request(Sky_ctx_t *ctx)
{
    return ctx->request;
}

/*! \brief field extraction for dynamic use of Nanopb (ctx request_size)
 *
 *  @param ctx workspace buffer
 *
 *  @return size of request buffer
 */
size_t get_ctx_request_size(Sky_ctx_t *ctx)
{
    return sizeof(ctx->request);
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

/*! \brief field extraction for dynamic use of Nanopb (ctx sky_aes_key_id)
 *
 *  @param ctx workspace buffer
 *
 *  @return sky_aes_key_id
 */
uint32_t get_ctx_aes_key_id(Sky_ctx_t *ctx)
{
    return ctx->cache->sky_aes_key_id;
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
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
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
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    if (t == SKY_BEACON_AP) {
        if (ctx->beacon[ctx->ap_low].h.type == t)
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
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
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
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[ctx->ap_low + idx].ap.mac;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/channel)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon channel info
 */
int64_t get_ap_channel(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > ctx->ap_len) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[ctx->ap_low + idx].ap.channel;
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
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[ctx->ap_low + idx].ap.rssi;
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
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->ap_low + idx == ctx->connected;
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
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[ctx->ap_low + idx].ap.age;
}

/*! \brief field extraction for dynamic use of Nanopb (num gsm)
 *
 *  @param ctx workspace buffer
 *
 *  @return number of gsm beacons
 */
int32_t get_num_gsm(Sky_ctx_t *ctx)
{
    if (ctx == NULL) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return get_num_beacons(ctx, SKY_BEACON_GSM);
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/ci)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon ci info
 */
int64_t get_gsm_ci(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.ci;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/mcc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon mcc info
 */
int64_t get_gsm_mcc(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.mcc;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/mnc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon mnc info
 */
int64_t get_gsm_mnc(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.mnc;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/lac)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon lac info
 */
int64_t get_gsm_lac(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.lac;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/rssi)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon rssi info
 */
int64_t get_gsm_rssi(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.rssi;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/is_connected)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon is_connected info
 */
bool get_gsm_is_connected(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->connected == get_base_beacons(ctx, SKY_BEACON_GSM) + idx;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/timestamp)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon timestamp info
 */
int64_t get_gsm_age(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[ctx->ap_low + idx].gsm.age;
}

/*! \brief field extraction for dynamic use of Nanopb (num nbiot)
 *
 *  @param ctx workspace buffer
 *
 *  @return number of nbiot beacons
 */
int32_t get_num_nbiot(Sky_ctx_t *ctx)
{
    if (ctx == NULL) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return get_num_beacons(ctx, SKY_BEACON_NBIOT);
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/mcc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon mcc info
 */
int64_t get_nbiot_mcc(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx].nbiot.mcc;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/mnc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon mnc info
 */
int64_t get_nbiot_mnc(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx].nbiot.mnc;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/e_cellid)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon e cellid info
 */
int64_t get_nbiot_ecellid(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx]
            .nbiot.e_cellid;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/tac)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon tac info
 */
int64_t get_nbiot_tac(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx].nbiot.tac;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/rssi)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon rssi info
 */
int64_t get_nbiot_rssi(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx].nbiot.rssi;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/is_connected)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon is_connected info
 */
bool get_nbiot_is_connected(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->connected == get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/timestamp)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return beacon timestamp info
 */
int64_t get_nbiot_age(Sky_ctx_t *ctx, uint32_t idx)
{
    if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
        // logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
        return 0;
    }
    return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx].nbiot.age;
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
