/*! \file sample_client/sample_client.c
 *  \brief Sample Client - Skyhook Embedded Library
 *
 * Copyright (c) 2020 Skyhook, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libel.h"
#include "send.h"
#include "math.h"
#include "config.h"

typedef enum {
    TYPE_RESERVED = 0,
    TYPE_BLE = 2,
    TYPE_NR = 3,
    TYPE_LTE = 4,
    TYPE_UMTS = 5,
    TYPE_NBIOT = 6,
    TYPE_CDMA = 7,
    TYPE_GSM = 8,
} Type_t;

/* scan list definitions (platform dependant) */
struct ap_scan {
    char mac[MAC_SIZE * 2 + 1];
    uint32_t age;
    uint32_t frequency;
    int16_t rssi;
    bool connected;
};

struct cell_scan {
    Type_t type; // TYPE_NR/LTE/UMTS/NBIOT/CDMA/GSM
    uint32_t age; // age in sec (time since scan was collected)
    uint16_t ss; // Signal Strength
    uint16_t id1; // mcc (gsm, umts, lte, nr, nb-iot). SKY_UNKNOWN_ID1 if unknown.
    uint16_t id2; // mnc (gsm, umts, lte, nr, nb-iot) or sid (cdma). SKY_UNKNOWN_ID2 if unknown.
    int32_t
        id3; // lac (gsm, umts) or tac (lte, nr, nb-iot) or nid (cdma). SKY_UNKNOWN_ID3 if unknown.
    int64_t id4; // cell id (gsm, umts, lte, nb-iot, nr), bsid (cdma). SKY_UNKNOWN_ID4 if unknown.
    int16_t
        id5; // bsic (gsm), psc (umts), pci (lte, nr) or ncid (nb-iot). SKY_UNKNOWN_ID5 if unknown.
    int32_t
        freq; // arfcn(gsm), uarfcn (umts), earfcn (lte, nb-iot), nrarfcn (nr). SKY_UNKNOWN_ID6 if unknown.
    int32_t ta; // SKY_UNKNOWN_TA if unknown.
    bool connected;
};

struct gnss_scan {
    uint32_t age;
    float lat;
    float lon;
    uint16_t hpe;
    float altitude;
    uint16_t vpe;
    float speed;
    float bearing;
    uint16_t nsat;
};

/* Multiple sets of scans */
/* some rssi values intentionally out of range */

/* Scan set 1 */
struct ap_scan aps1[] = /* clang-format off */
    { { "283B8264E08B", 300, 3660, -8, 0 },
    { "823AB292D699", 30, 3660, -30, 1 }, { "2A32825649F0", 300, 3660, -70, 0 },
    { "826AB092DC99", 30, 3660, -130, 0 }, { "283B823629F0", 300, 3660, -90, 0 },
    { "283B821C712A", 30, 3660, -77, 0 }, { "283B821CC232", 30, 3660, -91, 0 },
    { "74DADA5E1015", 300, 3660, -88, 0 }, { "B482FEA46221", 30, 3660, -89, 0 },
    { "74DAD95E1015", 300, 3660, -88, 0 }, { "B482F1A46221", 30, 3660, -89, 0 },
    { "283B821CC232", 300, 3660, -91, 0 }, { "283B822CC232", 30, 3660, -91, 0 },
    { "283B823CC232", 300, 3660, -91, 0 }, { "283B824CC232", 300, 3660, -91, 0 },
    { "283B825CC232", 30, 3660, -91, 0 }, { "EC22809E00DB", 300, 3660, -90, 0 },
    { .mac = { '\0' }, .age = 0, .frequency = 0, .rssi = 0, .connected = 0 } };

