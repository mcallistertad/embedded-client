#ifndef ELG_PB_H
#define ELG_PB_H

#include "elg.pb.h"

// TODO: Delete these accessor prototypes after they become available in one of Geoff's header file.
uint32_t get_num_aps(void* ctx);
uint8_t* get_ap_mac(void* ctx, uint32_t idx);

// Encode and encrypt request into buffer.
int32_t serialize_request(void* ctx,
                          uint8_t* buf,
                          size_t buf_len,
                          uint32_t partner_id,
                          uint8_t aes_key[16],
                          uint8_t* device_id,
                          uint32_t device_id_length);

// Decrypt and decode response info from buffer.
int32_t deserialize_response(uint8_t* buf,
                             size_t buf_len,
                             uint8_t aes_key[16],
                             Rs* rs);

#endif
