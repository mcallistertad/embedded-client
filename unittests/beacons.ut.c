TEST_FUNC(test_validate_workspace)
{
    /* check the following:
        ctx == NULL
        ctx->len < 0 || ctx->ap_len < 0
        ctx->len > TOTAL_BEACONS + 1
        ctx->ap_len > MAX_AP_BEACONS + 1
        ctx->connected > TOTAL_BEACONS + 1
        ctx->header.crc32 == sky_crc32(...)
        ctx->beacon[i].h.magic != BEACON_MAGIC || ctx->beacon[i].h.type > SKY_BEACON_MAX
        */
    TEST("should return false with NULL ctx", ctx, { ASSERT(false == validate_workspace(NULL)); });

    TEST("should return true with default ctx", ctx, { ASSERT(true == validate_workspace(ctx)); });

    TEST("should return false with too small length in ctx", ctx, {
        ctx->len = -34;
        ASSERT(false == validate_workspace(ctx));
    });

    TEST("should return false with too big length in ctx", ctx, {
        ctx->len = 1234;
        ASSERT(false == validate_workspace(ctx));
    });

    TEST("should return false with too small AP length in ctx", ctx, {
        ctx->ap_len = -3;
        ASSERT(false == validate_workspace(ctx));
    });

    TEST("should return false with bad comnected info in ctx", ctx, {
        ctx->connected = 1234;
        ASSERT(false == validate_workspace(ctx));
    });

    TEST("should return false with bad crc in ctx", ctx, {
        ctx->header.crc32 = 1234;
        ASSERT(false == validate_workspace(ctx));
    });

    TEST("should return false with corrupt beacon in ctx (magic)", ctx, {
        ctx->beacon[0].h.magic = 1234;
        ASSERT(false == validate_workspace(ctx));
    });

    TEST("should return false with corrupt beacon in ctx (type)", ctx, {
        ctx->beacon[0].h.type = 1234;
        ASSERT(false == validate_workspace(ctx));
    });
}

