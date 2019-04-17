/*! \file libelg/unit_test.c
 *  \brief unit tests - Skyhook ELG API Version 3.0 (IoT)
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
#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "../.submodules/tiny-AES128-C/aes.h"
#define SKY_LIBELG 1
#include "config.h"
#include "beacons.h"
#include "workspace.h"
#include "libelg.h"
#include "utilities.h"
#include "crc32.h"

#include "test_beacons.h"
#include "send.h"
#include "config.h"

Sky_cache_t nv_space;

/* Example assumes a scan with 100 AP beacons
 */
#define SCAN_LIST_SIZE 100

/*! \brief set mac address. 30% are virtual AP
 *
 *  @param mac pointer to mac address
 *
 *  @returns 0 for success or negative number for error
 */
void set_mac(uint8_t *mac)
{
    if (rand() % 3 == 0) {
        /* Virtual MAC */
        uint8_t ref[] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e };
        memcpy(mac, ref, sizeof(ref));
        mac[rand() % 3 + 3] ^= (0x01 << (rand() % 7));
    } else {
        /* Non Virtual MAC */
        uint8_t ref[] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e };
        memcpy(mac, ref, sizeof(ref));
        mac[rand() % 3] = (rand() % 256);
        mac[rand() % 3 + 3] = (rand() % 256);
    }
}

/*! \brief dump the beacons in the workspace
 *
 *  @param ctx workspace pointer
 *
 *  @returns 0 for success or negative number for error
 */
void dump(Sky_ctx_t *ctx)
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
                   i, ctx->beacon[i].ap.mac[0],
                   ctx->beacon[i].ap.mac[1],
                   ctx->beacon[i].ap.mac[2],
                   ctx->beacon[i].ap.mac[3],
                   ctx->beacon[i].ap.mac[4],
                   ctx->beacon[i].ap.mac[5],
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
                   i, ctx->beacon[i].nbiot.mcc,
                   ctx->beacon[i].nbiot.mnc,
                   ctx->beacon[i].nbiot.e_cellid,
                   ctx->beacon[i].nbiot.tac,
                   ctx->beacon[i].nbiot.rssi);
            break;
        }
    }
#if 0
    uint32_t *p = (void *)ctx;

    for (i = 0; i < sky_sizeof_workspace(TOTAL_BEACONS) / sizeof(int); i += 8)
        printf("ctx: %08X %08X %08X %08X  %08X %08X %08X %08X\n",
               p[i + 000], p[i + 001], p[i + 002], p[i + 003],
               p[i + 004], p[i + 005], p[i + 006], p[i + 007]);
#endif
}

/*! \brief validate fundamental functionality of the ELG IoT library
 *
 *  @param ac arg count
 *  @param av arg vector
 *
 *  @returns 0 for success or negative number for error
 */
int logger(Sky_log_level_t level, const char *s, int max)
{
    printf("Skyhook libELG %s: %.*s\n",
           level == SKY_LOG_LEVEL_CRITICAL ?
               "CRIT" :
               level == SKY_LOG_LEVEL_ERROR ?
               "ERRR" :
               level == SKY_LOG_LEVEL_WARNING ?
               "WARN" :
               level == SKY_LOG_LEVEL_DEBUG ? "DEBG" : "UNKN",
           max, s);
    return 0;
}

/*! \brief check for saved cache state
 *
 *  @param void
 *
 *  @returns NULL for failure to restore cache, pointer to cache otherwise
 */
Sky_cache_t *nv_cache(void)
{
    FILE *fio;
    uint8_t *p = (void *)&nv_space;

    if ((fio = fopen("nv_cache", "r")) != NULL) {
        if (fread(p, sizeof(Sky_header_t), 1, fio) == 1 &&
            nv_space.header.magic == SKY_MAGIC &&
            nv_space.header.crc32 ==
                sky_crc32(
                    &nv_space.header.magic,
                    (uint8_t *)&nv_space.header.crc32 -
                        (uint8_t *)&nv_space.header.magic)) {
            if (fread(p + sizeof(Sky_header_t),
                  nv_space.header.size - sizeof(Sky_header_t),
                  1, fio) == 1) {
                if (validate_cache(&nv_space)) {
                    printf("validate_cache: Restoring Cache\n");
                    return &nv_space;
                } else
                    printf("validate_cache: false\n");
            }
        }
    }
    return NULL;
}

