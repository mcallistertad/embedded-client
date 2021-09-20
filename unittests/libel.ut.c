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
    TEST("sky_open succeeds the first time it is called and fails the second", rctx, {
        Sky_errno_t sky_errno;
        Sky_sctx_t nv_state;

        memset(&nv_state, 0, sizeof(nv_state));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 666,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time));
        ASSERT(sky_errno == SKY_ERROR_NONE);
        ASSERT(nv_state.partner_id == 666);

        ASSERT(SKY_SUCCESS == sky_close(&nv_state, &sky_errno));
        ASSERT(sky_sizeof_session_ctx(&nv_state) == sizeof(Sky_sctx_t));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 911,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time));
        ASSERT(sky_errno == SKY_ERROR_NONE);
        ASSERT(nv_state.partner_id == 911);

        ASSERT(SKY_ERROR == sky_open(&sky_errno, (uint8_t *)"ABCDEFGH", 8, 666,
                                (uint8_t *)"01234567890123", "sk", 0, &nv_state,
                                SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time));
        ASSERT(sky_errno == SKY_ERROR_ALREADY_OPEN);
    });
    TEST("sky_close fails if LibEL is not open", rctx, {
        Sky_errno_t sky_errno;
        Sky_sctx_t nv_state;

        memset(&nv_state, 0, sizeof(nv_state));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 666,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, _test_log, sky_rand_fn, good_time));
        ASSERT(SKY_SUCCESS == sky_close(&nv_state, &sky_errno));
        ASSERT(SKY_ERROR == sky_close(&nv_state, &sky_errno) && sky_errno == SKY_ERROR_NEVER_OPEN);
    });
}

TEST_FUNC(test_sky_new_request)
{
    TEST(
        "sky_new_request set errno to SKY_ERROR_SERVICE_DENIED after first failed registration with bad time",
        rctx, {
            Sky_errno_t sky_errno;

            rctx->session->timefn = bad_time;
            rctx->session->sku[0] = 's';
            rctx->session->sku[1] = '\0';
            rctx->session->backoff = SKY_AUTH_NEEDS_TIME;
            ASSERT(NULL ==
                   sky_new_request(rctx, sizeof(Sky_rctx_t), rctx->session, NULL, 0, &sky_errno));
            ASSERT(sky_errno == SKY_ERROR_SERVICE_DENIED);
        });
    TEST("sky_new_request succeeds after first failed registration with good time", rctx, {
        Sky_errno_t sky_errno;

        rctx->session->timefn = good_time;
        rctx->session->sku[0] = 's';
        rctx->session->sku[1] = '\0';
        rctx->session->backoff = SKY_AUTH_NEEDS_TIME;
        ASSERT(
            rctx == sky_new_request(rctx, sizeof(Sky_rctx_t), rctx->session, NULL, 0, &sky_errno));
        ASSERT(sky_errno == SKY_ERROR_NONE);
    });
}

