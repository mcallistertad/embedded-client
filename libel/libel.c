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
#include <time.h>
#include <math.h>
#include <ctype.h>
#define SKY_LIBEL
#include "libel.h"
#include "proto.h"

/* A monotonically increasing version number intended to track the client
 * software version, and which is sent to the server in each request. Clumsier
 * than just including the Git version string (since it will need to be updated
 * manually for every release) but cheaper bandwidth-wise.
 */
#define SW_VERSION 15

/* Interval in seconds between requests for config params */
#define CONFIG_REQUEST_INTERVAL (24 * SECONDS_IN_HOUR) /* 24 hours */

/* The following definition is intended to be changed only for QA purposes */
#define BACKOFF_UNITS_PER_HR 3600 // time in seconds

static Sky_state_t state;

/*! \brief keep track of when the user has opened the library */
static uint32_t sky_open_flag = 0;

/*! \brief keep track of user handler functions */
static Sky_randfn_t sky_rand_bytes;
static Sky_loggerfn_t sky_logf;
static Sky_log_level_t sky_min_level;
static Sky_timefn_t sky_time;
static bool sky_debounce;

/*! \brief base of plugin chain */
static Sky_plugin_table_t *sky_plugins = NULL;

/* Local functions */
static bool validate_device_id(uint8_t *device_id, uint32_t id_len);
static bool validate_partner_id(uint32_t partner_id);
static bool validate_aes_key(uint8_t aes_key[AES_SIZE]);
static size_t strnlen_(char *s, size_t maxlen);
static char *sky_psource(struct sky_location *l);

/*! \brief Copy state buffer
 *
 *  Note: Old state may have less dynamic configuration parameters
 *
 *  @param sky_state Pointer to the old state buffer
 *
 *  @return sky_status_t SKY_SUCCESS or SKY_ERROR
 */
static Sky_status_t copy_state(Sky_errno_t *sky_errno, Sky_state_t *dest, Sky_state_t *src)
{
    bool update = false;

    if (src != NULL) {
        if (src->header.size < sizeof(Sky_state_t)) {
            memset((uint8_t *)dest + src->header.size, 0, dest->header.size - src->header.size);
            update = true;
        } else if (src->header.size > sizeof(Sky_state_t))
            return set_error_status(sky_errno, SKY_ERROR_BAD_STATE);
        memmove(dest, src, src->header.size);
        config_defaults(dest);
        if (update)
            dest->config.last_config_time = 0; /* force an update */
        return set_error_status(sky_errno, SKY_ERROR_NONE);
    }
    return set_error_status(sky_errno, SKY_ERROR_BAD_STATE);
}

/*! \brief Initialize Skyhook library and verify access to resources
 *
 *  @param sky_errno if sky_open returns failure, sky_errno is set to the error code
 *  @param device_id Device unique ID (example mac address of the device)
 *  @param id_len length if the Device ID, typically 6, Max 16 bytes
 *  @param partner_id Skyhook assigned credentials
 *  @param aes_key Skyhook assigned encryption key
 *  @param sku unique name of device family, must be non-empty to enable TBR Auth
 *  @param cc County code where device is being registered, 0 if unknown
 *  @param state_buf pointer to a state buffer (provided by sky_close) or NULL
 *  @param min_level logging function is called for msg with equal or greater level
 *  @param logf pointer to logging function
 *  @param rand_bytes pointer to random function
 *  @param gettime pointer to time function
 *  @param debounce true if cached beacons should be added to request rather than newly scanned
 *
 *  @return sky_status_t SKY_SUCCESS or SKY_ERROR
 *
 *  sky_open can be called many times with the same parameters. This does
 *  nothing and returns SKY_SUCCESS. However, sky_close must be called
 *  in order to change the parameter values. Device ID length will
 *  be truncated to 16 if larger, without causing an error.
 */
