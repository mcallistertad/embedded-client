/*! \file libel/unit_test.c
 *  \brief unit tests - Skyhook Embedded Library
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
#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#define SKY_LIBEL
#include "libel.h"
#include "crc32.h"

Sky_cache_t nv_space;

/* Example assumes a scan with 100 AP beacons
 */
#define SCAN_LIST_SIZE 100

/* Set to true to test with fake(bad) Network Time
 *     When true, should see "add_to_cache: Error appropriate with fake network time"
 */
#define FAKE_NETWORK_TIME false

/*! \brief time function
 *
 *  @param t where to save the time
 *
 *  @returns the time in seconds since the epoc (linux time)
 */

time_t mytime(time_t *t)
{
    time_t now;
    printf("mytime 0x%08llX\n", (long long)t);
#if FAKE_NETWORK_TIME
    /* truncate actual time to skew it much older making cache operations fail */
    printf("truncate actual time to skew it much older making cache operations fail\n");
    if (t != NULL) {
        *t = time(NULL) & 0x0fffffff;
        return *t;
    } else
        return time(NULL) & 0x0fffffff;
#else
    now = time(NULL);
    printf("mytime now = %lld\n", (long long)now);
#if 0
    if (t != NULL) {
        printf("mytime saved now = %lld\n", (long long)now);
        *t = now;
    }
#endif
    return now;
#endif
}

/*! \brief set mac address. 30% are virtual AP
 *
 *  @param mac pointer to mac address
 *
 *  @returns 0 for success or negative number for error
 */
void set_mac(uint8_t *mac)
{
    uint8_t refs[5][MAC_SIZE] = /* clang-format off */
    { { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e },
      { 0xe4, 0x75, 0x64, 0xb2, 0xf5, 0x7e },
      { 0xf4, 0x65, 0x64, 0xb2, 0xf5, 0x7e },
      { 0x14, 0x55, 0x64, 0xb2, 0xf5, 0x7e },
      { 0x24, 0x45, 0x64, 0xb2, 0xf5, 0x7e } };
    /* clang-format on */

    if (rand() % 3 == 0) {
        /* Virtual MAC */
        memcpy(mac, refs[0], sizeof(refs[0]));
        mac[rand() % 2 + 4] ^= (0x01 << (rand() % 8));
        printf("Virt MAC\n");
    } else if (rand() % 3 != 0) {
        /* rand MAC */
        memcpy(mac, refs[rand() % 3], sizeof(refs[0]));
        if (rand() % 3 == 0) {
            mac[rand() % 3] = (rand() % 256);
            printf("Rand MAC\n");
        } else {
            printf("Known MAC\n");
        }
    } else {
        /* Non Virtual MAC */
        uint8_t ref[] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e };
        memcpy(mac, ref, sizeof(ref));
        mac[rand() % 3] = (rand() % 256);
        mac[rand() % 3 + 3] = (rand() % 256);
        printf("Non-Virt MAC\n");
    }
}

/*! \brief logging function
 *
 *  @param level log level of this message
 *  @param s this message
 *
 *  @returns 0 for success or negative number for error
 */
int logger(Sky_log_level_t level, char *s)
{
    printf("libEL %s: %.*s\n",
        level == SKY_LOG_LEVEL_CRITICAL ?
            "CRIT" :
            level == SKY_LOG_LEVEL_ERROR ?
            "ERRR" :

            level == SKY_LOG_LEVEL_WARNING ? "WARN" :
                                             level == SKY_LOG_LEVEL_DEBUG ? "DEBG" : "UNKN",
        SKY_LOG_LENGTH, s);
    return 0;
}

/*! \brief generate random byte sequence
 *
 *  @param rand_buf pointer to buffer where rand bytes are put
 *  @param bufsize length of rand bytes
 *
 *  @returns 0 for failure, length of rand sequence for success
 */
int rand_bytes(uint8_t *rand_buf, uint32_t bufsize)
{
    int i;

    printf("rand_bytes\n");
    if (!rand_buf)
        return 0;

    for (i = 0; i < bufsize; i++)
        rand_buf[i] = rand() % 256;
    return bufsize;
}

/*! \brief cound cache lines in use
 *
 *  @returns number of cache lines not empty
 */