TEST_FUNC(test_sky_add)
{
    TEST("sky_add_ap_beacon set sky_errno to SKY_ERROR_BAD_TIME with bad timestamp", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x28, 0x3B, 0x82, 0x64, 0xE0, 0x8B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;

        ASSERT(SKY_ERROR ==
               sky_add_ap_beacon(rctx, &sky_errno, mac, (time_t)666, rssi, freq, connected));
        ASSERT(SKY_ERROR_BAD_TIME == sky_errno);
    });

    TEST("sky_add_ap_beacon set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad mac", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;

        ASSERT(SKY_ERROR == sky_add_ap_beacon(rctx, &sky_errno, mac, rctx->header.time - 3, rssi,
                                freq, connected));
        ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
    });
    TEST("sky_add_ap_beacon set age to 0 with bad timestamp", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;

        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(rctx->beacon[0].h.age == 0 && rctx->header.time != TIME_UNAVAILABLE);
    });
    TEST("sky_add_ap_beacon set last_config to zero first time", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ASSERT(rctx->session->config.last_config_time == CONFIG_UPDATE_DUE);
        ASSERT(SKY_SUCCESS == sky_add_ap_beacon(rctx, &sky_errno, mac, rctx->header.time - 3, rssi,
                                  freq, connected));
        ASSERT(sky_sizeof_request_buf(rctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(rctx->session->config.last_config_time == CONFIG_UPDATE_DUE);
    });
    TEST("sky_add_ap_beacon set last_config to timestamp second time", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        rctx->session->config.last_config_time = time(NULL);
        ASSERT(SKY_SUCCESS == sky_add_ap_beacon(rctx, &sky_errno, mac, rctx->header.time - 3, rssi,
                                  freq, connected));
        ASSERT(sky_sizeof_request_buf(rctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(rctx->session->config.last_config_time != CONFIG_UPDATE_DUE);
    });
    TEST("sky_add_ap_beacon set last_config to zero with bad timestamp", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(sky_sizeof_request_buf(rctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        ASSERT(rctx->session->config.last_config_time == CONFIG_UPDATE_DUE);
    });
}

TEST_FUNC(test_sky_option)
{
    TEST("Add 4 beacons with default config results in 4 in request context", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4D };
        bool connected = false;
        uint32_t value;

        ASSERT(SKY_SUCCESS == sky_get_option(rctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value > 4);
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(rctx->num_beacons == 4);
        ASSERT(rctx->num_ap == 4);
    });
    TEST("Add 4 beacons with max_ap_beacons 3 results in 3 in request context", rctx, {
        Sky_errno_t sky_errno;
        uint8_t mac1[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        uint8_t mac2[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4C };
        uint8_t mac3[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4A };
        uint8_t mac4[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4D };
        bool connected = false;
        uint32_t value;

        ASSERT(SKY_SUCCESS == sky_set_option(rctx, &sky_errno, CONF_MAX_AP_BEACONS, 3));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac1, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac2, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac3, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(rctx, &sky_errno, mac4, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(rctx->num_beacons == 3);
        ASSERT(rctx->num_ap == 3);
        ASSERT(SKY_SUCCESS == sky_get_option(rctx, &sky_errno, CONF_MAX_AP_BEACONS, &value) &&
               value == 3);
    });

    TEST("set options reports Bad Parameters appropriately", rctx, {
        Sky_errno_t sky_errno;

        ASSERT(SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_UNKNOWN, -1) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_TOTAL_BEACONS, 100) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_MAX_AP_BEACONS, 100) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_CACHE_BEACON_THRESHOLD, 100) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_CACHE_NEG_RSSI_THRESHOLD, 230) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(
            SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_CACHE_MATCH_ALL_THRESHOLD, 1000) &&
            sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(
            SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_CACHE_MATCH_USED_THRESHOLD, 1000) &&
            sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_MAX_VAP_PER_AP, 1000) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
        ASSERT(SKY_ERROR == sky_set_option(rctx, &sky_errno, CONF_MAX_VAP_PER_RQ, 1000) &&
               sky_errno == SKY_ERROR_BAD_PARAMETERS);
    });
}

