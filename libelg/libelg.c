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
#include "libelg.h"
#include <string.h>

/*! \brief keep track of when the user has opened the library */
static uint32_t sky_open_flag = 0;

/*! \brief keep track of the device using the library */
static uint8_t sky_device_id[MAC_SIZE];

/*! \brief keep track of user credentials */
static uint32_t sky_partner_id;

/*! \brief keep track of user credentials */
static uint32_t sky_aes_key_id;

/*! \brief keep track of user credentials */
static uint8_t sky_aes_key[16];

/*! \brief Initialize Skyhook library and verify access to resources
 *
 *  @param sky_errno sky_errno is set to the error code
 *  @param device_id Device unique ID (example mac address of the device)
 *  @param partner_id Skyhook assigned credentials
 *  @param aes_key_id Skyhook assigned credentials
 *  @param aes_key Skyhook assigned encryption key
 *  @param sky_state pointer to a state buffer (provided by sky_close)
 *  @param puts pointer to logging function
 *
 *  @return sky_status_t SKY_SUCCESS or SKY_ERROR
 */
sky_status_t sky_open(sky_errno_t *sky_errno, uint8_t device_id[MAC_SIZE],
                      uint32_t partner_id, uint32_t aes_key_id,
                      uint8_t aes_key[16], uint8_t *sky_state,
                      int (*puts)(const char *s))
{
    /* if open already */
    if (sky_open_flag) {
        /* parameters must be the same (no-op) or fail */
        if (memcmp(device_id, sky_device_id, sizeof(sky_device_id)) == 0 &&
            partner_id == sky_partner_id && aes_key_id == sky_aes_key_id &&
            memcmp(aes_key, sky_aes_key, sizeof(sky_aes_key)) == 0)
            return sky_return(sky_errno, SKY_ERROR_NONE);
        else
            return sky_return(sky_errno, SKY_ERROR_ALREADY_OPEN);
    }
    /* Sanity check */
    if (!validate_device_id(device_id) || !validate_partner_id(partner_id) ||
        !validate_aes_key_id(aes_key_id) || !validate_aes_key(aes_key))
        return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);

    memcpy(sky_device_id, device_id, sizeof(device_id));
    sky_partner_id = partner_id;
    sky_aes_key_id = aes_key_id;
    memcpy(sky_aes_key, aes_key, sizeof(sky_aes_key));
    return sky_return(sky_errno, SKY_SUCCESS);
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

/*! \brief Determines the size of the non-volatile memory state buffer
 *
 *  @param sky_state Pointer to state buffer
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
    return sizeof(struct sky_header) + MAX_BEACONS * sizeof(beacon_t) +
           sizeof(gps_t);
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
    sky_ctx_t *ctx = (sky_ctx_t *)buf;

    /* TODO we could check that the bufsize >= sky_sizeof_workspace() */
    /* update header in workspace */
    ctx->header.magic = SKY_MAGIC;
    ctx->header.size = bufsize;
    ctx->header.crc32 =
        sky_crc32(&ctx->header.magic,
                  sizeof(ctx->header.magic) + sizeof(ctx->header.size));
    return ctx;
}