int beacons_in_cache_rssi(Sky_cache_t *c)
{
    int i, j, total = 0;

    for (i = 0; i < CACHE_SIZE; i++)
        for (j = 0; j < c->cacheline[i].ap_len; j++)
            total += c->cacheline[i].beacon[j].h.rssi;
    return total;
}

/*! \brief check for saved cache state
 *
 *   If state found, initialize random number generator based on number of beacons in saved state
 *
 *  @returns NULL for failure to restore cache, pointer to cache otherwise
 */
void *nv_cache(void)
{
    FILE *fio;
    uint8_t *p = (void *)&nv_space;

    if ((fio = fopen("nv_cache", "r")) != NULL) {
        if (fread(p, sizeof(Sky_header_t), 1, fio) == 1 && nv_space.header.magic == SKY_MAGIC &&
            nv_space.header.crc32 ==
                sky_crc32(&nv_space.header.magic,
                    (uint8_t *)&nv_space.header.crc32 - (uint8_t *)&nv_space.header.magic)) {
            if (fread(p + sizeof(Sky_header_t), nv_space.header.size - sizeof(Sky_header_t), 1,
                    fio) == 1) {
                if (validate_cache(&nv_space, &logger)) {
                    printf("validate_cache: Restoring Cache\n");
                    /* Randomize if restoring cache from previous run */
                    srand((unsigned)beacons_in_cache_rssi(&nv_space));
                    printf("Rand( %d )\n", beacons_in_cache_rssi(&nv_space));
                    fclose(fio);
                    return &nv_space;
                } else {
                    printf("validate_cache: false\n");
                }
            }
        }
        fclose(fio);
    }
    printf("cache restore: failed\n");
    return NULL;
}

/*! \brief save cache state
 *
 *  @param p pointer to cache buffer
 *
 *  @returns 0 for success or negative number for error
 */
Sky_status_t nv_cache_save(void *p)
{
    FILE *fio;
    Sky_cache_t *c = p;

    if (validate_cache(c, &logger)) {
        if ((fio = fopen("nv_cache", "w+")) != NULL) {
            if (fwrite(p, c->header.size, 1, fio) == 1) {
                printf("nv_cache_save: cache size %d (%lu)\n", c->header.size, sizeof(Sky_cache_t));
                fclose(fio);
                return SKY_SUCCESS;
            } else
                printf("fwrite failed\n");
            fclose(fio);
        } else
            printf("fopen failed\n");
    } else
        printf("nv_cache_save: failed to validate cache\n");
    return SKY_ERROR;
}

/*! \brief validate fundamental functionality of the Embedded library
 *
 *  @param ac arg count
 *  @param av arg vector
 *
 *  @returns 0 for success or negative number for error
 */
#define SCAN_SIZE (TOTAL_BEACONS * 3)
#define SCAN_AP (TOTAL_BEACONS * 2)
#define SCAN_CELL (TOTAL_BEACONS)
int main(int ac, char **av)
{
    int i;
    // Sky_errno_t sky_errno = SKY_ERROR_MAX;
    Sky_errno_t sky_errno;
    Sky_ctx_t *ctx;
    uint32_t *p;
    uint32_t bufsize;
    uint8_t aes_key[AES_SIZE] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e, 0xd4, 0x85, 0x64, 0xb2, 0xf5,
        0x7e, 0xd4, 0x85, 0x64, 0xb2 };
    uint8_t mac[MAC_SIZE] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e };
    time_t timestamp = time(NULL);
    void *pstate;
    uint32_t response_size;
    Sky_beacon_type_t t;
    Beacon_t b[TOTAL_BEACONS * 3];
    Sky_location_t loc = { 0 };
    int scan_ap = SCAN_AP / 2 + rand() % (SCAN_AP / 2);
    int scan_cell = (SCAN_CELL / 10) + rand() % (SCAN_CELL / 10);

    if (sky_open(&sky_errno, mac /* device_id */, MAC_SIZE, 1, aes_key, nv_cache(),
            SKY_LOG_LEVEL_ALL, &logger, &rand_bytes, &mytime) == SKY_ERROR) {
        printf("sky_open returned bad value, Can't continue\n");
        exit(-1);
    }
    /* Test sky_sizeof_workspace */
    bufsize = sky_sizeof_workspace();

    /* sky_sizeof_workspace should return a value below 5k and above 0 */
    if (bufsize == 0 || bufsize > 4096) {
        printf("sky_sizeof_workspace returned bad value, Can't continue\n");
        exit(-1);
    }

    /* allocate workspace */
    ctx = (Sky_ctx_t *)(p = alloca(bufsize));

    /* initialize the workspace */
    memset(p, 0, bufsize);

    if (sky_new_request(ctx, bufsize, &sky_errno) != ctx) {
        printf("sky_new_request() returned bad value\n");
        printf("sky_errno contains '%s'\n", sky_perror(sky_errno));
    }

