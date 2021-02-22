/*
 * Copyright (c) 2019 Skyhook, Inc.
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
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "el.pb.h"
#define SKY_LIBEL
#include "proto.h"
#include "aes.h"
#include "limits.h"
#include "assert.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* extract the n-th bit from used AP array of l bytes */
#define GET_USED_AP(u, l, n) ((((u)[l - 1 - (((n) / CHAR_BIT))]) & (0x01 << ((n) % CHAR_BIT))) != 0)

static bool apply_config_overrides(Sky_cache_t *c, Rs *rs);
static int64_t get_gnss_lat_scaled(Sky_ctx_t *ctx, uint32_t idx);
static int64_t get_gnss_lon_scaled(Sky_ctx_t *ctx, uint32_t idx);
static int64_t get_gnss_alt_scaled(Sky_ctx_t *ctx, uint32_t idx);
static int64_t get_gnss_speed_scaled(Sky_ctx_t *ctx, uint32_t idx);

typedef uint8_t *(*DataGetterb)(Sky_ctx_t *, uint32_t);
typedef int64_t (*DataGetter)(Sky_ctx_t *, uint32_t);
typedef int64_t (*DataWrapper)(int64_t);
typedef bool (*EncodeSubmsgCallback)(Sky_ctx_t *, pb_ostream_t *);

static int64_t mac_to_int(Sky_ctx_t *ctx, uint32_t idx)
{
    size_t i;

    // This is a wrapper function around get_ap_mac(). It converts the 8-byte
    // mac array to an uint64_t.
    //
    uint8_t *mac = get_ap_mac(ctx, idx);

    uint64_t ret_val = 0;

    for (i = 0; i < 6; i++)
        ret_val = ret_val * 256 + mac[i];

    return ret_val;
}

static int64_t flip_sign(int64_t value)
{
    return -value;
}

/*! \brief Get cell type
 *
 *  @param cell Pointer to beacon (cell)
 *
 *  @return cell type
 */
static int16_t map_cell_type(Beacon_t *cell)
{
    uint16_t map[] = { /* force tidy code formatting */
        [SKY_BEACON_NR] = Cell_Type_NR,
        [SKY_BEACON_LTE] = Cell_Type_LTE,
        [SKY_BEACON_UMTS] = Cell_Type_UMTS,
        [SKY_BEACON_NBIOT] = Cell_Type_NBIOT,
        [SKY_BEACON_CDMA] = Cell_Type_CDMA,
        [SKY_BEACON_GSM] = Cell_Type_GSM,
        [SKY_BEACON_MAX] = Cell_Type_UNKNOWN
    };
    if (!is_cell_type(cell))
        return Cell_Type_UNKNOWN;
    else
        return map[cell->h.type];
}

static bool encode_repeated_int_field(Sky_ctx_t *ctx, pb_ostream_t *ostream, uint32_t tag,
    uint32_t num_elems, DataGetter getter, DataWrapper wrapper)
{
    size_t i;

    // Get and encode the field size.
    pb_ostream_t substream = PB_OSTREAM_SIZING;

    // Encode field tag.
    if (!pb_encode_tag(ostream, PB_WT_STRING, tag))
        return false;

    for (i = 0; i < num_elems; i++) {
        int64_t data = getter(ctx, i);

        if (wrapper != NULL)
            data = wrapper(data);

        if (!pb_encode_varint(&substream, data))
            return false;
    }

    if (!pb_encode_varint(ostream, substream.bytes_written))
        return false;

    // Now encode the field for real.
    for (i = 0; i < num_elems; i++) {
        int64_t data = getter(ctx, i);

        if (wrapper != NULL)
            data = wrapper(data);

        if (!pb_encode_varint(ostream, data))
            return false;
    }

    return true;
}

static bool encode_vap_data(
    Sky_ctx_t *ctx, pb_ostream_t *ostream, uint32_t tag, uint32_t num_elems, DataGetterb getter)
{
    size_t i;

    // Get and encode the field size.
    pb_ostream_t substream = PB_OSTREAM_SIZING;

    // Encode field tag.
    if (!pb_encode_tag(ostream, PB_WT_STRING, tag))
        return false;

    for (i = 0; i < num_elems; i++) {
        uint8_t *data = getter(ctx, i);

        /* *data == len, data + 1 == first byte of data */
        if (!pb_encode_string(&substream, data + 1, *data))
            return false;
    }

    if (!pb_encode_varint(ostream, substream.bytes_written))
        return false;

    // Now encode the field for real.
    for (i = 0; i < num_elems; i++) {
        uint8_t *data = getter(ctx, i);

        /* *data == len, data + 1 == first byte of data */
        if (!pb_encode_string(ostream, data + 1, *data))
            return false;
    }

    return true;
}