Sky_status_t sky_open(Sky_errno_t *sky_errno, uint8_t *device_id, uint32_t id_len,
    uint32_t partner_id, uint8_t aes_key[AES_KEYLEN], char *sku, uint32_t cc, void *state_buf,
    Sky_log_level_t min_level, Sky_loggerfn_t logf, Sky_randfn_t rand_bytes, Sky_timefn_t gettime,
    bool debounce)
{
#if SKY_DEBUG
    char buf[SKY_LOG_LENGTH];
#endif
    Sky_state_t *sky_state = state_buf;
    uint32_t sku_len = 0;

    memset(&state, 0, sizeof(state));
    /* Only consider up to 16 bytes. Ignore any extra */
    id_len = (id_len > MAX_DEVICE_ID) ? MAX_DEVICE_ID : id_len;
    sku = !sku ? "" : sku;
    sku_len = strnlen_(sku, MAX_SKU_LEN);

    if (sky_state != NULL && !validate_cache(sky_state, logf)) {
        if (logf != NULL && SKY_LOG_LEVEL_WARNING <= min_level)
            (*logf)(SKY_LOG_LEVEL_WARNING, "Invalid state buffer was ignored!");
        sky_state = NULL;
    }

    sky_min_level = min_level;
    sky_logf = logf;
    sky_rand_bytes = rand_bytes == NULL ? sky_rand_fn : rand_bytes;
    sky_time = (gettime == NULL) ? &time : gettime;
    sky_debounce = debounce;

    if (sky_register_plugins(&sky_plugins) != SKY_SUCCESS)
        return set_error_status(sky_errno, SKY_ERROR_NO_PLUGIN);

    /* if open already */
    if (sky_open_flag && sky_state) {
        /* if library is already open and sky_open parameters match those we already have, */
        /* we can ignore this call to sky_open, otherwise report error already open */
        if (memcmp(device_id, sky_state->sky_device_id, id_len) == 0 &&
            id_len == sky_state->sky_id_len && sky_state->header.size == sizeof(state) &&
            partner_id == sky_state->sky_partner_id &&
            memcmp(aes_key, sky_state->sky_aes_key, sizeof(sky_state->sky_aes_key)) == 0 &&
            strcmp(sku, sky_state->sky_sku) == 0 && cc == sky_state->sky_cc)
            return set_error_status(sky_errno, SKY_ERROR_NONE);
        else
            return set_error_status(sky_errno, SKY_ERROR_ALREADY_OPEN);
    } else if (!sky_state || copy_state(sky_errno, &state, sky_state) != SKY_SUCCESS) {
        memset(&state, 0, sizeof(state));
        state.header.magic = SKY_MAGIC;
        state.header.size = sizeof(state);
        state.header.time = (uint32_t)(*sky_time)(NULL);
        state.header.crc32 = sky_crc32(
            &state.header.magic, (uint8_t *)&state.header.crc32 - (uint8_t *)&state.header.magic);
#if CACHE_SIZE
        state.len = CACHE_SIZE;
        for (int i = 0; i < CACHE_SIZE; i++) {
            for (int j = 0; j < TOTAL_BEACONS; j++) {
                state.cacheline[i].beacon[j].h.magic = BEACON_MAGIC;
                state.cacheline[i].beacon[j].h.type = SKY_BEACON_MAX;
            }
        }
#endif
#if SKY_DEBUG
    } else {
        if (logf != NULL && SKY_LOG_LEVEL_DEBUG <= min_level) {
            snprintf(buf, sizeof(buf), "%s:%s() State buffer with CRC 0x%08X, size %d restored",
                sky_basename(__FILE__), __FUNCTION__, sky_crc32(sky_state, sky_state->header.size),
                sky_state->header.size);
            (*logf)(SKY_LOG_LEVEL_DEBUG, buf);
        }
#endif
    }
    config_defaults(&state);

    /* Sanity check */
    if (!validate_device_id(device_id, id_len) || !validate_partner_id(partner_id) ||
        !validate_aes_key(aes_key))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    state.sky_id_len = id_len;
    memcpy(state.sky_device_id, device_id, id_len);
    state.sky_partner_id = partner_id;
    memcpy(state.sky_aes_key, aes_key, sizeof(state.sky_aes_key));
    if (sku_len) {
        strncpy(state.sky_sku, sku, MAX_SKU_LEN); /* Only pass up to maximum characters of sku */
        state.sky_sku[MAX_SKU_LEN] = '\0'; /* Guarentee sku is null terminated */
        state.sky_cc = cc;
    }
    sky_open_flag = true;

    if (logf != NULL && SKY_LOG_LEVEL_DEBUG <= min_level)
        (*logf)(SKY_LOG_LEVEL_DEBUG, "Skyhook Embedded Library (Version: " VERSION ")");

    return set_error_status(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Determines the size of the non-volatile memory state buffer
 *
 *  @param sky_state Pointer to state buffer
 *
 *  @return Size of state buffer or 0 to indicate that the buffer was invalid
 */
int32_t sky_sizeof_state(void *sky_state)
{
    Sky_state_t *s = sky_state;

    /* Cache space required
     *
     * header - Magic number, size of space, checksum
     * body - number of entries
     */
    if (s->header.magic != SKY_MAGIC) {
        return 0;
    } else if (s->header.crc32 == sky_crc32(&s->header.magic,
                                      (uint8_t *)&s->header.crc32 - (uint8_t *)&s->header.magic)) {
        return s->header.size;
    }
    return 0;
}

/*! \brief Determines the size of the workspace required to build request
 *
 *  @return Size of state buffer or 0 to indicate that the buffer was invalid
 */
int32_t sky_sizeof_workspace(void)
{
    /* Total space required
     *
     * header - Magic number, size of space, checksum
     * body - number of beacons, beacon data, gnss, request buffer
     */
    return sizeof(Sky_ctx_t);
}

/*! \brief Validate backoff period
 *
 *  @param ctx Pointer to workspace provided by user
 *  @param now current time
 *
 *  @return false if backoff period has not passed
 */
static bool backoff_violation(Sky_ctx_t *ctx, time_t now)
{
    (void)ctx;
    /* Enforce backoff period, check that enough time has passed since last request was received */
    if (state.backoff != SKY_ERROR_NONE) { /* Retry backoff in progress */
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Backoff: %s, %d seconds so far",
            sky_perror(state.backoff), (int)(now - state.header.time));
        switch (state.backoff) {
        case SKY_AUTH_RETRY_8H:
            if (now - state.header.time < (8 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        case SKY_AUTH_RETRY_16H:
            if (now - state.header.time < (16 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        case SKY_AUTH_RETRY_1D:
            if (now - state.header.time < (24 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        case SKY_AUTH_RETRY_30D:
            if (now - state.header.time < (30 * 24 * BACKOFF_UNITS_PER_HR))
                return true;
            break;
        default:
            break;
        }
    }
    return false;
}

/*! \brief Initializes the workspace provided ready to build a request
 *
 *  @param workspace_buf Pointer to workspace provided by user
 *  @param bufsize Workspace buffer size (from sky_sizeof_workspace)
 *  @param sky_errno Pointer to error code
 *
 *  @return Pointer to the initialized workspace context buffer or NULL
 */
Sky_ctx_t *sky_new_request(void *workspace_buf, uint32_t bufsize, uint8_t *ul_app_data,
    uint32_t ul_app_data_len, Sky_errno_t *sky_errno)
{
    int i;
    Sky_ctx_t *ctx = (Sky_ctx_t *)workspace_buf;
    time_t now;

    if (!sky_open_flag) {
        *sky_errno = SKY_ERROR_NEVER_OPEN;
        return NULL;
    }
    if (bufsize != (uint32_t)sky_sizeof_workspace() || workspace_buf == NULL) {
        *sky_errno = SKY_ERROR_BAD_PARAMETERS;
        return NULL;
    }
    now = (uint32_t)(*sky_time)(NULL);

    memset(ctx, 0, bufsize);
    /* update header in workspace */
    ctx->header.magic = SKY_MAGIC;
    ctx->header.size = bufsize;
    ctx->header.time = now;
    ctx->header.crc32 = sky_crc32(
        &ctx->header.magic, (uint8_t *)&ctx->header.crc32 - (uint8_t *)&ctx->header.magic);

    ctx->state = &state;
    ctx->min_level = sky_min_level;
    ctx->logf = sky_logf;
    ctx->rand_bytes = sky_rand_bytes;
    ctx->gettime = sky_time;
    ctx->plugin = sky_plugins;
    ctx->debounce = sky_debounce;
    ctx->auth_state = !is_tbr_enabled(ctx) ?
                          STATE_TBR_DISABLED :
                          ctx->state->sky_token_id == TBR_TOKEN_UNKNOWN ? STATE_TBR_UNREGISTERED :
                                                                          STATE_TBR_REGISTERED;
    ctx->gps.lat = NAN; /* empty */
    for (i = 0; i < TOTAL_BEACONS; i++) {
        ctx->beacon[i].h.magic = BEACON_MAGIC;
        ctx->beacon[i].h.type = SKY_BEACON_MAX;
    }

    if (backoff_violation(ctx, now)) {
        *sky_errno = SKY_ERROR_SERVICE_DENIED;
        return NULL;
    }

#if CACHE_SIZE
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d cachelines present", ctx->state->len);
    for (i = 0; i < CACHE_SIZE; i++) {
        if (state.cacheline[i].ap_len > CONFIG(ctx->state, max_ap_beacons) ||
            state.cacheline[i].len > CONFIG(ctx->state, total_beacons)) {
            state.cacheline[i].time = 0;
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "cache %d of %d cleared due to new Dynamic Parameters. Total beacons %d vs %d, AP %d vs %d",
                i, CACHE_SIZE, CONFIG(ctx->state, total_beacons), ctx->state->cacheline[i].len,
                CONFIG(ctx->state, max_ap_beacons), ctx->state->cacheline[i].ap_len);
        }
        if (ctx->state->cacheline[i].time &&
            (now - ctx->state->cacheline[i].time) >
                ctx->state->config.cache_age_threshold * SECONDS_IN_HOUR) {
            ctx->state->cacheline[i].time = 0;
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "cache %d of %d cleared due to age (%d)", i,
                CACHE_SIZE, now - ctx->state->cacheline[i].time);
        }
    }
    DUMP_CACHE(ctx);
#else
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "No cachelines present");
#endif
    ctx->state->sky_ul_app_data_len = ul_app_data_len;
    memcpy(ctx->state->sky_ul_app_data, ul_app_data, ul_app_data_len);
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Partner_id: %d, Sku: %s", ctx->state->sky_partner_id,
        ctx->state->sky_sku);
    dump_hex16(__FILE__, "Device_id", ctx, SKY_LOG_LEVEL_DEBUG, ctx->state->sky_device_id,
        ctx->state->sky_id_len, 0);
    DUMP_WORKSPACE(ctx);
    return ctx;
}

/*! \brief  Adds the wifi ap information to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param mac pointer to mac address of the Wi-Fi beacon
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rssi Received Signal Strength Intensity, -10 through -127, -1 if unknown
 *  @param frequency center frequency of channel in MHz, 2400 through 6000, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_ap_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, uint8_t mac[6],
    time_t timestamp, int16_t rssi, int32_t frequency, bool is_connected)
{
    Beacon_t b;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%02X:%02X:%02X:%02X:%02X:%02X, %d MHz, rssi %d, %sage %d",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], frequency, rssi,
        is_connected ? "serve " : "",
        (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));

    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    /* Create AP beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_AP;
    b.h.connected = is_connected;
    if (rssi > -10 || rssi < -127)
        rssi = -1;
    b.h.rssi = rssi;
    memcpy(b.ap.mac, mac, MAC_SIZE);
    /* If beacon has meaningful timestamp */
    /* scan was before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        b.h.age = ctx->header.time - timestamp;
    if (frequency < 2400 || frequency > 6000)
        frequency = 0; /* 0's not sent to server */
    b.ap.freq = frequency;
    b.ap.property.in_cache = false;
    b.ap.property.used = false;

    return add_beacon(ctx, sky_errno, &b);
}

/*! \brief Add an lte cell beacon to request context
 *
 *  @param ctx Skyhook request context
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
Sky_status_t sky_add_cell_lte_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int32_t tac,
    int64_t e_cellid, uint16_t mcc, uint16_t mnc, int16_t pci, int32_t earfcn, int32_t ta,
    time_t timestamp, int16_t rsrp, bool is_connected)
{
    Beacon_t b;

    if (mcc != SKY_UNKNOWN_ID1 && mnc != SKY_UNKNOWN_ID2 && e_cellid != SKY_UNKNOWN_ID4)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, %d, %d MHz, ta %d, rsrp %d, %sage %d",
            mcc, mnc, tac, e_cellid, pci, earfcn, ta, rsrp, is_connected ? "serve, " : "",
            (int)(ctx->header.time - timestamp));

    /* If at least one of the primary IDs is unvalued, then *all* primary IDs must
     * be unvalued (meaning user is attempting to add a neighbor cell). Partial
     * specification of primary IDs is considered an error.
     */
    if ((mcc == SKY_UNKNOWN_ID1 || mnc == SKY_UNKNOWN_ID2 || e_cellid == SKY_UNKNOWN_ID4) &&
        !(mcc == SKY_UNKNOWN_ID1 && mnc == SKY_UNKNOWN_ID2 && e_cellid == SKY_UNKNOWN_ID4))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* range check parameters */
    if ((mcc != SKY_UNKNOWN_ID1 && (mcc < 200 || mcc > 799)) ||
        (mnc != SKY_UNKNOWN_ID2 && mnc > 999) ||
        (tac != SKY_UNKNOWN_ID3 && (tac < 1 || tac > 65535)) ||
        (e_cellid != SKY_UNKNOWN_ID4 && (e_cellid < 0 || e_cellid > 268435455)) ||
        (pci != SKY_UNKNOWN_ID5 && pci > 503) || (earfcn != SKY_UNKNOWN_ID6 && earfcn > 262143) ||
        (ta != SKY_UNKNOWN_TA && (ta < 0 || ta > 7690)))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    /* Create LTE beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_LTE;
    b.h.connected = is_connected;
    if (rsrp > -40 || rsrp < -140)
        rsrp = -1;
    b.h.rssi = rsrp;
    /* If beacon has meaningful timestamp */
    /* scan was before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        b.h.age = ctx->header.time - timestamp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = tac;
    b.cell.id4 = e_cellid;
    b.cell.id5 = pci;
    b.cell.freq = earfcn;
    if (!is_cell_nmr(&b))
        b.cell.ta = ta;
    else
        b.cell.ta = SKY_UNKNOWN_TA;

    return add_beacon(ctx, sky_errno, &b);
}

/*! \brief Add an lte cell neighbor beacon to request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param pci mobile pci (0-503, SKY_UNKNOWN_ID5 if unknown)
 *  @param earfcn channel (0-45589, SKY_UNKNOWN_ID6 if unknown)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rsrp Received Signal Receive Power, range -140 to -40dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_lte_neighbor_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int16_t pci,
    int32_t earfcn, time_t timestamp, int16_t rsrp)
{
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, rsrp %d, age %d", pci, earfcn, rsrp,
        (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));
    return sky_add_cell_lte_beacon(ctx, sky_errno, SKY_UNKNOWN_ID3, SKY_UNKNOWN_ID4,
        SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2, pci, earfcn, SKY_UNKNOWN_TA, timestamp, rsrp, false);
}

/*! \brief Adds a gsm cell beacon to the request context
 *
 *  @param ctx Skyhook request context
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
Sky_status_t sky_add_cell_gsm_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int32_t lac,
    int64_t ci, uint16_t mcc, uint16_t mnc, int32_t ta, time_t timestamp, int16_t rssi,
    bool is_connected)
{
    Beacon_t b;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, ta %d, rssi %d, %sage %d", lac, ci, mcc,
        mnc, ta, rssi, is_connected ? "serve, " : "", (int)(ctx->header.time - timestamp));

    /* If at least one of the primary IDs is unvalued, then *all* primary IDs must
     * be unvalued (meaning user is attempting to add a neighbor cell). Partial
     * specification of primary IDs is considered an error.
     */
    if (mcc == SKY_UNKNOWN_ID1 || mnc == SKY_UNKNOWN_ID2 || lac == SKY_UNKNOWN_ID3 ||
        ci == SKY_UNKNOWN_ID4)
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* range check parameters */
    if (mcc < 200 || mcc > 799 || mnc > 999 || lac == 0 ||
        (ta != SKY_UNKNOWN_TA && (ta < 0 || ta > 63)))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    /* Create GSM beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_GSM;
    b.h.connected = is_connected;
    if (rssi > -32 || rssi < -128)
        rssi = -1;
    b.h.rssi = rssi;
    /* If beacon has meaningful timestamp */
    /* scan was before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        b.h.age = ctx->header.time - timestamp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = lac;
    b.cell.id4 = ci;
    b.cell.ta = ta;

    return add_beacon(ctx, sky_errno, &b);
}

/*! \brief Adds a umts cell beacon to the request context
 *
 *  @param ctx Skyhook request context
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
Sky_status_t sky_add_cell_umts_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int32_t lac,
    int64_t ucid, uint16_t mcc, uint16_t mnc, int16_t psc, int16_t uarfcn, time_t timestamp,
    int16_t rscp, bool is_connected)
{
    Beacon_t b;

    if (mcc != SKY_UNKNOWN_ID1 && mnc != SKY_UNKNOWN_ID2 && ucid != SKY_UNKNOWN_ID4)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, %d, %d MHz, rscp %d, %sage %d", mcc,
            mnc, lac, ucid, psc, uarfcn, rscp, is_connected ? "serve, " : "",
            (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));

    /* If at least one of the primary IDs is unvalued, then *all* primary IDs must
     * be unvalued (meaning user is attempting to add a neighbor cell). Partial
     * specification of primary IDs is considered an error.
     */
    if ((mcc == SKY_UNKNOWN_ID1 || mnc == SKY_UNKNOWN_ID2 || ucid == SKY_UNKNOWN_ID4) &&
        !(mcc == SKY_UNKNOWN_ID1 && mnc == SKY_UNKNOWN_ID2 && ucid == SKY_UNKNOWN_ID4))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* range check parameters */
    if ((mcc != SKY_UNKNOWN_ID1 && (mcc < 200 || mcc > 799)) ||
        (mnc != SKY_UNKNOWN_ID2 && (mnc > 999)) ||
        (ucid != SKY_UNKNOWN_ID4 && (ucid < 0 || ucid > 268435455)) ||
        (psc != SKY_UNKNOWN_ID5 && (psc < 0 || psc > 511)) ||
        (uarfcn != SKY_UNKNOWN_ID6 && (uarfcn < 412 || uarfcn > 10838)))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    /* Create UMTS beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_UMTS;
    b.h.connected = is_connected;
    if (rscp > -20 || rscp < -120)
        rscp = -1;
    b.h.rssi = rscp;
    /* If beacon has meaningful timestamp */
    /* scan was before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        b.h.age = ctx->header.time - timestamp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = lac;
    b.cell.id4 = ucid;
    b.cell.id5 = psc;

    return add_beacon(ctx, sky_errno, &b);
}

/*! \brief Adds a umts cell neighbor beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param psc primary scrambling code (0-511, SKY_UNKNOWN_ID5 if unknown)
 *  @param uarfcn channel (412-10833, SKY_UNKNOWN_ID6 if unknown)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rscp Received Signal Code Power, range -120dbm to -20dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_umts_neighbor_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int16_t psc,
    int16_t uarfcn, time_t timestamp, int16_t rscp)
{
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, rscp %d, age %d", psc, uarfcn, rscp,
        (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));
    return sky_add_cell_umts_beacon(ctx, sky_errno, SKY_UNKNOWN_ID3, SKY_UNKNOWN_ID4,
        SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2, psc, uarfcn, timestamp, rscp, false);
}

/*! \brief Adds a cdma cell beacon to the request context
 *
 *  @param ctx Skyhook request context
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
Sky_status_t sky_add_cell_cdma_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, uint32_t sid,
    int32_t nid, int64_t bsid, time_t timestamp, int16_t rssi, bool is_connected)
{
    Beacon_t b;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%u, %d, %lld, rssi %d, %sage %d", sid, nid, bsid, rssi,
        is_connected ? "serve, " : "",
        (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));

    /* Range check parameters */
    if (sid > 32767 || nid < 0 || nid > 65535 || bsid < 0 || bsid > 65535)
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    /* Create CDMA beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_CDMA;
    b.h.connected = is_connected;
    if (rssi > -49 || rssi < -140)
        rssi = -1;
    b.h.rssi = rssi;
    /* If beacon has meaningful timestamp */
    /* scan was before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        b.h.age = ctx->header.time - timestamp;
    b.cell.id2 = sid;
    b.cell.id3 = nid;
    b.cell.id4 = bsid;

    return add_beacon(ctx, sky_errno, &b);
}

/*! \brief Adds a nb_iot cell beacon to the request context
 *
 *  @param ctx Skyhook request context
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
Sky_status_t sky_add_cell_nb_iot_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, uint16_t mcc,
    uint16_t mnc, int64_t e_cellid, int32_t tac, int16_t ncid, int32_t earfcn, time_t timestamp,
    int16_t nrsrp, bool is_connected)
{
    Beacon_t b;

    if (mcc != SKY_UNKNOWN_ID1 && mnc != SKY_UNKNOWN_ID2 && e_cellid != SKY_UNKNOWN_ID4)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d, %lld, %d, %d MHz, nrsrp %d, %sage %d", mcc,
            mnc, tac, e_cellid, ncid, earfcn, nrsrp, is_connected ? "serve, " : "",
            (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));

    /* If at least one of the primary IDs is unvalued, then *all* primary IDs must
     * be unvalued (meaning user is attempting to add a neighbor cell). Partial
     * specification of primary IDs is considered an error.
     */
    if ((mcc == SKY_UNKNOWN_ID1 || mnc == SKY_UNKNOWN_ID2 || e_cellid == SKY_UNKNOWN_ID4) &&
        !(mcc == SKY_UNKNOWN_ID1 && mnc == SKY_UNKNOWN_ID2 && e_cellid == SKY_UNKNOWN_ID4))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* range check parameters */
    if ((mcc != SKY_UNKNOWN_ID1 && (mcc < 200 || mcc > 799)) ||
        (mnc != SKY_UNKNOWN_ID2 && mnc > 999) ||
        (tac != SKY_UNKNOWN_ID3 && (tac < 1 || tac > 65535)) ||
        (e_cellid != SKY_UNKNOWN_ID4 && (e_cellid < 0 || e_cellid > 268435455)) ||
        (ncid != SKY_UNKNOWN_ID5 && (ncid < 0 || ncid > 503)) ||
        (earfcn != SKY_UNKNOWN_ID6 && (earfcn < 0 || earfcn > 262143)))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    /* Create NB IoT beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_NBIOT;
    b.h.connected = is_connected;
    if (nrsrp > -44 || nrsrp < -156)
        nrsrp = -1;
    b.h.rssi = nrsrp;
    /* If beacon has meaningful timestamp */
    /* scan was before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        b.h.age = ctx->header.time - timestamp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = tac;
    b.cell.id4 = e_cellid;
    b.cell.id5 = ncid;
    b.cell.freq = earfcn;

    return add_beacon(ctx, sky_errno, &b);
}

/*! \brief Adds a nb_iot cell neighbor beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param ncid mobile cell ID (0-503, SKY_UNKNOWN_ID4 if unknown)
 *  @param earfcn channel (0-45589, SKY_UNKNOWN_ID6 if unknown)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param nrsrp Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_nb_iot_neighbor_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
    int16_t ncid, int32_t earfcn, time_t timestamp, int16_t nrsrp)
{
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, nrsrp %d, age %d", ncid, earfcn, nrsrp,
        (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));

    return sky_add_cell_nb_iot_beacon(ctx, sky_errno, SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2,
        SKY_UNKNOWN_ID4, SKY_UNKNOWN_ID3, ncid, earfcn, timestamp, nrsrp, false);
}

/*! \brief Adds a NR cell beacon to the request context
 *
 *  @param ctx          Skyhook request context
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

Sky_status_t sky_add_cell_nr_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, uint16_t mcc,
    uint16_t mnc, int64_t nci, int32_t tac, int16_t pci, int32_t nrarfcn, int32_t ta,
    time_t timestamp, int16_t csi_rsrp, bool is_connected)
{
    Beacon_t b;

    if (mcc != SKY_UNKNOWN_ID1 && mnc != SKY_UNKNOWN_ID2 && nci != SKY_UNKNOWN_ID4)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%u, %u, %d: %lld, %d, %d MHz, ta %d, rsrp %d, %sage %d",
            mcc, mnc, tac, nci, pci, nrarfcn, ta, csi_rsrp, is_connected ? "serve, " : "",
            (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));

    /* If at least one of the primary IDs is unvalued, then *all* primary IDs must
     * be unvalued (meaning user is attempting to add a neighbor cell). Partial
     * specification of primary IDs is considered an error.
     */
    if ((mcc == SKY_UNKNOWN_ID1 || mnc == SKY_UNKNOWN_ID2 || nci == SKY_UNKNOWN_ID4) &&
        !(mcc == SKY_UNKNOWN_ID1 && mnc == SKY_UNKNOWN_ID2 && nci == SKY_UNKNOWN_ID4))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* range check parameters */
    if ((mcc != SKY_UNKNOWN_ID1 && (mcc < 200 || mcc > 799)) ||
        (mnc != SKY_UNKNOWN_ID2 && mnc > 999) ||
        (nci != SKY_UNKNOWN_ID4 && (nci < 0 || nci > 68719476735)) ||
        (tac != SKY_UNKNOWN_ID3 && (tac < 1 || tac > 65535)) ||
        (pci != SKY_UNKNOWN_ID5 && (pci < 0 || pci > 1007)) ||
        (nrarfcn != SKY_UNKNOWN_ID6 && (nrarfcn < 0 || nrarfcn > 3279165)) ||
        (ta != SKY_UNKNOWN_TA && (ta < 0 || ta > 3846)))
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    /* Create NR beacon */
    memset(&b, 0, sizeof(b));
    b.h.magic = BEACON_MAGIC;
    b.h.type = SKY_BEACON_NR;
    b.h.connected = is_connected;
    /* If beacon has meaningful timestamp */
    /* scan was before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        b.h.age = ctx->header.time - timestamp;
    if (csi_rsrp > -40 || csi_rsrp < -140)
        csi_rsrp = -1;
    b.h.rssi = csi_rsrp;
    b.cell.id1 = mcc;
    b.cell.id2 = mnc;
    b.cell.id3 = tac;
    b.cell.id4 = nci;
    b.cell.id5 = pci;
    b.cell.freq = nrarfcn;
    if (!is_cell_nmr(&b))
        b.cell.ta = ta;
    else
        b.cell.ta = SKY_UNKNOWN_TA;

    return add_beacon(ctx, sky_errno, &b);
}

/*! \brief Adds a NR cell neighbor beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param physical cell ID (0-1007), SKY_UNKNOWN_ID5 if unknown
 *  @param nrarfcn channel (0-3279165), SKY_UNKNOWN_ID6 if unknown
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param nrsrp Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_add_cell_nr_neighbor_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, int16_t pci,
    int32_t nrarfcn, time_t timestamp, int16_t csi_rsrp)
{
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d, %d MHz, rsrp %d, age %d", pci, nrarfcn, csi_rsrp,
        (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));
    return sky_add_cell_nr_beacon(ctx, sky_errno, SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2,
        (int64_t)SKY_UNKNOWN_ID4, SKY_UNKNOWN_ID3, pci, nrarfcn, SKY_UNKNOWN_TA, timestamp,
        csi_rsrp, false);
}

/*! \brief Adds the position of the device from GNSS to the request context
 *
 *  @param ctx Skyhook request context
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
Sky_status_t sky_add_gnss(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, float lat, float lon,
    uint16_t hpe, float altitude, uint16_t vpe, float speed, float bearing, uint16_t nsat,
    time_t timestamp)
{
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d.%06d,%d.%06d, hpe %d, alt %d.%02d, vpe %d,", (int)lat,
        (int)fabs(round(1000000 * (lat - (int)lat))), (int)lon,
        (int)fabs(round(1000000 * (lon - (int)lon))), hpe, (int)altitude,
        (int)fabs(round(100 * (altitude - (int)altitude))), vpe);

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "%d.%01dm/s, bearing %d.%01d, nsat %d, age %d", (int)speed,
        (int)fabs(round(10 * (speed - (int)speed))), (int)bearing,
        (int)fabs(round(1 * (bearing - (int)bearing))), nsat,
        (int)timestamp == -1 ? -1 : (int)(ctx->header.time - timestamp));

    /* range check parameters */
    if (isnan(lat) || isnan(lon)) /* don't fail for empty gnss */
        return set_error_status(sky_errno, SKY_ERROR_NONE);

    if ((!isnan(altitude) && (altitude < -1200 || /* Lake Baikal */
                                 altitude > 8900)) || /* Everest */
        hpe < 0.0 ||
        hpe > 100000.0 || /* max range of cell tower */
        speed < 0.0 || speed > 343.0 || /* speed of sound */
        nsat < 4 || nsat > 100) /* 4 minimum to get fix, */
        /* 100 is conservative max gnss sat count */
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    ctx->gps.lat = lat;
    ctx->gps.lon = lon;
    ctx->gps.hpe = hpe;
    ctx->gps.alt = altitude;
    ctx->gps.vpe = vpe;
    ctx->gps.speed = speed;
    ctx->gps.bearing = bearing;
    ctx->gps.nsat = nsat;
    /* location was determined before sky_new_request and since Mar 1st 2019 */
    if (ctx->header.time > timestamp && timestamp > TIMESTAMP_2019_03_01)
        ctx->gps.age = ctx->header.time - timestamp;
    return set_error_status(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Determines the required size of the network request buffer
 *
 *  Size is determined by doing a dry run of encoding the request
 *  but this requires the workspace to be prepared first
 *
 *  @param ctx Skyhook request context
 *  @param size parameter which will be set to the size value
 *  @param sky_errno skyErrno is set to the error code
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_sizeof_request_buf(Sky_ctx_t *ctx, uint32_t *size, Sky_errno_t *sky_errno)
{
    int rc, rq_config = false;
#if CACHE_SIZE
    Sky_cacheline_t *cl;
#endif

    if (!validate_workspace(ctx))
        return set_error_status(sky_errno, SKY_ERROR_BAD_WORKSPACE);

    if (size == NULL)
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    /* determine whether request_client_conf should be true in request message */
    rq_config =
        (ctx->state->config.last_config_time == 0) ||
        (((*ctx->gettime)(NULL)-ctx->state->config.last_config_time) > CONFIG_REQUEST_INTERVAL);
    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Request config: %s",
        rq_config && ctx->state->config.last_config_time != 0 ? "Timeout" :
                                                                rq_config ? "Forced" : "No");

    if (rq_config)
        ctx->state->config.last_config_time = 0; /* request on next serialize */

    /* Trim any excess vap from workspace i.e. total number of vap
     * in workspace cannot exceed max that a request can carry
     */
    select_vap(ctx);

    /* check cache against beacons for match
     * setting from_cache if a matching cacheline is found
     * */
#if CACHE_SIZE
    get_from_cache(ctx);
    if (IS_CACHE_HIT(ctx)) {
        cl = &ctx->state->cacheline[ctx->get_from];

        /* cache hit */
        /* count of consecutive cache hits since last cache miss */
        if (ctx->state->cache_hits < 127) {
            ctx->state->cache_hits++;
            if (ctx->debounce) {
                /* overwrite workspace with cached beacons */
                LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "populate workspace with cached beacons");
                NUM_BEACONS(ctx) = cl->len;
                NUM_APS(ctx) = cl->ap_len;
                for (int j = 0; j < NUM_BEACONS(ctx); j++)
                    ctx->beacon[j] = cl->beacon[j];
            }
        } else {
            ctx->get_from = -1; /* force cache miss after 127 consecutive cache hits */
            ctx->state->cache_hits = 0; /* report 0 for cache miss */
        }
    }
#else
    ctx->get_from = -1; /* cache miss */
    ctx->state->cache_hits = 0; /* report 0 for cache miss */
#endif

    /* encode request into the bit bucket, just to determine the length of the
     * encoded message */
    rc = serialize_request(ctx, NULL, 0, SW_VERSION, rq_config);

    if (rc > 0) {
        *size = (uint32_t)rc;
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "sizeof request %d", rc);
        return set_error_status(sky_errno, SKY_ERROR_NONE);
    } else {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Failed to size request");
        return set_error_status(sky_errno, SKY_ERROR_ENCODE_ERROR);
    }
}

