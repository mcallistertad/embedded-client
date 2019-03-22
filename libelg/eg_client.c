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
#include "libelg.h"
#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t nv[1000];

/* stubs for functions to be provided */
void save_state(uint8_t *p)
{
	if (sizeof(nv) > sky_sizeof_state(p))
		memcpy(nv, p, sky_sizeof_state(p));
}
void send_request(uint8_t *req, uint32_t req_size)
{
}
void get_response(uint8_t *r, uint32_t size)
{
}

/* Example assumes a scan with 10 AP beacons
 */
#define SCAN_LIST_SIZE 10

/* From configuration
 */
uint32_t sky_partner_id = 2;
uint32_t sky_aes_key_id = 3;
uint8_t sky_aes_key[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

/*! \brief validate fundamental functionality of the ELG IoT library
 *
 *  @param ac arg count
 *  @param av arg vector
 *
 *  @returns 0 for success or negative number for error
 */
void dump(void *ctx)
{
	uint32_t *p = ctx;
	int i;

	for (i = 0; i < sky_sizeof_workspace(MAX_BEACONS) / sizeof(int); i += 8)
		printf("ctx: %08X %08X %08X %08X  %08X %08X %08X %08X\n",
		       p[i + 000], p[i + 001], p[i + 002], p[i + 003],
		       p[i + 004], p[i + 005], p[i + 006], p[i + 007]);
	printf("\n");
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
	sky_errno_t sky_errno = -1;
	sky_ctx_t *ctx;
	void *p;
	uint32_t bufsize;
	uint8_t mac[MAC_SIZE] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e };
	time_t timestamp = time(NULL);
	int8_t rssi = 50;
	uint32_t ch = 65;
	uint8_t *pstate;
	uint8_t *prequest;
	uint32_t request_size;
	uint32_t response_size;

	/* location result */
	float lat, lon;
	uint16_t hpe;
	time_t ts;

	if (sky_open(&sky_errno, mac /* device_id */, sizeof(mac),
		     sky_partner_id, sky_aes_key_id, sky_aes_key, NULL,
		     NULL) == SKY_ERROR) {
		printf("sky_open returned bad value, Can't continue\n");
		exit(-1);
	}

	bufsize = sky_sizeof_workspace(SCAN_LIST_SIZE);

	/* allocate workspace */
	if ((p = (sky_ctx_t *)alloca(bufsize)) == NULL) {
		printf("Can't alloc space\n");
		exit(-1);
	}

	if ((ctx = sky_new_request(p, bufsize, &sky_errno, SCAN_LIST_SIZE)) ==
	    NULL) {
		printf("sky_new_request() returned bad value\n");
		printf("sky_errno contains '%s'\n", sky_perror(sky_errno));
	}

	/* AP 1 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 2 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 3 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 4 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 5 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 6 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 7 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 8 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 9 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* AP 10 */
	if (sky_add_ap_beacon(ctx, &sky_errno, mac, timestamp, rssi, ch, 0))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	/* nb IoT 11 */
	if (sky_add_cell_nb_iot_beacon(ctx, &sky_errno, 200, 2, 174754934, 542,
				       -1, -1, 1))
		printf("sky_add_ap_beacon sky_errno contains '%s'\n",
		       sky_perror(sky_errno));

	/* e.g. call assuming no cached scan/location */
	if (sky_finalize_request(ctx, &sky_errno, &prequest, &request_size,
				 (void *)NULL, NULL, NULL, NULL,
				 &response_size))
		printf("sky_finalize_request sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	else {
		p = alloca(response_size);
		send_request(prequest, request_size);
		get_response(p, response_size);
		if (sky_decode_response(ctx, &sky_errno, p, response_size, &lat,
					&lon, &hpe, &ts))
			printf("sky_decode_response sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
	}

	if (sky_close(&sky_errno, &pstate))
		printf("sky_close sky_errno contains '%s'\n",
		       sky_perror(sky_errno));
	if (pstate != NULL)
		save_state(pstate);
}
