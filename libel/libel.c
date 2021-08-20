/*! \file libel/libel.c
 *  \brief sky entry points - Skyhook Embedded Library
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
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "libel.h"
#include "proto.h"

/* A monotonically increasing version number intended to track the client
 * software version, and which is sent to the server in each request. Clumsier
 * than just including the Git version string (since it will need to be updated
 * manually for every release) but cheaper bandwidth-wise.
 */
#define SW_VERSION 19

/* Interval in seconds between requests for config params */
#define CONFIG_REQUEST_INTERVAL (24 * SECONDS_IN_HOUR) /* 24 hours */

/* The following definition is intended to be changed only for QA purposes */
#define BACKOFF_UNITS_PER_HR 3600 // time in seconds

/* Local functions */
#if SANITY_CHECKS
static bool validate_device_id(const uint8_t *device_id, uint32_t id_len);
#endif
static size_t strnlen_(char *s, size_t maxlen);

/*! \brief Initialize Skyhook library and verify access to resources
 *
 *  @param sky_errno if sky_open returns failure, sky_errno is set to the error code
 *  @param device_id Device unique ID (example mac address of the device)
 *  @param id_len length if the Device ID, typically 6, Max 16 bytes
 *  @param partner_id Skyhook assigned credentials
 *  @param aes_key Skyhook assigned encryption key
 *  @param sku unique name of device family, must be non-empty to enable TBR Auth
 *  @param cc County code where device is being registered, 0 if unknown
 *  @param session_buf pointer to a session buffer
 *  @param min_level logging function is called for msg with equal or greater level
 *  @param logf pointer to logging function
 *  @param rand_bytes pointer to random function
 *  @param gettime pointer to time function
 *
 *  @return sky_status_t SKY_SUCCESS or SKY_ERROR
 *
 *  If session buffer is being restored from a previous session, cache is restored.
 *  If session buffer is an empty buffer, a new session is started with empty cache.
 *  sky_open will return an error if the library is already open and (sky_close has not been called).
 *  Device ID length will be truncated to 16 if larger, without causing an error.
 */
