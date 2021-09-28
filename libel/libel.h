/*! \file libel/libel.h
 *  \brief Top level header file for Skyhook Embedded Library
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
#ifndef SKY_LIBEL_H
#define SKY_LIBEL_H
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "aes.h"
#include "crc32.h"

#define AES_SIZE 16

#define MAX_DEVICE_ID 16
#define MAX_SKU_LEN 32 // excluding terminating NULL
#define TBR_TOKEN_UNKNOWN 0

/* March 1st 2019 */
#define TIMESTAMP_2019_03_01 1551398400
#define TIME_UNAVAILABLE ((time_t)0)
#define SECONDS_IN_HOUR (60 * 60)

/* Definitions for use when adding cell beacons with unknown parameters */
#define SKY_UNKNOWN_ID1 ((uint16_t)0xFFFF)
#define SKY_UNKNOWN_ID2 ((uint16_t)0xFFFF)
#define SKY_UNKNOWN_ID3 ((int32_t)-1)
#define SKY_UNKNOWN_ID4 ((int64_t)-1)
#define SKY_UNKNOWN_ID5 ((int16_t)-1)
#define SKY_UNKNOWN_ID6 ((int32_t)-1)
#define SKY_UNKNOWN_TA ((int32_t)-1)

/*! \brief API return value
 */
typedef enum {
    SKY_SUCCESS = 0,
    SKY_ERROR = -1,
} Sky_status_t;

/*! \brief sky_finalize_request return value
 */
typedef enum {
    SKY_FINALIZE_ERROR = -1,
    SKY_FINALIZE_LOCATION = 0,
    SKY_FINALIZE_REQUEST = 1,
} Sky_finalize_t;

/*! \brief sky_loc_source location source
 */
typedef enum {
    SKY_LOCATION_SOURCE_UNKNOWN = 0,
    SKY_LOCATION_SOURCE_HYBRID,
    SKY_LOCATION_SOURCE_CELL,
    SKY_LOCATION_SOURCE_WIFI,
    SKY_LOCATION_SOURCE_GNSS,
    SKY_LOCATION_SOURCE_RFPM,
    SKY_LOCATION_SOURCE_MAX,
} Sky_loc_source_t;

/*! \brief sky_loc_status location status
 */
typedef enum {
    SKY_LOCATION_STATUS_SUCCESS = 0,
    SKY_LOCATION_STATUS_UNSPECIFIED_ERROR = 1,
    SKY_LOCATION_STATUS_BAD_PARTNER_ID_ERROR = 2,
    SKY_LOCATION_STATUS_DECODE_ERROR = 3,
    SKY_LOCATION_STATUS_API_SERVER_ERROR = 4,
    SKY_LOCATION_STATUS_AUTH_ERROR = 5,
    SKY_LOCATION_STATUS_UNABLE_TO_LOCATE = 6,
} Sky_loc_status_t;

/*! \brief Skyhook location information
 */
typedef struct sky_location {
    float lat, lon; /* GNSS info */
    uint16_t hpe;
    time_t time;
    Sky_loc_source_t location_source;
    Sky_loc_status_t location_status;
    uint8_t *dl_app_data;
    uint32_t dl_app_data_len;
} Sky_location_t;

/*! \brief sky_errno Error Codes
 */