/*! \brief generate a Skyhook request from the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param request_buf Request to send to Skyhook server
 *  @param bufsize Request size in bytes
 *  @param loc where to save device latitude, longitude etc from cache if known
 *  @param response_size the space required to hold the server response
 *
 *  @return SKY_FINALIZE_REQUEST, SKY_FINALIZE_LOCATION or
 *          SKY_FINALIZE_ERROR and sets sky_errno with error code
 */
Sky_finalize_t sky_finalize_request(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, void *request_buf,
    uint32_t bufsize, Sky_location_t *loc, uint32_t *response_size)
{
    int rc;
    Sky_finalize_t ret = SKY_FINALIZE_ERROR;
#if CACHE_SIZE
    Sky_cacheline_t *cl;
#endif

    if (!validate_workspace(ctx)) {
        *sky_errno = SKY_ERROR_BAD_WORKSPACE;
        return ret;
    }

    if (backoff_violation(ctx, (uint32_t)(*sky_time)(NULL))) {
        *sky_errno = SKY_ERROR_SERVICE_DENIED;
        return ret;
    }

    /* There must be at least one beacon */
    if (NUM_BEACONS(ctx) == 0 && !has_gps(ctx)) {
        *sky_errno = SKY_ERROR_NO_BEACONS;
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Cannot process request with no beacons");
        return ret;
    }

    /* check cache match result */
#if CACHE_SIZE
    if (IS_CACHE_HIT(ctx)) {
        cl = &ctx->state->cacheline[ctx->get_from];
        if (loc != NULL) {
            *loc = cl->loc;
            /* no downlink data to report to user */
            loc->dl_app_data = NULL;
            loc->dl_app_data_len = 0;
        }
#if SKY_DEBUG
        time_t cached_time = loc->time;
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
            "Location from cache: %d.%06d,%d.%06d hpe:%d source:%s age:%d Sec", (int)loc->lat,
            (int)fabs(round(1000000 * (loc->lat - (int)loc->lat))), (int)loc->lon,
            (int)fabs(round(1000000 * (loc->lon - (int)loc->lon))), loc->hpe, sky_psource(loc),
            (ctx->header.time - cached_time));
#endif
        ret = SKY_FINALIZE_LOCATION;
    } else
        ret = SKY_FINALIZE_REQUEST;