Sky_status_t sky_open(Sky_errno_t *sky_errno, uint8_t *device_id, uint32_t id_len,
    uint32_t partner_id, uint8_t aes_key[AES_KEYLEN], char *sku, uint32_t cc, Sky_sctx_t *sctx,
    Sky_log_level_t min_level, Sky_loggerfn_t logf, Sky_randfn_t rand_bytes, Sky_timefn_t gettime)
{
    Sky_sctx_t *session;
    int sku_len;
#if SKY_LOGGING
    char buf[SKY_LOG_LENGTH];
#endif

    if (logf != NULL && SKY_LOG_LEVEL_DEBUG <= min_level)
        (*logf)(SKY_LOG_LEVEL_DEBUG, "Skyhook Embedded Library (Version: " VERSION ")");

    if (sctx == NULL)
        if (logf != NULL && SKY_LOG_LEVEL_ERROR <= min_level) {
            (*logf)(SKY_LOG_LEVEL_ERROR, "sky_open(): Must provide session buffer!");
            return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
        }
    session = (Sky_sctx_t *)sctx;

    /* Only consider up to 16 bytes. Ignore any extra */
    id_len = (id_len > MAX_DEVICE_ID) ? MAX_DEVICE_ID : id_len;
    sku = !sku ? "" : sku;
    sku_len = (int)strnlen_(sku, MAX_SKU_LEN);

    if (session->header.magic == 0)
        ; /* No warning if buffer has been zeroed */
    else if (!validate_session_ctx(session, logf)) {
        if (logf != NULL && SKY_LOG_LEVEL_WARNING <= min_level)
            (*logf)(SKY_LOG_LEVEL_WARNING, "Ignoring invalid session buffer!");
        session->header.magic = 0;
    } else {
        /* retrieved valid saved session. Check that sky_close was called and authentication is the same */
        if (session->open_flag) {
            return set_error_status(sky_errno, SKY_ERROR_ALREADY_OPEN);
        }
        if (session->num_cachelines != CACHE_SIZE) {
            if (logf != NULL && SKY_LOG_LEVEL_WARNING <= min_level)
                (*logf)(SKY_LOG_LEVEL_WARNING, "cache configuration changed. Clearing state.");
            session->header.magic = 0;
        } else if (partner_id != 0 &&
                   (memcmp(sku, session->sku, sku_len) != 0 || partner_id != session->partner_id ||
                       memcmp(aes_key, session->aes_key, AES_SIZE) != 0)) {
            if (logf != NULL && SKY_LOG_LEVEL_WARNING <= min_level)
                (*logf)(SKY_LOG_LEVEL_WARNING, "New Authendication configuration. Clearing state.");
            session->header.magic = 0;
        }
    }

    rand_bytes = (rand_bytes == NULL) ? sky_rand_fn : rand_bytes;
    if (gettime == NULL) {
        if (logf != NULL && SKY_LOG_LEVEL_ERROR <= min_level)
            (*logf)(SKY_LOG_LEVEL_ERROR, "Must provide gettime callback function!");
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* Initialize the session context if needed */
    if (session->header.magic == 0) {
        memset(session, 0, sizeof(Sky_sctx_t));
        session->header.magic = SKY_MAGIC;
        session->header.size = sizeof(Sky_sctx_t);
        session->header.time = (*gettime)(NULL);
        session->header.crc32 = sky_crc32(&session->header.magic,
            (uint8_t *)&session->header.crc32 - (uint8_t *)&session->header.magic);
#if CACHE_SIZE
        session->num_cachelines = CACHE_SIZE;
        for (int i = 0; i < CACHE_SIZE; i++) {
            for (int j = 0; j < TOTAL_BEACONS; j++) {
                session->cacheline[i].beacon[j].h.magic = BEACON_MAGIC;
                session->cacheline[i].beacon[j].h.type = SKY_BEACON_MAX;
            }
        }
#endif
#if SKY_LOGGING
    } else {
        if (logf != NULL && SKY_LOG_LEVEL_DEBUG <= min_level) {
            snprintf(buf, sizeof(buf), "%s:%s() State buffer with CRC 0x%08X, size %d restored",
                sky_basename(__FILE__), __FUNCTION__, sky_crc32(session, session->header.size),
                session->header.size);
            (*logf)(SKY_LOG_LEVEL_DEBUG, buf);
        }
#endif
    }
    config_defaults(session);

#if SANITY_CHECKS
    if (!validate_device_id(device_id, id_len) || aes_key == NULL)
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
#endif

    session->id_len = id_len;
    memcpy(session->device_id, device_id, id_len);
    session->partner_id = partner_id;
    memcpy(session->aes_key, aes_key, sizeof(session->aes_key));
    if (sku_len) {
        strncpy(session->sku, sku, MAX_SKU_LEN); /* Only pass up to maximum characters of sku */
        session->sku[MAX_SKU_LEN] = '\0'; /* Guarantee sku is null terminated */
        session->cc = cc;
    }
    session->min_level = min_level;
    session->logf = logf;
    session->rand_bytes = rand_bytes;
    session->timefn = gettime;
    session->plugins = NULL; /* re-register plugins */

    if (sky_register_plugins((Sky_plugin_table_t **)&session->plugins) != SKY_SUCCESS)
        return set_error_status(sky_errno, SKY_ERROR_NO_PLUGIN);

    session->open_flag = true;

    return set_error_status(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Determines the size of a session buffer
 *
 *  @param session Pointer to session buffer or NULL
 *
 *  @return Size of session buffer or 0 to indicate that the buffer was invalid
 */
int32_t sky_sizeof_session_ctx(Sky_sctx_t *sctx)
{
    /* if no session pointer provided, return size required,
     * else return the size of the session buffer
     */
    if (sctx == NULL)
        return sizeof(Sky_sctx_t);

    /* Cache space required
     *
     * header - Magic number, size of space, checksum
     * body - number of entries
     */
    if (sctx->header.magic != SKY_MAGIC ||
        sctx->header.crc32 != sky_crc32(&sctx->header.magic, (uint8_t *)&sctx->header.crc32 -
                                                                 (uint8_t *)&sctx->header.magic)) {
        return 0;
    }
    if (sctx->header.size >= sizeof(Sky_sctx_t))
        return (int32_t)sctx->header.size;
    return 0;
}

/*! \brief Returns the size of the request ctx required to build request
 *
 *  @return Size of request buffer
 */
int32_t sky_sizeof_request_ctx(void)
{
    /* Total space required
     *
     * header - Magic number, size of space, checksum
     * body - number of beacons, beacon data, gnss, request buffer
     */
    return sizeof(Sky_rctx_t);
}

/*! \brief Validate backoff period
 *
 *  @param rctx Pointer to request rctx provided by user
 *  @param now current time
 *
 *  @return false if backoff period has not passed
 */
static bool backoff_violation(Sky_rctx_t *rctx, time_t now)
{
    Sky_sctx_t *sctx = rctx->session;

    /* Enforce backoff period, check that enough time has passed since last request was received */
    if (sctx->backoff != SKY_ERROR_NONE) { /* Retry backoff in progress */
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Backoff: %s, %d seconds so far",
            sky_perror(sctx->backoff), (int)(now - sctx->header.time));
        switch (sctx->backoff) {
        case SKY_AUTH_RETRY_8H:
            if (now - sctx->header.time < (8 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        case SKY_AUTH_RETRY_16H:
            if (now - sctx->header.time < (16 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        case SKY_AUTH_RETRY_1D:
            if (now - sctx->header.time < (24 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        case SKY_AUTH_RETRY_30D:
            if (now - sctx->header.time < (30 * 24 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        case SKY_AUTH_NEEDS_TIME:
            /* Waiting for time to be available */
            if (now == TIME_UNAVAILABLE)
                return true;
            break;
        default:
            break;
        }
    }
    return false;
}

/*! \brief Initializes the request ctx provided ready to build a request
 *
 *  @param r Pointer to request ctx provided by user
 *  @param session_buf Pointer to session ctx provided by user
 *  @param rbufsize Request ctx buffer size (from sky_sizeof_request_ctx)
 *  @param sky_errno Pointer to error code
 *
 *  @return Pointer to the initialized request context buffer or NULL
 */
Sky_rctx_t *sky_new_request(Sky_rctx_t *rctx, uint32_t rbufsize, Sky_sctx_t *sctx,
    uint8_t *ul_app_data, uint32_t ul_app_data_len, Sky_errno_t *sky_errno)
{
    int i;
    time_t now;

    if (rbufsize != sizeof(Sky_rctx_t) || rctx == NULL || sctx == NULL) {
        if (sky_errno != NULL)
            *sky_errno = SKY_ERROR_BAD_PARAMETERS;
        return NULL;
    }
    if (!sctx->open_flag) {
        if (sky_errno != NULL)
            *sky_errno = SKY_ERROR_NEVER_OPEN;
        return NULL;
    }
    now = (*sctx->timefn)(NULL);

    memset(rctx, 0, rbufsize);
    /* update header in request rctx */
    rctx->header.magic = SKY_MAGIC;
    rctx->header.size = rbufsize;
    rctx->header.time = now;
    rctx->header.crc32 = sky_crc32(
        &rctx->header.magic, (uint8_t *)&rctx->header.crc32 - (uint8_t *)&rctx->header.magic);

    rctx->hit = false;
    rctx->get_from = rctx->save_to = -1;
    rctx->session = sctx;
    rctx->auth_state =
        !is_tbr_enabled(rctx) ?
            STATE_TBR_DISABLED :
            sctx->token_id == TBR_TOKEN_UNKNOWN ? STATE_TBR_UNREGISTERED : STATE_TBR_REGISTERED;
    rctx->gnss.lat = NAN; /* empty */
    for (i = 0; i < TOTAL_BEACONS; i++) {
        rctx->beacon[i].h.magic = BEACON_MAGIC;
        rctx->beacon[i].h.type = SKY_BEACON_MAX;
    }
    rctx->gnss.lat = NAN;

    if (now < TIMESTAMP_2019_03_01) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Don't have good time of day!");
        now = TIME_UNAVAILABLE; /* note that time was bad when request was started */
    }

    if (backoff_violation(rctx, now)) {
        if (sky_errno != NULL)
            *sky_errno = SKY_ERROR_SERVICE_DENIED;
        return NULL;
    }

#if CACHE_SIZE
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d cachelines configured", sctx->num_cachelines);
    for (i = 0; i < CACHE_SIZE; i++) {
        if (sctx->cacheline[i].num_ap > CONFIG(sctx, max_ap_beacons) ||
            sctx->cacheline[i].num_beacons > CONFIG(sctx, total_beacons)) {
            sctx->cacheline[i].time = TIME_UNAVAILABLE;
            LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                "cache %d of %d cleared due to new Dynamic Parameters. Total beacons %d vs %d, AP %d vs %d",
                i, CACHE_SIZE, CONFIG(sctx, total_beacons), sctx->cacheline[i].num_beacons,
                CONFIG(sctx, max_ap_beacons), sctx->cacheline[i].num_ap);
        }
        if (sctx->cacheline[i].time != CACHE_EMPTY && now == TIME_UNAVAILABLE) {
            sctx->cacheline[i].time = CACHE_EMPTY;
            LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                "cache %d of %d cleared due to time being unavailable", i, CACHE_SIZE);
        } else if (sctx->cacheline[i].time != CACHE_EMPTY &&
                   (now - sctx->cacheline[i].time) >
                       CONFIG(sctx, cache_age_threshold) * SECONDS_IN_HOUR) {
            sctx->cacheline[i].time = CACHE_EMPTY;
            LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "cache %d of %d cleared due to age (%d)", i,
                CACHE_SIZE, now - sctx->cacheline[i].time);
        }
    }
#endif
    sctx->ul_app_data_len = ul_app_data_len;
    memcpy(sctx->ul_app_data, ul_app_data, ul_app_data_len);
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Partner_id: %d, Sku: %sctx", sctx->partner_id, sctx->sku);
    dump_hex16(__FILE__, "Device_id", rctx, SKY_LOG_LEVEL_DEBUG, sctx->device_id, sctx->id_len, 0);
    DUMP_CACHE(rctx);
    return rctx;
}

/*! \brief  Adds the wifi ap information to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param mac pointer to mac address of the Wi-Fi beacon
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rssi Received Signal Strength Intensity, -10 through -127, -1 if unknown
 *  @param frequency center frequency of channel in MHz, 2400 through 6000, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_ap_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint8_t mac[6],
    time_t timestamp, int16_t rssi, int32_t frequency, bool is_connected)
{
    Beacon_t b;

    if (!rctx)
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%02X:%02X:%02X:%02X:%02X:%02X, %d MHz, rssi %d, %sage %d",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], frequency, rssi,
        is_connected ? "serve " : "",
        (int)timestamp == TIME_UNAVAILABLE ? 0 : (int)(rctx->header.time - timestamp));

    /* Create AP beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_AP;
    b.h.connected = (int8_t)is_connected;
    b.h.rssi = rssi;
    memcpy(b.ap.mac, mac, MAC_SIZE);
    b.ap.freq = frequency;
    b.ap.property.in_cache = false;
    b.ap.property.used = false;

    return add_beacon(rctx, sky_errno, &b, timestamp);
}

/*! \brief Add an lte cell beacon to request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param tac lte tracking area code identifier (1-65535), SKY_UNKNOWN_ID3 if unknown
 *  @param e_cellid lte beacon identifier 28bit (0-268435455)
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code (0-999)
 *  @param pci mobile pci (0-503), SKY_UNKNOWN_ID5 if unknown
 *  @param earfcn channel (0-45589, SKY_UNKNOWN_ID6 if unknown)
 *  @param ta  timing-advance (0-7690), SKY_UNKNOWN_TA if unknown
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rsrp Received Signal Receive Power, range -140 to -40dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_lte_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int32_t tac,
    int64_t e_cellid, uint16_t mcc, uint16_t mnc, int16_t pci, int32_t earfcn, int32_t ta,
    time_t timestamp, int16_t rsrp, bool is_connected)
{
    Beacon_t b;

    if (mcc != SKY_UNKNOWN_ID1 || mnc != SKY_UNKNOWN_ID2 || e_cellid != SKY_UNKNOWN_ID4)
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, %d, %d MHz, ta %d, rsrp %d, %sage %d",
            mcc, mnc, tac, e_cellid, pci, earfcn, ta, rsrp, is_connected ? "serve, " : "",
            (int)(rctx->header.time - timestamp));

    /* Create LTE beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_LTE;
    b.h.connected = (int8_t)is_connected;
    b.h.rssi = rsrp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = tac;
    b.cell.id4 = e_cellid;
    b.cell.id5 = pci;
    b.cell.freq = earfcn;
    b.cell.ta = ta;

    return add_beacon(rctx, sky_errno, &b, timestamp);
}

/*! \brief Add an lte cell neighbor beacon to request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param pci mobile pci (0-503, SKY_UNKNOWN_ID5 if unknown)
 *  @param earfcn channel (0-45589, SKY_UNKNOWN_ID6 if unknown)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rsrp Received Signal Receive Power, range -140 to -40dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_lte_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int16_t pci,
    int32_t earfcn, time_t timestamp, int16_t rsrp)
{
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, rsrp %d, age %d", pci, earfcn, rsrp,
        (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));
    return sky_add_cell_lte_beacon(rctx, sky_errno, SKY_UNKNOWN_ID3, SKY_UNKNOWN_ID4,
        SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2, pci, earfcn, SKY_UNKNOWN_TA, timestamp, rsrp, false);
}

/*! \brief Adds a gsm cell beacon to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lac gsm location area code identifier (1-65535)
 *  @param ci gsm cell identifier (0-65535)
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code  (0-999)
 *  @param ta timing-advance (0-63), SKY_UNKNOWN_TA if unknown
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rssi Received Signal Strength Intensity, range -128 to -32dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_gsm_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int32_t lac,
    int64_t ci, uint16_t mcc, uint16_t mnc, int32_t ta, time_t timestamp, int16_t rssi,
    bool is_connected)
{
    Beacon_t b;

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, ta %d, rssi %d, %sage %d", lac, ci, mcc,
        mnc, ta, rssi, is_connected ? "serve, " : "", (int)(rctx->header.time - timestamp));

    /* Create GSM beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_GSM;
    b.h.connected = (int8_t)is_connected;
    b.h.rssi = rssi;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = lac;
    b.cell.id4 = ci;
    b.cell.ta = ta;

    return add_beacon(rctx, sky_errno, &b, timestamp);
}

/*! \brief Adds a umts cell beacon to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lac umts location area code identifier (1-65535), SKY_UNKNOWN_ID3 if unknown
 *  @param ucid umts cell identifier 28bit (0-268435455)
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code  (0-999)
 *  @param psc primary scrambling code (0-511, SKY_UNKNOWN_ID5 if unknown)
 *  @param uarfcn channel (412-10833, SKY_UNKNOWN_ID6 if unknown)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rscp Received Signal Code Power, range -120dbm to -20dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_umts_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int32_t lac,
    int64_t ucid, uint16_t mcc, uint16_t mnc, int16_t psc, int16_t uarfcn, time_t timestamp,
    int16_t rscp, bool is_connected)
{
    Beacon_t b;

    if (!rctx)
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);

    if (mcc != SKY_UNKNOWN_ID1 || mnc != SKY_UNKNOWN_ID2 || ucid != SKY_UNKNOWN_ID4)
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, %d, %d MHz, rscp %d, %sage %d", mcc,
            mnc, lac, ucid, psc, uarfcn, rscp, is_connected ? "serve, " : "",
            (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));

    /* Create UMTS beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_UMTS;
    b.h.connected = (int8_t)is_connected;
    b.h.rssi = rscp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = lac;
    b.cell.id4 = ucid;
    b.cell.id5 = psc;
    b.cell.freq = uarfcn;

    return add_beacon(rctx, sky_errno, &b, timestamp);
}

/*! \brief Adds a umts cell neighbor beacon to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param psc primary scrambling code (0-511, SKY_UNKNOWN_ID5 if unknown)
 *  @param uarfcn channel (412-10833, SKY_UNKNOWN_ID6 if unknown)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rscp Received Signal Code Power, range -120dbm to -20dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_umts_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno,
    int16_t psc, int16_t uarfcn, time_t timestamp, int16_t rscp)
{
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, rscp %d, age %d", psc, uarfcn, rscp,
        (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));
    return sky_add_cell_umts_beacon(rctx, sky_errno, SKY_UNKNOWN_ID3, SKY_UNKNOWN_ID4,
        SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2, psc, uarfcn, timestamp, rscp, false);
}

/*! \brief Adds a cdma cell beacon to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param sid cdma system identifier (0-32767)
 *  @param nid cdma network identifier(0-65535)
 *  @param bsid cdma base station identifier (0-65535)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rssi Received Signal Strength Intensity
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_cdma_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint32_t sid,
    int32_t nid, int64_t bsid, time_t timestamp, int16_t rssi, bool is_connected)
{
    Beacon_t b;

    if (!rctx)
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%u, %d, %lld, rssi %d, %sage %d", sid, nid, bsid, rssi,
        is_connected ? "serve, " : "",
        (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));

    /* Create CDMA beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_CDMA;
    b.h.connected = (int8_t)is_connected;
    b.h.rssi = rssi;
    b.cell.id2 = sid;
    b.cell.id3 = nid;
    b.cell.id4 = bsid;

    return add_beacon(rctx, sky_errno, &b, timestamp);
}

/*! \brief Adds a nb_iot cell beacon to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code  (0-999)
 *  @param e_cellid nbiot beacon identifier (0-268435455)
 *  @param tac nbiot tracking area code identifier (1-65535), SKY_UNKNOWN_ID3 if unknown
 *  @param ncid mobile cell ID (0-503), SKY_UNKNOWN_ID5 if unknown
 *  @param earfcn channel (0-45589), SKY_UNKNOWN_ID6 if unknown
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param nrsrp Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_nb_iot_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint16_t mcc,
    uint16_t mnc, int64_t e_cellid, int32_t tac, int16_t ncid, int32_t earfcn, time_t timestamp,
    int16_t nrsrp, bool is_connected)
{
    Beacon_t b;

    if (!rctx)
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);

    if (mcc != SKY_UNKNOWN_ID1 || mnc != SKY_UNKNOWN_ID2 || e_cellid != SKY_UNKNOWN_ID4)
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, %d, %d MHz, nrsrp %d, %sage %d", mcc,
            mnc, tac, e_cellid, ncid, earfcn, nrsrp, is_connected ? "serve, " : "",
            (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));

    /* Create NB IoT beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_NBIOT;
    b.h.connected = (int8_t)is_connected;
    b.h.rssi = nrsrp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = tac;
    b.cell.id4 = e_cellid;
    b.cell.id5 = ncid;
    b.cell.freq = earfcn;

    return add_beacon(rctx, sky_errno, &b, timestamp);
}

/*! \brief Adds a nb_iot cell neighbor beacon to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param ncid mobile cell ID (0-503, SKY_UNKNOWN_ID4 if unknown)
 *  @param earfcn channel (0-45589, SKY_UNKNOWN_ID6 if unknown)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param nrsrp Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_nb_iot_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno,
    int16_t ncid, int32_t earfcn, time_t timestamp, int16_t nrsrp)
{
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, nrsrp %d, age %d", ncid, earfcn, nrsrp,
        (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));

    return sky_add_cell_nb_iot_beacon(rctx, sky_errno, SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2,
        SKY_UNKNOWN_ID4, SKY_UNKNOWN_ID3, ncid, earfcn, timestamp, nrsrp, false);
}

/*! \brief Adds a NR cell beacon to the request context
 *
 *  @param rctx          Skyhook request context
 *  @param sky_errno    sky_errno is set to the error code
 *  @param mcc          mobile country code (200-799)
 *  @param mnc          mobile network code (0-999)
 *  @param nci          nr cell identity (0-68719476735)
 *  @param tac          tracking area code identifier (1-65535), SKY_UNKNOWN_ID3 if unknown
 *  @param pci          physical cell ID (0-1007), SKY_UNKNOWN_ID5 if unknown
 *  @param nrarfcn      channel (0-3279165), SKY_UNKNOWN_ID6 if unknown
 *  @param ta           timing-advance (0-3846), SKY_UNKNOWN_TA if unknown
 *  @param timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param csi_rsrp     CSI Reference Signal Received Power, range -140 to -40dBm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 * Returns      SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */

Sky_status_t sky_add_cell_nr_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint16_t mcc,
    uint16_t mnc, int64_t nci, int32_t tac, int16_t pci, int32_t nrarfcn, int32_t ta,
    time_t timestamp, int16_t csi_rsrp, bool is_connected)
{
    Beacon_t b;

    if (mcc != SKY_UNKNOWN_ID1 && mnc != SKY_UNKNOWN_ID2 && nci != SKY_UNKNOWN_ID4)
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d: %lld, %d, %d MHz, ta %d, rsrp %d, %sage %d",
            mcc, mnc, tac, nci, pci, nrarfcn, ta, csi_rsrp, is_connected ? "serve, " : "",
            (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));

    /* Create NR beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_NR;
    b.h.connected = (int8_t)is_connected;
    b.h.rssi = csi_rsrp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = tac;
    b.cell.id4 = nci;
    b.cell.id5 = pci;
    b.cell.freq = nrarfcn;
    b.cell.ta = ta;

    return add_beacon(rctx, sky_errno, &b, timestamp);
}

/*! \brief Adds a NR cell neighbor beacon to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param physical cell ID (0-1007), SKY_UNKNOWN_ID5 if unknown
 *  @param nrarfcn channel (0-3279165), SKY_UNKNOWN_ID6 if unknown
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param nrsrp Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_nr_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int16_t pci,
    int32_t nrarfcn, time_t timestamp, int16_t csi_rsrp)
{
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, rsrp %d, age %d", pci, nrarfcn, csi_rsrp,
        (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));
    return sky_add_cell_nr_beacon(rctx, sky_errno, SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2,
        (int64_t)SKY_UNKNOWN_ID4, SKY_UNKNOWN_ID3, pci, nrarfcn, SKY_UNKNOWN_TA, timestamp,
        csi_rsrp, false);
}

/*! \brief Adds the position of the device from GNSS to the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lat device latitude
 *  @param lon device longitude
 *  @param hpe pointer to horizontal Positioning Error in meters with 68% confidence, 0 if unknown
 *  @param altitude pointer to altitude above mean sea level, in meters, NaN if unknown
 *  @param vpe pointer to vertical Positioning Error in meters with 68% confidence, 0 if unknown
 *  @param speed pointer to speed in meters per second, Nan if unknown
 *  @param bearing pointer to bearing of device in degrees, counterclockwise from north
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param nsat number of satelites used to determine the location, 0 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_gnss(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, float lat, float lon,
    uint16_t hpe, float altitude, uint16_t vpe, float speed, float bearing, uint16_t nsat,
    time_t timestamp)
{
    if (!rctx)
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d.%06d,%d.%06d, hpe %d, alt %d.%02d, vpe %d,", (int)lat,
        (int)fabs(round(1000000.0 * (lat - (int)lat))), (int)lon,
        (int)fabs(round(1000000.0 * (lon - (int)lon))), hpe, (int)altitude,
        (int)fabs(round(100.0 * (altitude - (int)altitude))), vpe);

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "%d.%01dm/s, bearing %d.%01d, nsat %d, age %d", (int)speed,
        (int)fabs(round(10.0 * (speed - (int)speed))), (int)bearing,
        (int)fabs(round(1.0 * (bearing - (int)bearing))), nsat,
        (int)timestamp == -1 ? -1 : (int)(rctx->header.time - timestamp));

    /* location was determined before sky_new_request and since Mar 1st 2019 */
    if (rctx->header.time == TIME_UNAVAILABLE || timestamp == TIME_UNAVAILABLE)
        rctx->gnss.age = 0;
    else if (rctx->header.time >= timestamp && timestamp > TIMESTAMP_2019_03_01)
        rctx->gnss.age = rctx->header.time - timestamp;
    else
        return set_error_status(sky_errno, SKY_ERROR_BAD_TIME);

    /* range check parameters */
    if (isnan(lat) || isnan(lon)) /* don't fail for empty gnss */
        return set_error_status(sky_errno, SKY_ERROR_NONE);

    if ((!isnan(altitude) && (altitude < -1200 || /* Lake Baikal meters above sea level */
                                 altitude > 8900)) || /* Everest meters above sea level */
        hpe < 0.0 ||
        hpe > 100000.0 || /* max range of cell tower */
        speed < 0.0 || speed > 343.0 || /* speed of sound in meters per second */
        nsat > 100) /* 100 is conservative max gnss sat count */
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

#if SANITY_CHECKS
    if (!validate_request_ctx(rctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
#endif

    rctx->gnss.lat = lat;
    rctx->gnss.lon = lon;
    rctx->gnss.hpe = hpe;
    rctx->gnss.alt = altitude;
    rctx->gnss.vpe = vpe;
    rctx->gnss.speed = speed;
    rctx->gnss.bearing = bearing;
    rctx->gnss.nsat = nsat;
    return set_error_status(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Determines whether the request matches the cached result
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param cache_hit where to save true/false result
 *  @param loc where to save device latitude, longitude etc from cache if known
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_search_cache(
    Sky_rctx_t *rctx, Sky_errno_t *sky_errno, bool *cache_hit, Sky_location_t *loc)
{
    Sky_sctx_t *sctx = rctx->session;
#if CACHE_SIZE
    Sky_cacheline_t *cl;
#endif

#if SANITY_CHECKS
    if (!validate_request_ctx(rctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
#endif

#if CACHE_SIZE
    /* check cachelines against new beacons for best match
     * setting from_cache if a matching cacheline is found
     * and hit true if match is above threshold
     * */
    search_cache(rctx);

    /* check cache match result */
    if (IS_CACHE_HIT(rctx)) {
        cl = &sctx->cacheline[rctx->get_from];
        if (loc != NULL) {
            *loc = cl->loc;
            /* no downlink data to report to user */
            loc->dl_app_data = NULL;
            loc->dl_app_data_len = 0;
        }
        if (cache_hit != NULL)
            *cache_hit = true;
#if SKY_DEBUG
        time_t cached_time = loc->time;
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
            "Location from cache: %d.%06d,%d.%06d hpe:%d source:%sctx age:%d Sec", (int)loc->lat,
            (int)fabs(round(1000000 * (loc->lat - (int)loc->lat))), (int)loc->lon,
            (int)fabs(round(1000000 * (loc->lon - (int)loc->lon))), loc->hpe, sky_psource(loc),
            (rctx->header.time - cached_time));
#endif
        return set_error_status(sky_errno, SKY_ERROR_NONE);
    }
#endif
    rctx->hit = false;
    sctx->cache_hits = 0; /* report 0 for cache miss */
    if (cache_hit != NULL)
        *cache_hit = false;

    loc->location_source = SKY_LOCATION_SOURCE_UNKNOWN;
    loc->location_status = SKY_LOCATION_STATUS_UNABLE_TO_LOCATE;
    return set_error_status(sky_errno, SKY_ERROR_NONE);
}

/*! \brief force cache miss status
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_ignore_cache_hit(Sky_rctx_t *rctx, Sky_errno_t *sky_errno)
{
#if SANITY_CHECKS
    if (!validate_request_ctx(rctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
#endif

#if CACHE_SIZE
    if (IS_CACHE_HIT(rctx)) {
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Clearing hit status for cacheline %d!", rctx->get_from);
        rctx->hit = false;
    } else {
        LOGFMT(rctx, SKY_LOG_LEVEL_WARNING, "No cache entry selected to clear");
    }
    return set_error_status(sky_errno, SKY_ERROR_NONE);
#else
    LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Can't ignore cache with no cache");
    return set_error_status(sky_errno, SKY_ERROR_BAD_SESSION_CTX);
#endif
}

/*! \brief Determines the required size of the network request buffer
 *
 *  Size is determined by doing a dry run of encoding the request
 *
 *  @param rctx Skyhook request context
 *  @param size parameter which will be set to the size value
 *  @param sky_errno skyErrno is set to the error code
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_sizeof_request_buf(Sky_rctx_t *rctx, uint32_t *size, Sky_errno_t *sky_errno)
{
    int rc, rq_config = false;
    Sky_sctx_t *sctx = rctx->session;
#if CACHE_SIZE
    Sky_cacheline_t *cl;
#endif

#if SANITY_CHECKS
    if (!validate_request_ctx(rctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
#endif

    if (size == NULL)
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* determine whether request_client_conf should be true in request message */
    rq_config = CONFIG(sctx, last_config_time) == CONFIG_UPDATE_DUE ||
                rctx->header.time == TIME_UNAVAILABLE ||
                (rctx->header.time - CONFIG(sctx, last_config_time)) > CONFIG_REQUEST_INTERVAL;
    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Request config: %s",
        rq_config && CONFIG(sctx, last_config_time) != CONFIG_UPDATE_DUE ?
            "Timeout" :
            rq_config ? "Forced" : "No");

    if (rq_config)
        CONFIG(sctx, last_config_time) = CONFIG_UPDATE_DUE; /* request on next serialize */

        /* check cache against beacons for match
     * setting from_cache if a matching cacheline is found
     * */
#if CACHE_SIZE
    if (IS_CACHE_HIT(rctx)) {
        cl = &sctx->cacheline[rctx->get_from];

        /* cache hit */
        /* count of consecutive cache hits since last cache miss */
        if (sctx->cache_hits < 127) {
            sctx->cache_hits++;
            if (rctx->hit) {
                /* overwrite beacons in request rctx with cached beacons */
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "populate request rctx with cached beacons");
                NUM_BEACONS(rctx) = cl->num_beacons;
                NUM_APS(rctx) = cl->num_ap;
                for (int j = 0; j < NUM_BEACONS(rctx); j++)
                    rctx->beacon[j] = cl->beacon[j];
                rctx->gnss = cl->gnss;
            }
        } else {
            rctx->get_from = -1; /* force cache miss after 127 consecutive cache hits */
            sctx->cache_hits = 0; /* report 0 for cache miss */
        }
    } else {
        /* Trim any excess vap from request rctx i.e. total number of vap
         * in request rctx cannot exceed max that a request can carry
         */
        select_vap(rctx);
    }
#else
    rctx->get_from = -1; /* cache miss */
    sctx->cache_hits = 0; /* report 0 for cache miss */
#endif

    /* encode request into the bit bucket, just to determine the length of the
     * encoded message */
    rc = serialize_request(rctx, NULL, 0, SW_VERSION, rq_config);

    if (rc > 0) {
        *size = (uint32_t)rc;
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "sizeof request %d", rc);
        return set_error_status(sky_errno, SKY_ERROR_NONE);
    } else {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Failed to size request");
        return set_error_status(sky_errno, SKY_ERROR_ENCODE_ERROR);
    }
}

/*! \brief generate a Skyhook request from the request context
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param request_buf Request to send to Skyhook server
 *  @param bufsize Request size in bytes
 *  @param response_size the space required to hold the server response
 *
 *  @return SKY_SUCCESS if request was encoded,
 *          SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_encode_request(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, void *request_buf,
    uint32_t bufsize, uint32_t *response_size)
{
    int rc;
    Sky_sctx_t *sctx = rctx->session;

#if SANITY_CHECKS
    if (!validate_request_ctx(rctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_REQUEST_CTX);
#endif

    if (backoff_violation(rctx, rctx->header.time)) {
        return set_error_status(sky_errno, SKY_ERROR_SERVICE_DENIED);
    }

    /* There must be at least one beacon */
    if (NUM_BEACONS(rctx) == 0 && !has_gnss(rctx)) {
        *sky_errno = SKY_ERROR_NO_BEACONS;
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Cannot process request with no beacons");
        return set_error_status(sky_errno, SKY_ERROR_NO_BEACONS);
    }

    if (request_buf == NULL) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Buffer pointer is bad");
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Processing request with %d beacons into %d byte buffer",
        NUM_BEACONS(rctx), bufsize);

#if SKY_LOGGING
    if (CONFIG(sctx, last_config_time) == CONFIG_UPDATE_DUE)
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Requesting new dynamic configuration parameters");
    else
        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Configuration parameter: %d",
            CONFIG(sctx, last_config_time));
#endif

    /* encode request */
    rc = serialize_request(rctx, request_buf, bufsize, SW_VERSION,
        CONFIG(sctx, last_config_time) == CONFIG_UPDATE_DUE);

    if (rc > 0) {
        *response_size = get_maximum_response_size();

        LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG, "Request buffer of %d bytes prepared %s", rc,
            rctx->hit ? "for cache hit" : "from request rctx");
        LOG_BUFFER(rctx, SKY_LOG_LEVEL_DEBUG, request_buf, rc);
        return set_error_status(sky_errno, SKY_ERROR_NONE);
    } else {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Failed to encode request");
        return set_error_status(sky_errno, SKY_ERROR_ENCODE_ERROR);
    }
}

/*! \brief decodes a Skyhook server response
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param response_buf buffer holding the skyhook server response
 *  @param bufsize Request size in bytes
 *  @param loc where to save device latitude, longitude etc from cache if known
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_decode_response(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, void *response_buf,
    uint32_t bufsize, Sky_location_t *loc)
{
    Sky_sctx_t *sctx = rctx->session;
    time_t now = (*sctx->timefn)(NULL);

    if (loc == NULL || response_buf == NULL || bufsize == 0) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Bad parameters");
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* note the time of this server response in the request context */
    sctx->header.time = now;
    sctx->header.crc32 = sky_crc32(
        &sctx->header.magic, (uint8_t *)&sctx->header.crc32 - (uint8_t *)&sctx->header.magic);

    /* decode response to get lat/lon */
    if (deserialize_response(rctx, response_buf, bufsize, loc) < 0) {
        LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Response decode failure");
        return set_error_status(sky_errno, SKY_ERROR_DECODE_ERROR);
    } else {
        /* if this is a response from a cache miss, clear cache_hits count */
        if (IS_CACHE_MISS(rctx))
            sctx->cache_hits = 0;

        /* set error status based on server error code */
        switch (loc->location_status) {
        case SKY_LOCATION_STATUS_SUCCESS:
            /* Server reports success so clear backoff period tracking */
            sctx->backoff = SKY_ERROR_NONE;
            loc->time = (*sctx->timefn)(NULL);

#if CACHE_SIZE > 0
            /* Add location and current beacons to Cache */
            if (sky_plugin_add_to_cache(rctx, sky_errno, loc) != SKY_SUCCESS)
                LOGFMT(rctx, SKY_LOG_LEVEL_WARNING, "failed to add to cache");
            if (IS_CACHE_MISS(rctx))
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                    "Location from server: %d.%06d,%d.%06d hpe:%d, Source:%s app-data:%d",
                    (int)loc->lat, (int)fabs(round(1000000.0 * (loc->lat - (int)loc->lat))),
                    (int)loc->lon, (int)fabs(round(1000000.0 * (loc->lon - (int)loc->lon))),
                    loc->hpe, sky_psource(loc), loc->dl_app_data_len);
            else
                LOGFMT(rctx, SKY_LOG_LEVEL_DEBUG,
                    "Server location for cache hit: %d.%06d,%d.%06d hpe:%d, Source:%s app-data:%d",
                    (int)loc->lat, (int)fabs(round(1000000.0 * (loc->lat - (int)loc->lat))),
                    (int)loc->lon, (int)fabs(round(1000000.0 * (loc->lon - (int)loc->lon))),
                    loc->hpe, sky_psource(loc), loc->dl_app_data_len);
#endif

            return set_error_status(sky_errno, SKY_ERROR_NONE);
        case SKY_LOCATION_STATUS_AUTH_ERROR:
            LOGFMT(rctx, SKY_LOG_LEVEL_ERROR, "Authentication required, retry.");
            if (rctx->auth_state ==
                STATE_TBR_DISABLED) { /* Non-TBR Location request failed auth, error */
                return set_error_status(sky_errno, SKY_ERROR_AUTH);
            } else if (rctx->auth_state ==
                       STATE_TBR_REGISTERED) { /* Location request failed auth, retry */
                sctx->backoff = SKY_ERROR_NONE;
                return set_error_status(sky_errno, SKY_AUTH_RETRY);
            } else if (sctx->backoff ==
                       SKY_ERROR_NONE) /* Registration request failed auth, retry */
                return set_error_status(sky_errno, (sctx->backoff = SKY_AUTH_RETRY));
            else if (sctx->backoff ==
                     SKY_AUTH_RETRY) /* Registration request failed again, retry after 8hr */
                return set_error_status(sky_errno, (sctx->backoff = SKY_AUTH_RETRY_8H));
            else if (sctx->backoff ==
                     SKY_AUTH_RETRY_8H) /* Registration request failed again, retry after 16hr */
                return set_error_status(sky_errno, (sctx->backoff = SKY_AUTH_RETRY_16H));
            else if (sctx->backoff ==
                     SKY_AUTH_RETRY_16H) /* Registration request failed again, retry after 24hr */
                return set_error_status(sky_errno, (sctx->backoff = SKY_AUTH_RETRY_1D));
            else
                return set_error_status(sky_errno, (sctx->backoff = SKY_AUTH_RETRY_30D));
        case SKY_LOCATION_STATUS_BAD_PARTNER_ID_ERROR:
        case SKY_LOCATION_STATUS_DECODE_ERROR:
            return set_error_status(sky_errno, SKY_ERROR_AUTH);
        case SKY_LOCATION_STATUS_UNABLE_TO_LOCATE:
            return set_error_status(sky_errno, SKY_ERROR_LOCATION_UNKNOWN);
        default:
            return set_error_status(sky_errno, SKY_ERROR_SERVER_ERROR);
        }
    }
}