#if PREMIUM
    /* Register the plugins to be used */
    extern Sky_plugin_op_t ap_plugin_vap_used_table[SKY_OP_MAX];
    extern Sky_plugin_op_t cell_plugin_best)_tableSKY_OP_MAX];
    sky_plugin_init(ctx, &sky_errno, ap_plugin_vap_used_table);
    sky_plugin_init(ctx, &sky_errno, cell_plugin_best_table);
#endif
    /* Register the plugins to be used */
    extern Sky_plugin_op_t ap_plugin_basic_table[SKY_OP_MAX];
    extern Sky_plugin_op_t cell_plugin_basic_table[SKY_OP_MAX];
    sky_plugin_init(ctx, &sky_errno, ap_plugin_basic_table);
    sky_plugin_init(ctx, &sky_errno, cell_plugin_basic_table);

    for (i = 0; i < scan_ap; i++) {
        b[i].h.magic = BEACON_MAGIC;
        b[i].h.type = SKY_BEACON_AP;
        b[i].h.rssi = -rand() % 128;
        set_mac(b[i].ap.mac);
        b[i].ap.freq = (b[i].ap.mac[0] * 14) + 2400; /* range 2400 - 6000 */
    }

    for (i = 0; i < scan_ap; i++) {
        if (sky_add_ap_beacon(ctx, &sky_errno, b[i].ap.mac, timestamp - (rand() % 3), b[i].h.rssi,
                b[i].ap.freq, rand() % 2)) {
            printf("sky_add_ap_beacon sky_errno contains '%s'\n", sky_perror(sky_errno));
        } else {
            printf(
                "Added Test Beacon % 2d: Type: %d, MAC %02X:%02X:%02X:%02X:%02X:%02X freq: %d, rssi: %d\n",
                i, b[i].h.type, b[i].ap.mac[0], b[i].ap.mac[1], b[i].ap.mac[2], b[i].ap.mac[3],
                b[i].ap.mac[4], b[i].ap.mac[5], b[i].ap.freq, b[i].h.rssi);
        }
    }

    for (i = 0; i < scan_cell; i++) {
        b[i].h.magic = BEACON_MAGIC;
        b[i].h.type = SKY_BEACON_NBIOT;
        b[i].h.rssi = -(44 + (rand() % 113)); /* nrsrp -156 thru -44 */
        b[i].cell.id1 = 200 + (rand() % 600); /* mcc 200 thru 799 */
        b[i].cell.id2 = rand() % 1000; /* mnc 0 thru 999 */
        b[i].cell.id3 = 1 + rand() % 65535; /* tac 1 thru 65535 */
        b[i].cell.id4 = rand() % 268435456; /* e_cellid 0 thru 268435455 */
        b[i].cell.id5 = rand() % 504; /* ncid 0 thru 503 */
        b[i].cell.freq = rand() % 262144; /* freq 0 thru 262143 */
    }

    for (i = 0; i < scan_cell; i++) {
        if (sky_add_cell_nb_iot_beacon(ctx, &sky_errno, b[i].cell.id1, b[i].cell.id2, b[i].cell.id4,
                b[i].cell.id3, b[i].cell.id5, b[i].cell.freq, timestamp, b[i].h.rssi, 1)) {
            printf("sky_add_nbiot_beacon sky_errno contains '%s'\n", sky_perror(sky_errno));
        } else {
            printf(
                "Added Test Beacon % 2d: Type: %d, mcc: %d, mnc: %d, e_cellid: %lld, tac: %d, ncid: %d, earfcn: %d, rssi: %d\n",
                i, b[i].h.type, b[i].cell.id1, b[i].cell.id2, (long long int)b[i].cell.id4,
                b[i].cell.id3, b[i].cell.id5, b[i].cell.freq, b[i].h.rssi);
        }
    }

    sky_add_cell_cdma_beacon(ctx, &sky_errno,
        1552, // sid
        45004, // nid
        37799, // bsid
        timestamp - 315, // timestamp
        -159, // rscp
        0); // serving

    scan_cell = (SCAN_CELL / 10) + rand() % (SCAN_CELL / 10);
    for (i = 0; i < scan_cell; i++) {
        b[i].h.magic = BEACON_MAGIC;
        b[i].h.type = SKY_BEACON_GSM;
        b[i].h.rssi = -(32 + (rand() % 96)); /* rssi -128 thru -32 */
        b[i].cell.id1 = 200 + (rand() % 599); /* mcc */
        b[i].cell.id2 = rand() % 999; /* mnc */
        b[i].cell.id3 = rand() % 65535; /* lac */
        b[i].cell.id4 = rand() % 65535; /* ci */
        b[i].cell.id5 = SKY_UNKNOWN_ID5;
        b[i].cell.freq = SKY_UNKNOWN_ID6;
    }

    for (i = 0; i < scan_cell; i++) {
        if (sky_add_cell_gsm_beacon(ctx, &sky_errno, b[i].cell.id3, b[i].cell.id4, b[i].cell.id1,
                b[i].cell.id2, timestamp, b[i].h.rssi, 1)) {
            printf("sky_add_gsm_beacon sky_errno contains '%s'\n", sky_perror(sky_errno));
        } else {
            printf(
                "Added Test Beacon % 2d: Type: %d, lac: %d, ci: %lld, mcc: %d, mnc: %d, rssi: %d\n",
                i, b[i].h.type, b[i].cell.id3, (long long int)b[i].cell.id4, b[i].cell.id1,
                b[i].cell.id2, b[i].h.rssi);
        }
    }

    /* Determine how big the network request buffer must be, and allocate a */
    /* buffer of that length. This function must be called for each request. */
    if (sky_sizeof_request_buf(ctx, &bufsize, &sky_errno) == SKY_ERROR) {
        printf("Error getting size of request buffer: %s\n", sky_perror(sky_errno));
        exit(-1);
    } else
        printf("Required buffer size = %d\n", bufsize);

    p = calloc(bufsize, sizeof(uint8_t));
    switch (sky_finalize_request(ctx, &sky_errno, p, bufsize, &loc, &response_size)) {
    case SKY_FINALIZE_LOCATION:

        printf("sky_finalize_request: GPS: %d.%06d,%d.%06d,%d\n", (int)loc.lat,
            (int)fabs(round(1000000 * (loc.lat - (int)loc.lat))), (int)loc.lon,
            (int)fabs(round(1000000 * (loc.lon - (int)loc.lon))), loc.hpe);

        if (sky_close(&sky_errno, &pstate))
            printf("sky_close sky_errno contains '%s'\n", sky_perror(sky_errno));
        if (pstate != NULL)
            nv_cache_save(pstate);
        exit(0);
        break;
    default:
    case SKY_FINALIZE_ERROR:
        printf("sky_finalize_request sky_errno contains '%s'\n", sky_perror(sky_errno));
        if (sky_close(&sky_errno, &pstate))
            printf("sky_close sky_errno contains '%s'\n", sky_perror(sky_errno));
        exit(-1);
        break;
    case SKY_FINALIZE_REQUEST:
        break;
    }

    free(p);
    DUMP_WORKSPACE(ctx);

    for (t = SKY_BEACON_AP; t != SKY_BEACON_MAX; t++) {
        printf("get_num_beacons: Type: %d, count: %d\n", t, i = get_num_beacons(ctx, t));
        if (t == SKY_BEACON_AP) {
            for (i--; i >= 0; i--) {
                uint8_t *m = get_ap_mac(ctx, i);
                m = m;

                printf("ap mac:       %d MAC %02X:%02X:%02X:%02X:%02X:%02X\n", i, m[0], m[1], m[2],
                    m[3], m[4], m[5]);
                printf("ap freq:   %d, %lld\n", i, (long long)get_ap_freq(ctx, i));
                printf("ap rssi:      %d, %lld\n", i, (long long)get_ap_rssi(ctx, i));
                printf("ap is_connected:      %d, %d\n", i, get_ap_is_connected(ctx, i));
                printf("ap age:      %d, %lld\n", i, (long long)get_ap_age(ctx, i));
            }
            for (int v = 0; v < get_num_vaps(ctx); v++) {
                printf("vap: %d\n", v);
                get_vap_data(ctx, v);
            }
        }
        if (t == SKY_BEACON_GSM)
            for (i--; i >= 0; i--) {
                printf("gsm mcc:       %d, %d\n", i,
                    (int)get_cell_id1(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("gsm mnc:       %d, %d\n", i,
                    (int)get_cell_id2(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("gsm lac:       %d, %d\n", i,
                    (int)get_cell_id3(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("gsm ci:        %d, %lld\n", i,
                    (long long)get_cell_id4(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("gsm rssi:      %d, %lld\n", i,
                    (long long)get_cell_rssi(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("gsm connected: %d, %d\n", i,
                    get_cell_connected_flag(ctx, &ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("gsm age:       %d, %lld\n", i,
                    (long long)get_cell_age(&ctx->beacon[get_base_beacons(ctx, t) + i]));
            }
        if (t == SKY_BEACON_NBIOT)
            for (i--; i >= 0; i--) {
                printf("nbiot mcc:     %d, %d\n", i,
                    (int)get_cell_id1(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("nbiot mnc:     %d, %d\n", i,
                    (int)get_cell_id2(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("nbiot tac:     %d, %d\n", i,
                    (int)get_cell_id3(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("nbiot ecellid: %d, %lld\n", i,
                    (long long)get_cell_id4(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("nbiot rssi:    %d, %lld\n", i,
                    (long long)get_cell_rssi(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("nbiot is connected:      %d, %d\n", i,
                    get_cell_connected_flag(ctx, &ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("nbiot age:      %d, %lld\n", i,
                    (long long)get_cell_age(&ctx->beacon[get_base_beacons(ctx, t) + i]));
            }
        if (t == SKY_BEACON_CDMA)
            for (i--; i >= 0; i--) {
                printf("cdma sid:     %d, %d\n", i,
                    (int)get_cell_id2(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("cdma nid:     %d, %d\n", i,
                    (int)get_cell_id3(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("cdma bsid:     %d, %lld\n", i,
                    (long long)get_cell_id4(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("cdma rssi:    %d, %lld\n", i,
                    (long long)get_cell_rssi(&ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("cdma is connected:      %d, %d\n", i,
                    get_cell_connected_flag(ctx, &ctx->beacon[get_base_beacons(ctx, t) + i]));
                printf("cdma age:      %d, %lld\n", i,
                    (long long)get_cell_age(&ctx->beacon[get_base_beacons(ctx, t) + i]));
            }
    }

        /* Save to cache with a location */
        /* Set some new dynamic parameters */
        /* Create location and add to cache */
#if 0
    if (sky_decode_response(ctx, &sky_errno, NULL, 0, &loc))
        printf("sky_decode_response sky_errno contains '%s'\n",
            sky_perror(sky_errno))
#else
    loc.lat = -80.0 - ((float)rand() / RAND_MAX * 30.0);
    loc.lon = 30.0 + (30.0 * (float)rand() / RAND_MAX);
    loc.hpe = 30.0 + (500.0 * (float)rand() / RAND_MAX);
    loc.time = (*ctx->gettime)(NULL);
    loc.location_source = SKY_LOCATION_SOURCE_WIFI;
    loc.location_status = SKY_LOCATION_STATUS_SUCCESS;

#if FAKE_NETWORK_TIME
    if (add_to_cache(ctx, &loc) != SKY_SUCCESS)
        printf("add_to_cache: Error appropriate with fake network time\n");
#else
    // add_to_cache(ctx, &loc);
#endif
    DUMP_CACHE(ctx);
    /* simulate new config from server */
    ctx->cache->config.total_beacons = 14;
    ctx->cache->config.max_ap_beacons = 8;
    ctx->cache->config.cache_match_threshold = 49;

#endif

    if (sky_close(&sky_errno, &pstate))
        printf("sky_close sky_errno contains '%s'\n", sky_perror(sky_errno));
    if (pstate != NULL)
        nv_cache_save(pstate);
}