TEST_FUNC(test_compare)
{
    GROUP("beacon_compare APs");
    TEST("should return true when 2 identical APs are passed", ctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCDD", 10, -108, 4433, true);
        ASSERT(true == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return false when 2 different APs are passed", ctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCFD", 10, -108, 4433, true);
        ASSERT(false == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return false and calc RSSI diff with different APs", ctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCDE", 10, -78, 4433, true);
        int diff;
        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == a.h.rssi - b.h.rssi);
    });

    TEST("should return false and calc vg diff with different APs", ctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCDE", 10, -108, 4433, true);
        a.ap.vg_len = 2;
        b.ap.vg_len = 6;

        int diff;
        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == a.ap.vg_len - b.ap.vg_len);
    });

    GROUP("beacon_compare Cells identical");
    TEST("should return true and with 2 identical NR cell beacons", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        NR(b, 10, -98, true, 213, 142, 15614, 25564526, 287, 1040);

        ASSERT(true == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return true and with 2 identical LTE cell beacons", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);

        ASSERT(true == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return true and with 2 identical UMTS cell beacons", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        UMTS(b, 10, -98, true, 515, 2, 32768, 16843545, 0, 0);

        ASSERT(true == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return true and with 2 identical NBIOT cell beacons", ctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        NBIOT(b, 10, -98, true, 515, 2, 20263, 15664525, 25583, 255);

        ASSERT(true == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return true and with 2 identical CDMA cell beacons", ctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        CDMA(b, 10, -98, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(true == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return true and with 2 identical GSM cell beacons", ctx, {
        GSM(a, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
        GSM(b, 10, -98, true, 515, 2, 20263, 22265, 0, 0);

        ASSERT(true == beacon_compare(ctx, &a, &b, NULL));
    });

    TEST("should return false and calc diff with one connected with different cells", ctx, {
        LTE(a, 10, -108, true, 110, 485, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, false, 311, 480, 25614, 25664526, 387, 1000);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == 1); // 1st better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -1); // 2nd better
    });

    GROUP("beacon_compare NMR Cells");
    TEST("should return false and calc diff with one NMR with same cell type", ctx, {
        LTE(a, 10, -108, true, 110, 485, 25614, 25664526, 387, 1000);
        LTE_NMR(b, 10, -108, false, 387, 1000);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == 1); // 1st better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -1); // 2nd better
    });

    TEST("should return false and calc diff with two NMR one younger", ctx, {
        LTE_NMR(a, 10, -108, false, 387, 1000);
        LTE_NMR(b, 8, -10, false, 38, 100);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -2); // 2nd better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == 2); // 1st better
    });

    TEST("should return false and calc diff with two NMR one stronger", ctx, {
        LTE_NMR(a, 10, -108, false, 387, 1000);
        LTE_NMR(b, 10, -10, false, 38, 100);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -98); // 2nd better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == 98); // 1st better
    });

    TEST("should return false and report 1st best with two very similar cells", ctx, {
        LTE(a, 10, -108, true, 110, 485, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, true, 110, 222, 25614, 25664526, 45, 1000);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == 1); // 1st better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == 1); // 1st better
    });

    TEST("should return false and report 1st best with two NMR very similar", ctx, {
        LTE_NMR(a, 10, -108, false, 387, 1000);
        LTE_NMR(b, 10, -108, false, 38, 100);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == 1); // 1st better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == 1); // 1st better
    });

    TEST("should return false and calc diff with one NMR with different cell type", ctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        LTE_NMR(b, 10, -108, false, 387, 1000);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == 1); // 1st better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -1); // 2nd better
    });

    GROUP("beacon_compare Cells different");
    TEST("should return false and calc diff: NR better than LTE", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        LTE(b, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_NR - SKY_BEACON_LTE));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_LTE - SKY_BEACON_NR));
    });

    TEST("should return false and calc diff: NR better than UMTS", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_NR - SKY_BEACON_UMTS));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_UMTS - SKY_BEACON_NR));
    });

    TEST("should return false and calc diff: NR better than NBIOT", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_NR - SKY_BEACON_NBIOT));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_NBIOT - SKY_BEACON_NR));
    });

    TEST("should return false and calc diff: NR better than CDMA", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_NR - SKY_BEACON_CDMA));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_CDMA - SKY_BEACON_NR));
    });

    TEST("should return false and calc diff: NR better than GSM", ctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 287, 1040);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_NR - SKY_BEACON_GSM));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_GSM - SKY_BEACON_NR));
    });

    TEST("should return false and calc diff: LTE better than UMTS", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_LTE - SKY_BEACON_UMTS));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_UMTS - SKY_BEACON_LTE));
    });

    TEST("should return false and calc diff: LTE better than NBIOT", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_LTE - SKY_BEACON_NBIOT));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_NBIOT - SKY_BEACON_LTE));
    });

    TEST("should return false and calc diff: LTE better than CDMA", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_LTE - SKY_BEACON_CDMA));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_CDMA - SKY_BEACON_LTE));
    });

    TEST("should return false and calc diff: LTE better than GSM", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_LTE - SKY_BEACON_GSM));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_GSM - SKY_BEACON_LTE));
    });

    TEST("should return false and calc diff: UMTS better than NBIOT", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_UMTS - SKY_BEACON_NBIOT));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_NBIOT - SKY_BEACON_UMTS));
    });

    TEST("should return false and calc diff: UMTS better than CDMA", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_UMTS - SKY_BEACON_CDMA));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_CDMA - SKY_BEACON_UMTS));
    });

    TEST("should return false and calc diff: UMTS better than GSM", ctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 0, 0);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_UMTS - SKY_BEACON_GSM));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_GSM - SKY_BEACON_UMTS));
    });

    TEST("should return false and calc diff: NBIOT better than CDMA", ctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_NBIOT - SKY_BEACON_CDMA));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_CDMA - SKY_BEACON_NBIOT));
    });

    TEST("should return false and calc diff: NBIOT better than GSM", ctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 25583, 255);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_NBIOT - SKY_BEACON_GSM));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_GSM - SKY_BEACON_NBIOT));
    });

    TEST("should return false and calc diff: CDMA better than GSM", ctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 0, 0);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == -(SKY_BEACON_CDMA - SKY_BEACON_GSM));
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -(SKY_BEACON_GSM - SKY_BEACON_CDMA));
    });

    GROUP("beacon_compare Cells same type");
    TEST("should return false and calc diff: one connected", ctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 123, -94, false, 310, 470, 25613, 25664526, 387, 1000);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == 1); // 1st better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -1); // 2nd better
    });

    TEST("should return false and calc diff: one NMR", ctx, {
        LTE(a, 123, -94, false, 310, 470, 25613, 25664526, 387, 1000);
        LTE_NMR(b, 10, -108, false, 387, 1000);
        int diff;

        ASSERT(false == beacon_compare(ctx, &a, &b, &diff));
        ASSERT(diff == 1); // 1st better
        ASSERT(false == beacon_compare(ctx, &b, &a, &diff));
        ASSERT(diff == -1); // 2nd better
    });
}

