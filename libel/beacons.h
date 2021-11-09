/*! \file libel/beacons.h
 *  \brief Skyhook Embedded Library
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
#ifndef SKY_BEACONS_H
#define SKY_BEACONS_H

#include <inttypes.h>
#include <time.h>
#include <math.h>

#define SKY_MAGIC 0xD1967806
#define BEACON_MAGIC 0xf0f0

#define MAC_SIZE 6

#define NUM_CELLS(p) ((uint16_t)((p)->num_beacons - (p)->num_ap))
#define NUM_APS(p) ((p)->num_ap)
#define NUM_BEACONS(p) ((p)->num_beacons)
#define IMPLIES(a, b) (!(a) || (b))
#define NUM_VAPS(b) ((b)->ap.vg_len)

/* VAP data is prefixed by length and AP index */
#define VAP_LENGTH (0)
#define VAP_PARENT (1)
#define VAP_FIRST_DATA (2)

#define NIBBLE_MASK(n) (0xF0 >> (4 * ((n)&1)))
#define LOCAL_ADMIN_MASK(byte) (0x02 & (byte))

#define is_ap_type(c) ((c)->h.type == SKY_BEACON_AP)

#define is_cell_type(c)                                                                            \
    ((c)->h.type >= SKY_BEACON_FIRST_CELL_TYPE && (c)->h.type <= SKY_BEACON_LAST_CELL_TYPE)

#if !SKY_EXCLUDE_CELL_SUPPORT
/* For all cell types, id2 is a key parameter, i.e. Unknown is not allowed unless it is an nmr */
#define is_cell_nmr(c) (is_cell_type(c) && ((c)->cell.id2 == SKY_UNKNOWN_ID2))
#else
#define is_cell_nmr(c) (false)
#endif // !SKY_EXCLUDE_CELL_SUPPORT

#if !SKY_EXCLUDE_GNSS_SUPPORT
#define has_gnss(c) ((c) != NULL && !isnan((c)->gnss.lat))
#else
#define has_gnss(c) (false)
#endif // !SKY_EXCLUDE_GNSS_SUPPORT

#define CACHE_EMPTY ((time_t)0)
#define CONFIG_UPDATE_DUE ((time_t)0)
#define IS_CACHE_HIT(c) (c->get_from != -1 && (c)->hit)
#define IS_CACHE_MISS(c) ((c)->hit == false)

#define EFFECTIVE_RSSI(rssi) ((rssi) == -1 ? (-127) : (rssi))

/* Comparisons result in positive difference when beacon a is higher priority */
/* when comparing type, lower type enum is better so invert difference */
#define COMPARE_TYPE(a, b) ((b)->h.type - (a)->h.type)
/* when comparing age, lower value (younger) is better so invert difference */
#define COMPARE_AGE(a, b) ((b)->h.age - (a)->h.age)
/* when comparing rssi, higher value (stronger) is better */
#define COMPARE_RSSI(a, b) (EFFECTIVE_RSSI((a)->h.rssi) - EFFECTIVE_RSSI((b)->h.rssi))
/* when comparing mac, lower value is better so invert difference */
#define COMPARE_MAC(a, b) (memcmp((b)->ap.mac, (a)->ap.mac, MAC_SIZE))
/* when comparing connected, higher (true) value is better */
#define COMPARE_CONNECTED(a, b) ((a)->h.connected - (b)->h.connected)
/* when comparing priority, higher value is better */
#define COMPARE_PRIORITY(a, b) ((a)->h.priority - (b)->h.priority)

/*! \brief Types of beacon in compare order
 */
typedef enum {
    SKY_BEACON_AP = 1,
    SKY_BEACON_BLE = 2,
    SKY_BEACON_NR = 3,
    SKY_BEACON_FIRST_CELL_TYPE = SKY_BEACON_NR,
    SKY_BEACON_LTE = 4,
    SKY_BEACON_UMTS = 5,
    SKY_BEACON_NBIOT = 6,
    SKY_BEACON_CDMA = 7,
    SKY_BEACON_GSM = 8,
    SKY_BEACON_LAST_CELL_TYPE = SKY_BEACON_GSM,
    SKY_BEACON_MAX, /* add more before this */
} Sky_beacon_type_t;

/*! \brief Property of beacon
 */
typedef struct {
    uint8_t in_cache : 1;
    uint8_t used : 1;
} Sky_beacon_property_t;