static bool encode_connected_field(Sky_ctx_t *ctx, pb_ostream_t *ostream, uint32_t num_beacons,
    uint32_t tag, bool (*callback)(Sky_ctx_t *, uint32_t idx))
{
    bool retval = true;
    size_t i;

    for (i = 0; i < num_beacons; i++) {
        if (callback(ctx, i)) {
            retval = pb_encode_tag(ostream, PB_WT_VARINT, tag) && pb_encode_varint(ostream, i + 1);

            break;
        }
    }

    return retval;
}

static bool encode_optimized_repeated_field(Sky_ctx_t *ctx, pb_ostream_t *ostream,
    uint32_t num_beacons, uint32_t tag1, uint32_t tag2, DataGetter getter)
{
    // Encode fields. Optimization: send only a single common value if
    // all ages are the same.
    int64_t value = getter(ctx, 0);
    bool value_all_same = true;
    size_t i;

    for (i = 1; value_all_same && i < num_beacons; i++) {
        if (getter(ctx, i) != value)
            value_all_same = false;
    }

    if (num_beacons > 1 && value_all_same) {
        return pb_encode_tag(ostream, PB_WT_VARINT, tag1) && pb_encode_varint(ostream, value + 1);
    } else {
        return encode_repeated_int_field(ctx, ostream, tag2, num_beacons, getter, NULL);
    }
}

static bool encode_ap_fields(Sky_ctx_t *ctx, pb_ostream_t *ostream)
{
    uint32_t num_beacons = get_num_aps(ctx);

    return encode_connected_field(
               ctx, ostream, num_beacons, Aps_connected_idx_plus_1_tag, get_ap_is_connected) &&
           encode_repeated_int_field(ctx, ostream, Aps_mac_tag, num_beacons, mac_to_int, NULL) &&
           encode_optimized_repeated_field(ctx, ostream, num_beacons, Aps_common_freq_plus_1_tag,
               Aps_frequency_tag, get_ap_freq) &&
           encode_repeated_int_field(
               ctx, ostream, Aps_neg_rssi_tag, num_beacons, get_ap_rssi, flip_sign) &&
           encode_optimized_repeated_field(
               ctx, ostream, num_beacons, Aps_common_age_plus_1_tag, Aps_age_tag, get_ap_age);
}

static bool encode_cell_field_element(
    pb_ostream_t *ostream, uint32_t tag, int64_t val, int64_t unknown)
{
    if (val != unknown)
        // Unknown values are not sent on the wire, meaning they "show up" with
        // the default value 0 at the server.
        return pb_encode_tag(ostream, PB_WT_VARINT, tag) && pb_encode_varint(ostream, val + 1);
    else
        return true;
}

static bool encode_cell_field(Sky_ctx_t *ctx, pb_ostream_t *ostream, Beacon_t *cell)
{
    return pb_encode_tag(ostream, PB_WT_VARINT, Cell_type_tag) &&
           pb_encode_varint(ostream, map_cell_type(cell)) &&
           encode_cell_field_element(
               ostream, Cell_id1_plus_1_tag, get_cell_id1(cell), SKY_UNKNOWN_ID1) &&
           encode_cell_field_element(
               ostream, Cell_id2_plus_1_tag, get_cell_id2(cell), SKY_UNKNOWN_ID2) &&
           encode_cell_field_element(
               ostream, Cell_id3_plus_1_tag, get_cell_id3(cell), SKY_UNKNOWN_ID3) &&
           encode_cell_field_element(
               ostream, Cell_id4_plus_1_tag, get_cell_id4(cell), SKY_UNKNOWN_ID4) &&
           encode_cell_field_element(
               ostream, Cell_id5_plus_1_tag, get_cell_id5(cell), SKY_UNKNOWN_ID5) &&
           encode_cell_field_element(
               ostream, Cell_id6_plus_1_tag, get_cell_id6(cell), SKY_UNKNOWN_ID6) &&
           pb_encode_tag(ostream, PB_WT_VARINT, Cell_connected_tag) &&
           pb_encode_varint(ostream, get_cell_connected_flag(ctx, cell)) &&
           pb_encode_tag(ostream, PB_WT_VARINT, Cell_neg_rssi_tag) &&
           pb_encode_varint(ostream, -get_cell_rssi(cell)) &&
           pb_encode_tag(ostream, PB_WT_VARINT, Cell_age_tag) &&
           pb_encode_varint(ostream, get_cell_age(cell)) &&
           encode_cell_field_element(
               ostream, Cell_ta_plus_1_tag, get_cell_ta(cell), SKY_UNKNOWN_TA);
}

