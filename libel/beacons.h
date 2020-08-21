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

#define BEACON_MAGIC 0xf0f0

#define MAC_SIZE 6

#define NUM_CELLS(p) ((p)->len - (p)->ap_len)
#define NUM_APS(p) ((p)->ap_len)
#define NUM_BEACONS(p) ((p)->len)
#define IMPLIES(a, b) (!(a) || (b))
#define NUM_VAPS(b) ((b)->ap.vg_len)

/* VAP data is prefixed by length and AP index */
#define VAP_LENGTH (0)
#define VAP_PARENT (1)
#define VAP_FIRST_DATA (2)

/*! \brief Types of beacon in protity order
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
    uint32_t age;
    int16_t rssi; // -255 unkonwn - map it to - 128
    int8_t connected; /* beacon connected */
};

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
    Sky_beacon_property_t used_cached[MAX_VAP_PER_AP]; /* Virtual APs Used/Cached info */
};

// http://wiki.opencellid.org/wiki/API
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
};

// blue tooth
struct ble {
    uint16_t magic; /* Indication that this beacon entry is valid */
    uint16_t type; /* sky_beacon_type_t */
    uint16_t major;
    uint16_t minor;
    uint8_t mac[MAC_SIZE];
    uint8_t uuid[16];
    int16_t rssi;
};

typedef union beacon {
    struct header h;
    struct ap ap;
    struct ble ble;
    struct cell cell;
} Beacon_t;

typedef struct gps {
    double lat;
    double lon;
    uint32_t hpe;
    float alt; // altitude
    uint32_t vpe;
    float speed;
    float bearing;
    uint32_t nsat;
    uint32_t age;
} Gps_t;

#endif
