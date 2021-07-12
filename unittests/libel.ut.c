#include <alloca.h>

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

TEST_FUNC(test_sky_open)
{
    TEST("sky_open succeeds the first time it is called and fails the second", ctx, {
        Sky_errno_t sky_errno;
        Sky_session_t nv_state;

        memset(&nv_state, 0, sizeof(nv_state));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 666,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_NONE);
        ASSERT(nv_state.partner_id == 666);

        ASSERT(SKY_SUCCESS == sky_close(&nv_state, &sky_errno));
        ASSERT(sky_sizeof_session_ctx(&nv_state) == sizeof(Sky_session_t));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 911,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_NONE);
        ASSERT(nv_state.partner_id == 911);

        ASSERT(SKY_ERROR == sky_open(&sky_errno, (uint8_t *)"ABCDEFGH", 8, 666,
                                (uint8_t *)"01234567890123", "sk", 0, &nv_state,
                                SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_ALREADY_OPEN);
    });
    TEST("sky_close fails if LibEL is not open", ctx, {
        Sky_errno_t sky_errno;
        Sky_session_t nv_state;

        memset(&nv_state, 0, sizeof(nv_state));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 666,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time, true));
        ASSERT(SKY_SUCCESS == sky_close(&nv_state, &sky_errno));
        ASSERT(SKY_ERROR == sky_close(&nv_state, &sky_errno) && sky_errno == SKY_ERROR_NEVER_OPEN);
    });
}

TEST_FUNC(test_sky_new_request)
{
    TEST(
        "sky_new_request set errno to SKY_ERROR_SERVICE_DENIED after first failed registration with bad time",
        ctx, {
            Sky_errno_t sky_errno;

            ctx->session->timefn = bad_time;
            ctx->session->sku[0] = 's';
            ctx->session->sku[1] = '\0';
            ctx->session->backoff = SKY_AUTH_NEEDS_TIME;
            ASSERT(
                NULL == sky_new_request(ctx, sizeof(Sky_ctx_t), ctx->session, NULL, 0, &sky_errno));
            ASSERT(sky_errno == SKY_ERROR_SERVICE_DENIED);
        });
    TEST("sky_new_request succeeds after first failed registration with good time", ctx, {
        Sky_errno_t sky_errno;

        ctx->session->timefn = good_time;
        ctx->session->sku[0] = 's';
        ctx->session->sku[1] = '\0';
        ctx->session->backoff = SKY_AUTH_NEEDS_TIME;
        ASSERT(ctx == sky_new_request(ctx, sizeof(Sky_ctx_t), ctx->session, NULL, 0, &sky_errno));
        ASSERT(sky_errno == SKY_ERROR_NONE);
    });
}

