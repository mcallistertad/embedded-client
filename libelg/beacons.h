/*! \file libelg/beacons.h
 *  \brief Skyhook ELG API intermediate structures
 *
 * Copyright 2019 Skyhook Inc.
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
#include <inttypes.h>
#include <time.h>

#ifndef SKY_BEACONS_H
#define SKY_BEACONS_H

#define BEACON_MAGIC 0xf0f0
#define MAC_SIZE 6
#define IPV4_SIZE 4
#define IPV6_SIZE 16

#define MAX_MACS 2 /* max # of mac addresses */
#define MAX_IPS 2  // max # of ip addresses

#define MAX_APS 100 // max # of access points
#define MAX_GPSS 2  // max # of gps
#define MAX_CELLS 7 // max # of cells
#define MAX_BLES 5  // max # of blue tooth

/*! \brief Types of beacon
 */
typedef enum {
    SKY_BEACON_AP = 1,
    SKY_BEACON_BLE,
    SKY_BEACON_CDMA,
    SKY_BEACON_GSM,
    SKY_BEACON_LTE,
    SKY_BEACON_UMTS,
    SKY_BEACON_MAX, /* add more before this */
} sky_beacon_type_t;

/*! \brief Access Point data
 */
struct ap {
    uint16_t magic; /* Indication that this beacon entry is valid */
    uint16_t type;  /* sky_beacon_type_t */
    uint8_t mac[MAC_SIZE];
    time_t age;
    uint32_t channel;
    int8_t rssi;
    uint8_t flag; /* bit fields:                                        */
    /* bit 0: 1 if the device is currently connected to this AP. 0      */
    /* otherwise. bits 1-3: Band indicator. Allowable values:           */
    /*                                             0: unknown           */
    /*                                             1: 2.4 GHz           */
    /*                                             2: 5 GHz             */
    /*                                             3-7: Reserved        */
    /* bits 4-7: Reserved                                               */
};

// http://wiki.opencellid.org/wiki/API
struct gsm {
    uint32_t ci;
    uint32_t age;
    uint16_t mcc; // country
    uint16_t mnc;
    uint16_t lac;
    int8_t rssi; // -255 unkonwn - map it to - 128
};

// 64-bit aligned due to double
struct cdma {
    double lat;
    double lon;
    uint32_t age;
    uint16_t sid;
    uint16_t nid;
    uint16_t bsid;
    int8_t rssi;
};

struct umts {
    uint32_t ci;
    uint32_t age;
    uint16_t mcc; // country
    uint16_t mnc;
    uint16_t lac;
    int8_t rssi;
};

struct lte {
    uint32_t age;
    uint32_t eucid;
    uint16_t mcc;
    uint16_t mnc;
    int8_t rssi;
};

// blue tooth
struct ble {
    uint16_t major;
    uint16_t minor;
    uint8_t MAC[MAC_SIZE];
    uint8_t uuid[16];
    int8_t rssi;
};

typedef union beacon {
    struct gsm gsm;
    struct cdma cdma;
    struct umts umts;
    struct lte lte;
    struct ap ap;
    struct ble ble;
} beacon_t;

typedef struct gps {
    double lat;
    double lon;
    float hdop;
    float alt; // altitude
    float hpe;
    float speed;
    uint32_t age;
    uint8_t nsat;
    uint8_t fix;
} gps_t;

#endif