static bool encode_cell_fields(Sky_ctx_t *ctx, pb_ostream_t *ostream)
{
    size_t i;
    uint32_t num_cells = get_num_cells(ctx);

    // Encode the Cell submessages one by one.
    for (i = 0; i < num_cells; i++) {
        pb_ostream_t substream = PB_OSTREAM_SIZING;
        Beacon_t *cell = get_cell(ctx, i);

        // Get the field size.
        encode_cell_field(ctx, &substream, cell);

        // Encode field tag.
        pb_encode_tag(ostream, PB_WT_STRING, Rq_cells_tag);

        // Encode the field size.
        pb_encode_varint(ostream, substream.bytes_written);

        // Now encode the field for real.
        encode_cell_field(ctx, ostream, cell);
    }

    return true;
}

static bool encode_gnss_fields(Sky_ctx_t *ctx, pb_ostream_t *ostream)
{
    uint32_t num_gnss = get_num_gnss(ctx);

    return encode_repeated_int_field(
               ctx, ostream, Gnss_lat_tag, num_gnss, get_gnss_lat_scaled, NULL) &&
           encode_repeated_int_field(
               ctx, ostream, Gnss_lon_tag, num_gnss, get_gnss_lon_scaled, NULL) &&
           encode_repeated_int_field(ctx, ostream, Gnss_hpe_tag, num_gnss, get_gnss_hpe, NULL) &&
           encode_repeated_int_field(
               ctx, ostream, Gnss_alt_tag, num_gnss, get_gnss_alt_scaled, NULL) &&
           encode_repeated_int_field(ctx, ostream, Gnss_vpe_tag, num_gnss, get_gnss_vpe, NULL) &&
           encode_repeated_int_field(
               ctx, ostream, Gnss_speed_tag, num_gnss, get_gnss_speed_scaled, NULL) &&
           encode_repeated_int_field(
               ctx, ostream, Gnss_bearing_tag, num_gnss, get_gnss_bearing, NULL) &&
           encode_repeated_int_field(ctx, ostream, Gnss_nsat_tag, num_gnss, get_gnss_nsat, NULL) &&
           encode_repeated_int_field(ctx, ostream, Gnss_age_tag, num_gnss, get_gnss_age, NULL);
}

static bool encode_submessage(
    Sky_ctx_t *ctx, pb_ostream_t *ostream, uint32_t tag, EncodeSubmsgCallback func)
{
    // Get and encode the submessage size.
    pb_ostream_t substream = PB_OSTREAM_SIZING;

    // Encode the submessage tag.
    if (!pb_encode_tag(ostream, PB_WT_STRING, tag))
        return false;

    if (!func(ctx, &substream))
        return false;

    if (!pb_encode_varint(ostream, substream.bytes_written))
        return false;

    // Encode the submessage.
    if (!func(ctx, ostream))
        return false;

    return true;
}

bool Rq_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field)
{
    Sky_ctx_t *ctx = *(Sky_ctx_t **)field->pData;

    /* If we are building request which uses TBR auth,
     * and we do not currently have a token_id,
     * then we need to encode a registration request,
     * which does not include any beacon info
     */
    if (ctx && (!is_tbr_enabled(ctx) || ctx->cache->sky_token_id != TBR_TOKEN_UNKNOWN)) {
        // Per the documentation here:
        // https://jpa.kapsi.fi/nanopb/docs/reference.html#pb-encode-delimited
        //
        switch (field->tag) {
        case Rq_aps_tag:
            if (get_num_aps(ctx))
                return encode_submessage(ctx, ostream, field->tag, encode_ap_fields);
            break;
        case Rq_vaps_tag:
            return encode_vap_data(ctx, ostream, Rq_vaps_tag, get_num_vaps(ctx), get_vap_data);
            break;
        case Rq_cells_tag:
            if (get_num_cells(ctx))
                return encode_cell_fields(ctx, ostream);
            break;
        case Rq_gnss_tag:
            if (get_num_gnss(ctx))
                return encode_submessage(ctx, ostream, field->tag, encode_gnss_fields);
            break;
        default:
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "Unknown tag %d", field->tag);
            break;
        }
    }

    return true;
}