/*! \brief query value of a configuration parameter
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno Error code for which to provide descriptive string
 *  @param name configuration parameter to read
 *  @param value Pointer to location where value should be returned
 *
 *  @return SKY_SUCCESS if value was updated, SKY_ERROR otherwise
 */
Sky_status_t sky_get_option(
    Sky_rctx_t *rctx, Sky_errno_t *sky_errno, Sky_config_name_t name, uint32_t *value)
{
    Sky_sctx_t *sctx = rctx->session;
    Sky_errno_t err = SKY_ERROR_NONE;

    switch (name) {
    case CONF_TOTAL_BEACONS:
        *value = sctx->config.total_beacons;
        break;
    case CONF_MAX_AP_BEACONS:
        *value = sctx->config.max_ap_beacons;
        break;
    case CONF_CACHE_AGE_THRESHOLD:
        *value = sctx->config.cache_age_threshold;
        break;
    case CONF_CACHE_BEACON_THRESHOLD:
        *value = sctx->config.cache_beacon_threshold;
        break;
    case CONF_CACHE_NEG_RSSI_THRESHOLD:
        *value = sctx->config.cache_neg_rssi_threshold;
        break;
    case CONF_CACHE_MATCH_ALL_THRESHOLD:
        *value = sctx->config.cache_match_all_threshold;
        break;
    case CONF_CACHE_MATCH_USED_THRESHOLD:
        *value = sctx->config.cache_match_used_threshold;
        break;
    case CONF_MAX_VAP_PER_AP:
        *value = sctx->config.max_vap_per_ap;
        break;
    case CONF_MAX_VAP_PER_RQ:
        *value = sctx->config.max_vap_per_rq;
        break;
    case CONF_LOGGING_LEVEL:
        *value = sctx->min_level;
        break;
    default:
        err = SKY_ERROR_BAD_PARAMETERS;
        break;
    }
    return set_error_status(sky_errno, err);
}

