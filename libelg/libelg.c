/*! \file libelg/libelg.c
 *  \brief sky entry points - Skyhook ELG API Version 3.0 (IoT)
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
#include <stdlib.h>
#include <time.h>
#define SKY_LIBELG 1
#include "beacons.h"
#include "config.h"
#include "response.h"
#include "crc32.h"
#include "workspace.h"
#include "libelg.h"
#include "utilities.h"

/*! \brief keep track of when the user has opened the library */
static uint32_t sky_open_flag = 0;

/*! \brief keep track of the device using the library */
static uint8_t sky_id_len;
static uint8_t sky_device_id[MAC_SIZE];

/*! \brief keep track of logging function */
static int (*sky_logf)(sky_log_level_t level, const char *s, int max);
static sky_log_level_t sky_min_level;

/*! \brief keep track of user credentials */
static uint32_t sky_partner_id;

/*! \brief keep track of user credentials */
static uint32_t sky_aes_key_id;

/*! \brief keep track of user credentials */
static uint8_t sky_aes_key[16];

/* Local functions */
static bool validate_device_id(uint8_t *device_id, uint32_t id_len);
static bool validate_partner_id(uint32_t partner_id);
static bool validate_aes_key_id(uint32_t aes_key_id);
static bool validate_aes_key(uint8_t aes_key[AES_SIZE]);

/*! \brief qsort cmp for beacon type 
 *
 *  @param a Pointer to beacon info
 *  @param b Pointer to beacon info
 *
 *  @return -1, 0 or 1 based on beacon type less, equal or greater
 */
int32_t cmp_beacon(const void *a, const void *b)
{
	// Sort on type, low-to-high.
	int8_t typeA = ((beacon_t *)a)->h.type;
	int8_t typeB = ((beacon_t *)b)->h.type;

	return typeA < typeB ? -1 : typeA > typeB;
}

/*! \brief Initialize Skyhook library and verify access to resources
 *
 *  @param sky_errno if sky_open returns failure, sky_errno is set to the error code
 *  @param device_id Device unique ID (example mac address of the device)
 *  @param id_len length if the Device ID, typically 6, Max 16 bytes
 *  @param partner_id Skyhook assigned credentials
 *  @param aes_key_id Skyhook assigned credentials
 *  @param aes_key Skyhook assigned encryption key
 *  @param sky_state pointer to a state buffer (provided by sky_close) or NULL
 *  @param min_level logging function is called for msg with equal or greater level
 *  @param logf pointer to logging function
 *
 *  @return sky_status_t SKY_SUCCESS or SKY_ERROR
 *
 *  sky_open can be called many times with the same parameters. This does
 *  nothing and returns SKY_SUCCESS. However, sky_close must be called
 *  in order to change the parameter values. Device ID length will
 *  be truncated to 16 if larger, without causing an error.
 */
sky_status_t
sky_open(sky_errno_t *sky_errno, uint8_t *device_id, uint32_t id_len,
	 uint32_t partner_id, uint32_t aes_key_id, uint8_t aes_key[16],
	 uint8_t *sky_state, sky_log_level_t min_level,
	 int (*logf)(sky_log_level_t level, const char *s, int len))
{
	/* Only concider up to 16 bytes. Ignore any extra */
	id_len = (id_len > MAX_DEVICE_ID) ? 16 : id_len;

	/* if open already */
	if (sky_open_flag) {
		/* parameters must be the same (no-op) or fail */
		if (memcmp(device_id, sky_device_id, id_len) == 0 &&
		    id_len == sky_id_len && partner_id == sky_partner_id &&
		    aes_key_id == sky_aes_key_id &&
		    memcmp(aes_key, sky_aes_key, sizeof(sky_aes_key)) == 0)
			return sky_return(sky_errno, SKY_ERROR_NONE);
		else
			return sky_return(sky_errno, SKY_ERROR_ALREADY_OPEN);
	}

	/* Sanity check */
	if (!validate_device_id(device_id, id_len) ||
	    !validate_partner_id(partner_id) ||
	    !validate_aes_key_id(aes_key_id) || !validate_aes_key(aes_key))
		return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);

	sky_id_len = id_len;
	sky_min_level = min_level;
	sky_logf = logf;
	memcpy(sky_device_id, device_id, id_len);
	sky_partner_id = partner_id;
	sky_aes_key_id = aes_key_id;
	memcpy(sky_aes_key, aes_key, sizeof(sky_aes_key));
	sky_open_flag = true;

	(*logf)(SKY_LOG_LEVEL_DEBUG, "Skyhook libELG Version 3.0\n", 80);

	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Determines the size of the non-volatile memory state buffer
 *
 *  @param sky_state Pointer to state buffer
 *
 *  @return Size of state buffer or 0 to indicate that the buffer was invalid
 */