int32_t get_maximum_response_size(void)
{
    // Account for space needed for downlink app data too
    return RsHeader_size + CryptoInfo_size + 1 +
           /*
           AES_BLOCKLEN * ((Rs_size - ClientConfig_size + MAX_CLIENTCONFIG_SIZE +
                               SKY_MAX_DL_APP_DATA + AES_BLOCKLEN - 1) /
                              AES_BLOCKLEN);
                              */
           AES_BLOCKLEN * ((Rs_size + AES_BLOCKLEN - 1) / AES_BLOCKLEN);
}

int32_t serialize_request(
    Sky_ctx_t *ctx, uint8_t *buf, uint32_t buf_len, uint32_t sw_version, bool request_config)
{
    size_t rq_size, aes_padding_length, crypto_info_size, hdr_size, total_length;
    int32_t bytes_written;
    struct AES_ctx aes_ctx;

    RqHeader rq_hdr = RqHeader_init_default;
    CryptoInfo rq_crypto_info = CryptoInfo_init_default;

    Rq rq;

    pb_ostream_t ostream;

    assert(MAX_SKU_LEN >= sizeof(rq.tbr.sku));

    rq_hdr.partner_id = get_ctx_partner_id(ctx);

    // sky_new_request initializes rand_bytes if user does not
    if (ctx->rand_bytes != NULL)
        ctx->rand_bytes(rq_crypto_info.iv.bytes, AES_BLOCKLEN);

    // Initialize crypto_info
    rq_crypto_info.iv.size = AES_BLOCKLEN;

    memset(&rq, 0, sizeof(rq));

    rq.aps = rq.vaps = rq.cells = rq.gnss = ctx;

    rq.timestamp = (int64_t)ctx->header.time;

    /* if we have been given a sku, then
     * if we don't yet have a token_id,
     * then build a tbr registration request
     * else
     * make a legacy style request
     */
    if (is_tbr_enabled(ctx)) {
        if (ctx->cache->sky_token_id == TBR_TOKEN_UNKNOWN) {
            /* build a tbr registration request */
            rq.device_id.size = get_ctx_id_length(ctx);
            memcpy(rq.device_id.bytes, get_ctx_device_id(ctx), rq.device_id.size);
            strncpy(rq.tbr.sku, get_ctx_sku(ctx), MAX_SKU_LEN);
            rq.tbr.cc = get_ctx_cc(ctx);
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "TBR Registration required: Partner ID: %d, SKU '%s'",
                rq_hdr.partner_id, rq.tbr.sku);
        } else {
            /* build tbr location request */
            rq.token_id = ctx->cache->sky_token_id;
            rq.max_dl_app_data = SKY_MAX_DL_APP_DATA;
            rq.ul_app_data.size = get_ctx_ul_app_data_length(ctx);
            memcpy(rq.ul_app_data.bytes, get_ctx_ul_app_data(ctx), rq.ul_app_data.size);
#if SKY_TBR_DEVICE_ID
            rq.device_id.size = get_ctx_id_length(ctx);
            memcpy(rq.device_id.bytes, get_ctx_device_id(ctx), rq.device_id.size);
#endif
            LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "TBR location request: token %d", rq.token_id);
        }
    } else {
        /* build legacy location request */
        rq.ul_app_data.size = get_ctx_ul_app_data_length(ctx);
        memcpy(rq.ul_app_data.bytes, get_ctx_ul_app_data(ctx), rq.ul_app_data.size);
        rq.device_id.size = get_ctx_id_length(ctx);
        memcpy(rq.device_id.bytes, get_ctx_device_id(ctx), rq.device_id.size);
        LOGFMT(
            ctx, SKY_LOG_LEVEL_DEBUG, "simple location request: partner id %d", rq_hdr.partner_id);
    }

    // Create and serialize the request message.
    pb_get_encoded_size(&rq_size, Rq_fields, &rq);

    // Account for necessary encryption padding.
    aes_padding_length = (AES_BLOCKLEN - rq_size % AES_BLOCKLEN) % AES_BLOCKLEN;

    rq_size += aes_padding_length;

    rq_crypto_info.aes_padding_length = aes_padding_length;

    pb_get_encoded_size(&crypto_info_size, CryptoInfo_fields, &rq_crypto_info);

    // Initialize request header.
    rq_hdr.crypto_info_length = crypto_info_size;
    rq_hdr.rq_length = rq_size;
    rq_hdr.sw_version = sw_version;
    rq_hdr.request_client_conf = request_config ? 1 : 0;

    // First byte of message on wire is the length (in bytes) of the request
    // header.
    pb_get_encoded_size(&hdr_size, RqHeader_fields, &rq_hdr);

    total_length = 1 + hdr_size + crypto_info_size + rq_size;

    DUMP_WORKSPACE(ctx);

    // Exit if we've been called just for the purpose of determining how much
    // buffer space is necessary.
    //
    if (buf == NULL)
        return total_length;

    // Return an error indication if the supplied buffer is too small.
    if (total_length > buf_len) {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "supplied buffer is too small %d > %d", total_length,
            buf_len);
        return -1;
    }

    *buf = (uint8_t)hdr_size;

    bytes_written = 1;

    ostream = pb_ostream_from_buffer(buf + 1, hdr_size);

    if (pb_encode(&ostream, RqHeader_fields, &rq_hdr))
        bytes_written += ostream.bytes_written;
    else {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "encoding request header");
        return -1;
    }

    // Serialize the crypto_info message.
    ostream = pb_ostream_from_buffer(buf + bytes_written, crypto_info_size);

    if (pb_encode(&ostream, CryptoInfo_fields, &rq_crypto_info))
        bytes_written += ostream.bytes_written;
    else {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "encoding crypto info");
        return -1;
    }

    // Serialize the request body.
    //
    buf += bytes_written;

    ostream = pb_ostream_from_buffer(buf, rq_size);

    // Initialize request body.
    if (pb_encode(&ostream, Rq_fields, &rq))
        bytes_written += ostream.bytes_written;
    else {
        LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "encoding request fields");
        return -1;
    }

    // Encrypt the (serialized) request body.
    //
    // TODO: value the padding bytes explicitly instead of just letting them be
    // whatever is in the buffer.
    //

    AES_init_ctx_iv(&aes_ctx, get_ctx_aes_key(ctx), rq_crypto_info.iv.bytes);

    AES_CBC_encrypt_buffer(&aes_ctx, buf, rq_size);

    return bytes_written + aes_padding_length;
}

