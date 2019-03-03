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

void init_rq(uint32_t partner_id, const char* hex_key)
{
    memset(&rq_hdr, 0, sizeof(rq_hdr));

    rq_hdr.partner_id = partner_id;

    memset(&rq, 0, sizeof(rq));

    unsigned char aes_key_buf[16];
    unsigned char aes_iv_buf[16];

    // TODO: properly value the header.iv field.
    memset(aes_iv_buf, 1, sizeof(aes_iv_buf));

    memcpy(rq_crypto_info.iv.bytes, aes_iv_buf, sizeof(aes_iv_buf));
    rq_crypto_info.iv.size=16;

    for (size_t i=0; i < 16; i++)
    {
        sscanf(hex_key, "%2hhx", &aes_key_buf[i]);
        hex_key += 2;
    }

    AES_init_ctx_iv(&aes_ctx, aes_key_buf, aes_iv_buf);
}

void add_ap(const char mac_hex_str[12],
            int8_t rssi,
            bool is_connected,
            Aps_ApBand band)
{
    rq.aps.mac[rq.aps.mac_count++] = strtoll(mac_hex_str, 0, 16);
    rq.aps.rssi[rq.aps.rssi_count++] = rssi;
    rq.aps.connected[rq.aps.connected_count++] = is_connected;
    rq.aps.band[rq.aps.band_count++] = band;
}

int32_t serialize_request(uint8_t* buf, size_t buf_len)
{
    // Create and serialize the request header message.
    size_t rq_size;
    pb_get_encoded_size(&rq_size, Rq_fields, &rq);

    rq_hdr.body_length = rq_size;

    size_t aes_padding = (16 - rq_size % 16) % 16;

    // Account for necessary encryption padding.
    rq_hdr.remaining_length = rq_size + aes_padding;

    size_t crypto_info_size;
    pb_get_encoded_size(&crypto_info_size, CryptoInfo_fields, &rq_crypto_info);

    rq_hdr.remaining_length += crypto_info_size;

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
    AES_CBC_encrypt_buffer(&aes_ctx, body, rq_size + aes_padding);

    return bytes_written + aes_padding;
}
