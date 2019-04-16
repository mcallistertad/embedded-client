#ifndef ELG_PB_H
#define ELG_PB_H

#define SKY_LIBELG

#include "libelg.h"

// Encode and encrypt request into buffer.
int32_t serialize_request(Sky_ctx_t* ctx);

// Decrypt and decode response info from buffer.
int32_t deserialize_response(Sky_ctx_t* ctx,
                             uint8_t* buf,
                             uint32_t buf_len,
                             Sky_location_t* loc);
#endif