TEST_FUNC(test_sky_add)
{
    TEST("sky_add_ap_beacon set sky_errno to SKY_ERROR_BAD_TIME with bad timestamp", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x28, 0x3B, 0x82, 0x64, 0xE0, 0x8B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;

        ASSERT(SKY_ERROR ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, (time_t)666, rssi, freq, connected));
        ASSERT(SKY_ERROR_BAD_TIME == sky_errno);
    });

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
    TEST("sky_add_ap_beacon set age to 0 with bad timestamp", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;

        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(ctx->beacon[0].h.age == 0 && ctx->header.time != TIME_UNAVAILABLE);
    });
    TEST("sky_add_ap_beacon set last_config to zero first time", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ASSERT(ctx->session->config.last_config_time == CONFIG_UPDATE_DUE);
        ASSERT(SKY_SUCCESS == sky_add_ap_beacon(ctx, &sky_errno, mac, ctx->header.time - 3, rssi,
                                  freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(ctx->session->config.last_config_time == CONFIG_UPDATE_DUE);
    });
    TEST("sky_add_ap_beacon set last_config to timestamp second time", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ctx->session->config.last_config_time = time(NULL);
        ASSERT(SKY_SUCCESS == sky_add_ap_beacon(ctx, &sky_errno, mac, ctx->header.time - 3, rssi,
                                  freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(ctx->session->config.last_config_time != CONFIG_UPDATE_DUE);
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
        ASSERT(ctx->session->config.last_config_time == CONFIG_UPDATE_DUE);
    });
}

TEST_FUNC(test_sky_option)
{
    TEST("Add 4 beacons with default config results in 4 in request context", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4D };
        bool connected = false;
        uint32_t value;

        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 20);
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(ctx->num_beacons == 4);
        ASSERT(ctx->num_ap == 4);
    });
    TEST("Add 4 beacons with max_ap_beacons 3 results in 3 in request context", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4D };
        bool connected = false;
        uint32_t value;

        ASSERT(SKY_SUCCESS == sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 3));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(ctx->num_beacons == 3);
        ASSERT(ctx->num_ap == 3);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
    });
    TEST("set/get operates for report_cache and logging level", ctx, {
        Sky_errno_t sky_errno;
        uint32_t value;

        /* check defaults for a new request */
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_LOGGING_LEVEL, &value) &&
               value == SKY_LOG_LEVEL_DEBUG);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_REPORT_CACHE, &value) &&
               value == 0);

        ASSERT(SKY_SUCCESS ==
               sky_set_option(ctx, &sky_errno, CONF_LOGGING_LEVEL, SKY_LOG_LEVEL_CRITICAL));
        ASSERT(SKY_SUCCESS == sky_set_option(ctx, &sky_errno, CONF_REPORT_CACHE, 1));

        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_LOGGING_LEVEL, &value) &&
               value == SKY_LOG_LEVEL_CRITICAL);
        ASSERT(SKY_SUCCESS == sky_get_option(ctx, &sky_errno, CONF_REPORT_CACHE, &value) &&
               value == 1);
    });
    TEST("set options reports Bad Parameters appropriately", ctx, {
        Sky_errno_t sky_errno;

        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_UNKNOWN, -1) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_TOTAL_BEACONS, 100) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_MAX_AP_BEACONS, 100) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_CACHE_BEACON_THRESHOLD, 100) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_CACHE_NEG_RSSI_THRESHOLD, 230) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_CACHE_MATCH_ALL_THRESHOLD, 1000) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(
            SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_CACHE_MATCH_USED_THRESHOLD, 1000) &&
            sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_MAX_VAP_PER_AP, 1000) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(ctx, &sky_errno, CONF_MAX_VAP_PER_RQ, 1000) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
    });
}

#if CACHE_SIZE != 0
TEST_FUNC(test_sky_gnss)
{
    TEST("to cache plugin copies gnss to cache", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 0, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };

        ctx->beacon[0] = c;
        ctx->num_beacons = 1;
        ctx->num_ap = 0;
        ctx->gnss.lat = 35.511315;
        ctx->gnss.lon = 139.618906;
        ctx->gnss.hpe = 16;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        ASSERT(ctx->gnss.lat == ctx->session->cacheline[0].gnss.lat);
        ASSERT(ctx->gnss.lon == ctx->session->cacheline[0].gnss.lon);
        ASSERT(ctx->gnss.hpe == ctx->session->cacheline[0].gnss.hpe);
    });
    TEST("debounce true copies gnss from cache", ctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_WIFI,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 0, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };
        uint32_t size;

        ctx->session->report_cache = true;
        ctx->beacon[0] = c;
        ctx->num_beacons = 1;
        ctx->num_ap = 0;
        ctx->gnss.lat = 35.511315;
        ctx->gnss.lon = 139.618906;
        ctx->gnss.hpe = 16;
        loc.time = ctx->header.time;

        sky_plugin_add_to_cache(ctx, &sky_errno, &loc);
        ctx->session->cacheline[0].gnss.lat = 36.511315;
        ctx->session->cacheline[0].gnss.lon = 140.618906;
        ctx->session->cacheline[0].gnss.hpe = 17;
        ASSERT(SKY_SUCCESS == sky_sizeof_request_buf(ctx, &size, &sky_errno));
        ASSERT(ctx->gnss.lat == ctx->session->cacheline[0].gnss.lat);
        ASSERT(ctx->gnss.lon == ctx->session->cacheline[0].gnss.lon);
        ASSERT(ctx->gnss.hpe == ctx->session->cacheline[0].gnss.hpe);
    });
}
#endif

BEGIN_TESTS(libel_test)

GROUP_CALL("sky open", test_sky_open);
GROUP_CALL("sky new request", test_sky_new_request);
GROUP_CALL("sky add tests", test_sky_add);
GROUP_CALL("sky option tests", test_sky_option);
#if CACHE_SIZE != 0
GROUP_CALL("sky gnss tests", test_sky_gnss);
#endif

END_TESTS();
