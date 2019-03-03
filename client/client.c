#include <stdio.h>
#include "proto.h"

int main(int argc, char** argv)
{
    init_rq(123, "000102030405060708090a0b0c0d0e0f");

    add_ap("aabbcc112233", -10, false, Aps_ApBand_UNKNOWN);
    add_ap("aabbcc112244", -20, true, Aps_ApBand_UNKNOWN);
    add_ap("aabbcc112255", -30, false, Aps_ApBand_BAND_5);
    add_ap("aabbcc112266", -40, true, Aps_ApBand_BAND_2_4);

    unsigned char buf[1024];

    size_t len = serialize_request(buf, sizeof(buf));

    /* Write it to a file */
    FILE* fp = fopen("rq.bin", "wb");

    fwrite(buf, len, 1, fp);
    fclose(fp);
}
