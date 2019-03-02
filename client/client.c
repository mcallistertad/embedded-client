#include <stdio.h>
#include "proto.h"

int main(int argc, char** argv)
{
    init_rq(123);

    add_ap("aabbcc112233", -10, false, Aps_ApBand_UNKNOWN);
    add_ap("aabbcc112244", -20, false, Aps_ApBand_UNKNOWN);
    add_ap("aabbcc112255", -30, false, Aps_ApBand_UNKNOWN);

    unsigned char buf[1024];

    size_t len = serialize_request(buf, sizeof(buf));

    /* Write it to a file */
    FILE* fp = fopen("rq.bin", "wb");

    fwrite(buf, len, 1, fp);
    fclose(fp);
}