/*! \brief set value of a configuration parameter
 *
 *  @param rctx Skyhook request context
 *  @param sky_errno Error code for which to provide descriptive string
 *  @param name configuration parameter to set
 *  @param value value to set
 *
 *  @return SKY_SUCCESS if value was updated, SKY_ERROR otherwise
 */
Sky_status_t sky_set_option(
    Sky_rctx_t *rctx, Sky_errno_t *sky_errno, Sky_config_name_t name, int32_t value)
{
    Sky_sctx_t *sctx = rctx->session;
    Sky_errno_t err = SKY_ERROR_NONE;

    switch (name) {
    case CONF_TOTAL_BEACONS:
        if (value > TOTAL_BEACONS || value < 6) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.total_beacons = value;
        break;
    case CONF_MAX_AP_BEACONS:
        if (value > MAX_AP_BEACONS) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.max_ap_beacons = value;
        break;
    case CONF_CACHE_AGE_THRESHOLD:
        sctx->config.cache_age_threshold = value;
        break;
    case CONF_CACHE_BEACON_THRESHOLD:
        if (value > TOTAL_BEACONS) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.cache_beacon_threshold = value;
        break;
    case CONF_CACHE_NEG_RSSI_THRESHOLD:
        if (value > 128) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.cache_neg_rssi_threshold = value;
        break;
    case CONF_CACHE_MATCH_ALL_THRESHOLD:
        if (value > 100) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.cache_match_all_threshold = value;
        break;
    case CONF_CACHE_MATCH_USED_THRESHOLD:
        if (value > 100) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.cache_match_used_threshold = value;
        break;
    case CONF_MAX_VAP_PER_AP:
        if (value > TOTAL_BEACONS) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.max_vap_per_ap = value;
        break;
    case CONF_MAX_VAP_PER_RQ:
        if (value > TOTAL_BEACONS) {
            err = SKY_ERROR_BAD_PARAMETERS;
            break;
        }
        sctx->config.max_vap_per_rq = value;
        break;
    case CONF_LOGGING_LEVEL:
        sctx->min_level = value;
        break;
    default:
        err = SKY_ERROR_BAD_PARAMETERS;
    }
    return set_error_status(sky_errno, err);
}