#else
    (void)loc; /* suppress warning of unused parameter */
    ret = SKY_FINALIZE_REQUEST;
#endif

    if (request_buf == NULL) {
        *sky_errno = SKY_ERROR_BAD_PARAMETERS;
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Buffer pointer is bad");
        return SKY_FINALIZE_ERROR;
    }

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Processing request with %d beacons into %d byte buffer",
        NUM_BEACONS(ctx), bufsize);

#if SKY_DEBUG
    if (ctx->state->config.last_config_time == 0)
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Requesting new dynamic configuration parameters");
    else
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Configuration parameter: %d",
            ctx->state->config.last_config_time);
#endif

    /* encode request */
    rc = serialize_request(
        ctx, request_buf, bufsize, SW_VERSION, ctx->state->config.last_config_time == 0);

    if (rc > 0) {
        *response_size = get_maximum_response_size();

        *sky_errno = SKY_ERROR_NONE;

        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Request buffer of %d bytes prepared %s", rc,
            (ctx->debounce && ret == SKY_FINALIZE_LOCATION) ? "from cache(debounce)" :
                                                              "from workspace");
        LOG_BUFFER(ctx, SKY_LOG_LEVEL_DEBUG, request_buf, rc);
        return ret;
    } else {
        *sky_errno = SKY_ERROR_ENCODE_ERROR;

        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Failed to encode request");
        return SKY_FINALIZE_ERROR;
    }
}

