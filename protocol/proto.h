#include "elg.pb.h"

// Sequence for sending request to server is:
// 1. Initialize the static request message structures by calling init_rq()
// 2. Add scan info via the various add_<beacon-type>() funtions.
// 3. Serialize the message into a buffer by calling serialize_request().
// 4. Send the request to the destination using platform-specific network
//    transport mechanism.
//
void init_rq(uint32_t partner_id, const char* hex_key);

void add_ap(const char mac_hex_str[12],
            int8_t rssi, 
            bool is_connected,
            Aps_ApBand band);

int32_t serialize_request(uint8_t* buf, size_t buf_len);