/*! \brief returns a string which describes the meaning of sky_errno codes
 *
 *  @param sky_errno Error code for which to provide descriptive string
 *
 *  @return pointer to string or NULL if the code is invalid
 */
char *sky_perror(Sky_errno_t sky_errno)
{
    register char *str = NULL;
    switch (sky_errno) {
    case SKY_ERROR_NONE:
        str = "No error";
        break;
    case SKY_ERROR_NEVER_OPEN:
        str = "Must open first";
        break;
    case SKY_ERROR_ALREADY_OPEN:
        str = "Must close before opening with new parameters";
        break;
    case SKY_ERROR_BAD_PARAMETERS:
        str = "Validation of parameters failed";
        break;
    case SKY_ERROR_BAD_REQUEST_CTX:
        str = "The request ctx buffer is corrupt";
        break;
    case SKY_ERROR_BAD_SESSION_CTX:
        str = "The session buffer is corrupt";
        break;
    case SKY_ERROR_ENCODE_ERROR:
        str = "The request could not be encoded";
        break;
    case SKY_ERROR_DECODE_ERROR:
        str = "The response could not be decoded";
        break;
    case SKY_ERROR_RESOURCE_UNAVAILABLE:
        str = "Can\'t allocate non-volatile storage";
        break;
    case SKY_ERROR_NO_BEACONS:
        str = "At least one beacon must be added";
        break;
    case SKY_ERROR_LOCATION_UNKNOWN:
        str = "Server failed to determine location";
        break;
    case SKY_ERROR_SERVER_ERROR:
        str = "Server responded with an error";
        break;
    case SKY_ERROR_NO_PLUGIN:
        str = "At least one plugin must be registered";
        break;
    case SKY_ERROR_INTERNAL:
        str = "An unexpected error occured";
        break;
    case SKY_ERROR_SERVICE_DENIED:
        str = "Service blocked due to repeated errors";
        break;
    case SKY_AUTH_RETRY:
        str = "Operation unauthorized, retry now";
        break;
    case SKY_AUTH_RETRY_8H:
        str = "Operation unauthorized, retry in 8 hours";
        break;
    case SKY_AUTH_RETRY_16H:
        str = "Operation unauthorized, retry in 16 hours";
        break;
    case SKY_AUTH_RETRY_1D:
        str = "Operation unauthorized, retry in 24 hours";
        break;
    case SKY_AUTH_RETRY_30D:
        str = "Operation unauthorized, retry in a month";
        break;
    case SKY_AUTH_NEEDS_TIME:
        str = "Operation needs good time of day";
        break;
    case SKY_ERROR_AUTH:
        str = "Operation failed due to authentication error";
        break;
    case SKY_ERROR_BAD_TIME:
        str = "Operation failed due to timestamp out of range";
        break;
    default:
        str = "Unknown error code";
        break;
    }
    return str;
}