int32_t apply_used_info_to_ap(Sky_ctx_t *ctx, uint8_t *used, int size)
{
    int i, v, nap = 0;

    if (!ctx || size > TOTAL_BEACONS * MAX_VAP_PER_AP)
        return -1;

    for (i = 0; i < NUM_APS(ctx); i++) {
        ctx->beacon[nap].ap.property.used = GET_USED_AP(used, size, nap);
        if (nap++ > size * CHAR_BIT)
            break;
    }
    for (v = 0; v < CONFIG(ctx->cache, max_vap_per_ap); v++) {
        for (i = 0; i < NUM_APS(ctx); i++) {
            if (v < NUM_VAPS(&ctx->beacon[i])) {
                ctx->beacon[i].ap.vg_prop[v].used = GET_USED_AP(used, size, nap);
                if (nap++ > size * CHAR_BIT)
                    break;
            }
        }
        if (nap > size * CHAR_BIT)
            break;
    }

    return 0;
}

int32_t deserialize_response(Sky_ctx_t *ctx, uint8_t *buf, uint32_t buf_len, Sky_location_t *loc)
{
    uint8_t hdr_size = *buf;
    struct AES_ctx aes_ctx;
    int32_t ret = -1;

    RsHeader header;

    Rs rs = Rs_init_default;

    CryptoInfo crypto_info;

    pb_istream_t istream;

    // We assume that buf contains the response message in its entirety. (Since
    // the server closes the connection after sending the response, the client
    // doesn't need to know how many bytes to read - it just keeps reading
    // until the connection is closed by the server.)
    //
    // Deserialize the header. First byte of input buffer represents length of
    // header.
    //
    if (buf_len < 1)
        return ret;

    buf += 1;

    if (buf_len < 1 + hdr_size)
        return ret;

    istream = pb_istream_from_buffer(buf, hdr_size);

    if (!pb_decode(&istream, RsHeader_fields, &header)) {
        LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "failed to decode header");
        return ret;
    }

    memset(loc, 0, sizeof(*loc));
    loc->location_status = (Sky_loc_status_t)header.status;

    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "header.rs_length %d", header.rs_length);
    if (header.rs_length) {
        buf += hdr_size;

        // Deserialize the crypto_info.
        if (buf_len < 1 + hdr_size + header.crypto_info_length + header.rs_length)
            return ret;

        istream = pb_istream_from_buffer(buf, header.crypto_info_length);

        if (!pb_decode(&istream, CryptoInfo_fields, &crypto_info)) {
            return ret;
        }

        buf += header.crypto_info_length;

        // Decrypt the response body.
        AES_init_ctx_iv(&aes_ctx, get_ctx_aes_key(ctx), crypto_info.iv.bytes);

        AES_CBC_decrypt_buffer(&aes_ctx, buf, header.rs_length);

        // Deserialize the response body.
        istream = pb_istream_from_buffer(buf, header.rs_length - crypto_info.aes_padding_length);

        if (!pb_decode(&istream, Rs_fields, &rs)) {
            LOGFMT(ctx, SKY_LOG_LEVEL_ERROR, "pb_decode returned failure");
            return ret;
        } else {
            switch (ctx->auth_state) {
            case STATE_TBR_UNREGISTERED:
                if (rs.token_id == TBR_TOKEN_UNKNOWN) {
                    /* failed TBR registration */
                    /* Auth state remains unchanged */
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "TBR registration failed!");
                } else {
                    /* successful TBR registration response */
                    /* Save the token_id for use in subsequent location requests. */
                    ctx->auth_state = STATE_TBR_REGISTERED;
                    ctx->cache->sky_token_id = rs.token_id;
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "New TBR token received from server");
                }
                /* User must retry because this was a registration */
                loc->location_status = SKY_LOCATION_STATUS_AUTH_RETRY;
                break;
            case STATE_TBR_REGISTERED:
                if (header.status == RsHeader_Status_AUTH_ERROR) {
                    /* failed TBR location request */
                    /* Clear the token_id because it is invalid. */
                    ctx->auth_state = STATE_TBR_UNREGISTERED;
                    ctx->cache->sky_token_id = TBR_TOKEN_UNKNOWN;
                    /* Application must re-register */
                    loc->location_status = SKY_LOCATION_STATUS_AUTH_RETRY;
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "TBR authentication failed!");
                    break;
                } else {
                    /* fall through */
                }
            case STATE_TBR_DISABLED:
                if (header.status == RsHeader_Status_AUTH_ERROR) {
                    /* failed legacy location request */
                    loc->location_status = SKY_LOCATION_STATUS_API_SERVER_ERROR;
                    LOGFMT(ctx, SKY_LOG_LEVEL_DEBUG, "Auth Error");
                } else {
                    /* Legacy or tbr location request */
                    loc->lat = rs.lat;
                    loc->lon = rs.lon;
                    loc->hpe = (uint16_t)rs.hpe;
                    loc->location_source = (Sky_loc_source_t)rs.source;
                    /* copy any downlink data to state buffer */
                    loc->dl_app_data = ctx->sky_dl_app_data;
                    ctx->sky_dl_app_data_len = loc->dl_app_data_len =
                        MIN(rs.dl_app_data.size, sizeof(ctx->sky_dl_app_data));
                    memmove(ctx->sky_dl_app_data, rs.dl_app_data.bytes, loc->dl_app_data_len);
                    // Extract Used info for each AP from the Used_aps bytes
                    apply_used_info_to_ap(ctx, (void *)rs.used_aps.bytes, (int)rs.used_aps.size);
                }
                break;
            }
            ret = 0;
        }
        if (apply_config_overrides(ctx->cache, &rs)) {
            if (ctx->logf && SKY_LOG_LEVEL_DEBUG <= ctx->min_level)
                (*ctx->logf)(SKY_LOG_LEVEL_DEBUG, "New config overrides received from server");
        }
        if (CONFIG(ctx->cache, last_config_time) == 0)
            CONFIG(ctx->cache, last_config_time) = (*ctx->gettime)(NULL);
    } else if (hdr_size > 0) {
        (*ctx->logf)(SKY_LOG_LEVEL_DEBUG, "hdr_size > 0");
        if (!is_tbr_enabled(ctx)) {
            (*ctx->logf)(SKY_LOG_LEVEL_DEBUG, "hdr_size > 0");
            loc->location_status = SKY_LOCATION_STATUS_BAD_PARTNER_ID_ERROR;
        }
        ret = 0;
    }

    return ret;
}

