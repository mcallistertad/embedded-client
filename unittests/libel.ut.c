#include <alloca.h>
#define CONFIG_UPDATE_DUE ((time_t)0)
#define TIME_UNAVAILABLE ((time_t)0)

time_t bad_time(time_t *t)
{
    (void)t;
    return (time_t)0;
}

time_t good_time(time_t *t)
{
    (void)t;
    return time(NULL);
}

TEST_FUNC(test_sky_add)
{
    TEST("sky_add_ap_beacon set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad mac", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;

        ASSERT(SKY_ERROR == sky_add_ap_beacon(
                                ctx, &sky_errno, mac, ctx->header.time - 3, rssi, freq, connected));
        ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
    });
    TEST("sky_add_ap_beacon set last_config to zero first time", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ASSERT(ctx->state->config.last_config_time == CONFIG_UPDATE_DUE);
        ASSERT(SKY_SUCCESS == sky_add_ap_beacon(ctx, &sky_errno, mac, ctx->header.time - 3, rssi,
                                  freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(ctx->state->config.last_config_time == CONFIG_UPDATE_DUE);
    });
    TEST("sky_add_ap_beacon set last_config to timestamp second time", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ctx->state->config.last_config_time = time(NULL);
        ASSERT(SKY_SUCCESS == sky_add_ap_beacon(ctx, &sky_errno, mac, ctx->header.time - 3, rssi,
                                  freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(ctx->state->config.last_config_time != CONFIG_UPDATE_DUE);
    });
    TEST("sky_add_ap_beacon set last_config to zero with bad timestamp", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(ctx->state->config.last_config_time == CONFIG_UPDATE_DUE);
    });
}

