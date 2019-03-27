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
#include "libelg.h"

/* Maximum number of beacons in scans lists */
#define MAX_AP_SCAN_LIST_SIZE 100
#define MAX_NB_IOT_SCAN_LIST_SIZE 1
#define MAX_SCAN_LIST_SIZE (MAX_AP_SCAN_LIST_SIZE + MAX_NB_IOT_SCAN_LIST_SIZE)

/* scan list definitions (platform dependant) */
struct ap_scan {
	uint8_t mac[MAC_SIZE];
	uint32_t channel;
	int8_t rssi;
};
struct nb_iot_scan {
	uint16_t mcc;
	uint16_t mnc;
	uint32_t e_cellid;
	uint16_t tac;
	int8_t nrsrp;
};

/* functions to be provided */
/* allocate/free some space */
extern void *alloc_space(size_t size);
extern void free_space(void *space);
/* stash the library state in non-volatile memory */
extern void save_state(uint8_t *p, int32_t size);
/* get pointer to any saved state or NULL */
extern uint8_t *get_state(void);
/* send request to Skyhook server */
extern void send_request(uint8_t *req, uint32_t req_size);
/* get response from Skyhook server or timeout */
extern void get_response(uint8_t *r, uint32_t size);
/* return location result */
extern void new_location(float lat, float lon, uint16_t hpe, time_t ts);

/* From configuration
 */
uint32_t sky_partner_id = 2;
uint32_t sky_aes_key_id = 3;
uint8_t sky_aes_key[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

/*! \brief Typical use case
 *
 *  @param ap_scan latest ap scan data
 *  @param ap_size latest ap scan data length
 *  @param ap_scan_ts time of latest ap scan data
 *  @param nb_iot_scan latest nb_iot scan data
 *  @param nb_iot_scan_ts time of latest nb_iot scan data
 *
 *  @returns 0 for success or negative number for error
 */
int get_skyhook_location(struct ap_scan *ap_scan, size_t ap_size,
			 time_t ap_scan_ts, struct nb_iot_scan *nb_iot_scan,
			 time_t nb_iot_scan_ts)
{
	uint8_t *pstate = NULL;
	struct ap_scan *pap;
	sky_errno_t sky_errno = -1;
	sky_ctx_t *ctx;
	sky_finalize_t fret;
	void *p;
	uint32_t bufsize;
	uint8_t mac[MAC_SIZE] = { 0xd4, 0x85, 0x64, 0xb2, 0xf5, 0x7e };
	uint8_t *prequest;
	uint32_t request_size;
	uint32_t response_size;
	int ret = -1;

	/* location result */
	float lat, lon;
	uint16_t hpe;
	time_t ts;

	if (sky_open(&sky_errno, mac /* device_id */, sizeof(mac),
		     sky_partner_id, sky_aes_key_id, sky_aes_key, get_state(),
		     NULL) == SKY_ERROR) {
		printf("sky_open returned bad value: '%s', Can't continue\n",
		       sky_perror(sky_errno));
		return ret;
	}

	bufsize = sky_sizeof_workspace(MAX_SCAN_LIST_SIZE);

	/* allocate workspace */
	if ((p = alloc_space(bufsize)) == NULL) {
		printf("Can't alloc space\n");
	} else {
		/* initialize workspace */
		if ((ctx = sky_new_request(p, bufsize, &sky_errno,
					   MAX_SCAN_LIST_SIZE)) == NULL)
			printf("sky_new_request() returned '%s'\n",
			       sky_perror(sky_errno));

		/* add AP beacons */
		for (pap = ap_scan; pap - ap_scan < ap_size; pap++) {
			if (sky_add_ap_beacon(ctx, &sky_errno, pap->mac,
					      ap_scan_ts, pap->rssi,
					      pap->channel, false) == SKY_ERROR)
				printf("sky_add_ap_beacon sky_errno contains '%s'\n",
				       sky_perror(sky_errno));
			/* continue to try and process request without this beacon */
		}
		/* add a single nb IoT beacon */
		if (sky_add_cell_nb_iot_beacon(
			    ctx, &sky_errno, nb_iot_scan->mcc, nb_iot_scan->mnc,
			    nb_iot_scan->e_cellid, nb_iot_scan->tac,
			    nb_iot_scan_ts, nb_iot_scan->nrsrp,
			    true) == SKY_ERROR)
			printf("sky_add_cell_nb_iot_beacon sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
		/* continue to try and process request without this beacon */

		/* process the beacon info */
		if ((fret = sky_finalize_request(ctx, &sky_errno, &prequest,
						 &request_size, &lat, &lon,
						 &hpe, &ts, &response_size)) ==
		    SKY_FINALIZE_ERROR)
			printf("sky_finalize_request sky_errno contains '%s'\n",
			       sky_perror(sky_errno));
		else if (fret == SKY_FINALIZE_LOCATION) {
			/* report location result (from cache) */
			new_location(lat, lon, hpe, ts);
			ret = 0;
		} else {
			p = alloc_space(response_size);
			send_request(prequest, request_size);
			get_response(p, response_size);
			/* decode response */
			if (sky_decode_response(ctx, &sky_errno, p,
						response_size, &lat, &lon, &hpe,
						&ts) == SKY_ERROR)
				printf("sky_decode_response sky_errno contains '%s'\n",
				       sky_perror(sky_errno));
			else {
				/* report location result (from server) */
				new_location(lat, lon, hpe, ts);
				ret = 0;
			}
			free_space(p);
		}
		free_space(ctx);
	}

	/* close library and get state info.
     * Note: return success if location was reported */
	if (sky_close(&sky_errno, &pstate))
		printf("sky_close sky_errno contains '%s'\n",
		       sky_perror(sky_errno));

	/* if close returned a state, copy it to non-volatile memory */
	if (pstate != NULL)
		save_state(pstate, sky_sizeof_state(pstate));

	return ret;
}