/*! \brief returns a string which describes the meaning of Sky_loc_status_t codes
 *
 *  @param status Error code for which to provide descriptive string
 *
 *  @return pointer to string or NULL if the code is invalid
 */
char *sky_pserver_status(Sky_loc_status_t status)
{
    register char *str = NULL;
    switch (status) {
    case SKY_LOCATION_STATUS_SUCCESS:
        str = "Server success";
        break;
    case SKY_LOCATION_STATUS_UNSPECIFIED_ERROR:
        str = "Server reports unspecified error";
        break;
    case SKY_LOCATION_STATUS_BAD_PARTNER_ID_ERROR:
        str = "Server reports bad partner id error";
        break;
    case SKY_LOCATION_STATUS_DECODE_ERROR:
        str = "Server reports error decoding request body";
        break;
    case SKY_LOCATION_STATUS_API_SERVER_ERROR:
        str = "Server error determining location";
        break;
    case SKY_LOCATION_STATUS_AUTH_ERROR:
        str = "Server error authentication error";
        break;
    case SKY_LOCATION_STATUS_UNABLE_TO_LOCATE:
        str = "Server reports unable to determine location";
        break;
    default:
        str = "Unknown server status";
        break;
    }
    return str;
}

/*! \brief returns a string which describes the meaning of Sky_beacon_type_t
 *
 *  @param b beacon type
 *
 *  @return pointer to string or NULL if the code is invalid
 */