TEST_FUNC(test_cache_match)
{
    TEST("AP matches cache with same AP", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t b = { .ap.h = { BEACON_MAGIC, SKY_BEACON_AP, 1, -30, 1 },
            .ap.mac = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B },
            .ap.freq = 3660,
            .ap.property = { 0, 0 },
            .ap.vg_len = 0 };
        uint32_t size;

        ctx->beacon[0] = b;
        ctx->len = 1;
        ctx->ap_len = 1;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from != -1); /* Cache hit */
    });
    TEST("4 APs match cache with same 4 AP", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t b = { .ap.h = { BEACON_MAGIC, SKY_BEACON_AP, 1, -30, 1 },
            .ap.mac = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B },
            .ap.freq = 3660,
            .ap.property = { 0, 0 },
            .ap.vg_len = 0 };
        uint32_t size;

        /* four different APs */
        ctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        ctx->beacon[1] = b;
        b.ap.mac[3] = 0x99;
        ctx->beacon[2] = b;
        b.ap.mac[3] = 0x88;
        ctx->beacon[3] = b;
        ctx->len = 4;
        ctx->ap_len = 4;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from != -1); /* Cache hit */
    });
    TEST("4 APs misses cache with different 4 AP, 2 different", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t b = { .ap.h = { BEACON_MAGIC, SKY_BEACON_AP, 1, -30, 1 },
            .ap.mac = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B },
            .ap.freq = 3660,
            .ap.property = { 0, 0 },
            .ap.vg_len = 0 };
        uint32_t size;

        /* four different APs */
        ctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        ctx->beacon[1] = b;
        b.ap.mac[3] = 0x99;
        ctx->beacon[2] = b;
        b.ap.mac[3] = 0x88;
        ctx->beacon[3] = b;
        ctx->len = 4;
        ctx->ap_len = 4;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        ctx->state->cacheline[0].beacon[0].ap.mac[3] = 0x77;
        ctx->state->cacheline[0].beacon[1].ap.mac[3] = 0x66;
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from == -1); /* Cache miss */
    });
    TEST("3 APs misses cache with 3 AP same, 1 extra ", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t b = { .ap.h = { BEACON_MAGIC, SKY_BEACON_AP, 1, -30, 1 },
            .ap.mac = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B },
            .ap.freq = 3660,
            .ap.property = { 0, 0 },
            .ap.vg_len = 0 };
        uint32_t size;

        /* 3 different APs */
        ctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        ctx->beacon[1] = b;
        b.ap.mac[3] = 0x99;
        ctx->beacon[2] = b;
        ctx->len = 3;
        ctx->ap_len = 3;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        b.ap.mac[3] = 0x88;
        ctx->state->cacheline[0].beacon[3] = b;
        ctx->state->cacheline[0].len = 4;
        ctx->state->cacheline[0].ap_len = 4;
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from == -1); /* Cache miss */
    });
    TEST("2 APs misses cache with 1 AP", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t b = { .ap.h = { BEACON_MAGIC, SKY_BEACON_AP, 1, -30, 1 },
            .ap.mac = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B },
            .ap.freq = 3660,
            .ap.property = { 0, 0 },
            .ap.vg_len = 0 };
        uint32_t size;

        /* 2 different APs */
        ctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        ctx->beacon[1] = b;
        ctx->len = 2;
        ctx->ap_len = 2;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        b.ap.mac[3] = 0x88;
        ctx->state->cacheline[0].beacon[2] = b;
        ctx->state->cacheline[0].len = 3;
        ctx->state->cacheline[0].ap_len = 3;
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from == -1); /* Cache miss */
    });
    TEST("2 APs + cell misses cache with 2 AP + different cell", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t b = { .ap.h = { BEACON_MAGIC, SKY_BEACON_AP, 1, -30, 0 },
            .ap.mac = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B },
            .ap.freq = 3660,
            .ap.property = { 0, 0 },
            .ap.vg_len = 0 };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };
        uint32_t size;

        /* 2 different APs */
        ctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        ctx->beacon[1] = b;
        ctx->beacon[2] = c;
        ctx->len = 3;
        ctx->ap_len = 2;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        c.cell.id2 = 47;
        ctx->state->cacheline[0].beacon[2] = c;
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from == -1); /* Cache miss */
    });
    TEST("4 APs + cell misses cache with 4 AP + different cell", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t b = { .ap.h = { BEACON_MAGIC, SKY_BEACON_AP, 1, -30, 0 },
            .ap.mac = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B },
            .ap.freq = 3660,
            .ap.property = { 0, 0 },
            .ap.vg_len = 0 };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };
        uint32_t size;

        /* 2 different APs */
        ctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        ctx->beacon[1] = b;
        ctx->beacon[2] = c;
        ctx->len = 3;
        ctx->ap_len = 2;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        c.cell.id2 = 47;
        ctx->state->cacheline[0].beacon[2] = c;
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from == -1); /* Cache miss */
    });
    TEST("cell matches cache with cell", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };
        uint32_t size;

        ctx->beacon[0] = c;
        ctx->len = 1;
        ctx->ap_len = 0;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from != -1); /* Cache hit */
    });
    TEST("cell misses cache with different cell", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };
        uint32_t size;

        ctx->beacon[0] = c;
        ctx->len = 1;
        ctx->ap_len = 0;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        c.cell.id2 = 47;
        ctx->state->cacheline[0].beacon[0] = c;
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from == -1); /* Cache miss */
    });
}
TEST_FUNC(test_sky_gnss)
{
    TEST("cell misses cache with different gnss", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };
        uint32_t size;

        ctx->beacon[0] = c;
        ctx->len = 1;
        ctx->ap_len = 0;
        ctx->gps.lat = NAN;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        ctx->gps.lat = 3.1; /* new scan has gps (not NaN) */
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->get_from == -1); /* Cache miss */
    });
}

BEGIN_TESTS(libel_test)

GROUP_CALL("sky add tests", test_sky_add);
GROUP_CALL("Sky cache match", test_cache_match);
GROUP_CALL("sky gnss tests", test_sky_gnss);

END_TESTS();
