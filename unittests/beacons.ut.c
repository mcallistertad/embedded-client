TEST_FUNC(test_validate_request_ctx)
{
    /* check the following:
        ctx == NULL
        ctx->num_beacons < 0 || ctx->num_ap < 0
        ctx->num_beacons > TOTAL_BEACONS + 1
        ctx->num_ap > MAX_AP_BEACONS + 1
        ctx->header.crc32 == sky_crc32(...)
        ctx->beacon[i].h.magic != BEACON_MAGIC || ctx->beacon[i].h.type > SKY_BEACON_MAX
        */
    TEST(
        "should return false with NULL ctx", ctx, { ASSERT(false == validate_request_ctx(NULL)); });

    TEST(
        "should return true with default ctx", ctx, { ASSERT(true == validate_request_ctx(ctx)); });

    TEST("should return false with too small length in ctx", ctx, {
        ctx->num_beacons = -34;
        ASSERT(false == validate_request_ctx(ctx));
    });

    TEST("should return false with too big length in ctx", ctx, {
        ctx->num_beacons = 1234;
        ASSERT(false == validate_request_ctx(ctx));
    });

    TEST("should return false with too small AP length in ctx", ctx, {
        ctx->num_ap = -3;
        ASSERT(false == validate_request_ctx(ctx));
    });

    TEST("should return false with bad crc in ctx", ctx, {
        ctx->header.crc32 = 1234;
        ASSERT(false == validate_request_ctx(ctx));
    });

    TEST("should return false with corrupt beacon in ctx (magic)", ctx, {
        ctx->beacon[0].h.magic = 1234;
        ASSERT(false == validate_request_ctx(ctx));
    });

    TEST("should return false with corrupt beacon in ctx (type)", ctx, {
        ctx->beacon[0].h.type = 1234;
        ASSERT(false == validate_request_ctx(ctx));
    });
}