struct header {
    uint16_t magic; /* Indication that this beacon entry is valid */
    uint16_t type; /* sky_beacon_type_t */
    uint32_t age; /* age of scan in seconds relative to when this request was started */
    int16_t rssi; /* -128 through -10, Uknownn = -1 */
    float priority; /* used to remove worst beacon. Higher values are better, always positive */
    int8_t connected; /* beacon connected */
};

#if !SKY_EXCLUDE_WIFI_SUPPORT
/*! \brief Virtual AP member
 */
typedef union {
    struct {
        uint8_t value : 4; /* replacement value for the nibble indexed */
        uint8_t nibble_idx : 4; /* 0-11 index into mac address by nibble */
    } data;
    uint8_t len; /* number of bytes in child patch data */
    uint8_t ap; /* index of parent AP */
} Vap_t;

/*! \brief Access Point data
 */
struct ap {
    struct header h;
    uint8_t mac[MAC_SIZE];
    uint32_t freq;
    Sky_beacon_property_t property; /* ap is in cache and used? */
    uint8_t vg_len;
    Vap_t vg[MAX_VAP_PER_AP + 2]; /* Virtual APs */
    Sky_beacon_property_t vg_prop[MAX_VAP_PER_AP]; /* Virtual AP properties */
};
#endif // !SKY_EXCLUDE_WIFI_SUPPORT

#if !SKY_EXCLUDE_CELL_SUPPORT
/*! \brief http://wiki.opencellid.org/wiki/API
 */
struct cell {
    struct header h;
    uint16_t id1; // mcc (gsm, umts, lte, nr, nb-iot). SKY_UNKNOWN_ID1 if unknown.
    uint16_t id2; // mnc (gsm, umts, lte, nr, nb-iot) or sid (cdma). SKY_UNKNOWN_ID2 if unknown.
    int32_t
        id3; // lac (gsm, umts) or tac (lte, nr, nb-iot) or nid (cdma). SKY_UNKNOWN_ID3 if unknown.
    int64_t id4; // cell id (gsm, umts, lte, nb-iot, nr), bsid (cdma). SKY_UNKNOWN_ID4 if unknown.
    int16_t
        id5; // bsic (gsm), psc (umts), pci (lte, nr) or ncid (nb-iot). SKY_UNKNOWN_ID5 if unknown.
    int32_t
        freq; // arfcn(gsm), uarfcn (umts), earfcn (lte, nb-iot), nrarfcn (nr). SKY_UNKNOWN_ID6 if unknown.
    int32_t ta; // SKY_UNKNOWN_TA if unknown.
};
#endif // !SKY_EXCLUDE_CELL_SUPPORT

/*! \brief blue tooth
 */
struct ble {
    struct header h;
    uint16_t major;
    uint16_t minor;
    uint8_t mac[MAC_SIZE];
    uint8_t uuid[16];
};

/*! \brief universal beacon structure
 */
typedef union beacon {
    struct header h;
#if !SKY_EXCLUDE_WIFI_SUPPORT
    struct ap ap;
#endif // !SKY_EXCLUDE_WIFI_SUPPORT
    struct ble ble;
#if !SKY_EXCLUDE_CELL_SUPPORT
    struct cell cell;
#endif // !SKY_EXCLUDE_CELL_SUPPORT
} Beacon_t;

#if !SKY_EXCLUDE_GNSS_SUPPORT
/*! \brief gnss/gnss
 */
typedef struct gnss {
    float lat;
    float lon;
    uint32_t hpe;
    float alt; // altitude
    uint32_t vpe;
    float speed;
    float bearing;
    uint32_t nsat;
    uint32_t age;
} Gnss_t;
#endif // !SKY_EXCLUDE_GNSS_SUPPORT

/*! \brief each cacheline holds a copy of a scan and the server response
 */
typedef struct sky_cacheline {
    uint16_t num_beacons; /* number of beacons */
    uint16_t num_ap; /* number of AP beacons in list (0 == none) */
    time_t time;
    Beacon_t beacon[TOTAL_BEACONS]; /* beacons */
#if !SKY_EXCLUDE_GNSS_SUPPORT
    Gnss_t gnss; /* GNSS info */
#endif // !SKY_EXCLUDE_GNSS_SUPPORT
    Sky_location_t loc; /* Skyhook location */
} Sky_cacheline_t;

/*! \brief TBR states, 1) Disabled (not in use), 2) Unregistered, 3) Registered
 */
typedef enum sky_tbr_state {
    STATE_TBR_DISABLED, /* not configured for TBR */
    STATE_TBR_UNREGISTERED, /* need to register */
    STATE_TBR_REGISTERED /* we have a valid token */
} Sky_tbr_state_t;

