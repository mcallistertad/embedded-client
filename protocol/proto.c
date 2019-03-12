#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "proto.h"
#include "aes.h"

static Rq rq;
static RqHeader rq_hdr;

static CryptoInfo rq_crypto_info;
static struct AES_ctx aes_ctx;

static unsigned char aes_key_buf[16];

static size_t crypto_info_size;

static void hex_str_to_bin(const char* hex_str, uint8_t bin_buff[], size_t buff_len)
{
    const char* pos = hex_str;
    size_t i;

    for (i=0; i < buff_len; i++)
    {
        sscanf(pos, "%2hhx", &bin_buff[i]);
        pos += 2;
    }
}

void init_rq(uint32_t partner_id, const char* hex_key, const char client_mac[12])
{
    memset(&rq_hdr, 0, sizeof(rq_hdr));

    rq_hdr.partner_id = partner_id;

    memset(&rq, 0, sizeof(rq));

    unsigned char aes_iv_buf[16];

    // TODO: properly value the header.iv field.
    memset(aes_iv_buf, 1, sizeof(aes_iv_buf));

    memcpy(rq_crypto_info.iv.bytes, aes_iv_buf, sizeof(aes_iv_buf));
    rq_crypto_info.iv.size=16;

    hex_str_to_bin(hex_key, aes_key_buf, sizeof(aes_key_buf));

    rq.client_mac = strtoll(client_mac, 0, 16);

    AES_init_ctx_iv(&aes_ctx, aes_key_buf, aes_iv_buf);
}

void add_ap(const char mac_hex_str[12],
            int8_t rssi,
            bool is_connected,
            Aps_ApBand band)
{
    rq.aps.mac[rq.aps.mac_count++] = strtoll(mac_hex_str, 0, 16);
    rq.aps.rssi[rq.aps.rssi_count++] = rssi;
    //rq.aps.connected[rq.aps.connected_count++] = is_connected;
    rq.aps.band[rq.aps.band_count++] = band;
}

void add_lte_cell(uint32_t mcc,
                  uint32_t mnc,
                  uint32_t eucid,
                  int32_t rssi,
                  uint32_t age)
{
    rq.lte_cells.mcc[rq.lte_cells.mcc_count++] = mcc;
    rq.lte_cells.mnc[rq.lte_cells.mnc_count++] = mnc;
    rq.lte_cells.eucid[rq.lte_cells.eucid_count++] = eucid;
    rq.lte_cells.age[rq.lte_cells.age_count++] = age;
    rq.lte_cells.rssi[rq.lte_cells.rssi_count++] = rssi;
}

int32_t serialize_request(uint8_t* buf, size_t buf_len)
{
    // Create and serialize the request header message.
    size_t rq_size;
    pb_get_encoded_size(&rq_size, Rq_fields, &rq);

    // Account for necessary encryption padding.
    size_t aes_padding_length = (16 - rq_size % 16) % 16;

    rq_crypto_info.aes_padding_length_plus_one = aes_padding_length + 1;

    pb_get_encoded_size(&crypto_info_size, CryptoInfo_fields, &rq_crypto_info);

    rq_hdr.remaining_length += rq_size + aes_padding_length + crypto_info_size;

    int32_t bytes_written = -1;

    pb_ostream_t hdr_ostream = pb_ostream_from_buffer(buf, buf_len);

    if (pb_encode(&hdr_ostream, RqHeader_fields, &rq_hdr))
        bytes_written = hdr_ostream.bytes_written;
    else
        return -1;

    // Serialize the crypto_info message.
    pb_ostream_t crypto_info_ostream = pb_ostream_from_buffer(buf + bytes_written,
                                                              buf_len - bytes_written);

    if (pb_encode(&crypto_info_ostream, CryptoInfo_fields, &rq_crypto_info))
        bytes_written += crypto_info_ostream.bytes_written;
    else
        return -1;

    // Serialize the request body.
    //
    uint8_t* body = buf + bytes_written;

    pb_ostream_t rq_ostream = pb_ostream_from_buffer(body,
                                                     buf_len - bytes_written);

    if (pb_encode(&rq_ostream, Rq_fields, &rq))
        bytes_written += rq_ostream.bytes_written;
    else
        return -1;

    // Encrypt the (serialized) request body.
    AES_CBC_encrypt_buffer(&aes_ctx, body, rq_size + aes_padding_length);

    return bytes_written + aes_padding_length;
}

int32_t deserialize_response(uint8_t* buf, size_t buf_len, Rs* rs)
{
    // We assume that buf contains the response message in its entirety. (Since
    // the server closes the connection after sending the response, the client
    // doesn't need to know how many bytes to read - it just keeps reading
    // until the connection is closed by the server.)
    //
    // Deserialize the header.
    RsHeader header;

    pb_istream_t hdr_istream = pb_istream_from_buffer(buf, RsHeader_size);

    if (!pb_decode(&hdr_istream, RsHeader_fields, &header))
        return -1;

    // Deserialize the crypto_info.
    CryptoInfo crypto_info;

    pb_istream_t crypto_info_istream = 
        pb_istream_from_buffer(buf + RsHeader_size, crypto_info_size);

    if (!pb_decode(&crypto_info_istream, CryptoInfo_fields, &crypto_info))
        return -1;

    // Decrypt the response body.
    AES_init_ctx_iv(&aes_ctx, aes_key_buf, crypto_info.iv.bytes);

    size_t body_size = buf_len - RsHeader_size - crypto_info_size;
    uint8_t* body_buf = buf + RsHeader_size + crypto_info_size;

    AES_CBC_decrypt_buffer(&aes_ctx, body_buf, body_size);

    // Deserialize the response body.
    pb_istream_t body_info_istream = pb_istream_from_buffer(body_buf, body_size - crypto_info.aes_padding_length_plus_one + 1);

    if (!pb_decode(&body_info_istream, Rs_fields, rs))
        return -1;

    return 0;
}