int32_t sky_sizeof_state(uint8_t *sky_state)
{
	/* Cache space required
     *
     * header - Magic number, size of space, checksum
     * body - number of entries
     */
	return sizeof(struct sky_header) +
	       CACHE_SIZE * (sizeof(beacon_t) + sizeof(gps_t));
}

/*! \brief Determines the size of the workspace required to build request
 *
 *  @param number_beacons The number of beacons user will add
 *
 *  @return Size of state buffer or 0 to indicate that the buffer was invalid
 */
int32_t sky_sizeof_workspace(uint16_t number_beacons)
{
	/* Total space required
     *
     * header - Magic number, size of space, checksum
     * body - number of beacons, beacon data, gps, request buffer
     */
	return sizeof(sky_ctx_t);
}

/*! \brief Initializes the workspace provided ready to build a request
 *
 *  @param buf Pointer to workspace provided by user
 *  @param bufsize Workspace buffer size (from sky_sizeof_workspace)
 *  @param sky_errno Pointer to error code
 *  @param number_beacons The number of beacons user will add
 *
 *  @return Pointer to the initialized workspace context buffer or NULL
 */
sky_ctx_t *sky_new_request(void *buf, int32_t bufsize, sky_errno_t *sky_errno,
			   uint8_t number_beacons)
{
	int i;

	if (!sky_open_flag) {
		sky_return(sky_errno, SKY_ERROR_NEVER_OPEN);
		return NULL;
	}
	if (bufsize != sky_sizeof_workspace(MAX_BEACONS) || buf == NULL) {
		sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);
		return NULL;
	}

	sky_ctx_t *ctx = (sky_ctx_t *)buf;

	/* update header in workspace */
	ctx->header.magic = SKY_MAGIC;
	ctx->header.size = bufsize;
	ctx->header.time = time(NULL);
	ctx->header.crc32 =
		sky_crc32(&ctx->header.magic,
			  sizeof(ctx->header) - sizeof(ctx->header.crc32));

	ctx->logf = sky_logf;
	ctx->min_level = sky_min_level;
	ctx->expect = number_beacons;
	ctx->len = 0; /* empty */
	ctx->ap_len = 0; /* empty */
	for (i = 0; i < MAX_BEACONS; i++)
		ctx->beacon[i].h.magic = BEACON_MAGIC;
	ctx->connected = -1; /* all unconnected */
	return ctx;
}

/*! \brief  Adds the wifi ap information to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param mac pointer to mac address of the Wi-Fi beacon
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rssi Received Signal Strength Intensity, -10 through -127, -1 if unknown
 *  @param channel Frequency Channel , -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_ap_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
			       uint8_t mac[6], time_t timestamp, int16_t rssi,
			       int32_t channel, bool is_connected)
{
	beacon_t b;

	if (!sky_open_flag)
		return sky_return(sky_errno, SKY_ERROR_NEVER_OPEN);

	if (!validate_workspace(ctx))
		return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);

	if (ctx->expect == 0)
		logfmt(ctx, SKY_LOG_LEVEL_WARNING,
		       "adding more beacons than expected");

	/* Decrement the number of beacons expected to be added */
	ctx->expect--;

	/* Create AP beacon */
	b.h.magic = BEACON_MAGIC;
	b.h.type = SKY_BEACON_AP;
	memcpy(b.ap.mac, mac, MAC_SIZE);
	/* If beacon has meaningful timestamp */
	/* scan was before sky_new_request and since Mar 1st 2019 */
	if (ctx->header.time > timestamp && ctx->header.time > 1551398400)
		b.ap.age = ctx->header.time - timestamp;
	else
		b.ap.age = 0;
	b.ap.channel = channel;
	b.ap.rssi = rssi;
	b.ap.flag = 0; /* TODO map channel? */

	return add_beacon(ctx, sky_errno, &b, is_connected);
}