TEST_FUNC(test_compare)
{
    GROUP("is_beacon_better APs");
    TEST("should return positive when 2 identical APs are passed", ctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCDD", 10, -108, 4433, true);
        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
    });

    TEST("should return != 0 when 2 different APs are passed", ctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCFD", 10, -108, 4433, true);
        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
        ASSERT(is_beacon_better(ctx, &b, &a) < 0);
    });

    TEST("should return != 0 and calc RSSI diff with different APs", ctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCDE", 10, -78, 4433, true);
        ASSERT(is_beacon_better(ctx, &a, &b) < 0); /* B is better */
        ASSERT(is_beacon_better(ctx, &b, &a) > 0); /* A(first) is better */
    });

    GROUP("is_beacon_better Cells identical");
    TEST("should return positive with 2 identical NR cell beacons", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        NR(b, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);

        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical LTE cell beacons", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);

        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical UMTS cell beacons", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);

        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical NBIOT cell beacons", ctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);

        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical CDMA cell beacons", ctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical GSM cell beacons", ctx, {
        GSM(a, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);

        ASSERT(is_beacon_better(ctx, &a, &b) > 0);
    });

    GROUP("is_beacon_better Cells");
    TEST("should return a better with one connected with different cells", ctx, {
        LTE(a, 10, -108, true, 210, 485, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, false, 311, 480, 25614, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return a better with one NMR with same cell type", ctx, {
        LTE(a, 10, -108, true, 110, 485, 25614, 25664526, 387, 1000);
        LTE_NMR(b, 10, -108, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return a better with two NMR one younger", ctx, {
        LTE_NMR(a, 8, -10, 38, 100);
        LTE_NMR(b, 10, -108, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return a better diff with two NMR one stronger", ctx, {
        LTE_NMR(a, 10, -10, 38, 100);
        LTE_NMR(b, 10, -108, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should report 1st better with two very similar cells", ctx, {
        LTE(a, 10, -108, true, 110, 485, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, true, 110, 222, 25614, 25664526, 45, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&b, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) > 0);
    });

    TEST("should report 1st best with two NMR very similar", ctx, {
        LTE_NMR(a, 10, -108, 387, 1000);
        LTE_NMR(b, 10, -108, 38, 100);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&b, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) > 0);
    });

    TEST("should return a better with one NMR with different cell type", ctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        LTE_NMR(b, 10, -108, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    GROUP("is_beacon_better Cells different");
    TEST("should return false and calc diff: NR better than LTE", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        LTE(b, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: NR better than UMTS", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: NR better than NBIOT", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: NR better than CDMA", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: NR better than GSM", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: LTE better than UMTS", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: LTE better than NBIOT", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: LTE better than CDMA", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: LTE better than GSM", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: UMTS better than NBIOT", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: UMTS better than CDMA", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: UMTS better than GSM", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: NBIOT better than CDMA", ctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: NBIOT better than GSM", ctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: CDMA better than GSM", ctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    GROUP("is_beacon_better Cells same type");
    TEST("should return false and calc diff: one connected", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 123, -94, false, 310, 470, 25613, 25664526, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });

    TEST("should return false and calc diff: one NMR", ctx, {
        LTE(a, 10, -94, false, 310, 470, 25613, 25664526, 387, 1000);
        LTE_NMR(b, 10, -108, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, NULL, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(is_beacon_better(ctx, &ctx->beacon[0], &ctx->beacon[1]) > 0);
        ASSERT(is_beacon_better(ctx, &ctx->beacon[1], &ctx->beacon[0]) < 0);
    });
}

TEST_FUNC(test_insert)
{
    GROUP("insert_beacon error cases");
    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with NULL ctx", ctx,
        {
            BEACON(a, SKY_BEACON_MAX, 1605549363, -108, true);
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == insert_beacon(NULL, &sky_errno, &a));
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });

    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with corrupt ctx",
        ctx, {
            BEACON(a, SKY_BEACON_MAX, 5, -108, true);
            Sky_errno_t sky_errno;

            ctx->header.magic = 1234;
            ASSERT(SKY_ERROR == insert_beacon(ctx, &sky_errno, &a));
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });

    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon",
        ctx, {
            AP(a, "ABCDEF010203", 1, -108, 5745, true);
            Sky_errno_t sky_errno;

            a.h.type = SKY_BEACON_MAX;
            ASSERT(SKY_ERROR == insert_beacon(ctx, &sky_errno, &a));
            ASSERT(ctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });

    GROUP("insert_beacon rssi order");
    TEST("should insert AP in ctx->beacon[] at index 0", ctx, {
        AP(a, "ABCDEF010203", 1, -108, 5745, true);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon));
    });

    TEST("should insert 3 APs A, B, C in ctx in rssi order B, A, C", ctx, {
        AP(a, "ABCDEF010203", 1, -88, 2412, false);
        AP(b, "ABCDEF010201", 1, -48, 2412, false);
        AP(c, "CBADEF010201", 1, -108, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs C, A, B in ctx in rssi order B, A, C", ctx, {
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs A, C, B in ctx in rssi order B, A, C", ctx, {
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs C, B, A in ctx in rssi order B, A, C", ctx, {
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs B, A, C in ctx in rssi order B, A, C", ctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A in ctx in rssi order B, A, C", ctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with C connected, in rssi order B, A, C", ctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, true);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with C in_cache, in rssi order B, A, C", ctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon + 1));
        ctx->beacon[1].ap.property.in_cache = true; /* mark C in_cache */

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with C younger, in rssi order B, A, C", ctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 1, -108, 2412, false); /* youngest */
        AP(a, "ABCDEF010203", 2, -88, 2412, true); /* connected */
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&c, ctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with only MAC diff, in mac order C, A, B", ctx, {
        AP(b, "ABCDEF010F01", 2, -48, 2412, false);
        AP(c, "ABCDEF010201", 2, -48, 2412, false);
        AP(a, "ABCDEF010401", 2, -48, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(AP_EQ(&c, ctx->beacon));
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
        ASSERT(AP_EQ(&b, ctx->beacon + 2));
    });

    TEST("should insert 3 Cells B, C, A, with only age diff, in priority order C, A, B", ctx, {
        LTE(b, 12, -94, false, 311, 470, 25613, 25664526, 387, 1000);
        LTE(c, 10, -94, false, 312, 470, 25613, 25664526, 387, 1000);
        LTE(a, 11, -94, false, 310, 470, 25613, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(CELL_EQ(&c, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(ctx) == 3);
        ASSERT(CELL_EQ(&c, ctx->beacon));
        ASSERT(CELL_EQ(&a, ctx->beacon + 1));
        ASSERT(CELL_EQ(&b, ctx->beacon + 2));
    });

    GROUP("insert_beacon duplicate handling");

    TEST("should insert 1 for duplicate APs age diff ignore rssi", ctx, {
        AP(a, "ABCDEF010203", 5, -98, 2412, false);
        /* better, younger but weaker */
        AP(b, "ABCDEF010203", 3, -108, 2412, false);
        /* worse older and weaker */
        AP(c, "ABCDEF010203", 7, -118, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(ctx) == 1);
        ASSERT(AP_EQ(&b, ctx->beacon));
    });

    TEST("should insert 1 for duplicate APs age diff ignore connected", ctx, {
        AP(a, "ABCDEF010203", 5, -108, 2412, false);
        /* better, younger and stronger */
        AP(b, "ABCDEF010203", 3, -98, 2412, false);
        /* worse older and stronger */
        AP(c, "ABCDEF010203", 7, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(ctx) == 1);
        ASSERT(AP_EQ(&b, ctx->beacon));
    });

    TEST("should insert 1 for duplicate APs A, B, C with rssi and connected diff", ctx, {
        AP(a, "ABCDEF010203", 1, -108, 2412, false);
        /* rssi better, connected */
        AP(b, "ABCDEF010203", 1, -98, 2412, true);
        /* rssi worse not connected */
        AP(c, "ABCDEF010203", 1, -102, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, ctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(ctx) == 1);
        ASSERT(AP_EQ(&b, ctx->beacon));
    });

    TEST("should insert 1 for duplicate Cells B, C, A, with only age diff", ctx, {
        LTE(a, 10, -94, false, 311, 470, 25613, 25664526, 387, 1000);
        /* worse, age */
        LTE(b, 12, -94, false, 311, 470, 25613, 25664526, 387, 1000);
        /* better connected */
        LTE(c, 10, -94, true, 311, 470, 25613, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(NUM_BEACONS(ctx) == 1);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&a, ctx->beacon));
        ASSERT(NUM_BEACONS(ctx) == 1);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(ctx) == 1);
        ASSERT(CELL_EQ(&c, ctx->beacon));
    });
}

BEGIN_TESTS(beacon_test)

GROUP_CALL("validate_request_ctx", test_validate_request_ctx);
GROUP_CALL("is_beacon_better", test_compare);
GROUP_CALL("beacon_insert", test_insert);

END_TESTS();