typedef enum {
    SKY_ERROR_NONE = 0,
    SKY_ERROR_NEVER_OPEN, // Operation failed because sky_open has not been completed
    SKY_ERROR_ALREADY_OPEN, // Operation failed because sky_open has already been called
    SKY_ERROR_BAD_PARAMETERS, // Operation failed because a parameter is invalid
    SKY_ERROR_BAD_REQUEST_CTX, // Operation failed because request context buffer failed sanity checks
    SKY_ERROR_BAD_SESSION_CTX, // Operation failed because libel session context buffer failed sanity checks
    SKY_ERROR_DECODE_ERROR, // Network message could not be decoded
    SKY_ERROR_ENCODE_ERROR, // Network message could not be encoded
    SKY_ERROR_RESOURCE_UNAVAILABLE, // Operation failed because resourse could not be assigned
    SKY_ERROR_NO_BEACONS, // Operation failed because no beacons were added
    SKY_ERROR_LOCATION_UNKNOWN, // Operation failed because server failed to determine location
    SKY_ERROR_SERVER_ERROR, // Operation failed because server reported an error
    SKY_ERROR_NO_PLUGIN, // Operation failed because required plugin was not found
    SKY_ERROR_INTERNAL, // Operation failed due to unexpected behavior
    SKY_ERROR_SERVICE_DENIED, // Service blocked due to repeated errors
    SKY_AUTH_RETRY, // Retry operation now to complete authentication
    SKY_AUTH_RETRY_8H, // Retry operation in 8hr to query authentication,
    SKY_AUTH_RETRY_16H, // Retry operation in 16hr to query authentication,
    SKY_AUTH_RETRY_1D, // Retry operation in 1 day to query authentication,
    SKY_AUTH_RETRY_30D, // Retry operation in 30 days to query authentication,
    SKY_AUTH_NEEDS_TIME, // No more registration attempts until time is available */
    SKY_ERROR_AUTH, // Operation failed because server reported authentication error
    SKY_ERROR_BAD_TIME, // Operation failed because a timestamp was out of range
    SKY_ERROR_MAX,
} Sky_errno_t;

/*! \brief sky_log_level logging levels
 */
typedef enum {
    SKY_LOG_LEVEL_CRITICAL = 1,
    SKY_LOG_LEVEL_ERROR,
    SKY_LOG_LEVEL_WARNING,
    SKY_LOG_LEVEL_DEBUG,
    SKY_LOG_LEVEL_ALL = SKY_LOG_LEVEL_DEBUG,
} Sky_log_level_t;

/*! \brief pointer to logger callback function
 */
typedef int (*Sky_loggerfn_t)(Sky_log_level_t level, char *s);

/*! \brief pointer to random bytes callback function
 */
typedef int (*Sky_randfn_t)(uint8_t *rand_buf, uint32_t bufsize);

/*! \brief pointer to gettime callback function
 */
typedef time_t (*Sky_timefn_t)(time_t *t);

/*! \brief session context header
 */
typedef struct sky_header {
    uint32_t magic; /* SKY_MAGIC */
    uint32_t size; /* total number of bytes in structure */
    uint32_t time; /* timestamp when structure was allocated */
    uint32_t crc32; /* crc32 over header */
} Sky_header_t;
#define SKY_SIZEOF_SESSION_HEADER (sizeof(Sky_header_t))

/* \brief dynamically trunable configuration parameters
 */
typedef enum config_names {
    CONF_TOTAL_BEACONS = 0,
    CONF_MAX_AP_BEACONS,
    CONF_CACHE_MATCH_THRESHOLD,
    CONF_CACHE_AGE_THRESHOLD,
    CONF_CACHE_BEACON_THRESHOLD,
    CONF_CACHE_NEG_RSSI_THRESHOLD,
    CONF_CACHE_MATCH_ALL_THRESHOLD,
    CONF_CACHE_MATCH_USED_THRESHOLD,
    CONF_MAX_VAP_PER_AP,
    CONF_MAX_VAP_PER_RQ,
    CONF_LOGGING_LEVEL,
    /* Add more config variables here */
    CONF_UNKNOWN,
} Sky_config_name_t;

#include "aes.h"
#include "config.h"
#include "beacons.h"
#include "utilities.h"
#include "plugin.h"

Sky_status_t sky_open(Sky_errno_t *sky_errno, uint8_t *device_id, uint32_t id_len,
    uint32_t partner_id, uint8_t aes_key[AES_KEYLEN], char *sku, uint32_t cc, Sky_sctx_t *sctx,
    Sky_log_level_t min_level, Sky_loggerfn_t logf, Sky_randfn_t rand_bytes, Sky_timefn_t gettime);

Sky_status_t sky_search_cache(
    Sky_rctx_t *rctx, Sky_errno_t *sky_errno, bool *cache_hit, Sky_location_t *loc);

int32_t sky_sizeof_session_ctx(Sky_sctx_t *sctx);

