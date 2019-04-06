#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "proto.h"
#include "aes.h"

typedef int64_t (*DataCallback) (Sky_ctx_t*, uint32_t);
typedef bool (*EncodeSubmsgCallback) (Sky_ctx_t*, pb_ostream_t*);

int64_t mac_to_int(Sky_ctx_t* ctx, uint32_t idx)
{
    // This is a wrapper function around get_ap_mac(). It converts the 8-byte
    // mac array to an uint64_t.
    //
    uint8_t* mac = get_ap_mac(ctx, idx);

    uint64_t ret_val = 0;

    for (size_t i = 0; i < 6; i++)
        ret_val = ret_val * 256 + mac[i];

    return ret_val;
}

int64_t negative_rssi(Sky_ctx_t* ctx, uint32_t idx)
{
    return -get_ap_rssi(ctx, idx);
}

bool encode_repeated_int_field(Sky_ctx_t* ctx,
                               pb_ostream_t* ostream,
                               uint32_t tag,
                               uint32_t num_elems,
                               DataCallback func)
{
    // Encode field tag.
    if (!pb_encode_tag(ostream, PB_WT_STRING, tag))
        return false;

    // Get and encode the field size.
    pb_ostream_t substream = PB_OSTREAM_SIZING;

    for (size_t i = 0; i < num_elems; i++)
    {
        if (!pb_encode_varint(&substream, func(ctx, i)))
            return false;
    }

    if (!pb_encode_varint(ostream, substream.bytes_written))
        return false;

    // Now encode the field for real.
    for (size_t i = 0; i < num_elems; i++)
    {
        if (!pb_encode_varint(ostream, func(ctx, i)))
            return false;
    }

    return true;
}

bool encode_aps_fields(Sky_ctx_t* ctx, pb_ostream_t* ostream)
{
    uint32_t num_aps = get_num_aps(ctx);

    if (num_aps == 0) 
        return true;

    // Encode index connected_ap_idx_plus_1.
    for (size_t i = 0; i < num_aps; i++)
    {
        if (get_ap_connected(ctx, i))
        {
            pb_encode_tag(ostream, PB_WT_VARINT, Aps_connected_ap_idx_plus_1_tag);
            pb_encode_varint(ostream, i+1);
            break;
        }
    }
    
    // Encode macs, channels, and rssi fields.
    encode_repeated_int_field(ctx, ostream, Aps_mac_tag, num_aps, mac_to_int);
    encode_repeated_int_field(ctx, ostream, Aps_channel_number_tag, num_aps, get_ap_channel);
    encode_repeated_int_field(ctx, ostream, Aps_neg_rssi_tag, num_aps, negative_rssi);

    // Encode age fields. Optimization: send only a single common age value if
    // all ages are the same. 
    uint32_t age = get_ap_age(ctx, 0);
    bool ages_all_same = true;

    for (size_t i = 1; ages_all_same && i < num_aps; i++)
    {
        if (get_ap_age(ctx, i) != age)
            ages_all_same = false;
    }

    if (ages_all_same)
    {
        pb_encode_tag(ostream, PB_WT_VARINT, Aps_common_age_plus_1_tag);
        pb_encode_varint(ostream, age + 1);
    }
    else
    {
        encode_repeated_int_field(ctx, ostream, Aps_age_tag, num_aps, get_ap_age);
    }

    return true;
}

bool encode_submessage(Sky_ctx_t* ctx, pb_ostream_t* ostream, uint32_t tag, EncodeSubmsgCallback func) 
{
    // Encode the submessage tag.
    if (!pb_encode_tag(ostream, PB_WT_STRING, tag))
        return false;

    // Get and encode the submessage size.
    pb_ostream_t substream = PB_OSTREAM_SIZING;

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
    // Actual type of ctx will eventually be a sky_ctx_t*. Baby steps....
    char* ctx = *(char**) field->pData;

    // Per the documentation here:
    // https://jpa.kapsi.fi/nanopb/docs/reference.html#pb-encode-delimited
    //
    switch (field->tag)
    {
        case Rq_aps_tag:
            return encode_submessage(ctx, ostream, field->tag, encode_aps_fields);
            break;
    }

    printf("unknown Rq field: %d\n", field->tag);

    return true;
}

