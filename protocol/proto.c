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

struct Ap {
	uint8_t mac[6];
	uint32_t age; // ms
	uint32_t channel;
	int32_t rssi;
    bool connected;
};

struct Ap aps[] = {
    { {0x00, 0x00, 0x00, 0x00, 0x00, 0x0a}, /* age */ 1, /* channel */ 10, /* rssi */ -150, false},
    { {0x00, 0x00, 0x00, 0x00, 0x00, 0x0b}, /* age */ 1, /* channel */ 10, /* rssi */ -150, false},
    //{ {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, /* age */ 32000, /* channel */ 160, /* rssi */ -10, true}
};

size_t num_aps = sizeof(aps) / sizeof(struct Ap);

uint64_t mac_to_int(uint8_t mac[6])
{
    uint64_t ret_val = 0;

    for (size_t i = 0; i < 6; i++)
        ret_val = ret_val * 256 + mac[i];

    return ret_val;
}

bool encode_macs(pb_ostream_t* ostream)
{
    // Encode the array of macs as a sequence of varints.
    for (size_t i = 0; i < num_aps; i++)
    {
        pb_encode_varint(ostream, mac_to_int(aps[i].mac));
    }

    return true;
}

bool encode_macs_field(pb_ostream_t* ostream)
{
    if (!pb_encode_tag(ostream, PB_WT_STRING, Aps_mac_tag))
    {
        printf("pb_encode_tag (macs) returned false!");
        return false;
    }

    // Determine length of macs array.
    pb_ostream_t substream = PB_OSTREAM_SIZING;

    encode_macs(&substream);

    // Now encode the length.
    pb_encode_varint(ostream, substream.bytes_written);

    // Now encode the macs themselves, this time for real.
    encode_macs(ostream);

    return true;
}

bool encode_aps_field(pb_ostream_t* ostream, const pb_field_t* field)
{
    // The Aps field is a submessage. So we need to encode first the tag, and then the length.

    // Encode Aps tag.
    if (!pb_encode_tag(ostream, PB_WT_STRING, field->tag))
    {
        printf("pb_encode_tag(Aps) returned false!");
        return false;
    }

    // Determine length of macs field.
    pb_ostream_t substream = PB_OSTREAM_SIZING;

    if (!encode_macs_field(&substream))
    {
        printf("mac sizing failed!");
        return false;
    }

    printf("mac field size = %zu\n", substream.bytes_written);

    // Now encode the length of the macs field.
    if (!pb_encode_varint(ostream, substream.bytes_written))
    {
        printf("pb_encode_varint (Aps length) returned false!");
        return false;
    }

    // Now encode the mac field itself.
    encode_macs_field(ostream);

    return true;
}

bool Rq_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field)
{
    printf("callback with tag %d\n", field->tag);

    printf("context data = %s\n", * (char**) field->pData);

    // Per the documentation here:
    // https://jpa.kapsi.fi/nanopb/docs/reference.html#pb-encode-delimited
    //
    if (field->tag == Rq_aps_tag)
        return encode_aps_field(ostream, field);
    else
        return false;
}

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

    // Put some data into the struct which will then be available to the encode
    // callback. Just stick a string in there for now, but eventually we'll
    // want to pass the full request context, which contains the scan data,
    // etc.
    static char data[] = "context";

    rq.aps = data;

    unsigned char aes_iv_buf[16];

    // TODO: properly value the header.iv field. And maybe move this stuff down
    // into the serialize() method.
    //
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
            uint32_t channel,
            uint32_t ts)
{
    //rq.aps.mac[rq.aps.mac_count++] = strtoll(mac_hex_str, 0, 16);
    //rq.aps.rssi[rq.aps.rssi_count++] = rssi;
    //rq.aps.connected[rq.aps.connected_count++] = is_connected;
    //rq.aps.band[rq.aps.band_count++] = band;
}

void add_lte_cell(uint32_t mcc,
                  uint32_t mnc,
                  uint32_t eucid,
                  int32_t rssi,
                  uint32_t ts)
{
    rq.lte_cells.mcc[rq.lte_cells.mcc_count++] = mcc;
    rq.lte_cells.mnc[rq.lte_cells.mnc_count++] = mnc;
    rq.lte_cells.eucid[rq.lte_cells.eucid_count++] = eucid;
    rq.lte_cells.ts[rq.lte_cells.ts_count++] = ts;
    rq.lte_cells.rssi[rq.lte_cells.rssi_count++] = rssi;
}

void suppress_degenerate_fields()
{
#if 0
    // Remove certain repeated fields if all elements contain default values.
    // This is purely a bandwidth utilization optimization.
    //
    bool suppress_ts = true;
    bool suppress_channel = true;

    for (size_t i = 0; (suppress_ts || suppress_channel) && i < rq.aps.ts_count; i++)
    {
        if (rq.aps.ts[i] != 0)
            suppress_ts = false;

        if (rq.aps.channel_number[i] != 0)
            suppress_channel = false;
    }

    if (suppress_ts)
        rq.aps.ts_count = 0;

    if (suppress_channel)
    {
        rq.aps.channel_number_count = 0;
    }
#endif
}

int32_t serialize_request(uint8_t* buf, size_t buf_len)
{
    suppress_degenerate_fields();

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
    uint8_t hdr_size;

    pb_get_encoded_size((size_t*) &hdr_size, RqHeader_fields, &rq_hdr);

    *buf = hdr_size;

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

int32_t deserialize_response(uint8_t* buf, size_t buf_len, Rs* rs)
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
    AES_init_ctx_iv(&aes_ctx, aes_key_buf, crypto_info.iv.bytes);

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