struct cell_scan cells1[] =
    { { TYPE_UMTS, 45, -100, 603, 1, 16101, 14962, 33, 440, SKY_UNKNOWN_TA, 0},
      { TYPE_LTE, 45, -86, 311, 480, 25614, 25629196, 114, 66536, SKY_UNKNOWN_TA, 0},
      { TYPE_LTE, 154, -105, 311, 480, 25614, 25664524, 387, 66536, SKY_UNKNOWN_TA, 1},
      { TYPE_LTE, 154, -112, SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2, SKY_UNKNOWN_ID3, SKY_UNKNOWN_ID4, 214, 66536, SKY_UNKNOWN_TA, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

struct gnss_scan gnss1 =
    {15,36.740028, 3.049608, 108, 219.0, 40, 10.0, 270.0, 5};


/* Scan set 2 */
struct ap_scan aps2[] =
    { { "287AEEBA96C0", 0, 2462, -89, 0 },
      { .mac = {'\0'}, .age = 0, .frequency = 0, .rssi = 0, .connected = 0}
    };

struct cell_scan cells2[] =
    { { TYPE_LTE, 154, -105, 311, 480, 25614, 25664524, 387, 66536, SKY_UNKNOWN_TA, 1},
      { TYPE_LTE, 154, -112, SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2, SKY_UNKNOWN_ID3, SKY_UNKNOWN_ID4, 214, 66536, SKY_UNKNOWN_TA, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

/* Scan set 3 */
struct ap_scan aps3[] =
    { { "287AEEBA96C0", 0, 2462, -89, 0 },
      { "287AEEBA96C3", 0, 2412, -89, 0 },
      { "287AEEBA96C7", 0, 2412, -89, 0 },
      { .mac = {'\0'}, .age = 0, .frequency = 0, .rssi = 0, .connected = 0}
    };

struct cell_scan cells3[] =
    { { TYPE_LTE, 154, -105, 311, 480, 25614, 25664526, 387, 1000, SKY_UNKNOWN_TA, 1},
      { TYPE_LTE, 154, -112, SKY_UNKNOWN_ID1, SKY_UNKNOWN_ID2, SKY_UNKNOWN_ID3, SKY_UNKNOWN_ID4, 214, 66536, SKY_UNKNOWN_TA, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

/* Scan set 4 */
struct ap_scan aps4[] =
    { { "287AEEBA96C0", 0, 2412, -89, 0 },
      { "287AEEBA96C7", 0, 2412, -89, 0 },
      { .mac = {'\0'}, .age = 0, .frequency = 0, .rssi = 0, .connected = 0}
    };

struct cell_scan cells4[] =
    { { TYPE_LTE, 154, -105, 311, 480, 25614, 25664526, 387, 1000, SKY_UNKNOWN_TA, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

/* clang-format on */

/*! \brief save cache state
 *
 *  @param p - pointer to cache
 *  @param file_name - name of file to use as non-volatile storage
 *
 *  @returns 0 for success or negative number for error
 */
static int save_state(void *p, char *file_name)
{
    FILE *fio;
    uint32_t state_size;

    if ((state_size = sky_sizeof_session_ctx(p)) == 0 || strlen(file_name) == 0) {
        free(p);
        return -1; /* nothing to save */
    }
    if ((fio = fopen(file_name, "w+")) != NULL) {
        if (fwrite(p, state_size, 1, fio) == 1) {
            printf("Saved state: size %d bytes\n", state_size);
            fclose(fio);
            free(p);
            return 0;
        } else {
            fclose(fio);
            printf("Error: write to file %s failed\n", file_name);
        }
    } else
        printf("Error: could not open file %s\n", file_name);
    free(p);
    return -1;
}

/*! \brief restore saved cache state
 *
 *  @param file_name name of file to read
 *
 *  @returns NULL for failure to restore cache state, pointer to c state otherwise
 */
static void *restore_state(char *file_name)
{
    void *pstate;

    uint8_t tmp[SKY_SIZEOF_SESSION_HEADER];
    uint32_t state_size;
    FILE *fio;

    if (strlen(file_name) != 0) {
        /* Open file and read header */
        if ((fio = fopen(file_name, "r")) != NULL) {
            if (fread((void *)tmp, sizeof(tmp), 1, fio) == 1) {
                /* query header for actual size */
                if ((state_size = sky_sizeof_session_ctx(tmp)) > 0) {
                    rewind(fio);
                    pstate = malloc(state_size);
                    if (fread(pstate, state_size, 1, fio) == 1) {
                        fclose(fio);
                        printf("Restored state from %s (%d bytes)\n", file_name, state_size);
                        return pstate;
                    }
                }
            }
            fclose(fio);
        }
    }
    state_size = sky_sizeof_session_ctx(NULL); /* size of new buffere */
    pstate = malloc(state_size);
    memset(pstate, 0, state_size);
    printf("Allocating new state buffer %d bytes\n", state_size);
    return pstate;
}

/* From c-faq.com/lib/rand.html
 * Here is a portable C implementation of the ``minimal standard'' generator proposed by Park and Miller
 *
 * WARNING - You should provide a suitably cryptographically secure random number generator for your application
 */
#define a 16807
#define m 2147483647
#define q (m / a)
#define r (m % a)

static long int seed = 1;

static void PMseed(long int new_seed)
{
    seed = new_seed;
}

static long int PMrand()
{
    long int hi = seed / q;
    long int lo = seed % q;
    long int test = a * lo - r * hi;
    if (test > 0)
        seed = test;
    else
        seed = test + m;
    return seed;
}

/*! \brief generate random byte sequence
 *
 *  Libel uses the rand_bytes function to create initialization vectors used during encryption of requests using AES 128 CBC.
 *  This example function uses a pseudo random number generator and is not cryptographically secure. You
 *  should replace it with one that is sufficiently good to match the security requirement of your application.
 *
 *  @param rand_buf pointer to buffer where rand bytes are put
 *  @param bufsize length of rand bytes
 *
 *  @returns 0 for failure, length of rand sequence for success
 */
static int rand_bytes(uint8_t *rand_buf, uint32_t bufsize)
{
    uint32_t i;

    if (!rand_buf)
        return 0;

    for (i = 0; i < bufsize; i++)
        rand_buf[i] = PMrand() % 256;
    return bufsize;
}

/*! \brief logging function
 *
 *  @param level log level of this message
 *  @param s this message
 *
 *  @returns 0 for success or negative number for error
 */
static int logger(Sky_log_level_t level, char *s)
{
    printf("Skyhook libEL %s: %.*s\n",
        level == SKY_LOG_LEVEL_CRITICAL ?
            "CRIT" :
            level == SKY_LOG_LEVEL_ERROR ?
            "ERRR" :
            level == SKY_LOG_LEVEL_WARNING ? "WARN" :
                                             level == SKY_LOG_LEVEL_DEBUG ? "DEBG" : "UNKN",
        SKY_LOG_LENGTH, s);
    return 0;
}

/*! \brief time function
 *
 *  @param t where to save the time
 *
 *  @returns the time in seconds since the epoc (linux time)
 */
static time_t mytime(time_t *t)
{
    if (t != NULL) {
        return time(t);
    } else
        return time(NULL);
}

/*! \brief locate function
 *
 *  Add a set of beacon scans process the request
 *  return the location structure
 *
 *  @param request ctx pointer to the buffer allocated
 *  @param bufsize size of the the request ctx buffer allocated
 *  @param session ctx pointer to the buffer allocated
 *  @param config pointer to the config structure
 *  @param ap pointer to array of wifi data
 *  @param cp pointer to array of cell data
 *  @param gp pointer to gnss data
 *  @param ul_data pointer to uplink data buffer
 *  @param data_len length of uplink data buffer
 *  @param server_request true to force server_request
 *  @param loc pointer to location structure for result
 *
 *  @returns true if loc was updated, otherwise false
 */
static int locate(void *ctx, uint32_t bufsize, void *session, Config_t *config, struct ap_scan *ap,
    struct cell_scan *cp, struct gnss_scan *gp, uint8_t *ul_data, uint32_t data_len,
    bool server_request, Sky_location_t *loc)
{
    uint32_t i;
    uint32_t request_size;
    uint32_t response_size;
    void *prequest, *response;
    time_t timestamp = mytime(NULL);
    Sky_status_t ret_status;
    Sky_errno_t sky_errno;

    /* Start new request */
    if (sky_new_request(ctx, bufsize, session, ul_data, data_len, &sky_errno) != ctx) {
        printf("sky_new_request() ERROR: '%s'\n", sky_perror(sky_errno));
        return false;
    }

    /* Add APs to the request */
    for (i = 0; ap; i++, ap++) {
        uint8_t mac[MAC_SIZE];

        if (hex2bin(ap->mac, MAC_SIZE * 2, mac, MAC_SIZE) == MAC_SIZE) {
            ret_status = sky_add_ap_beacon(
                ctx, &sky_errno, mac, timestamp - ap->age, ap->rssi, ap->frequency, ap->connected);
            if (ret_status != SKY_SUCCESS)
                printf("sky_add_ap_beacon sky_errno contains '%s'", sky_perror(sky_errno));
        } else if (ap->mac[0] == '\0')
            break;
        else
            printf("Ignoring AP beacon with bad MAC Address '%s'\n", ap->mac);
    }

    /* add cells to request */
    for (i = 0; cp; i++, cp++) {
        if (cp->type == TYPE_RESERVED)
            break;
        switch (cp->type) {
        case TYPE_CDMA:
            if (sky_add_cell_cdma_beacon(ctx, &sky_errno, cp->id2, cp->id3, cp->id4,
                    timestamp - cp->age, cp->ss, cp->connected) != SKY_SUCCESS)
                printf("sky_add_cell_cdma_beacon sky_errno contains '%s'", sky_perror(sky_errno));
            break;
        case TYPE_GSM:
            if (sky_add_cell_gsm_beacon(ctx, &sky_errno, cp->id3, cp->id4, cp->id1, cp->id2, cp->ta,
                    timestamp - cp->age, cp->ss, cp->connected) != SKY_SUCCESS)
                printf("sky_add_cell_gsm_beacon sky_errno contains '%s'", sky_perror(sky_errno));
            break;
        case TYPE_LTE:
            if (sky_add_cell_lte_beacon(ctx, &sky_errno, cp->id3, cp->id4, cp->id1, cp->id2,
                    cp->id5, cp->freq, cp->ta, timestamp - cp->age, cp->ss,
                    cp->connected) != SKY_SUCCESS)
                printf("sky_add_cell_lte_beacon sky_errno contains '%s'", sky_perror(sky_errno));
            break;
        case TYPE_NBIOT:
            if (sky_add_cell_nb_iot_beacon(ctx, &sky_errno, cp->id1, cp->id2, cp->id4, cp->id3,
                    cp->id5, cp->freq, timestamp - cp->age, cp->ss, cp->connected) != SKY_SUCCESS)
                printf("sky_add_cell_nb_iot_beacon sky_errno contains '%s'", sky_perror(sky_errno));
            break;
        case TYPE_NR:
            if (sky_add_cell_nr_beacon(ctx, &sky_errno, cp->id1, cp->id2, cp->id4, cp->id3, cp->id5,
                    cp->freq, cp->ta, timestamp - cp->age, cp->ss, cp->connected) != SKY_SUCCESS)
                printf("sky_add_cell_nr_beacon sky_errno contains '%s'", sky_perror(sky_errno));
            break;
        case TYPE_UMTS:
            if (sky_add_cell_umts_beacon(ctx, &sky_errno, cp->id3, cp->id4, cp->id1, cp->id2,
                    cp->id5, cp->freq, timestamp - cp->age, cp->ss, cp->connected) != SKY_SUCCESS)
                printf("sky_add_cell_umts_beacon sky_errno contains '%s'", sky_perror(sky_errno));
            break;
        default:
            printf("Error: unknown cell type at index %d", i);
            break;
        }
    }

    if (gp) {
        if (sky_add_gnss(ctx, &sky_errno, gp->lat, gp->lon, gp->hpe, gp->altitude, gp->vpe,
                gp->speed, gp->bearing, gp->nsat, timestamp - gp->age) != SKY_SUCCESS) {
            printf("Error adding GNSS: '%s'\n", sky_perror(sky_errno));
        }
    }

retry_after_auth:
    /* Determine how big the network request buffer must be, and allocate a */
    /* buffer of that length. This function must be called for each request. */
    if (sky_sizeof_request_buf(ctx, &request_size, &sky_errno) != SKY_SUCCESS) {
        printf("Error getting size of request buffer: %s\n", sky_perror(sky_errno));
        return false;
    }
    printf("Required buffer size = %d\n", request_size);

    if ((prequest = malloc(request_size)) == NULL) {
        printf("Error allocating request buffer\n");
        return false;
    }

    /* Finalize the request. This will return either SKY_FINALIZE_LOCATION, in */
    /* which case the loc parameter will contain the location result which was */
    /* obtained from the cache, or SKY_FINALIZE_REQUEST, which means that the */
    /* request buffer must be sent to the Skyhook server. */
    Sky_finalize_t finalize =
        sky_finalize_request(ctx, &sky_errno, prequest, request_size, loc, &response_size);

    switch (finalize) {
    case SKY_FINALIZE_ERROR:
        free(prequest);
        printf("sky_finalize_request error '%s'", sky_perror(sky_errno));
        return false;
        break;
    case SKY_FINALIZE_LOCATION:
        /* Location was found in the cache. No need to go to server. */
        printf("Location found in cache\n");
        if (!server_request)
            return true;
        printf("Making server request\n");
        /* fall through */
    case SKY_FINALIZE_REQUEST:
        /* send the request to the server. */
        response = malloc(response_size);
        if (response == NULL) {
            free(prequest);
            return false;
        }
        printf("server=%s, port=%d\n", config->server, config->port);
        printf("Sending request of length %d to server\nResponse buffer length %d %s\n",
            request_size, response_size, response != NULL ? "alloc'ed" : "bad alloc");
        int32_t rc = send_request((char *)prequest, (int)request_size, response, response_size,
            config->server, config->port);
        free(prequest);

        if (rc > 0)
            printf("Received response of length %d from server\n", rc);
        else {
            free(response);
            printf("ERROR: No response from server!\n");
            return false;
        }

        /* Decode the server response */
        ret_status = sky_decode_response(ctx, &sky_errno, response, response_size, loc);
        free(response);

        if (ret_status == SKY_SUCCESS) {
            return true;
        } else {
            printf("sky_decode_response: '%s'\n", sky_perror(sky_errno));
            if (sky_errno == SKY_AUTH_RETRY) {
                /* Repeat request if Authentication was required for last message */
                goto retry_after_auth;
            }
        }
        break;
    }
    return false;
}

static void report_location(Sky_location_t *loc)
{
    char hex_data[200];
    printf("Skyhook location: status: %s, lat: %d.%06d, lon: %d.%06d, hpe: %d, source: %d\n",
        sky_pserver_status(loc->location_status), (int)loc->lat,
        (int)fabs(round(1000000 * (loc->lat - (int)loc->lat))), (int)loc->lon,
        (int)fabs(round(1000000 * (loc->lon - (int)loc->lon))), loc->hpe, loc->location_source);
    bin2hex(hex_data, sizeof(hex_data), loc->dl_app_data, loc->dl_app_data_len);
    printf("Downlink data: %s(%d)\n", hex_data, loc->dl_app_data_len);
}

/*! \brief validate fundamental functionality of the Embedded Library
 *
 *  @param argc count
 *  @param argv vector of arguments
 *
 *  @returns 0 for success or negative number for error
 */
int main(int argc, char *argv[])
{
    Sky_errno_t sky_errno = -1;
    Sky_status_t ret_status;
    uint32_t *ctx;
    uint32_t bufsize;
    void *pstate;
    Sky_location_t loc;
    char *configfile = NULL;
    Config_t config;
    int ret_code = 0;

    /* Seed the random number generator
     */
    PMseed((long int)time(NULL));

    if (argc > 1)
        configfile = argv[1];
    else
        configfile = "sample_client.conf";

    /* Load the configuration */
    ret_code = load_config(configfile, &config);
    if (ret_code == -1)
        exit(-1);
    print_config(&config);

    /* Retrieve saved state, if any. State includes cached scans and
     * registration information. Failure to restore state will force a
     * reregistration sequence and will limit stationary detection,
     * which will result in needless additional messaging to and from
     * the Skyhook server.
     */
    pstate = restore_state(config.statefile); /* may return an empty buffer */

    /* Initialize the Skyhook resources and restore any saved state.
     * A real device would do this at boot time, or perhaps the first
     * time a location is to be performed.
     */
    ret_status = sky_open(&sky_errno, config.device_id, config.device_len, config.partner_id,
        config.key, config.sku, config.cc, pstate, SKY_LOG_LEVEL_ALL, &logger, &rand_bytes, &mytime,
        config.debounce);
    if (ret_status != SKY_SUCCESS) {
        printf("sky_open returned error (%s), Can't continue\n", sky_perror(sky_errno));
        exit(-1);
    }

    /* Allocate request ctx */
    bufsize = sky_sizeof_request_ctx();
    ctx = malloc(bufsize);

    /* Perform several locations using simulated scan data. A real
     * device would perform locations periodically (perhaps once every
     * hour) rather than one immediately after another.
     */
    if (locate(ctx, bufsize, pstate, &config, aps1, cells1, &gnss1, config.ul_app_data,
            config.ul_app_data_len, true, &loc) == false) {
        printf("ERROR: Failed to resolve location\n");
    } else {
        report_location(&loc);
    }

    if (locate(ctx, bufsize, pstate, &config, aps2, cells2, NULL, NULL, 0, false, &loc) == false) {
        printf("ERROR: Failed to resolve location\n");
    } else {
        report_location(&loc);
    }

    if (locate(ctx, bufsize, pstate, &config, aps3, cells3, NULL, config.ul_app_data,
            config.ul_app_data_len, true, &loc) == false) {
        printf("ERROR: Failed to resolve location\n");
    } else {
        report_location(&loc);
    }

    if (locate(ctx, bufsize, pstate, &config, aps4, cells4, NULL, config.ul_app_data,
            config.ul_app_data_len, true, &loc) == false) {
        printf("ERROR: Failed to resolve location\n");
    } else {
        report_location(&loc);
    }

    /* Close Skyhook library and save library state. A real
     * device would normally do this at system shutdown time.
     * Saved state should be restored to the library the next
     * time skyhook_open() is called (see comments above
     * immediately preceding the call to sky_open()).
     */
    if (sky_close(ctx, &sky_errno) != SKY_SUCCESS)
        printf("sky_close sky_errno contains '%s'\n", sky_perror(sky_errno));

    if (pstate != NULL) {
        save_state(pstate, config.statefile);
        free(ctx);
    }

    printf("Done.\n\n");
}