static int64_t get_gnss_lat_scaled(Sky_ctx_t *ctx, uint32_t idx)
{
    return get_gnss_lat(ctx, idx) * 1000000;
}

static int64_t get_gnss_lon_scaled(Sky_ctx_t *ctx, uint32_t idx)
{
    return get_gnss_lon(ctx, idx) * 1000000;
}

static int64_t get_gnss_alt_scaled(Sky_ctx_t *ctx, uint32_t idx)
{
    return get_gnss_alt(ctx, idx) * 10;
}

static int64_t get_gnss_speed_scaled(Sky_ctx_t *ctx, uint32_t idx)
{
    return get_gnss_speed(ctx, idx) * 10;
}

/*! \brief update dynamic config params with server overrides
 *
 *  @param c cache buffer
 *
 *  @return bool true if new override is recived from server
 */
static bool apply_config_overrides(Sky_cache_t *c, Rs *rs)
{
    bool override = false;

    config_defaults(c);
    if (rs->config.total_beacons != 0 && rs->config.total_beacons != CONFIG(c, total_beacons)) {
        if (rs->config.total_beacons < TOTAL_BEACONS && rs->config.total_beacons > 1) {
            CONFIG(c, total_beacons) = rs->config.total_beacons;
        }
    }
    if (rs->config.max_ap_beacons != 0 && rs->config.max_ap_beacons != CONFIG(c, max_ap_beacons)) {
        if (rs->config.max_ap_beacons < MAX_AP_BEACONS) {
            CONFIG(c, max_ap_beacons) = rs->config.max_ap_beacons;
        }
    }
    if (rs->config.cache_match_all_threshold != 0 &&
        rs->config.cache_match_all_threshold != CONFIG(c, cache_match_all_threshold)) {
        if (rs->config.cache_match_all_threshold > 0 &&
            rs->config.cache_match_all_threshold <= 100) {
            CONFIG(c, cache_match_all_threshold) = rs->config.cache_match_all_threshold;
        }
    }
    if (rs->config.cache_match_used_threshold != 0 &&
        rs->config.cache_match_used_threshold != CONFIG(c, cache_match_used_threshold)) {
        if (rs->config.cache_match_used_threshold > 0 &&
            rs->config.cache_match_used_threshold <= 100) {
            CONFIG(c, cache_match_used_threshold) = rs->config.cache_match_used_threshold;
        }
    }
    if (rs->config.cache_age_threshold != 0 &&
        rs->config.cache_age_threshold != CONFIG(c, cache_age_threshold)) {
        if (rs->config.cache_age_threshold < 9000) {
            CONFIG(c, cache_age_threshold) = rs->config.cache_age_threshold;
        }
    }
    if (rs->config.cache_beacon_threshold != 0 &&
        rs->config.cache_beacon_threshold != CONFIG(c, cache_beacon_threshold)) {
        if (rs->config.cache_beacon_threshold < CONFIG(c, total_beacons)) {
            CONFIG(c, cache_beacon_threshold) = rs->config.cache_beacon_threshold;
        }
    }
    if (rs->config.cache_neg_rssi_threshold != 0 &&
        rs->config.cache_neg_rssi_threshold != CONFIG(c, cache_neg_rssi_threshold)) {
        if (rs->config.cache_neg_rssi_threshold >= 10 &&
            rs->config.cache_neg_rssi_threshold < 128) {
            CONFIG(c, cache_neg_rssi_threshold) = rs->config.cache_neg_rssi_threshold;
        }
    }
    override = (rs->config.total_beacons != 0 || rs->config.max_ap_beacons != 0 ||
                rs->config.cache_match_all_threshold != 0 ||
                rs->config.cache_match_used_threshold != 0 || rs->config.cache_age_threshold != 0 ||
                rs->config.cache_beacon_threshold != 0 || rs->config.cache_neg_rssi_threshold != 0);

    /* Add new config parameters here */

    return override;
}
