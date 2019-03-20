/*! \file libelg/unit_test.c
 *  \brief unit tests - Skyhook ELG API Version 3.0 (IoT)
 *
 * Copyright 2019 Skyhook Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "libelg.h"
#include <alloca.h>
#include <stdio.h>
#include <zlib.h>

/* Example assumes a scan with 100 AP beacons
 */
#define SCAN_LIST_SIZE 100

/*! \brief validate fundamental functionality of the ELG IoT library
 *
 */
int main(int ac, char **av)
{
    sky_errno_t sky_errno = -1;
    sky_ctx_t *w;
    uint32_t *p;

    /* get size of workspace */
    printf("sky_sizeof_workspace(SCAN_LIST_SIZE) = %d\n",
           sky_sizeof_workspace(SCAN_LIST_SIZE));

    /* allocate workspace */
    w = p = alloca(sky_sizeof_workspace(SCAN_LIST_SIZE));

    /* initialize the workspace */
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[7] = 0;
    printf("sky_new_request() returns 0x%X\n",
           sky_new_request(w, sky_sizeof_workspace(SCAN_LIST_SIZE), &sky_errno,
                           SCAN_LIST_SIZE));
    printf("sky_errno contains '%s'\n", sky_perror(sky_errno));
    printf("ctx: %08X %08X %08X %08X  %08X %08X %08X %08X\n", p[000], p[001],
           p[002], p[003], p[004], p[005], p[006], p[007]);

    /* validate crc32 using zlib */
    // uLong crc = crc32(0L, Z_NULL, 0);
    uLong crc = (uLong)-1;
    printf("zlib init = 0x%08X\n", crc);
    crc = crc32(crc, (const Bytef *)p, 8);
    printf("zlib crc32 = 0x%08X\n", crc);
}
