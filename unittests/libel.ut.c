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
        Sky_state_t *pstate, nv_state;

        ASSERT(SKY_SUCCESS == sky_close(&sky_errno, (void **)&pstate));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 666,
                                  (uint8_t *)"0123456789012345", "sku", 0, NULL,
                                  SKY_LOG_LEVEL_DEBUG, sky_logf, sky_rand_bytes, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_NONE);

        ASSERT(SKY_SUCCESS == sky_close(&sky_errno, (void **)&pstate));
        ASSERT(sky_sizeof_state(pstate) == sizeof(nv_state));
        memmove(&nv_state, pstate, sizeof(nv_state));
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 666,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, sky_logf, sky_rand_bytes, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_NONE);
        ASSERT(SKY_SUCCESS == sky_open(&sky_errno, (uint8_t *)"ABCDEF", 6, 666,
                                  (uint8_t *)"0123456789012345", "sku", 0, &nv_state,
                                  SKY_LOG_LEVEL_DEBUG, sky_logf, sky_rand_bytes, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_NONE);

        ASSERT(SKY_ERROR == sky_open(&sky_errno, (uint8_t *)"ABCDEFGH", 8, 666,
                                (uint8_t *)"01234567890123", "sk", 0, &nv_state,
                                SKY_LOG_LEVEL_DEBUG, sky_logf, sky_rand_bytes, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_ALREADY_OPEN);

#ifdef SHOULD_BE_ERROR
        ASSERT(SKY_ERROR == sky_open(&sky_errno, (uint8_t *)"ABCDEFGH", 8, 666,
                                (uint8_t *)"01234567890123", "sk", 0, NULL, SKY_LOG_LEVEL_DEBUG,
                                sky_logf, sky_rand_bytes, good_time, true));
        ASSERT(sky_errno == SKY_ERROR_ALREADY_OPEN);
#endif
    });
}

TEST_FUNC(test_sky_new_request)
{
    TEST(
        "sky_new_request set errno to SKY_ERROR_SERVICE_DENIED after first failed registration with bad time",
        ctx, {
            Sky_errno_t sky_errno;

            sky_time = bad_time;
            ctx->state->sky_sku[0] = 's';
            ctx->state->sky_sku[1] = '\0';
            ctx->state->backoff = SKY_AUTH_NEEDS_TIME;
            ASSERT(NULL == sky_new_request(ctx, sizeof(Sky_ctx_t), NULL, 0, &sky_errno));
            ASSERT(sky_errno == SKY_ERROR_SERVICE_DENIED);
        });
    TEST("sky_new_request succeeds after first failed registration with good time", ctx, {
        Sky_errno_t sky_errno;

        sky_time = good_time;
        ctx->state->sky_sku[0] = 's';
        ctx->state->sky_sku[1] = '\0';
        ctx->state->backoff = SKY_AUTH_NEEDS_TIME;
        ASSERT(ctx == sky_new_request(ctx, sizeof(Sky_ctx_t), NULL, 0, &sky_errno));
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

        ASSERT(SKY_ERROR ==
               sky_add_ap_beacon(ctx, &sky_errno, mac, time(NULL), rssi, freq, connected));
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
        ASSERT(ctx->beacon[0].h.age == TIME_UNAVAILABLE && ctx->header.time != TIME_UNAVAILABLE);
    });
    TEST("sky_add_ap_beacon set last_config to zero first time", ctx, {
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
    TEST("sky_add_ap_beacon set last_config to timestamp second time", ctx, {
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
    TEST("sky_add_ap_beacon set last_config to zero with bad timestamp", ctx, {
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

GROUP_CALL("sky open", test_sky_open);
GROUP_CALL("sky new request", test_sky_new_request);
GROUP_CALL("sky add tests", test_sky_add);

END_TESTS();
