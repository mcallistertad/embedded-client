/*! \file libelg/libelg.h
 *  \brief Top level header file for Skyhook ELG API Version 3.0 (IoT)
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
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifndef SKY_LIBELG_H
#define SKY_LIBELG_H

#define SKY_PROTOCOL_VERSION 3

#define URL_SIZE 512
#define AUTH_SIZE 512

#define MAC_SIZE 6
#define AES_SIZE 16
#define IPV4_SIZE 4
#define IPV6_SIZE 16

#define MAX_MACS 2 // max # of mac addresses
#define MAX_IPS 2 // max # of ip addresses

#define MAX_APS 100 // max # of access points
#define MAX_GPSS 2 // max # of gps
#define MAX_CELLS 7 // max # of cells
#define MAX_BLES 5 // max # of blue tooth

#define MAX_DEVICE_ID 16

#ifndef SKY_LIBELG
typedef void Sky_ctx_t;
#endif

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

/*! \brief sky_errno Error Codes
 */
typedef enum {
	SKY_ERROR_NONE = 0,
	SKY_ERROR_NEVER_OPEN,
	SKY_ERROR_ALREADY_OPEN,
	SKY_ERROR_BAD_PARAMETERS,
	SKY_ERROR_TOO_MANY,
	SKY_ERROR_BAD_WORKSPACE,
	SKY_ERROR_BAD_STATE,
	SKY_ERROR_DECODE_ERROR,
	SKY_ERROR_RESOURCE_UNAVAILABLE,
	SKY_ERROR_CLOSE,
	SKY_ERROR_BAD_KEY,
	SKY_ERROR_NO_BEACONS,
	SKY_ERROR_ADD_CACHE,
	SKY_ERROR_GET_CACHE
} Sky_errno_t;

/*! \brief sky_log_level logging levels
 */
#ifndef SKY_LIBELG
typedef enum {
	SKY_LOG_LEVEL_CRITICAL = 1,
	SKY_LOG_LEVEL_ERROR,
	SKY_LOG_LEVEL_WARNING,
	SKY_LOG_LEVEL_DEBUG,
	SKY_LOG_LEVEL_ALL = SKY_LOG_LEVEL_DEBUG,
} Sky_log_level_t;
#endif

Sky_status_t sky_open(Sky_errno_t *sky_errno, uint8_t *device_id,
		      uint32_t id_len, uint32_t partner_id, uint32_t aes_key_id,
		      uint8_t aes_key[16], void *state_buf,
		      Sky_log_level_t min_level,
		      int (*logf)(Sky_log_level_t level, const char *s),
		      int (*rand_bytes)(uint8_t *rand_buf, uint32_t bufsize));

int32_t sky_sizeof_state(uint8_t *sky_state);

int32_t sky_sizeof_workspace(uint16_t number_beacons);

Sky_ctx_t *sky_new_request(void *workspace_buf, uint32_t bufsize,
			   Sky_errno_t *sky_errno, uint8_t number_beacons);

Sky_status_t sky_add_ap_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
			       uint8_t mac[6], time_t timestamp, int16_t rssi,
			       int32_t channel, bool is_connected);

Sky_status_t sky_add_cell_lte_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
				     uint16_t tac, uint32_t eucid, uint16_t mcc,
				     uint16_t mnc, time_t timestamp,
				     int16_t rsrp, bool is_connected);

Sky_status_t sky_add_cell_gsm_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
				     uint16_t lac, uint32_t ui, uint16_t mcc,
				     uint16_t mnc, time_t timestamp,
				     int16_t rssi, bool is_connected);

Sky_status_t sky_add_cell_umts_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
				      uint16_t lac, uint32_t ui, uint16_t mcc,
				      uint16_t mnc, time_t timestamp,
				      int16_t rscp, bool is_connected);

Sky_status_t sky_add_cell_cdma_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
				      uint32_t sid, uint16_t nid, uint16_t bsid,
				      time_t timestamp, int16_t rssi,
				      bool is_connected);

Sky_status_t sky_add_cell_nb_iot_beacon(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
					uint16_t mcc, uint16_t mnc,
					uint32_t e_cellid, uint32_t tac,
					time_t timestamp, int16_t nrsrp,
					bool is_connected);

Sky_status_t sky_add_gps(Sky_ctx_t *ctx, Sky_errno_t *sky_errno, float lat,
			 float lon, uint16_t hpe, float altitude, uint16_t vpe,
			 float speed, float bearing, time_t timestamp);

Sky_finalize_t sky_finalize_request(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
				    void **request_buf, uint32_t *bufsize,
				    float *lat, float *lon, uint16_t *hpe,
				    uint32_t *timestamp,
				    uint32_t *response_size);

Sky_status_t sky_decode_response(Sky_ctx_t *ctx, Sky_errno_t *sky_errno,
				 void *response_buf, uint32_t bufsize,
				 float *lat, float *lon, uint16_t *hpe,
				 uint32_t *timestamp);

char *sky_perror(Sky_errno_t sky_errno);

Sky_status_t sky_close(Sky_errno_t *sky_errno, void **sky_state);

#endif