TEST_FUNC(test_insert)
{
    // sanity checks
    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with NULL ctx", ctx,
        {
            BEACON(a, SKY_BEACON_MAX, 1605549363, -108, true);
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == insert_beacon(NULL, &sky_errno, &a, 0));
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });

    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with corrupt ctx",
        ctx, {
            BEACON(a, SKY_BEACON_MAX, 1605549363, -108, true);
            Sky_errno_t sky_errno;

            ctx->header.magic = 1234;
            ASSERT(SKY_ERROR == insert_beacon(ctx, &sky_errno, &a, 0));
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });

    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon",
        ctx, {
            AP(a, "ABCDEF010203", 1605633264, -108, 2, true);
            Sky_errno_t sky_errno;

            a.h.type = SKY_BEACON_MAX;
            ASSERT(SKY_ERROR == insert_beacon(ctx, &sky_errno, &a, NULL));
            ASSERT(ctx->len == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });

    TEST("should insert beacon in ctx->beacon[] at index 0 with NULL index", ctx, {
        AP(a, "ABCDEF010203", 1605633264, -108, 2, true);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a, NULL));
        ASSERT(AP_EQ(&a, ctx->beacon));
    });

    TEST("should insert beacon in ctx->beacon[] at index 0", ctx, {
        AP(a, "ABCDEF010203", 1605633264, -108, 2, true);
        Sky_errno_t sky_errno;
        int i;

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a, &i));
        ASSERT(AP_EQ(&a, ctx->beacon));
        ASSERT(i == 0);
    });

    TEST("should insert 2 beacons in ctx->beacon[] and set index", ctx, {
        AP(a, "ABCDEF010203", 1605633264, -108, 2, true);
        AP(b, "ABCDEF010201", 1605633264, -108, 2, true);
        Sky_errno_t sky_errno;

        int insert_idx;
        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &a, &insert_idx));
        ASSERT(AP_EQ(&a, ctx->beacon));
        ASSERT(insert_idx == 0);

        ASSERT(SKY_SUCCESS == insert_beacon(ctx, &sky_errno, &b, &insert_idx));
        ASSERT(AP_EQ(&b, ctx->beacon));
        ASSERT(insert_idx == 0);

        ASSERT(NUM_BEACONS(ctx) == 2);
        ASSERT(AP_EQ(&a, ctx->beacon + 1));
    });
}

BEGIN_TESTS(beacon_test)

GROUP_CALL("validate_workspace", test_validate_workspace);
GROUP_CALL("beacon_compare", test_compare);
GROUP_CALL("beacon_insert", test_insert);

END_TESTS();
