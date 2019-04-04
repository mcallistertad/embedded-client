/*! \file libelg/utilities.h
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
#ifndef SKY_UTILITIES_H
#define SKY_UTILITIES_H

sky_status_t sky_return(sky_errno_t *sky_errno, sky_errno_t code);
int validate_workspace(sky_ctx_t *ctx);
sky_status_t add_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno, beacon_t *b,
			bool is_connected);
int logfmt(sky_ctx_t *ctx, sky_log_level_t level, const char *fmt, ...);
int64_t get_num_beacons(sky_ctx_t *ctx, sky_beacon_type_t t);
int get_base_beacons(sky_ctx_t *ctx, sky_beacon_type_t t);
int64_t get_num_aps(sky_ctx_t *ctx);
uint8_t *get_ap_mac(sky_ctx_t *ctx, uint32_t idx);
int64_t get_ap_channel(sky_ctx_t *ctx, uint32_t idx);
int64_t get_ap_rssi(sky_ctx_t *ctx, uint32_t idx);
int64_t get_num_gsm(sky_ctx_t *ctx);
uint64_t get_gsm_ui(sky_ctx_t *ctx, uint32_t idx);
int64_t get_gsm_mcc(sky_ctx_t *ctx, uint32_t idx);
int64_t get_gsm_mnc(sky_ctx_t *ctx, uint32_t idx);
int64_t get_gsm_lac(sky_ctx_t *ctx, uint32_t idx);
int64_t get_gsm_rssi(sky_ctx_t *ctx, uint32_t idx);
int64_t get_num_nbiot(sky_ctx_t *ctx);
int64_t get_nbiot_mcc(sky_ctx_t *ctx, uint32_t idx);
int64_t get_nbiot_mnc(sky_ctx_t *ctx, uint32_t idx);
int64_t get_nbiot_ecellid(sky_ctx_t *ctx, uint32_t idx);
int64_t get_nbiot_tac(sky_ctx_t *ctx, uint32_t idx);
int64_t get_nbiot_lac(sky_ctx_t *ctx, uint32_t idx);
int64_t get_nbiot_rssi(sky_ctx_t *ctx, uint32_t idx);

#endif