int32_t sky_sizeof_request_ctx(void);

Sky_rctx_t *sky_new_request(Sky_rctx_t *rctx, uint32_t rbufsize, Sky_sctx_t *sctx,
    uint8_t *ul_app_data, uint32_t ul_app_data_len, Sky_errno_t *sky_errno);

Sky_status_t sky_add_ap_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint8_t mac[MAC_SIZE],
    time_t timestamp, int16_t rssi, int32_t freq, bool is_connected);

Sky_status_t sky_add_cell_lte_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int32_t tac,
    int64_t e_cellid, uint16_t mcc, uint16_t mnc, int16_t pci, int32_t earfcn, int32_t ta,
    time_t timestamp, int16_t rsrp, bool is_connected);

Sky_status_t sky_add_cell_lte_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int16_t pci,
    int32_t earfcn, time_t timestamp, int16_t rsrp);

Sky_status_t sky_add_cell_gsm_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int32_t lac,
    int64_t ci, uint16_t mcc, uint16_t mnc, int16_t bsic, int32_t arfcn, int32_t ta,
    time_t timestamp, int16_t rssi, bool is_connected);

Sky_status_t sky_add_cell_gsm_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno,
    int16_t bsic, int16_t arfcn, time_t timestamp, int16_t rscp);

Sky_status_t sky_add_cell_umts_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int32_t lac,
    int64_t ucid, uint16_t mcc, uint16_t mnc, int16_t psc, int16_t uarfcn, time_t timestamp,
    int16_t rscp, bool is_connected);

Sky_status_t sky_add_cell_umts_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno,
    int16_t psc, int16_t uarfcn, time_t timestamp, int16_t rscp);

Sky_status_t sky_add_cell_cdma_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint32_t sid,
    int32_t nid, int64_t bsid, time_t timestamp, int16_t rssi, bool is_connected);

Sky_status_t sky_add_cell_nb_iot_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint16_t mcc,
    uint16_t mnc, int64_t e_cellid, int32_t tac, int16_t ncid, int32_t earfcn, time_t timestamp,
    int16_t nrsrp, bool is_connected);

Sky_status_t sky_add_cell_nb_iot_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno,
    int16_t ncid, int32_t earfcn, time_t timestamp, int16_t nrsrp);

Sky_status_t sky_add_cell_nr_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, uint16_t mcc,
    uint16_t mnc, int64_t nci, int32_t tac, int16_t pci, int32_t nrarfcn, int32_t ta,
    time_t timestamp, int16_t csi_rsrp, bool is_connected);

Sky_status_t sky_add_cell_nr_neighbor_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, int16_t pci,
    int32_t nrarfcn, time_t timestamp, int16_t csi_rsrp);

Sky_status_t sky_add_gnss(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, float lat, float lon,
    uint16_t hpe, float altitude, uint16_t vpe, float speed, float bearing, uint16_t nsat,
    time_t timestamp);

Sky_status_t sky_ignore_cache_hit(Sky_rctx_t *rctx, Sky_errno_t *sky_errno);

Sky_status_t sky_encode_request(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, void *request_buf,
    uint32_t bufsize, uint32_t *response_size);

Sky_status_t sky_sizeof_request_buf(Sky_rctx_t *rctx, uint32_t *size, Sky_errno_t *sky_errno);

Sky_status_t sky_decode_response(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, void *response_buf,
    uint32_t bufsize, Sky_location_t *loc);

Sky_status_t sky_get_option(
    Sky_rctx_t *rctx, Sky_errno_t *sky_errno, Sky_config_name_t name, uint32_t *value);

Sky_status_t sky_set_option(
    Sky_rctx_t *rctx, Sky_errno_t *sky_errno, Sky_config_name_t name, int32_t value);

char *sky_perror(Sky_errno_t sky_errno);

char *sky_pserver_status(Sky_loc_status_t status);

char *sky_psource(struct sky_location *l);

char *sky_pbeacon(Beacon_t *b);

Sky_status_t sky_close(Sky_sctx_t *sctx, Sky_errno_t *sky_errno);

#endif