/*! \brief Add an lte cell beacon to request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param tac lte tracking area code identifier (1-65,535),0 if unknown
 *  @param e_cellid lte beacon identifier 28bit (0-268,435,456)
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code (0-999)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rsrp Received Signal Receive Power, range -140 to -40dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_lte_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
				     uint16_t tac, uint32_t e_cellid,
				     uint16_t mcc, uint16_t mnc,
				     time_t timestamp, int16_t rsrp,
				     bool is_connected)
{
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Adds a gsm cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lac gsm location area code identifier (1-65,535)
 *  @param ui gsm cell identifier (0-65,535)
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code  (0-999)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rssi Received Signal Strength Intensity, range -128 to -32dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_gsm_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
				     uint16_t lac, uint32_t ui, uint16_t mcc,
				     uint16_t mnc, time_t timestamp,
				     int16_t rssi, bool is_connected)
{
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Adds a umts cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lac umts location area code identifier (1-65,535), 0 if unknown
 *  @param ucid umts cell identifier 28bit (0-268,435,456)
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code  (0-999)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rscp Received Signal Code Power, range -120dbm to -20dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_umts_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
				      uint16_t lac, uint32_t ucid, uint16_t mcc,
				      uint16_t mnc, time_t timestamp,
				      int16_t rscp, bool is_connected)
{
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Adds a cdma cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param sid cdma system identifier (0-32767)
 *  @param nid cdma network identifier(0-65535)
 *  @param bsid cdma base station identifier (0-65535)
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param rssi Received Signal Strength Intensity
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_cdma_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
				      uint32_t sid, uint16_t nid, uint16_t bsid,
				      time_t timestamp, int16_t rssi,
				      bool is_connected)
{
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Adds a nb_iot cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param mcc mobile country code (200-799)
 *  @param mnc mobile network code  (0-999)
 *  @param e_cellid nbiot beacon identifier (0-268,435,456)
 *  @param tac nbiot tracking area code identifier (1-65,535), 0 if unknown
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *  @param nrsrp Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 *  @param is_connected this beacon is currently connected, false if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_nb_iot_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
					uint16_t mcc, uint16_t mnc,
					uint32_t e_cellid, uint32_t tac,
					time_t timestamp, int16_t nrsrp,
					bool is_connected)
{
	int i;

	if (!sky_open_flag)
		return sky_return(sky_errno, SKY_ERROR_NEVER_OPEN);

	if (!validate_workspace(ctx))
		return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);

	if (ctx->len > (MAX_BEACONS - 1)) /* room for one more? */
		return sky_return(sky_errno, SKY_ERROR_TOO_MANY);

	/* Create NB IoT beacon */
	i = (++ctx->len) - 1;
	ctx->beacon[i].h.type = SKY_BEACON_NBIOT;
	/* If beacon has meaningful timestamp */
	/* scan was before sky_new_request and since Mar 1st 2019 */
	if (ctx->header.time > timestamp && ctx->header.time > 1551398400)
		ctx->beacon[i].nb_iot.age = ctx->header.time - timestamp;
	else
		ctx->beacon[i].nb_iot.age = 0;
	ctx->beacon[i].nb_iot.mcc = mcc;
	ctx->beacon[i].nb_iot.mnc = mnc;
	ctx->beacon[i].nb_iot.e_cellid = e_cellid;
	ctx->beacon[i].nb_iot.tac = tac;
	ctx->beacon[i].nb_iot.nrsrp = nrsrp;
	if (is_connected)
		ctx->connected = ctx->len;
	/* sort beacons by type */
	qsort(ctx->beacon, ctx->len, sizeof(beacon_t), cmp_beacon);
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief Adds the position of the device from gps to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lat device latitude
 *  @param lon device longitude
 *  @param hpe pointer to horizontal Positioning Error in meters with 68% confidence, 0 if unknown
 *  @param altitude pointer to altitude above mean sea level, in meters, NaN if unknown
 *  @param vpe pointer to vertical Positioning Error in meters with 68% confidence, 0 if unknown
 *  @param speed pointer to speed in meters per second, Nan if unknown
 *  @param bearing pointer to bearing of device in degrees, counterclockwise from north
 *  @param timestamp time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_gps(sky_ctx_t *ctx, sky_errno_t *sky_errno, float lat,
			 float lon, uint16_t hpe, float altitude, uint16_t vpe,
			 float speed, float bearing, time_t timestamp)
{
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief generate a Skyhook request from the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param request Request to send to Skyhook server
 *  @param bufsize Request size in bytes
 *  @param lat where to save device latitude from cache if known
 *  @param lon where to save device longitude from cache if known
 *  @param hpe where to save horizontal Positioning Error from cache if known
 *  @param timestamp time in seconds (from 1970 epoch) indicating when fix was computed (from cache)
 *  @param response_size the space required to hold the server response
 *
 *  @return SKY_FINALIZE_REQUEST, SKY_FINALIZE_LOCATION or
 *          SKY_FINALIZE_ERROR and sets sky_errno with error code
 */
sky_finalize_t sky_finalize_request(sky_ctx_t *ctx, sky_errno_t *sky_errno,
				    uint8_t **request, uint32_t *bufsize,
				    float *lat, float *lon, uint16_t *hpe,
				    time_t *timestamp, uint32_t *response_size)
{
	if (!validate_workspace(ctx))
		return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);

	if (ctx->len == 0)
		return sky_return(sky_errno, SKY_ERROR_BAD_WORKSPACE);

	/* TODO encode request */
	strcpy((void *)ctx->request, "SKYHOOK REQUEST MSG");

	*request = ctx->request;
	*bufsize = strlen((void *)ctx->request);
	*response_size = sizeof(ctx->request);

	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief decodes a Skyhook server response
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param response buffer holding the skyhook server response
 *  @param bufsize Request size in bytes
 *  @param lat where to save device latitude from cache if known
 *  @param lon where to save device longitude from cache if known
 *  @param hpe where to save horizontal Positioning Error from cache if known
 *  @param timestamp pointer to time in seconds (from 1970 epoch) indicating when fix was computed (on server)
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_decode_response(sky_ctx_t *ctx, sky_errno_t *sky_errno,
				 char *response, int32_t bufsize, float *lat,
				 float *lon, uint16_t *hpe, time_t *timestamp)
{
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*! \brief returns a string which describes the meaning of sky_errno codes
 *
 *  @param sky_errno Error code for which to provide descriptive string
 *
 *  @return pointer to string or NULL if the code is invalid
 */
char *sky_perror(sky_errno_t sky_errno)
{
	register char *str = NULL;
	switch (sky_errno) {
	case SKY_ERROR_NONE:
		str = "No error";
		break;
	case SKY_ERROR_NEVER_OPEN:
		str = "Must open first";
		break;
	case SKY_ERROR_ALREADY_OPEN:
		str = "Must close before opening with new parameters";
		break;
	case SKY_ERROR_BAD_PARAMETERS:
		str = "Validation of parameters failed";
		break;
	case SKY_ERROR_TOO_MANY:
		str = "Too many beacons";
		break;
	case SKY_ERROR_BAD_WORKSPACE:
		str = "The workspace buffer is corrupt";
		break;
	case SKY_ERROR_BAD_STATE:
		str = "The state buffer is corrupt";
		break;
	case SKY_ERROR_DECODE_ERROR:
		str = "The response could not be decoded";
		break;
	case SKY_ERROR_RESOURCE_UNAVAILABLE:
		str = "Can\'t allocate non-volatile storage";
		break;
	case SKY_ERROR_CLOSE:
		str = "Failed to cleanup resources during close";
		break;
	case SKY_ERROR_BAD_KEY:
		str = "AES_Key is not valid format";
		break;
	case SKY_ERROR_NO_BEACONS:
		str = "At least one beacon must be added";
		break;
	}
	return str;
}

/*! \brief clean up library resourses
 *
 *  @param sky_errno skyErrno is set to the error code
 *  @param sky_state pointer to where the state buffer reference should be
 * stored
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_close(sky_errno_t *sky_errno, uint8_t **sky_state)
{
	if (!sky_open_flag)
		return sky_return(sky_errno, SKY_ERROR_NEVER_OPEN);

	sky_open_flag = false;

	if (sky_state != NULL)
		*sky_state = NULL;
	return sky_return(sky_errno, SKY_ERROR_NONE);
}

/*******************************************************************************
 * Static helper functions
 ******************************************************************************/

/*! \brief sanity check the device_id
 *
 *  @param device_id this is expected to be a binary mac address
 *
 *  @return true or false
 */
static bool validate_device_id(uint8_t *device_id, uint32_t id_len)
{
	if (device_id == NULL)
		return false;
	else
		return true; /* TODO check upper bound? */
}

/*! \brief sanity check the partner_id
 *
 *  @param partner_id this is expected to be in the range (1 - ???)
 *
 *  @return true or false
 */
static bool validate_partner_id(uint32_t partner_id)
{
	if (partner_id == 0)
		return false;
	else
		return true; /* TODO check upper bound? */
}

/*! \brief sanity check the aes_key_id
 *
 *  @param aes_key_id this is expected to be in the range (1 - ???)
 *
 *  @return true or false
 */
static bool validate_aes_key_id(uint32_t aes_key_id)
{
	if (aes_key_id == 0)
		return false;
	else
		return true; /* TODO check upper bound? */
}

/*! \brief sanity check the aes_key
 *
 *  @param aes_key this is expected to be a binary 16 byte value
 *
 *  @return true or false
 */
static bool validate_aes_key(uint8_t aes_key[AES_SIZE])
{
	if (aes_key == NULL)
		return false;
	else
		return true; /* TODO check for non-trivial values? e.g. zero */
}
