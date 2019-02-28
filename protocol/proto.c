#include <inttypes.h>
#include <stdlib.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "proto.h"

static Rq rq;

void init_rq()
{
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

int32_t serialize_rq(uint8_t* buf, size_t buf_len)
{
    pb_ostream_t ostream = pb_ostream_from_buffer(buf, buf_len);

    if (pb_encode(&ostream, Rq_fields, &rq))
        return ostream.bytes_written;
    else
        return -1;
}
