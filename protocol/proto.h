#ifndef ELG_PB_H
#define ELG_PB_H

#include "elg.pb.h"

// Sequence for sending request to server is:
// 1. Initialize the static request message structures by calling init_rq()
// 2. Add scan info via the various add_<beacon-type>() funtions.
// 3. Serialize the message into a buffer by calling serialize_request().
// 4. Send the request to the destination using platform-specific network
//    transport mechanism.
//
void init_rq(uint32_t partner_id, const char* hex_key, const char client_mac[12]);

void add_ap(const char mac_hex_str[12],
            int8_t rssi, 
            bool is_connected,
            uint32_t channel,
            uint32_t ts);

void add_lte_cell(uint32_t mcc,
                  uint32_t mnc,
                  uint32_t eucid,
                  int32_t rssi,
                  uint32_t ts);

// Encode and encrypt request into buffer.
int32_t serialize_request(void* ctx, uint8_t* buf, size_t buf_len);

// Decrypt and decode response info from buffer.
int32_t deserialize_response(uint8_t* buf, size_t buf_len, Rs* rs);

#endif