/*! \brief save cache state
 *
 *  @param void
 *
 *  @returns 0 for success or negative number for error
 */
Sky_status_t nv_cache_save(uint8_t *p)
{
    FILE *fio;
    Sky_cache_t *c = (void *)p;

    if (validate_cache(c)) {
        if ((fio = fopen("nv_cache", "w+")) != NULL) {
            if (fwrite(p, c->header.size, 1, fio) == 1) {
                printf("nv_cache_save: cache size %d (%lu)\n",
                       c->header.size, sizeof(Sky_cache_t));
                return 0;
            } else
                printf("fwrite failed\n");
        } else
            printf("fopen failed\n");
    } else
        printf("nv_cache_save: failed to validate cache\n");
    return -1;
}

/*! \brief validate fundamental functionality of the ELG IoT library
 *
 *  @param ac arg count
 *  @param av arg vector
 *
 *  @returns 0 for success or negative number for error
 */
int main(int argc, char *argv[])
{
    int i;
    Sky_errno_t sky_errno = -1;
    Sky_ctx_t *ctx;
    uint32_t *p;
    uint32_t bufsize;
    uint8_t aes_key[AES_SIZE] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e,
                      0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e,
                      0xd4, 0x85, 0x64, 0xb2 };
    uint8_t mac[MAC_SIZE] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e };
    uint8_t *pstate;
    uint8_t *prequest;
    uint32_t request_size;
    uint32_t response_size;
//  Sky_beacon_type_t t;

    Config_t config;
    int id = 0;
    if (argc > 2)
        id = atoi(argv[2]);

    load_config(argv[1], &config, id);
    exit(1);

    /* Intializes random number generator */
    srand((unsigned)time(NULL));

    if (sky_open(&sky_errno, mac /* device_id */, MAC_SIZE, 1, 1, aes_key,
             nv_cache(), SKY_LOG_LEVEL_ALL, &logger) == SKY_ERROR) {
        printf("sky_open returned bad value, Can't continue\n");
        exit(-1);
    }
    /* Test sky_sizeof_workspace */
    bufsize = sky_sizeof_workspace(SCAN_LIST_SIZE);

    /* sky_sizeof_workspace should return a value below 5k and above 0 */
    if (bufsize == 0 || bufsize > 4096) {
        printf("sky_sizeof_workspace returned bad value, Can't continue\n");
        exit(-1);
    }

    /* allocate workspace */
    ctx = (Sky_ctx_t *)(p = alloca(bufsize));

    /* initialize the workspace */
    memset(p, 0, bufsize);

    if (sky_new_request(ctx, bufsize, &sky_errno, bufsize) != ctx) {
        printf("sky_new_request() returned bad value\n");
        printf("sky_errno contains '%s'\n", sky_perror(sky_errno));
    }

    logfmt(ctx, SKY_LOG_LEVEL_DEBUG, "ctx: magic:%08X size:%08X crc:%08X",
           ctx->header.magic, ctx->header.size, ctx->header.crc32);

    load_beacons("filename");
    exit(1);
