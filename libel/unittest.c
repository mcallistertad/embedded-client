#include <string.h>
#include <stdlib.h>
#include "unittest.h"

void _test_init(Test_ctx *ctx, Test_opts *opts, const char *str)
{
    ctx->opts = opts;
    ctx->name = str;
    ctx->file = __FILENAME__;
    ctx->group = NULL;
    ctx->desc = NULL;

    ctx->ran = 0;
    ctx->failed = 0;
}

void _test_set_group(Test_ctx *ctx, const char *str)
{
    ctx->group = str;
}

void _test_set_desc(Test_ctx *ctx, const char *str)
{
    ctx->desc = str;
}

void _test_assert(Test_ctx *ctx, const char *file, int line, int res)
{
    ctx->ran++;
    if (!res)
        ctx->failed++;

    if (!res || ctx->opts->verbose) {
        const char *base = strrchr(file, '/');
        base = base ? base + 1 : file;

        fprintf(stdout, BRIGHT "%s" RESET ":%s:%s:%d [ %s" RESET " ] %s\n", ctx->name, ctx->group,
            base, line, res ? GREEN "PASS" : RED "FAIL", ctx->desc);
    }
}

void _test_print_rs(Test_opts *opts, Test_rs rs)
{
    if (rs.failed || opts->verbose) {
        fprintf(stdout, "%d Tests, %d Failures\n", rs.ran, rs.failed);
    }
}

int _test_log(Sky_log_level_t level, char *s)
{
    fprintf(stderr, " >>> %s\n", s);
    return 0;
}

int _test_beacon(
    Beacon_t *b, Sky_beacon_type_t type, time_t timestamp, int16_t rssi, bool is_connected)
{
    memset(b, 0, sizeof(*b));
    b->h.magic = BEACON_MAGIC;
    b->h.type = type;
    b->h.connected = is_connected;
    b->h.rssi = rssi;
    b->h.age = timestamp;

    return 1;
}

int _test_cell(Beacon_t *b, Sky_beacon_type_t type, time_t timestamp, int16_t rssi,
    bool is_connected, uint16_t id1, uint16_t id2, int32_t id3, int64_t id4, int16_t id5,
    int32_t freq)

{
    if (!_test_beacon(b, type, timestamp, rssi, is_connected))
        return 0;

    b->cell.id1 = id1;
    b->cell.id2 = id2;
    b->cell.id3 = id3;
    b->cell.id4 = id4;
    b->cell.id5 = id5;
    b->cell.freq = freq;

    return 1;
}

int _test_ap(Beacon_t *b, const char *mac, time_t timestamp, int16_t rssi, int32_t frequency,
    bool is_connected)
{
    if (!_test_beacon(b, SKY_BEACON_AP, timestamp, rssi, is_connected))
        return 0;

    // copy MAC
    const char *p;
    unsigned int c;
    int i;
    for (i = 0, p = mac; *p && i < MAC_SIZE; ++i, p += 2) {
        if (sscanf(p, "%02X", &c) != 1)
            break;
        b->ap.mac[i] = c;
    }

    b->ap.freq = frequency;
    b->ap.property.in_cache = false;
    b->ap.property.used = false;

    return 1;
}

Sky_ctx_t *_test_sky_ctx()
{
    Sky_ctx_t *ctx;
    Sky_errno_t _ctx_errno;
    uint8_t _aes_key[AES_KEYLEN] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
    uint32_t bufsize;

    if (sky_open(&_ctx_errno, (uint8_t *)TEST_DEVICE_ID, 6, TEST_PARTNER_ID, _aes_key, TEST_SKU,
            200, NULL, SKY_LOG_LEVEL_DEBUG, _test_log, NULL, NULL) == SKY_ERROR) {
        fprintf(stderr, "Failure setting up mock context, aborting!\n");
        exit(-1);
    }

    bufsize = sky_sizeof_workspace();
    if (bufsize == 0 || bufsize > 4096) {
        fprintf(stderr, "sky_sizeof_workspace returned bad value, Can't continue\n");
        exit(-1);
    }

    ctx = malloc(bufsize);
    //memset(ctx, 0, bufsize);

    Sky_errno_t sky_errno = -1;
    if (sky_new_request(ctx, bufsize, &sky_errno) != ctx) {
        fprintf(stderr, "sky_new_request() returned bad value\n");
        fprintf(stderr, "sky_errno contains '%s'\n", sky_perror(sky_errno));
        exit(-1);
    }

    return ctx;
}

bool _test_beacon_eq(const Beacon_t *a, const Beacon_t *b)
{
    return a->h.magic == b->h.magic && a->h.type == b->h.type;
}

bool _test_ap_eq(const Beacon_t *a, const Beacon_t *b)
{
    return _test_beacon_eq(a, b) &&
           0 == strncmp((const char *)a->ap.mac, (const char *)b->ap.mac, MAC_SIZE) &&
           a->ap.freq == b->ap.freq;
}