/*! \brief decodes a Skyhook server response
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param response_buf buffer holding the skyhook server response
 *  @param bufsize Request size in bytes
 *  @param loc where to save device latitude, longitude etc from cache if known
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_decode_response(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, void *response_buf,
    uint32_t bufsize, Sky_location_t *loc)
{
    Sky_state_t *s = ctx->state;

    if (loc == NULL || response_buf == NULL || bufsize == 0) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Bad parameters");
        return set_error_status(sky_errno, SKY_ERROR_BAD_PARAMETERS);
    }

    /* decode response to get lat/lon */
    if (deserialize_response(ctx, response_buf, bufsize, loc) < 0) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Response decode failure");
        return set_error_status(sky_errno, SKY_ERROR_DECODE_ERROR);
    } else {
        /* if this is a response from a cache miss, clear cache_hits count */
        if (IS_CACHE_MISS(ctx))
            ctx->state->cache_hits = 0;

        /* set error status based on server error code */
        switch (loc->location_status) {
        case SKY_LOCATION_STATUS_SUCCESS:
            /* Server reports success so clear backoff period tracking */
            s->backoff = SKY_ERROR_NONE;
            loc->time = (*ctx->gettime)(NULL);

            /* Add location and current beacons to Cache */
            if (sky_plugin_add_to_cache(ctx, sky_errno, loc) != SKY_SUCCESS)
                LOGFMT(ctx, SKY_LOG_LEVEL_WARNING, "failed to add to cache");

            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG,
                "Location from server %d.%06d,%d.%06d hpe:%d, Source:%s app-data:%d", (int)loc->lat,
                (int)fabs(round(1000000 * (loc->lat - (int)loc->lat))), (int)loc->lon,
                (int)fabs(round(1000000 * (loc->lon - (int)loc->lon))), loc->hpe, sky_psource(loc),
                loc->dl_app_data_len);

            return set_error_status(sky_errno, SKY_ERROR_NONE);
            break;
        case SKY_LOCATION_STATUS_AUTH_ERROR:
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Authentication required, retry.");
            /* note the time of this server response in the state */
            s->header.time = (uint32_t)(*sky_time)(NULL);
            s->header.crc32 = sky_crc32(
                &s->header.magic, (uint8_t *)&s->header.crc32 - (uint8_t *)&s->header.magic);

            if (ctx->auth_state ==
                STATE_TBR_DISABLED) { /* Non-TBR Location request failed auth, error */
                return set_error_status(sky_errno, SKY_ERROR_AUTH);
            } else if (ctx->auth_state ==
                       STATE_TBR_REGISTERED) { /* Location request failed auth, retry */
                ctx->state->backoff = SKY_ERROR_NONE;
                return set_error_status(sky_errno, SKY_AUTH_RETRY);
            } else if (ctx->state->backoff ==
                       SKY_ERROR_NONE) /* Registration request failed auth, retry */
                return set_error_status(sky_errno, (ctx->state->backoff = SKY_AUTH_RETRY));
            else if (ctx->state->backoff ==
                     SKY_AUTH_RETRY) /* Registration request failed again, retry after 8hr */
                return set_error_status(sky_errno, (ctx->state->backoff = SKY_AUTH_RETRY_8H));
            else if (ctx->state->backoff ==
                     SKY_AUTH_RETRY_8H) /* Registration request failed again, retry after 16hr */
                return set_error_status(sky_errno, (ctx->state->backoff = SKY_AUTH_RETRY_16H));
            else if (ctx->state->backoff ==
                     SKY_AUTH_RETRY_16H) /* Registration request failed again, retry after 24hr */
                return set_error_status(sky_errno, (ctx->state->backoff = SKY_AUTH_RETRY_1D));
            else
                return set_error_status(sky_errno, (ctx->state->backoff = SKY_AUTH_RETRY_30D));
            break;
        case SKY_LOCATION_STATUS_BAD_PARTNER_ID_ERROR:
        case SKY_LOCATION_STATUS_DECODE_ERROR:
            return set_error_status(sky_errno, SKY_ERROR_AUTH);
            break;
        case SKY_LOCATION_STATUS_UNABLE_TO_LOCATE:
            return set_error_status(sky_errno, SKY_ERROR_LOCATION_UNKNOWN);
            break;
        default:
            return set_error_status(sky_errno, SKY_ERROR_SERVER_ERROR);
        }
    }
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
    case SKY_ERROR_BAD_WORKSPACE:
        str = "The workspace buffer is corrupt";
        break;
    case SKY_ERROR_BAD_STATE:
        str = "The state buffer is corrupt";
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
    case SKY_ERROR_AUTH:
        str = "Operation failed due to authentication error";
        break;
    case SKY_ERROR_MAX:
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
static char *sky_psource(struct sky_location *l)
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

