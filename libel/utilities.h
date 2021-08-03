/*! \file libel/utilities.h
 *  \brief Skyhook Embedded Library request context structures
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
#ifndef SKY_UTILITIES_H
#define SKY_UTILITIES_H

#if SKY_LOGGING
#define DUMP_REQUEST_CTX(...) dump_request_ctx(__VA_ARGS__, __FILE__, __FUNCTION__)
#define DUMP_CACHE(...) dump_cache(__VA_ARGS__, __FILE__, __FUNCTION__)
#define LOGFMT(...) logfmt(__FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_BUFFER(c, l, b, s) log_buffer(__FILE__, __FUNCTION__, c, l, b, s)
#else
#define DUMP_REQUEST_CTX(...)                                                                      \
    do {                                                                                           \
    } while (0)
#define DUMP_CACHE(...)                                                                            \
    do {                                                                                           \
    } while (0)
#define LOGFMT(...)                                                                                \
    do {                                                                                           \
    } while (0)
#define LOG_BUFFER(c, l, b, s)
#endif

Sky_status_t set_error_status(Sky_errno_t *sky_errno, Sky_errno_t code);
bool validate_beacon(Beacon_t *b, Sky_rctx_t *rctx);
bool validate_request_ctx(Sky_rctx_t *rctx);
bool validate_session_ctx(Sky_sctx_t *sctx, Sky_loggerfn_t logf);
bool validate_mac(const uint8_t mac[6], Sky_rctx_t *rctx);
bool is_tbr_enabled(Sky_rctx_t *rctx);
#if SKY_LOGGING
const char *sky_basename(const char *path);
int logfmt(
    const char *file, const char *function, Sky_rctx_t *ctx, Sky_log_level_t level, char *fmt, ...);
int log_buffer(const char *file, const char *function, Sky_rctx_t *ctx, Sky_log_level_t level,
    void *buffer, uint32_t bufsize);
#endif
void dump_beacon(Sky_rctx_t *rctx, char *str, Beacon_t *b, const char *file, const char *func);
void dump_vap(Sky_rctx_t *rctx, char *prefix, Beacon_t *b, const char *file, const char *func);
void dump_ap(Sky_rctx_t *rctx, char *str, Beacon_t *b, const char *file, const char *func);
void dump_request_ctx(Sky_rctx_t *rctx, const char *file, const char *func);
void dump_cache(Sky_rctx_t *rctx, const char *file, const char *func);
int dump_hex16(const char *file, const char *function, Sky_rctx_t *rctx, Sky_log_level_t level,
    void *buffer, uint32_t bufsize, uint32_t buf_offset);
void config_defaults(Sky_sctx_t *sctx);
int32_t get_num_beacons(Sky_rctx_t *rctx, Sky_beacon_type_t t);
int32_t get_num_cells(Sky_rctx_t *rctx);
int get_base_beacons(Sky_rctx_t *rctx, Sky_beacon_type_t t);

uint32_t get_ctx_partner_id(Sky_rctx_t *rctx);
uint8_t *get_ctx_aes_key(Sky_rctx_t *rctx);
uint8_t *get_ctx_device_id(Sky_rctx_t *rctx);
uint32_t get_ctx_id_length(Sky_rctx_t *rctx);
uint8_t *get_ctx_ul_app_data(Sky_rctx_t *rctx);
uint32_t get_ctx_ul_app_data_length(Sky_rctx_t *rctx);
uint32_t get_ctx_token_id(Sky_rctx_t *rctx);
char *get_ctx_sku(Sky_rctx_t *rctx);
uint32_t get_ctx_sku_length(Sky_rctx_t *ctx);
uint32_t get_ctx_cc(Sky_rctx_t *rctx);

int32_t get_num_aps(Sky_rctx_t *rctx);
uint8_t *get_ap_mac(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_ap_freq(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_ap_rssi(Sky_rctx_t *rctx, uint32_t idx);
bool get_ap_is_connected(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_ap_age(Sky_rctx_t *rctx, uint32_t idx);

int32_t get_num_gnss(Sky_rctx_t *rctx);
float get_gnss_lat(Sky_rctx_t *rctx, uint32_t idx);
float get_gnss_lon(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_gnss_hpe(Sky_rctx_t *rctx, uint32_t idx);
float get_gnss_alt(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_gnss_vpe(Sky_rctx_t *rctx, uint32_t idx);
float get_gnss_speed(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_gnss_bearing(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_gnss_nsat(Sky_rctx_t *rctx, uint32_t idx);
int64_t get_gnss_age(Sky_rctx_t *rctx, uint32_t idx);

Beacon_t *get_cell(Sky_rctx_t *rctx, uint32_t idx);
int16_t get_cell_type(Beacon_t *cell);
int64_t get_cell_id1(Beacon_t *cell);
int64_t get_cell_id2(Beacon_t *cell);
int64_t get_cell_id3(Beacon_t *cell);
int64_t get_cell_id4(Beacon_t *cell);
int64_t get_cell_id5(Beacon_t *cell);
int64_t get_cell_id6(Beacon_t *cell);
int64_t get_cell_ta(Beacon_t *cell);
bool get_cell_connected_flag(Sky_rctx_t *rctx, Beacon_t *cell);
int64_t get_cell_rssi(Beacon_t *cell);
int64_t get_cell_age(Beacon_t *cell);

int32_t get_num_vaps(Sky_rctx_t *rctx);
uint8_t *get_vap_data(Sky_rctx_t *rctx, uint32_t idx);
uint8_t *select_vap(Sky_rctx_t *rctx);

int sky_rand_fn(uint8_t *rand_buf, uint32_t bufsize);
#endif