char *sky_pbeacon(Beacon_t *b)
{
    if (is_cell_type(b) && b->cell.id2 == SKY_UNKNOWN_ID2) {
        switch (b->h.type) {
        case SKY_BEACON_LTE:
            return "LTE-NMR";
        case SKY_BEACON_NBIOT:
            return "NB-IoT-NMR";
        case SKY_BEACON_UMTS:
            return "UMTS-NMR";
        case SKY_BEACON_NR:
            return "NR-NMR";
        default:
            return "\?\?\?-NMR";
        }
    } else {
        switch (b->h.type) {
        case SKY_BEACON_AP:
            return "Wi-Fi";
        case SKY_BEACON_BLE:
            return "BLE";
        case SKY_BEACON_CDMA:
            return "CDMA";
        case SKY_BEACON_GSM:
            return "GSM";
        case SKY_BEACON_LTE:
            return "LTE";
        case SKY_BEACON_NBIOT:
            return "NB-IoT";
        case SKY_BEACON_UMTS:
            return "UMTS";
        case SKY_BEACON_NR:
            return "NR";
        default:
            return "\?\?\?";
        }
    }
}

/*! \brief returns a string which describes the source of a location
 *
 *  @param l pointer to location structure
 *
 *  @return pointer to string or NULL if the code is invalid
 */