/*
    struct wifi_scan ap_scan;
    get_next_ap_scan(&ap_scan);

    for (i = 0; i < ap_scan.num; i++) {
        if (sky_add_ap_beacon(ctx, &sky_errno, ap_scan.scan.aps[i].mac, ap_scan.scan.aps[i].time,
                ap_scan.scan.aps[i].rssi, ap_scan.scan.aps[i].channel, 1))
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                   "sky_add_ap_beacon sky_errno contains '%s'",
                   sky_perror(sky_errno));
//      else
//          logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
//                 "Added Test Beacon % 2d: Type: %d, MAC %02X:%02X:%02X:%02X:%02X:%02X rssi: %d\n",
//                 i, b[i].ap.type, b[i].ap.mac[0], b[i].ap.mac[1],
//                 b[i].ap.mac[2], b[i].ap.mac[3], b[i].ap.mac[4],
//                 b[i].ap.mac[5], b[i].ap.rssi);
    }

    for (i = 0; i < 3; i++) {
        b[i].nbiot.magic = BEACON_MAGIC;
        b[i].nbiot.type = SKY_BEACON_NBIOT;
        b[i].nbiot.mcc = 200 + (rand() % 599);
        b[i].nbiot.mnc = rand() % 999;
        b[i].nbiot.e_cellid = rand() % 268435456;
        b[i].nbiot.tac = rand();
        b[i].nbiot.rssi = -(44 + (rand() % 112));
    }

    for (i = 0; i < 3; i++) {
        if (sky_add_cell_nb_iot_beacon(
                ctx, &sky_errno, b[i].nbiot.mcc, b[i].nbiot.mnc,
                b[i].nbiot.e_cellid, b[i].nbiot.tac, timestamp,
                b[i].nbiot.rssi, 1))
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                   "sky_add_nbiot_beacon sky_errno contains '%s'",
                   sky_perror(sky_errno));
        else
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                   "Added Test Beacon % 2d: Type: %d, mcc: %d, mnc: %d, e_cellid: %d, tac: %d, rssi: %d\n",
                   i, b[i].nbiot.type, b[i].nbiot.mcc,
                   b[i].nbiot.mnc, b[i].nbiot.e_cellid,
                   b[i].nbiot.tac, b[i].nbiot.rssi);
    }

    for (i = 0; i < 2; i++) {
        b[i].gsm.magic = BEACON_MAGIC;
        b[i].gsm.type = SKY_BEACON_GSM;
        b[i].gsm.lac = rand() % 65535;
        b[i].gsm.ci = rand() % 65535;
        b[i].gsm.mcc = 200 + (rand() % 599);
        b[i].gsm.mnc = rand() % 999;
        b[i].gsm.rssi = -(32 + (rand() % 96));
    }

    for (i = 0; i < 2; i++) {
        if (sky_add_cell_gsm_beacon(ctx, &sky_errno, b[i].gsm.lac,
                        b[i].gsm.ci, b[i].gsm.mcc,
                        b[i].gsm.mnc, timestamp,
                        b[i].gsm.rssi, 1))
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                   "sky_add_gsm_beacon sky_errno contains '%s'",
                   sky_perror(sky_errno));
        else
            logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
                   "Added Test Beacon % 2d: Type: %d, lac: %d, ui: %d, mcc: %d, mnc: %d, rssi: %d\n",
                   i, b[i].gsm.type, b[i].gsm.lac, b[i].gsm.ci,
                   b[i].gsm.mcc, b[i].gsm.mnc, b[i].gsm.rssi);
    }
*/
    if (sky_finalize_request(ctx, &sky_errno, &prequest, &request_size,
                 (void *)NULL, NULL, NULL, NULL,
                 &response_size))
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
               "sky_finalize_request sky_errno contains '%s'",
               sky_perror(sky_errno));
    if (strcmp((char *)prequest, "SKYHOOK REQUEST MSG"))
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
               "sky_finalize_request bad request buffer");
    printf("request = %s\nreq_size = %d, resp_size = %d\n", prequest, request_size, response_size);
//  dump(ctx);

    char response[response_size];
    int32_t rc = send_request((char*) prequest, (int) request_size, response, "127.0.0.1", 9756);
    if (rc > 0)
        printf("resp = %s, len = %d\n", response, rc);

/*
    float lat, lon;
    uint16_t hpe;
    time_t ts;

    if (sky_decode_response(ctx, &sky_errno, response, bufsize, &lat, &lon, &hpe, &ts)) {
        printf("Error: unable to decode response - %s",sky_perror(sky_errno));
    }
*/
    if (sky_close(&sky_errno, &pstate))
        logfmt(ctx, SKY_LOG_LEVEL_DEBUG,
               "sky_close sky_errno contains '%s'\n",
               sky_perror(sky_errno));
    if (pstate != NULL)
        nv_cache_save(pstate);

    printf("Done.\n\n");
}
