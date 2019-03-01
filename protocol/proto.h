#include "elg.pb.h"

void init_rq();

void add_ap(const char mac_hex_str[12],
            int8_t rssi, 
            bool is_connected,
            Aps_ApBand band);

int32_t serialize_rq(uint8_t* buf, size_t buf_len);