char *sky_psource(struct sky_location *l)
{
    if (l != NULL) {
        switch (l->location_source) {
        case SKY_LOCATION_SOURCE_CELL:
            return "Cell";
        case SKY_LOCATION_SOURCE_GNSS:
            return "GNSS";
        case SKY_LOCATION_SOURCE_HYBRID:
            return "Hybrid";
        case SKY_LOCATION_SOURCE_WIFI:
            return "Wi-Fi";
        default:
            return "\?\?\?";
        }
    }
    return NULL;
}

/*! \brief clean up library resources
 *
 *  @param sctx Skyhook session context
 *  @param sky_errno skyErrno is set to the error code
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_close(Sky_sctx_t *sctx, Sky_errno_t *sky_errno)
{
    if (!sctx->open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);
    sctx->open_flag = false;

    return set_error_status(sky_errno, SKY_ERROR_NONE);
}

    /*******************************************************************************
 * Static helper functions
 ******************************************************************************/

#if SANITY_CHECKS
/*! \brief sanity check the device_id
 *
 *  @param device_id this is expected to be a binary mac address
 *  @param id_len length of device_id
 *
 *  @return true or false
 */
static bool validate_device_id(const uint8_t *device_id, uint32_t id_len)
{
    if (device_id == NULL || id_len > MAX_DEVICE_ID)
        return false;
    else
        return true;
}
#endif

/*! \brief safely return bounded length of string
 *
 *  @param pointer to string
 *  @param maximum number of chars to scan
 *
 *  @return length of string (up to length provided)
 */
static size_t strnlen_(char *s, size_t maxlen)
{
    char *p;

    if ((p = memchr(s, '\0', maxlen)) != NULL) {
        return p - s;
    } else
        return maxlen;
}

#ifdef UNITTESTS

#include "libel.ut.c"

#endif