int32_t serialize_request(Sky_ctx_t* ctx,
                          uint8_t* buf,
                          size_t buf_len,
                          uint32_t partner_id,
                          uint8_t aes_key[16],
                          uint8_t* device_id,
                          uint32_t device_id_length)
{
    // Initialize request header.
    RqHeader rq_hdr;

    rq_hdr.partner_id = partner_id;

    // Initialize crypto_info.
    CryptoInfo rq_crypto_info;

    unsigned char aes_iv_buf[16];

    memset(aes_iv_buf, 1, sizeof(aes_iv_buf)); // TODO; initialize this properly.

    memcpy(rq_crypto_info.iv.bytes, aes_iv_buf, sizeof(aes_iv_buf));
    rq_crypto_info.iv.size=16;

    struct AES_ctx aes_ctx;

    AES_init_ctx_iv(&aes_ctx, aes_key, aes_iv_buf);

    // Initialize request body.
    Rq rq;

    memset(&rq, 0, sizeof(rq));

    rq.aps = rq.gsm_cells = ctx;

    memcpy(rq.device_id.bytes, device_id, device_id_length);
    rq.device_id.size = device_id_length;

    // Create and serialize the request header message.
    size_t rq_size;
    pb_get_encoded_size(&rq_size, Rq_fields, &rq);

    // Account for necessary encryption padding.
    size_t aes_padding_length = (16 - rq_size % 16) % 16;

    rq_crypto_info.aes_padding_length = aes_padding_length;

    size_t crypto_info_size;

    pb_get_encoded_size(&crypto_info_size, CryptoInfo_fields, &rq_crypto_info);

    rq_hdr.crypto_info_length = crypto_info_size;
    rq_hdr.rq_length = rq_size + aes_padding_length;

    // First byte of message on wire is the length (in bytes) of the request
    // header.
    size_t hdr_size;

    pb_get_encoded_size(&hdr_size, RqHeader_fields, &rq_hdr);

    if (1 + hdr_size + rq_hdr.crypto_info_length + rq_hdr.rq_length > buf_len)
        return -1;

    *buf = (uint8_t) hdr_size;

    int32_t bytes_written = 1;

    pb_ostream_t hdr_ostream = pb_ostream_from_buffer(buf + 1, buf_len);

    if (pb_encode(&hdr_ostream, RqHeader_fields, &rq_hdr))
        bytes_written += hdr_ostream.bytes_written;
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
    buf += bytes_written;

    pb_ostream_t rq_ostream = pb_ostream_from_buffer(buf,
                                                     buf_len - bytes_written);

    if (pb_encode(&rq_ostream, Rq_fields, &rq))
        bytes_written += rq_ostream.bytes_written;
    else
        return -1;

    // Encrypt the (serialized) request body.
    // TODO: value the padding bytes explicitly instead of just letting them be
    // whatever is in the buffer.
    //
    AES_CBC_encrypt_buffer(&aes_ctx, buf, rq_size + aes_padding_length);

    return bytes_written + aes_padding_length;
}

int32_t deserialize_response(uint8_t* buf,
                             size_t buf_len,
                             uint8_t aes_key[16],
                             Rs* rs)
{
    // We assume that buf contains the response message in its entirety. (Since
    // the server closes the connection after sending the response, the client
    // doesn't need to know how many bytes to read - it just keeps reading
    // until the connection is closed by the server.)
    //
    // Deserialize the header. First byte of input buffer represents length of
    // header.
    //
    uint8_t hdr_size = *buf;

    buf += 1;

    RsHeader header;

    pb_istream_t hdr_istream = pb_istream_from_buffer(buf, hdr_size);

    if (!pb_decode(&hdr_istream, RsHeader_fields, &header))
    {
        return -1;
    }

    buf += hdr_size;

    // Deserialize the crypto_info.
    CryptoInfo crypto_info;

    pb_istream_t crypto_info_istream = 
        pb_istream_from_buffer(buf, header.crypto_info_length);

    if (!pb_decode(&crypto_info_istream, CryptoInfo_fields, &crypto_info))
    {
        return -1;
    }

    buf += header.crypto_info_length;

    // Decrypt the response body.
    struct AES_ctx aes_ctx;

    AES_init_ctx_iv(&aes_ctx, aes_key, crypto_info.iv.bytes);

    size_t body_size = buf_len - 1 - hdr_size - header.crypto_info_length;

    AES_CBC_decrypt_buffer(&aes_ctx, buf, body_size);

    // Deserialize the response body.
    pb_istream_t body_info_istream = pb_istream_from_buffer(buf, body_size - crypto_info.aes_padding_length);

    if (!pb_decode(&body_info_istream, Rs_fields, rs))
    {
        return -1;
    }

    return 0;
}