/*! \brief clean up library resourses
 *
 *  @param sky_errno skyErrno is set to the error code
 *  @param sky_state pointer to where the state buffer reference should be
 * stored
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
Sky_status_t sky_close(Sky_errno_t *sky_errno, void **sky_state)
{
#if SKY_DEBUG
    char buf[SKY_LOG_LENGTH];
#endif
    if (!sky_open_flag)
        return set_error_status(sky_errno, SKY_ERROR_NEVER_OPEN);

    sky_open_flag = false;

    if (sky_state != NULL) {
        *sky_state = &state;
#if SKY_DEBUG
        if (sky_logf != NULL && SKY_LOG_LEVEL_DEBUG <= sky_min_level) {
            snprintf(buf, sizeof(buf), "%s:%s() State buffer with CRC 0x%08X and size %d",
                sky_basename(__FILE__), __FUNCTION__, sky_crc32(&state, state.header.size),
                state.header.size);
            (*sky_logf)(SKY_LOG_LEVEL_DEBUG, buf);
        }
#endif
    }
    return set_error_status(sky_errno, SKY_ERROR_NONE);
}

/*******************************************************************************
 * Static helper functions
 ******************************************************************************/

/*! \brief sanity check the device_id
 *
 *  @param device_id this is expected to be a binary mac address
 *  @param id_len length of device_id
 *
 *  @return true or false
 */
static bool validate_device_id(uint8_t *device_id, uint32_t id_len)
{
    if (device_id == NULL || id_len > MAX_DEVICE_ID)
        return false;
    else
        return true;
}

/*! \brief sanity check the partner_id
 *
 *  @param partner_id this is expected to be in the range (1 - ???)
 *
 *  @return true or false
 */
static bool validate_partner_id(uint32_t partner_id)
{
    if (partner_id == 0)
        return false;
    else
        return true; /* TODO check upper bound? */
}

/*! \brief safely return bounded length of string
 *
 *  @param pointer to string
 *  @param maximum number of chars to scan
 *
 *  @return length of string (up to length provided)
 */
static size_t strnlen_(char *s, size_t maxlen)
{
    char *p = s;

    if ((p = memchr(s, '\0', maxlen)) != NULL) {
        return p - s;
    } else
        return maxlen;
}

/*! \brief sanity check the aes_key
 *
 *  @param aes_key this is expected to be a binary 16 byte value
 *
 *  @return true or false
 */
static bool validate_aes_key(uint8_t aes_key[AES_SIZE])
{
    if (aes_key == NULL)
        return false;
    else
        return true; /* TODO check for non-trivial values? e.g. zero */
}
