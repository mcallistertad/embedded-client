#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "proto.h"

static Rq rq;
static RqHeader rq_hdr;

void init_rq(uint32_t partner_id)
{
    memset(&rq_hdr, 0, sizeof(rq_hdr));

    rq_hdr.partner_id = partner_id;

    // todo: properly value the header.iv field appropriately.
    rq_hdr.iv.size=16;

    memset(&rq, 0, sizeof(rq));
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
    // First create and serialize the request header.
    pb_ostream_t hdr_ostream = pb_ostream_from_buffer(buf, buf_len);

    size_t rq_size;

    pb_get_encoded_size(&rq_size, Rq_fields, &rq);

    rq_hdr.payload_length = rq_size;

    int32_t bytes_written = -1;

    if (pb_encode(&hdr_ostream, RqHeader_fields, &rq_hdr))
        bytes_written = hdr_ostream.bytes_written;
    else
        return -1;

    printf("encoded header len = %d\n", bytes_written);

    pb_ostream_t rq_ostream = pb_ostream_from_buffer(buf + bytes_written,
                                                     buf_len - bytes_written);

    if (pb_encode(&rq_ostream, Rq_fields, &rq))
        bytes_written += rq_ostream.bytes_written;
    else
        return -1;

    // To-do: encrypte the payload portion of the encoded buffer.

    return bytes_written;
}
