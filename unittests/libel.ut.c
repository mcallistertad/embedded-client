TEST_FUNC(test_sky_add)
{
    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_TIME with bad timestamp", ctx, {
            Sky_errno_t sky_errno;
            uint8_t mac[] = { 0x28, 0x3B, 0x82, 0x64, 0xE0, 0x8B };
            int16_t rssi = -30;
            int32_t freq = 3660;
            bool connected = false;

            ASSERT(SKY_ERROR ==
                   sky_add_ap_beacon(ctx, &sky_errno, mac, (time_t)666, rssi, freq, connected));
            ASSERT(SKY_ERROR_BAD_TIME == sky_errno);
        });

    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad mac", ctx, {
            Sky_errno_t sky_errno;
            uint8_t mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
            int16_t rssi = -30;
            int32_t freq = 3660;
            bool connected = false;

            ASSERT(SKY_ERROR ==
                   sky_add_ap_beacon(ctx, &sky_errno, mac, time(NULL), rssi, freq, connected));
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST("should return SKY_SUCCESS and set age to 0 with bad timestamp", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;

        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(ctx->beacon[0].h.age == TIME_UNAVAILABLE && ctx->header.time != TIME_UNAVAILABLE);
    });
    TEST("should return SKY_SUCCESS and set last_config to zero first time", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, time(NULL), rssi, freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno));
        ASSERT(ctx->state->config.last_config_time == TIME_UNAVAILABLE);
    });
    TEST("should return SKY_SUCCESS and set last_config to timestamp second time", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ctx->state->config.last_config_time = ctx->header.time;
        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, time(NULL), rssi, freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno));
        ASSERT(ctx->state->config.last_config_time != TIME_UNAVAILABLE);
    });
    TEST("should return SKY_SUCCESS and set last_config to zero with bad timestamp", ctx, {
        Sky_errno_t sky_errno;
        uint8_t mac[] = { 0x4C, 0x5E, 0x0C, 0xB0, 0x17, 0x4B };
        int16_t rssi = -30;
        int32_t freq = 3660;
        bool connected = false;
        uint32_t buf_size;

        ASSERT(SKY_SUCCESS ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, TIME_UNAVAILABLE, rssi, freq, connected));
        ASSERT(sky_sizeof_request_buf(ctx, &buf_size, &sky_errno));
        ASSERT(ctx->state->config.last_config_time == TIME_UNAVAILABLE);
    });
}

BEGIN_TESTS(libel_test)

GROUP_CALL("sky add tests", test_sky_add);

END_TESTS();
