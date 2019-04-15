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
void *nv_space;

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
	uint8_t refs[5][MAC_SIZE] = { { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e },
				      { 0xe4, 0x75, 0x64, 0xb2, 0xf5, 0x7e },
				      { 0xf4, 0x65, 0x64, 0xb2, 0xf5, 0x7e },
				      { 0x14, 0x55, 0x64, 0xb2, 0xf5, 0x7e },
				      { 0x24, 0x45, 0x64, 0xb2, 0xf5, 0x7e } };

	if (rand() % 7 == 0) {
		/* Virtual MAC */
		memcpy(mac, refs[0], sizeof(refs[0]));
		mac[rand() % 3 + 3] ^= (0x01 << (rand() % 7));
		printf("Virt MAC\n");
	} else if (rand() % 7 != 0) {
		/* rand MAC */
		memcpy(mac, refs[rand() % 5], sizeof(refs[0]));
		printf("Rand MAC\n");
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

/*! \brief check for saved cache state
 *
 *  @returns NULL for failure to restore cache, pointer to cache otherwise
 */
void *nv_cache(void)
{
	struct {
		uint32_t magic;
		uint32_t size;
		uint32_t time;
		uint32_t crc32;
	} tmp;
	FILE *fio;

	if ((fio = fopen("nv_cache", "r")) != NULL) {
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
 *  @param p pointer to cache buffer
 *
 *  @returns 0 for success or negative number for error
 */
Sky_status_t nv_cache_save(void *p)
{
	FILE *fio;
	struct {
		uint32_t magic;
		uint32_t size;
		uint32_t time;
		uint32_t crc32;
	} *c = p;

	if (c->crc32 ==
	    sky_crc32(&c->magic, (uint8_t *)&c->crc32 - (uint8_t *)&c->magic)) {
		if ((fio = fopen("nv_cache", "w+")) != NULL) {
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

/*! \brief validate fundamental functionality of the ELG IoT library
 *
 *  @param ac arg count
 *  @param av arg vector
 *
 *  @returns 0 for success or negative number for error
 */
int main(int ac, char **av)
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
	time_t timestamp = time(NULL);
	uint32_t ch = 65;
	void *pstate;
	void *prequest;
	uint32_t request_size;
	uint32_t response_size;
	Beacon_t b[25];
	Sky_location_t loc;

	/* Intializes random number generator */
	srand((unsigned)time(NULL));

	if (sky_open(&sky_errno, mac /* device_id */, MAC_SIZE, 1, 1, aes_key,
		     nv_cache(), SKY_LOG_LEVEL_ALL, &logger,
		     &rand_bytes) == SKY_ERROR) {
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
	ctx = (Sky_ctx_t *)(p = malloc(bufsize));

	/* initialize the workspace */
	memset(p, 0, bufsize);

	if (sky_new_request(ctx, bufsize, &sky_errno, bufsize) != ctx) {
		printf("sky_new_request() returned bad value\n");
		printf("sky_errno contains '%s'\n", sky_perror(sky_errno));
	}

	for (i = 0; i < 25; i++) {
		set_mac(b[i].ap.mac);
		b[i].ap.channel = b[i].ap.mac[0];
		b[i].ap.rssi = rand() % 128;
	}

	for (i = 0; i < 25; i++) {
		if (sky_add_ap_beacon(ctx, &sky_errno, b[i].ap.mac, timestamp,
				      b[i].ap.rssi, ch, 1))
			printf("sky_add_ap_beacon sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
		else
			printf("Added Test Beacon % 2d: Type: %d, MAC %02X:%02X:%02X:%02X:%02X:%02X rssi: %d\n",
			       i, b[i].ap.type, b[i].ap.mac[0], b[i].ap.mac[1],
			       b[i].ap.mac[2], b[i].ap.mac[3], b[i].ap.mac[4],
			       b[i].ap.mac[5], b[i].ap.rssi);
	}

	for (i = 0; i < 3; i++) {
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
			printf("sky_add_nbiot_beacon sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
		else
			printf("Added Test Beacon % 2d: Type: %d, mcc: %d, mnc: %d, e_cellid: %d, tac: %d, rssi: %d\n",
			       i, b[i].nbiot.type, b[i].nbiot.mcc,
			       b[i].nbiot.mnc, b[i].nbiot.e_cellid,
			       b[i].nbiot.tac, b[i].nbiot.rssi);
	}

	for (i = 0; i < 2; i++) {
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
			printf("sky_add_gsm_beacon sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
		else
			printf("Added Test Beacon % 2d: Type: %d, lac: %d, ui: %d, mcc: %d, mnc: %d, rssi: %d\n",
			       i, b[i].gsm.type, b[i].gsm.lac, b[i].gsm.ci,
			       b[i].gsm.mcc, b[i].gsm.mnc, b[i].gsm.rssi);
	}

	switch (sky_finalize_request(ctx, &sky_errno, &prequest, &request_size,
				     &loc, &response_size)) {
	case SKY_FINALIZE_LOCATION:
		printf("sky_finalize_request: GPS: %.6f,%.6f,%d\n", loc.lat,
		       loc.lon, loc.hpe);
		if (sky_close(&sky_errno, &pstate))
			printf("sky_close sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
		if (pstate != NULL)
			nv_cache_save(pstate);
		exit(0);
		break;
	default:
	case SKY_FINALIZE_ERROR:
		printf("sky_finalize_request sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
		if (sky_close(&sky_errno, &pstate))
			printf("sky_close sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
		exit(-1);
		break;
	case SKY_FINALIZE_REQUEST:
		break;
	}
	if (strcmp((char *)prequest, "SKYHOOK REQUEST MSG"))
		printf("sky_finalize_request bad request buffer");

	if (sky_decode_response(ctx, &sky_errno, NULL, 0, &loc))
		printf("sky_decode_response sky_errno contains '%s'\n",
		       sky_perror(sky_errno));

	if (sky_close(&sky_errno, &pstate))
		printf("sky_close sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	if (pstate != NULL)
		nv_cache_save(pstate);
}
