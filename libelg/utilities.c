/*! \file libelg/utilities.c
 *  \brief utilities - Skyhook ELG API Version 3.0 (IoT)
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
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#define SKY_LIBELG 1
#include "beacons.h"
#include "config.h"
#include "response.h"
#include "workspace.h"
#include "crc32.h"
#include "libelg.h"

/*! \brief set sky_errno and return Sky_status
 *
 *  @param sky_errno sky_errno is the error code
 *  @param code the sky_errno_t code to return
 *
 *  @return Sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
Sky_status_t sky_return(Sky_errno_t *sky_errno, Sky_errno_t code)
{
	if (sky_errno != NULL)
		*sky_errno = code;
	return (code == SKY_ERROR_NONE) ? SKY_SUCCESS : SKY_ERROR;
}

/*! \brief validate the workspace buffer
 *
 *  @param ctx workspace buffer
 *
 *  @return true if workspace is valid, else false
 */
int validate_workspace(Sky_ctx_t *ctx)
{
	int i;

	if (ctx != NULL &&
	    ctx->header.crc32 == sky_crc32(&ctx->header.magic,
					   sizeof(ctx->header) -
						   sizeof(ctx->header.crc32))) {
		for (i = 0; i < TOTAL_BEACONS; i++) {
			if (ctx->beacon[i].h.magic != BEACON_MAGIC ||
			    ctx->beacon[i].h.type >= SKY_BEACON_MAX)
				return false;
		}
	}
	if (ctx == NULL || ctx->len > TOTAL_BEACONS ||
	    ctx->connected > TOTAL_BEACONS)
		return false;
	return true;
}

/*! \brief formatted logging to user provided function
 *
 *  @param ctx workspace buffer
 *  @param level the log level of this msg
 *  @param fmt the msg
 *  @param ... variable arguments
 *
 *  @return 0 for success
 */
int logfmt(Sky_ctx_t *ctx, Sky_log_level_t level, const char *fmt, ...)
{
#if SKY_DEBUG
	va_list ap;
	char buf[96];
	int ret;
	if (level > ctx->min_level)
		return -1;
	va_start(ap, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	(*ctx->logf)(level, buf, sizeof(buf));
	va_end(ap);
	return ret;
#else
	return 0;
#endif
}

/*! \brief field extraction for dynamic use of Nanopb (count beacons)
 *
 *  @param ctx workspace buffer
 *  @param t type of beacon to count
 *
 *  @return number of beacons of the specified type
 */
int64_t get_num_beacons(Sky_ctx_t *ctx, Sky_beacon_type_t t)
{
	int i, b = 0;

	if (ctx == NULL || t > SKY_BEACON_MAX) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	if (t == SKY_BEACON_AP) {
		return ctx->ap_len;
	} else {
		for (i = ctx->ap_len, b = 0; i < ctx->len; i++) {
			if (ctx->beacon[i].h.type == t)
				b++;
			if (b && ctx->beacon[i].h.type != t)
				break; /* End of beacons of this type */
		}
	}
	return b;
}

/*! \brief field extraction for dynamic use of Nanopb (base of beacon type)
 *
 *  @param ctx workspace buffer
 *  @param t type of beacon to find
 *
 *  @return first beacon of the specified type
 */
int get_base_beacons(Sky_ctx_t *ctx, Sky_beacon_type_t t)
{
	int i = 0;

	if (ctx == NULL || t > SKY_BEACON_MAX) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	if (t == SKY_BEACON_AP) {
		if (ctx->beacon[ctx->ap_low].h.type == t)
			return i;
	} else {
		for (i = ctx->ap_len; i < ctx->len; i++) {
			if (ctx->beacon[i].h.type == t)
				return i;
		}
	}
	return -1;
}

/*! \brief field extraction for dynamic use of Nanopb (num AP)
 *
 *  @param ctx workspace buffer
 *
 *  @return 0 for success
 */
int64_t get_num_aps(Sky_ctx_t *ctx)
{
	if (ctx == NULL) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->ap_len;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/MAC)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
uint8_t *get_ap_mac(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > ctx->ap_len) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[ctx->ap_low + idx].ap.mac;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/channel)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_ap_channel(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > ctx->ap_len) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[ctx->ap_low + idx].ap.channel;
}

/*! \brief field extraction for dynamic use of Nanopb (AP/rssi)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_ap_rssi(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > ctx->ap_len) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[ctx->ap_low + idx].ap.rssi;
}

/*! \brief field extraction for dynamic use of Nanopb (num gsm)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_num_gsm(Sky_ctx_t *ctx)
{
	if (ctx == NULL) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return get_num_beacons(ctx, SKY_BEACON_GSM);
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/ui)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
uint64_t get_gsm_ui(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.ui;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/mcc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_gsm_mcc(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.mcc;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/mnc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_gsm_mnc(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.mnc;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/lac)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_gsm_lac(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.lac;
}

/*! \brief field extraction for dynamic use of Nanopb (gsm/rssi)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_gsm_rssi(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_gsm(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_GSM) + idx].gsm.rssi;
}
/*! \brief field extraction for dynamic use of Nanopb (num nbiot)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_num_nbiot(Sky_ctx_t *ctx)
{
	if (ctx == NULL) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return get_num_beacons(ctx, SKY_BEACON_NBIOT);
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/mcc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
uint64_t get_nbiot_mcc(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx]
		.nbiot.mcc;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/mnc)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_nbiot_mnc(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx]
		.nbiot.mnc;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/e_cellid)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_nbiot_ecellid(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx]
		.nbiot.e_cellid;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/tac)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_nbiot_tac(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx]
		.nbiot.tac;
}

/*! \brief field extraction for dynamic use of Nanopb (nbiot/rssi)
 *
 *  @param ctx workspace buffer
 *  @param idx index into beacons
 *
 *  @return 0 for success
 */
int64_t get_nbiot_rssi(Sky_ctx_t *ctx, uint32_t idx)
{
	if (ctx == NULL || idx > (ctx->len - get_num_nbiot(ctx))) {
		// logfmt(ctx, SKY_LOG_LEVEL_ERROR, "%s: bad param", __FUNCTION__);
		return 0;
	}
	return ctx->beacon[get_base_beacons(ctx, SKY_BEACON_NBIOT) + idx]
		.nbiot.rssi;
}
