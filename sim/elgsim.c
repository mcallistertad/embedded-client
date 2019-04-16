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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "../.submodules/tiny-AES128-C/aes.h"

#include "libelg.h"
#include "crc32.h"
#include "beacons.h"

#include "sim_beacons.h"
#include "sim_send.h"
#include "sim_config.h"

void *nv_space;

#define SCAN_LIST_SIZE 100

/*! \brief check for saved cache state
 *
 *  @param client_id
 *
 *  @returns NULL for failure to restore cache, pointer to cache otherwise
 */
void *nv_cache(uint16_t client_id)
{

	char cache_name[16];
	sprintf(cache_name, "nv_cache_%d", client_id);

    struct {
            uint32_t magic;
            uint32_t size;
            uint32_t time;
            uint32_t crc32;
    } tmp;
    FILE *fio;

    if ((fio = fopen(cache_name, "r")) != NULL) {
            if (fread((void *)&tmp, sizeof(tmp), 1, fio) == 1 &&
                tmp.crc32 == sky_crc32(&tmp.magic,
                                       (uint8_t *)&tmp.crc32 -
                                               (uint8_t *)&tmp.magic)) {
                    nv_space = malloc(tmp.size);
                    rewind(fio);
                    if (fread(nv_space, tmp.size, 1, fio) == 1)
                            return nv_space;
            }
    }
	return NULL;
}

/*! \brief save cache state
 *
 *  @param p - pointer to cache
 *  @param client_id - client id
 *
 *  @returns 0 for success or negative number for error
 */
Sky_status_t nv_cache_save(void *p, uint16_t client_id)
{
	char cache_name[16];
	sprintf(cache_name, "nv_cache_%d", client_id);

	FILE *fio;
    struct {
            uint32_t magic;
            uint32_t size;
            uint32_t time;
            uint32_t crc32;
    } *c = p;

    if (c->crc32 ==
        sky_crc32(&c->magic, (uint8_t *)&c->crc32 - (uint8_t *)&c->magic)) {
            if ((fio = fopen(cache_name, "w+")) != NULL) {
                    if (fwrite(p, c->size, 1, fio) == 1) {
                            printf("nv_cache_save: cache size %d\n",
                                   c->size);
                            return 0;
                    } else
                            printf("fwrite failed\n");
            } else
                    printf("fopen failed\n");
    } else
            printf("nv_cache_save: failed to validate cache\n");
    return -1;
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

        if (!rand_buf)
                return 0;

        for (i = 0; i < bufsize; i++)
                rand_buf[i] = rand() % 256;
        return bufsize;
}


/*! \brief logging function
 *
 *  @param level log level of this message
 *  @param s this message
 *
 *  @returns 0 for success or negative number for error
 */
int logger(Sky_log_level_t level, const char *s)
{
        printf("Skyhook libELG %s: %.*s\n",
               level == SKY_LOG_LEVEL_CRITICAL ?
                       "CRIT" :
                       level == SKY_LOG_LEVEL_ERROR ?
                       "ERRR" :
                       level == SKY_LOG_LEVEL_WARNING ?
                       "WARN" :
                       level == SKY_LOG_LEVEL_DEBUG ? "DEBG" : "UNKN",
               80, s);
        return 0;
}


/*! \brief validate fundamental functionality of the ELG IoT library
 *
 *  @param argc count
 *  @param argv vector of arguments
 *
 *  @returns 0 for success or negative number for error
 */