/*! \brief  Adds the wifi ap information to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param mac pointer to mac address of the Wi-Fi beacon
 *  @param timestamp when the scan was performed
 *  @param rssi Received Signal Strength Intensity
 *  @param channel Frequency Channel , -1 if unknown
 *  @param is_connected This beacon is currently connected
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_ap_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                               uint8_t mac[6], time_t timestamp, int8_t rssi,
                               int32_t channel, bool is_connected)
{
    return SKY_SUCCESS;
}

/*! \brief Add an lte cell beacon to request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param tac lte beacon identifier
 *  @param eucid lte beacon identifier
 *  @param mcc lte beacon identifier
 *  @param mnc lte beacon identifier
 *  @param timestamp when the scan was performed
 *  @param rsrp Received Signal Receive Power
 *  @param is_connected This beacon is currently connected
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_lte_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                                     uint16_t tac, uint32_t eucid, uint16_t mcc,
                                     uint16_t mnc, time_t timestamp,
                                     int8_t rsrp, bool is_connected)
{
    return SKY_SUCCESS;
}

/*! \brief Adds a gsm cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lac gsm beacon identifier
 *  @param ui gsm beacon identifier
 *  @param mcc gsm beacon identifier
 *  @param mnc gsm beacon identifier
 *  @param timestamp when the scan was performed
 *  @param rssi Received Signal Strength Intensity
 *  @param is_connected This beacon is currently connected
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_gsm_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                                     uint16_t lac, uint32_t ui, uint16_t mcc,
                                     uint16_t mnc, time_t timestamp,
                                     int8_t rssi, bool is_connected)
{
    return SKY_SUCCESS;
}

/*! \brief Adds a umts cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lac utms beacon identifier
 *  @param ui utms beacon identifier
 *  @param mcc utms beacon identifier
 *  @param mnc utms beacon identifier
 *  @param timestamp when the scan was performed
 *  @param rscp Received Signal Code Power
 *  @param is_connected This beacon is currently connected
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_umts_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                                      uint16_t lac, uint32_t ui, uint16_t mcc,
                                      uint16_t mnc, time_t timestamp,
                                      int8_t rscp, bool is_connected)
{
    return SKY_SUCCESS;
}

/*! \brief Adds a cdma cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param sid cdma beacon identifier
 *  @param nid cdma beacon identifier
 *  @param bsid cdma beacon identifier
 *  @param lat tower latitude
 *  @param lon tower longitude
 *  @param timestamp when the scan was performed
 *  @param rssi Received Signal Strength Intensity
 *  @param is_connected This beacon is currently connected
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_cdma_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                                      uint32_t sid, uint16_t nid, uint16_t bsid,
                                      float lat, float lon, time_t timestamp,
                                      int8_t rssi, bool is_connected)
{
    return SKY_SUCCESS;
}

/*! \brief Adds a nb_iot cell beacon to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param mcc nbiot beacon identifier
 *  @param mnc nbiot beacon identifier
 *  @param cellid nbiot beacon identifier
 *  @param tac nbiot beacon identifier
 *  @param ncellid nbiot beacon identifier
 *  @param timestamp when the scan was performed
 *  @param nrsrp Narrowband Reference Signal Received Power
 *  @param is_connected This beacon is currently connected
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_cell_nb_iot_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                                        uint16_t mcc, uint16_t mnc,
                                        uint32_t cellid, uint32_t tac,
                                        uint32_t ncellid, time_t timestamp,
                                        int8_t nrsrp, bool is_connected)
{
    return SKY_SUCCESS;
}

/*! \brief Adds the position of the device from gps to the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param lat device latitude
 *  @param lon device longitude
 *  @param hpe pointer to horizontal Positioning Error
 *  @param altitude pointer to altitude
 *  @param vpe pointer to vertical Positioning Error
 *  @param speed pointer to speed in meters per second
 *  @param bearing pointer to bearing of device in degrees, counterclockwise
 * from north
 *  @param timestamp when the location was determined
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_add_gps(sky_ctx_t *ctx, sky_errno_t *sky_errno, float lat,
                         float lon, uint16_t hpe, float altitude, uint16_t vpe,
                         float speed, float bearing, time_t timestamp)
{
    return SKY_SUCCESS;
}

/*! \brief generate a Skyhook request from the request context
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param request Request to send to Skyhook server
 *  @param bufsize Request size in bytes
 *  @param lat where to save device latitude
 *  @param lon where to save device longitude
 *  @param hpe where to save horizontal Positioning Error
 *  @param timestamp when the location was determined
 *  @param response_size the space required to hold the server response
 *
 *  @return SKY_FINALIZE_REQUEST, SKY_FINALIZE_LOCATION or
 *          SKY_FINALIZE_ERROR and sets sky_errno with error code
 */
sky_finalize_t sky_finalize_request(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                                    char **request, int32_t *bufsize,
                                    float *lat, float *lon, uint16_t *hpe,
                                    time_t timestamp, uint32_t *response_size)
{
    return SKY_SUCCESS;
}

/*! \brief decodes a Skyhook server response
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno skyErrno is set to the error code
 *  @param response buffer holding the skyhook server response
 *  @param bufsize Request size in bytes
 *  @param lat where to save device latitude
 *  @param lon where to save device longitude
 *  @param hpe where to save horizontal Positioning Error
 *  @param timestamp when the location was determined
 *
 *  @return SKY_SUCCESS or SKY_ERROR and sets sky_errno with error code
 */
sky_status_t sky_decode_response(sky_ctx_t *ctx, sky_errno_t *sky_errno,
                                 char *response, int32_t bufsize, float *lat,
                                 float *lon, uint16_t *hpe, time_t *timestamp)
{
    return SKY_SUCCESS;
}

/*! \brief returns a string which describes the meaning of sky_errno codes
 *
 *  @param sky_errno skyErrno is set to the error code
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
    return SKY_SUCCESS;
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
static bool validate_device_id(uint8_t device_id[MAC_SIZE])
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
