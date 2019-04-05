/*! \file libelg/workspace.h
 *  \brief Skyhook ELG API workspace structures
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
#ifndef SKY_WORKSPACE_H
#define SKY_WORKSPACE_H

#define SKY_MAGIC 0xD1967805
typedef struct sky_header {
	uint32_t magic;
	uint32_t size;
	time_t time;
	uint32_t crc32;
} Sky_header_t;

/*! \brief sky_log_level logging levels
 */
#ifdef SKY_LIBELG
typedef enum {
	SKY_LOG_LEVEL_CRITICAL = 1,
	SKY_LOG_LEVEL_ERROR,
	SKY_LOG_LEVEL_WARNING,
	SKY_LOG_LEVEL_DEBUG,
	SKY_LOG_LEVEL_ALL = SKY_LOG_LEVEL_DEBUG,
} Sky_log_level_t;
#endif

typedef struct sky_ctx {
	Sky_header_t header; /* magic, size, timestamp, crc32 */
	int (*logf)(Sky_log_level_t level, const char *s, int max);
	Sky_log_level_t min_level;
	int16_t expect; /* number of beacons to be added */
	int16_t len; /* number of beacons in list (0 == none) */
	Beacon_t beacon[MAX_BEACONS + 1]; /* beacon data */
	int16_t ap_len; /* number of AP beacons in list (0 == none) */
	int16_t ap_low; /* first of AP beacons in list (0 based index) */
	int16_t connected; /* which beacon is conneted (-1 == none) */
	Gps_t gps; /* GPS info */
	uint8_t request[sizeof(Sky_header_t) + sizeof(int32_t) +
			(sizeof(Beacon_t) * MAX_BEACONS) + sizeof(int32_t) +
			sizeof(Gps_t)];
} Sky_ctx_t;

typedef struct sky_state {
	Sky_header_t header; /* magic, size, timestamp, crc32 */
	uint8_t sky_id_len; /* device ID len */
	uint8_t sky_device_id[MAC_SIZE]; /* device ID */
	uint32_t sky_partner_id; /* partner ID */
	uint32_t sky_aes_key_id; /* aes key ID */
	uint8_t sky_aes_key[16]; /* aes key */
	int16_t len; /* number of beacons */
	Beacon_t beacon[TOTAL_BEACONS]; /* beacons */
} Sky_cache_t;

#endif