TEST_FUNC(test_sky_gnss)
{
    TEST("to cache plugin copies gnss to cache", rctx, {
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

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = 35.511315;
        rctx->gnss.lon = 139.618906;
        rctx->gnss.hpe = 16;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        ASSERT(rctx->gnss.lat == rctx->session->cacheline[0].gnss.lat);
        ASSERT(rctx->gnss.lon == rctx->session->cacheline[0].gnss.lon);
        ASSERT(rctx->gnss.hpe == rctx->session->cacheline[0].gnss.hpe);
    });
    TEST("cache hit true with no gnss)", rctx, {
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

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = NAN;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == true);
    });
    TEST("cache hit true copies gnss from cache to request context (gnss only in cache)", rctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315, /* API server response */
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
        uint32_t buf_size;

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = NAN; /* gnss empty in request rctx */
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        rctx->session->cacheline[0].gnss.lat = 35.511314;
        rctx->session->cacheline[0].gnss.lon = 139.618905;
        rctx->session->cacheline[0].gnss.hpe = 47;
        rctx->session->cacheline[0].loc = loc;
        /* clear location source in order to subsequently verify that it's */
        /* been copied out of the cache as expected */
        loc.location_source = SKY_LOCATION_SOURCE_UNKNOWN;

        ASSERT(IS_CACHE_HIT(rctx) == false);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == true);
        /* Verify location source is copied from the cache */
        ASSERT(loc.location_source == rctx->session->cacheline[0].loc.location_source);
        ASSERT(sky_sizeof_request_buf(rctx, &buf_size, &sky_errno) == SKY_SUCCESS);
        /* Verify that GNSS fix is copied from the cache. */
        ASSERT(rctx->gnss.lat == rctx->session->cacheline[0].gnss.lat);
        ASSERT(rctx->gnss.lon == rctx->session->cacheline[0].gnss.lon);
        ASSERT(rctx->gnss.hpe == rctx->session->cacheline[0].gnss.hpe);
    });
    TEST("cache miss gnss in new scan only", rctx, {
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

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = NAN;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        rctx->gnss.lat = 35.511315;
        rctx->gnss.lon = 139.618906;
        rctx->gnss.hpe = 56;
        ASSERT(IS_CACHE_HIT(rctx) == false);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("cache miss - New GNSS overlaps cached location but has smaller HPE", rctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 35.511315,
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_GNSS,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 0, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = 35.51131;
        rctx->gnss.lon = 139.6189;
        rctx->gnss.hpe = 90;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        rctx->session->cacheline[0].gnss.lat = 35.51132;
        rctx->session->cacheline[0].gnss.lon = 139.618;
        rctx->session->cacheline[0].gnss.hpe = 123;
        ASSERT(IS_CACHE_HIT(rctx) == false);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("cache miss - New GNSS has larger HPE but does not overlap cached location", rctx, {
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

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = 35.511315;
        rctx->gnss.lon = 139.618906;
        rctx->gnss.hpe = 57;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        rctx->session->cacheline[0].gnss.lat = 35; /* far away */
        rctx->session->cacheline[0].gnss.lon = 139;
        rctx->session->cacheline[0].gnss.hpe = 46; /* better accuracy */
        ASSERT(IS_CACHE_HIT(rctx) == false);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("cache miss - New GNSS has smaller HPE and does not overlap cached location", rctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 36.511315, /* API server response */
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_GNSS,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 0, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = 35.51131;
        rctx->gnss.lon = 139.6189;
        rctx->gnss.hpe = 30;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        rctx->session->cacheline[0].gnss.lat = 35.51132; /* position different but close (82m) */
        rctx->session->cacheline[0].gnss.lon = 139.618;
        rctx->session->cacheline[0].gnss.hpe = 47; /* hpe worse in cache */
        ASSERT(IS_CACHE_HIT(rctx) == false);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("cache hit - New GNSS overlaps cached location and has larger HPE", rctx, {
        Sky_errno_t sky_errno;
        Sky_location_t loc = { .lat = 36.511315, /* API server response */
            .lon = 139.618906,
            .hpe = 16,
            .location_source = SKY_LOCATION_SOURCE_GNSS,
            .location_status = SKY_LOCATION_STATUS_SUCCESS };
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 0, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        rctx->gnss.lat = 35.51131;
        rctx->gnss.lon = 139.6189;
        rctx->gnss.hpe = 90;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        rctx->session->cacheline[0].gnss.lat = 35.51132; /* position different but close (82m) */
        rctx->session->cacheline[0].gnss.lon = 139.618;
        rctx->session->cacheline[0].gnss.hpe = 47; /* hpe better in cache */
        ASSERT(IS_CACHE_HIT(rctx) == false);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == true);
    });
}

TEST_FUNC(test_cache_match)
{
    TEST("AP matches cache with same AP", rctx, {
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

        rctx->beacon[0] = b;
        rctx->num_beacons = 1;
        rctx->num_ap = 1;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == true);
    });
    TEST("4 APs match cache with same 4 AP", rctx, {
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

        /* four different APs */
        rctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        rctx->beacon[1] = b;
        b.ap.mac[3] = 0x99;
        rctx->beacon[2] = b;
        b.ap.mac[3] = 0x88;
        rctx->beacon[3] = b;
        rctx->num_beacons = 4;
        rctx->num_ap = 4;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == true);
    });
    TEST("4 APs misses cache with different 4 AP, 2 different", rctx, {
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

        /* four different APs */
        rctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        rctx->beacon[1] = b;
        b.ap.mac[3] = 0x99;
        rctx->beacon[2] = b;
        b.ap.mac[3] = 0x88;
        rctx->beacon[3] = b;
        rctx->num_beacons = 4;
        rctx->num_ap = 4;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        rctx->session->cacheline[0].beacon[0].ap.mac[3] = 0x77;
        rctx->session->cacheline[0].beacon[1].ap.mac[3] = 0x66;
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("3 APs misses cache with 3 AP same, 1 extra ", rctx, {
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

        /* 3 different APs */
        rctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        rctx->beacon[1] = b;
        b.ap.mac[3] = 0x99;
        rctx->beacon[2] = b;
        rctx->num_beacons = 3;
        rctx->num_ap = 3;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        b.ap.mac[3] = 0x88;
        rctx->session->cacheline[0].beacon[3] = b;
        rctx->session->cacheline[0].num_beacons = 4;
        rctx->session->cacheline[0].num_ap = 4;
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("2 APs misses cache with 1 AP", rctx, {
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

        /* 2 different APs */
        rctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        rctx->beacon[1] = b;
        rctx->num_beacons = 2;
        rctx->num_ap = 2;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        b.ap.mac[3] = 0x88;
        rctx->session->cacheline[0].beacon[2] = b;
        rctx->session->cacheline[0].num_beacons = 3;
        rctx->session->cacheline[0].num_ap = 3;
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("2 APs + cell misses cache with 2 AP + different cell", rctx, {
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
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 0, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };

        /* 2 different APs */
        rctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        rctx->beacon[1] = b;
        rctx->beacon[2] = c;
        rctx->num_beacons = 3;
        rctx->num_ap = 2;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        c.cell.id2 = 47;
        rctx->session->cacheline[0].beacon[2] = c;
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("4 APs + cell misses cache with 4 AP + different cell", rctx, {
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
        Beacon_t c = { .cell.h = { BEACON_MAGIC, SKY_BEACON_LTE, 1, -30, 0, 1 },
            .cell.id1 = 441,
            .cell.id2 = 53,
            .cell.id3 = 24674,
            .cell.id4 = 202274050,
            .cell.id5 = 21,
            .cell.freq = 5901,
            .cell.ta = 2 };

        /* 2 different APs */
        rctx->beacon[0] = b;
        b.ap.mac[3] = 0xaa;
        rctx->beacon[1] = b;
        rctx->beacon[2] = c;
        rctx->num_beacons = 3;
        rctx->num_ap = 2;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        c.cell.id2 = 47;
        rctx->session->cacheline[0].beacon[2] = c;
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
    TEST("cell matches cache with cell", rctx, {
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

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == true);
    });
    TEST("cell misses cache with different cell", rctx, {
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

        rctx->beacon[0] = c;
        rctx->num_beacons = 1;
        rctx->num_ap = 0;
        loc.time = rctx->header.time;

        sky_plugin_add_to_cache(rctx, &sky_errno, &loc);
        c.cell.id2 = 47;
        rctx->session->cacheline[0].beacon[0] = c;
        sky_search_cache(rctx, &sky_errno, NULL, &loc);
        ASSERT(IS_CACHE_HIT(rctx) == false);
    });
}

BEGIN_TESTS(libel_test)

GROUP_CALL("sky open", test_sky_open);
GROUP_CALL("sky new request", test_sky_new_request);
GROUP_CALL("sky add tests", test_sky_add);
GROUP_CALL("sky option tests", test_sky_option);
GROUP_CALL("sky match tests", test_cache_match);
GROUP_CALL("sky gnss tests", test_sky_gnss);

END_TESTS();