/* Access the cache config parameters */
#define CONFIG(session, param) (session->config.param)

/*! \brief Server tunable config parameters
 */
typedef struct sky_config_pad {
    time_t last_config_time; /* time when the last new config was received */
    uint32_t total_beacons;
    uint32_t max_ap_beacons;
    uint32_t cache_match_threshold;
    uint32_t cache_age_threshold;
    uint32_t cache_beacon_threshold;
    uint32_t cache_neg_rssi_threshold;
    uint32_t cache_match_all_threshold;
    uint32_t cache_match_used_threshold;
    uint32_t max_vap_per_ap;
    uint32_t max_vap_per_rq;
    /* add more configuration params here */
} Sky_config_t;

/*! \brief Session Context - holds cache lines as well as parameters defined when Libel is opened
 */
typedef struct sky_sctx {
    Sky_header_t header; /* magic, size, timestamp, crc32 */
    bool open_flag; /* true if sky_open() has been called by user */
    Sky_randfn_t rand_bytes; /* User rand_bytes fn */
    Sky_loggerfn_t logf; /* User logging fn */
    Sky_log_level_t min_level; /* User log level */
    Sky_timefn_t timefn; /* User time fn */
    void *plugins; /* root of registered plugin list */
    uint32_t id_len; /* device ID num_beacons */
    uint8_t device_id[MAX_DEVICE_ID]; /* device ID */
    uint32_t token_id; /* TBR token ID */
    uint32_t ul_app_data_len; /* uplink app data length */
    uint8_t ul_app_data[SKY_MAX_UL_APP_DATA]; /* uplink app data */
    uint32_t dl_app_data_len; /* downlink app data length */
    uint8_t dl_app_data[SKY_MAX_DL_APP_DATA]; /* downlink app data */
    char sku[MAX_SKU_LEN + 1]; /* product family ID */
    uint16_t cc; /* Optional Country Code (0 = unused) */
    Sky_errno_t backoff; /* last auth error */
    uint32_t partner_id; /* partner ID */
    uint8_t aes_key[AES_KEYLEN]; /* aes key */
#if CACHE_SIZE
    int num_cachelines; /* number of cachelines */
    Sky_cacheline_t cacheline[CACHE_SIZE]; /* beacons */
#endif // CACHE_SIZE
    Sky_config_t config; /* dynamic config parameters */
    uint8_t cache_hits; /* count the client cache hits */
} Sky_sctx_t;

/*! \brief Request Context - temporary space used to build a request
 */
typedef struct sky_rctx {
    Sky_header_t header; /* magic, size, timestamp, crc32 */
    uint16_t num_beacons; /* number of beacons in list (0 == none) */
    uint16_t num_ap; /* number of AP beacons in list (0 == none) */
    Beacon_t beacon[TOTAL_BEACONS + 1]; /* beacon data */
#if !SKY_EXCLUDE_GNSS_SUPPORT
    Gnss_t gnss; /* GNSS info */
#endif // !SKY_EXCLUDE_GNSS_SUPPORT
    bool hit; /* status of search of cache for match to new scan (true/false) */
    int16_t get_from; /* cacheline with good match to scan (-1 for miss) */
    int16_t save_to; /* cacheline with best match for saving scan*/
    Sky_sctx_t *session;
    Sky_tbr_state_t auth_state; /* tbr disabled, need to register or got token */
    uint32_t sky_dl_app_data_len; /* downlink app data length */
    uint8_t sky_dl_app_data[SKY_MAX_DL_APP_DATA]; /* downlink app data */
} Sky_rctx_t;

Sky_status_t add_beacon(Sky_rctx_t *rctx, Sky_errno_t *sky_errno, Beacon_t *b, time_t timestamp);
int ap_beacon_in_vg(Sky_rctx_t *rctx, Beacon_t *va, Beacon_t *vb, Sky_beacon_property_t *prop);
bool beacon_in_cache(Sky_rctx_t *rctx, Beacon_t *b, Sky_beacon_property_t *prop);
bool beacon_in_cacheline(
    Sky_rctx_t *rctx, Beacon_t *b, Sky_cacheline_t *cl, Sky_beacon_property_t *prop);
int serving_cell_changed(Sky_rctx_t *rctx, Sky_cacheline_t *cl);
int cached_gnss_worse(Sky_rctx_t *rctx, Sky_cacheline_t *cl);
int find_oldest(Sky_rctx_t *rctx);
int search_cache(Sky_rctx_t *rctx);
Sky_status_t remove_beacon(Sky_rctx_t *rctx, int index);

#endif // SKY_BEACONS_H
