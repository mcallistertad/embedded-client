//static void test_compare(Sky_ctx_t
TEST_FUNC(test_compare) {
    MOCK_SKY_CTX(ctx);

    TEST("should return true when 2 identical beacons are passed", {
        AP(a, "ABCDEFAACCDD", 1234, -108, 4433, true);
        AP(b, "ABCDEFAACCDD", 1234, -108, 4433, true);
        ASSERT( true == beacon_compare(ctx, &a, &b, NULL) );
        ASSERT( false );
    });

    TEST("should return false when 2 different beacons are passed", {
        AP(a, "ABCDEFAACCDD", 1234, -108, 4433, true);
        AP(b, "ABCDEFAACCFD", 1234, -108, 4433, true);
        ASSERT( false == beacon_compare(ctx, &a, &b, NULL) );
    });

    TEST("should return false and calc diff when 2 different beacon types are passed", {
        BEACON(a, SKY_BEACON_AP, 1234, -108, true);
        BEACON(b, SKY_BEACON_LTE, 1234, -108, true);
        int diff;

        ASSERT( false == beacon_compare(ctx, &a, &b, &diff) );
        ASSERT( diff == -(SKY_BEACON_AP - SKY_BEACON_LTE) );
    });

    TEST("should return false and calc RSSI diff with comparable beacons", {
        AP(a, "ABCDEFAACCDD", 1234, -108, 4433, true);
        AP(b, "ABCDEFAACCDE", 1234, -78, 4433, true);
        int diff;
        ASSERT( false == beacon_compare(ctx, &a, &b, &diff) );
        ASSERT( diff == a.h.rssi - b.h.rssi );
    });

    TEST("should return false and calc vg diff with comparable beacons", {
        AP(a, "ABCDEFAACCDD", 1234, -108, 4433, true);
        AP(b, "ABCDEFAACCDE", 1234, -108, 4433, true);
        a.ap.vg_len = 1;
        b.ap.vg_len = 2;

        int diff;
        ASSERT( false == beacon_compare(ctx, &a, &b, &diff) );
        ASSERT( diff == a.ap.vg_len - b.ap.vg_len );
    });

    TEST("should return true and with 2 identical cell beacons", {
        BEACON(a, SKY_BEACON_LTE, 1234, -108, false);
        BEACON(b, SKY_BEACON_LTE, 1234, -108, true);

        ASSERT( true == beacon_compare(ctx, &a, &b, NULL) );
    });

    TEST("should return false and calc diff with 2 comparable cell beacons with different connected states", {
        BEACON(a, SKY_BEACON_LTE, 1234, -108, false);
        BEACON(b, SKY_BEACON_GSM, 1234, -108, true);

        int diff;
        ASSERT( false == beacon_compare(ctx, &a, &b, &diff) );
        ASSERT( diff == -1 );
    });

    CLOSE_SKY_CTX(ctx);
}

TEST_FUNC(test_insert) {

    // sanity checks
    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS", {
        BEACON(a, SKY_BEACON_MAX, 1605549363, -108, true);
        Sky_errno_t sky_errno;

        ASSERT( SKY_ERROR != insert_beacon(NULL, &sky_errno, &a, 0 ) );
        ASSERT( SKY_ERROR_BAD_PARAMETERS == sky_errno );

    });

    AP(a, "ABCDEF010203", 1605633264, -108, 2, true);
    AP(b, "ABCDEF010201", 1605633264, -108, 2, true);
    TEST_DEF("should insert beacon in ctx->beacon[] at index 0", {
        MOCK_SKY_CTX(ctx);
        EXE({
            Sky_errno_t sky_errno;

            ASSERT( SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a, 0) );
            ASSERT( AP_EQ(&a, ctx->beacon) );
        });
        CLOSE_SKY_CTX(ctx);
    });

    TEST_WITH_CTX("should insert 2 beacons in ctx->beacon[] and set index", ctx, {
        Sky_errno_t sky_errno;

        int insert_idx;
        ASSERT( SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a, &insert_idx) );
        ASSERT( AP_EQ(&a, ctx->beacon) );
        ASSERT( insert_idx == 0 );

        ASSERT( SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b, &insert_idx) );
        ASSERT( AP_EQ(&b, ctx->beacon) );
        ASSERT( insert_idx == 0 );

        ASSERT( NUM_BEACONS(ctx) == 2 );
        ASSERT( AP_EQ(&a, ctx->beacon+1) );
    });
}

BEGIN_TESTS(beacon_test)

    GROUP_CALL("beacon_compare", test_compare);
    GROUP_CALL("beacon_insert", test_insert);

END_TESTS();