int main(int argc, char *argv[])
{
	int i;
	Sky_errno_t sky_errno = -1;
	Sky_ctx_t *ctx;
	Sky_status_t ret_status;
	uint32_t *p;
	uint32_t bufsize;
	void *pstate;
	void *prequest;
	uint8_t *response;
	uint32_t request_size;
	uint32_t response_size;
	Sky_location_t loc;

	Config_t config;
	int ret_code = 0;
	int id = 0;
	if (argc > 2)
		id = atoi(argv[2]);

	// Load the configuration
	ret_code = load_config(argv[1], &config, id);
	if (ret_code == -1)
		exit(-1);

	// Initialize the Skyhook resources
    if (sky_open(&sky_errno, config.device_mac, MAC_SIZE, 1, 1, config.key,
                 nv_cache(config.client_id), SKY_LOG_LEVEL_ALL, &logger,
                 &rand_bytes) == SKY_ERROR) {
            printf("sky_open returned bad value, Can't continue\n");
            exit(-1);
    }


    // Get the size of workspace needed
    bufsize = sky_sizeof_workspace(SCAN_LIST_SIZE);
    if (bufsize == 0 || bufsize > 4096) {
            printf("sky_sizeof_workspace returned bad value, Can't continue\n");
            exit(-1);
    }

    // Allocate and initialize workspace
    ctx = (Sky_ctx_t *)(p = malloc(bufsize));
    memset(p, 0, bufsize);

    // Start new request
    if (sky_new_request(ctx, bufsize, &sky_errno, bufsize) != ctx) {
            printf("sky_new_request() returned bad value\n");
            printf("sky_errno contains '%s'\n", sky_perror(sky_errno));
    }

    // Load test beacons from a file
	ret_code = load_beacons(config.scan_file);
	if (ret_code == -1)
		exit(-1);

	WifiScan_t scan;
	get_next_ap_scan(&scan);

	// Add APs to the request
	for (i = 0; i < scan.num_aps; i++) {
		ret_status = sky_add_ap_beacon(ctx, &sky_errno, scan.wifi.aps[i].mac, scan.wifi.aps[i].age,
				scan.wifi.aps[i].rssi, scan.wifi.aps[i].channel, 1);
		if (ret_status == SKY_SUCCESS)
			printf("AP #%d added\n",i);
		else
			printf("sky_add_ap_beacon sky_errno contains '%s'",
			       sky_perror(sky_errno));
	}

	// Add Cell to the request
	switch (scan.cell_type) {
		case SKY_BEACON_GSM:
            ret_status = sky_add_cell_gsm_beacon(ctx, &sky_errno, scan.cell.gsm.lac,
            		scan.cell.gsm.ci, scan.cell.gsm.mcc,
					scan.cell.gsm.mnc, scan.cell.gsm.age,
					scan.cell.gsm.rssi, 1);
            break;
		case SKY_BEACON_LTE:
            ret_status = sky_add_cell_lte_beacon(ctx, &sky_errno, 0,
            		scan.cell.lte.eucid, scan.cell.lte.mcc,
					scan.cell.lte.mnc, scan.cell.lte.age,
					scan.cell.lte.rssi, 1);
            break;
		case SKY_BEACON_NBIOT:
            ret_status = sky_add_cell_nb_iot_beacon(ctx, &sky_errno, scan.cell.nbiot.mcc,
            		scan.cell.nbiot.mnc, scan.cell.nbiot.e_cellid,
					scan.cell.nbiot.tac, scan.cell.nbiot.age,
					scan.cell.nbiot.rssi, 1);
            break;
		default:
			ret_status = SKY_ERROR;
	}
	if (ret_status == SKY_SUCCESS)
		printf("Cell added\n");
	else
		printf("sky_add_ap_beacon sky_errno contains '%s'",
		       sky_perror(sky_errno));

	// Finalize the request by check the cache
    switch (sky_finalize_request(ctx, &sky_errno, &prequest, &request_size,
                                 &loc, &response_size)) {
		case SKY_FINALIZE_LOCATION:
			printf("sky_finalize_request: lat: %.6f, lon: %.6f, hpe: %d, source: %d\n",
					loc.lat, loc.lon, loc.hpe, loc.location_source);
			if (sky_close(&sky_errno, &pstate))
					printf("sky_close sky_errno contains '%s'\n",
						   sky_perror(sky_errno));
			if (pstate != NULL)
					nv_cache_save(pstate, config.client_id);
			exit(0);
			break;
		default:
		case SKY_FINALIZE_ERROR:
			printf("sky_finalize_request sky_errno contains '%s'", sky_perror(sky_errno));
			if (sky_close(&sky_errno, &pstate))
					printf("sky_close sky_errno contains '%s'\n",
						   sky_perror(sky_errno));
			exit(-1);
			break;
		case SKY_FINALIZE_REQUEST:
			response = malloc(response_size * sizeof(uint8_t));
			int32_t rc = send_request((char*) prequest, (int) request_size, response, config.server, config.port);
			if (rc > 0)
				printf("resp = %s, len = %d\n", response, rc);
			break;
    }

    // Decode the response from server or cache
    ret_status = sky_decode_response(ctx, &sky_errno, response, bufsize, &loc);
    if (ret_status == SKY_SUCCESS)
    	printf("sky_decode_response: lat: %.6f, lon: %.6f, hpe: %d, source: %d\n",
    						loc.lat, loc.lon, loc.hpe, loc.location_source);
    else
    	printf("sky_decode_response sky_errno contains '%s'\n", sky_perror(sky_errno));

    ret_status = sky_close(&sky_errno, &pstate);
    if (ret_status != SKY_SUCCESS)
    	printf("sky_close sky_errno contains '%s'\n", sky_perror(sky_errno));

    if (pstate != NULL)
            nv_cache_save(pstate, config.client_id);

    printf("Done.\n\n");
}
