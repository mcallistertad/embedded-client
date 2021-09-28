TEST_FUNC(test_validate_request_ctx)
{
    /* check the following:
        rctx == NULL
        rctx->num_beacons < 0 || rctx->num_ap < 0
        rctx->num_beacons > TOTAL_BEACONS + 1
        rctx->num_ap > MAX_AP_BEACONS + 1
        rctx->header.crc32 == sky_crc32(...)
        rctx->beacon[i].h.magic != BEACON_MAGIC || rctx->beacon[i].h.type > SKY_BEACON_MAX
        */
    TEST("should return false with NULL rctx", rctx,
        { ASSERT(false == validate_request_ctx(NULL)); });

    TEST("should return true with default rctx", rctx,
        { ASSERT(true == validate_request_ctx(rctx)); });

    TEST("should return false with too small length in rctx", rctx, {
        rctx->num_beacons = -34;
        ASSERT(false == validate_request_ctx(rctx));
    });

    TEST("should return false with too big length in rctx", rctx, {
        rctx->num_beacons = 1234;
        ASSERT(false == validate_request_ctx(rctx));
    });

    TEST("should return false with too small AP length in rctx", rctx, {
        rctx->num_ap = -3;
        ASSERT(false == validate_request_ctx(rctx));
    });

    TEST("should return false with bad crc in rctx", rctx, {
        rctx->header.crc32 = 1234;
        ASSERT(false == validate_request_ctx(rctx));
    });

    TEST("should return false with corrupt beacon in rctx (magic)", rctx, {
        rctx->beacon[0].h.magic = 1234;
        ASSERT(false == validate_request_ctx(rctx));
    });

    TEST("should return false with corrupt beacon in rctx (type)", rctx, {
        rctx->beacon[0].h.type = 1234;
        ASSERT(false == validate_request_ctx(rctx));
    });
}

TEST_FUNC(test_beacon_order)
{
    GROUP("is_beacon_first APs");
    TEST("should return positive when 2 identical APs are passed", rctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCDD", 10, -108, 4433, true);
        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
    });

    TEST("should return != 0 when 2 different APs are passed", rctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCFD", 10, -108, 4433, true);
        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
        ASSERT(is_beacon_first(rctx, &b, &a) < 0);
    });

    TEST("should return != 0 and order by RSSI with different APs", rctx, {
        AP(a, "ABCDEFAACCDD", 10, -108, 4433, true);
        AP(b, "ABCDEFAACCDE", 10, -78, 4433, true);
        ASSERT(is_beacon_first(rctx, &a, &b) < 0); /* B is better */
        ASSERT(is_beacon_first(rctx, &b, &a) > 0); /* A(first) is better */
    });

    GROUP("is_beacon_first Cells identical");
    TEST("should return positive with 2 identical NR cell beacons", rctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 95, 1040);
        NR(b, 10, -108, true, 213, 142, 15614, 25564526, 95, 1040);

        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical LTE cell beacons", rctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);

        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical UMTS cell beacons", rctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 300, 415);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 300, 415);

        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical NBIOT cell beacons", rctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 283, 255);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 283, 255);

        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical CDMA cell beacons", rctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
    });

    TEST("should return positive with 2 identical GSM cell beacons", rctx, {
        GSM(a, 10, -108, true, 515, 2, 20263, 22265, SKY_UNKNOWN_ID5, SKY_UNKNOWN_ID6);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, SKY_UNKNOWN_ID5, SKY_UNKNOWN_ID6);

        ASSERT(is_beacon_first(rctx, &a, &b) > 0);
    });

    GROUP("is_beacon_first Cells");
    TEST("should return A better with stronger similar cell", rctx, {
        LTE(a, 10, -108, false, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 10, -85, false, 210, 485, 25614, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better NR over LTE", rctx, {
        LTE(a, 10, -108, false, 210, 485, 25614, 25664526, 387, 1000);
        NR(b, 10, -108, false, 213, 142, 15614, 25564526, 95, 1040);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better newer LTE over NR", rctx, {
        NR(a, 10, -108, false, 213, 142, 15614, 25564526, 95, 1040);
        LTE(b, 4, -108, false, 210, 485, 25614, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better connected LTE over newer NR", rctx, {
        NR(a, 4, -108, false, 213, 142, 15614, 25564526, 95, 1040);
        LTE(b, 10, -108, true, 210, 485, 25614, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better with one connected with different cells", rctx, {
        LTE(a, 10, -108, false, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, true, 210, 485, 25614, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better with one connected with older cells", rctx, {
        LTE(a, 10, -108, false, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 15, -108, true, 210, 485, 25614, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better with one NMR with same cell type", rctx, {
        LTE_NMR(b, 10, -108, 387, 1000);
        LTE(a, 10, -108, true, 310, 485, 25614, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better with two NMR one younger", rctx, {
        LTE_NMR(a, 8, -40, 38, 100);
        LTE_NMR(b, 10, -108, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should return A better diff with two NMR one stronger", rctx, {
        LTE_NMR(a, 10, -40, 38, 100);
        LTE_NMR(b, 10, -108, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("should report 1st better with two very similar cells", rctx, {
        LTE(a, 10, -108, true, 210, 485, 25614, 25664526, 387, 1000);
        LTE(b, 10, -108, true, 210, 222, 25614, 25664526, 45, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS ==
               insert_beacon(rctx, NULL, &b)); /* New cell inserted before if very similar */
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) > 0);
    });

    TEST("should report 1st best with two NMR very similar", rctx, {
        LTE_NMR(a, 10, -108, 387, 1000);
        LTE_NMR(b, 10, -108, 38, 100);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS ==
               insert_beacon(rctx, NULL, &b)); /* New NMR inserted before if very similar */
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) > 0);
    });

    TEST("should return A better with one NMR with different cell type", rctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        LTE_NMR(b, 10, -108, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    GROUP("is_beacon_first Cells different");
    TEST("is_beacon_first: NR better than LTE", rctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 95, 1040);
        LTE(b, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: NR better than UMTS", rctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 95, 1040);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 300, 415);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: NR better than NBIOT", rctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 95, 1040);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 283, 255);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: NR better than CDMA", rctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 95, 1040);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: NR better than GSM", rctx, {
        NR(a, 10, -108, true, 213, 142, 15614, 25564526, 95, 1040);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 63, 1023);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: LTE better than UMTS", rctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        UMTS(b, 10, -108, true, 515, 2, 32768, 16843545, 300, 415);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: LTE better than NBIOT", rctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 283, 255);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: LTE better than CDMA", rctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: LTE better than GSM", rctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 63, 1023);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: UMTS better than NBIOT", rctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 300, 415);
        NBIOT(b, 10, -108, true, 515, 2, 20263, 15664525, 283, 255);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: UMTS better than CDMA", rctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 300, 415);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: UMTS better than GSM", rctx, {
        UMTS(a, 10, -108, true, 515, 2, 32768, 16843545, 300, 415);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 63, 1023);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: NBIOT better than CDMA", rctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 283, 255);
        CDMA(b, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: NBIOT better than GSM", rctx, {
        NBIOT(a, 10, -108, true, 515, 2, 20263, 15664525, 283, 255);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 63, 1023);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: CDMA better than GSM", rctx, {
        CDMA(a, 10, -108, true, 5000, 16683, 25614, 22265, 0, 0);
        GSM(b, 10, -108, true, 515, 2, 20263, 22265, 63, 1023);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    GROUP("is_beacon_first Cells same type");
    TEST("is_beacon_first: one connected", rctx, {
        LTE(a, 10, -108, true, 311, 480, 25614, 25664526, 387, 1000);
        LTE(b, 123, -94, false, 310, 470, 25613, 25664526, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });

    TEST("is_beacon_first: one NMR", rctx, {
        LTE(a, 10, -94, false, 310, 470, 25613, 25664526, 387, 1000);
        LTE_NMR(b, 10, -108, 387, 1000);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &a));
        ASSERT(SKY_SUCCESS == insert_beacon(rctx, NULL, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(is_beacon_first(rctx, &rctx->beacon[0], &rctx->beacon[1]) > 0);
        ASSERT(is_beacon_first(rctx, &rctx->beacon[1], &rctx->beacon[0]) < 0);
    });
}

TEST_FUNC(test_add)
{
    GROUP("add_beacon error cases");
    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with NULL rctx",
        rctx, {
            BEACON(a, SKY_BEACON_MAX, 1605549363, -108, true);
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == add_beacon(NULL, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(SKY_ERROR_BAD_REQUEST_CTX == sky_errno);
        });

    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with corrupt rctx",
        rctx, {
            BEACON(a, SKY_BEACON_MAX, 5, -108, true);
            Sky_errno_t sky_errno;

            rctx->header.magic = 1234;
            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(SKY_ERROR_BAD_REQUEST_CTX == sky_errno);
        });

    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon type",
        rctx, {
            AP(a, "ABCDEF010203", 1, -108, 5745, true);
            Sky_errno_t sky_errno;

            a.h.type = SKY_BEACON_MAX; /* bad type */
            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon AP",
        rctx, {
            AP(a, "FFFFFFFFFFFF", 1, -108, 5745, true); /* bad mac */
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon LTE",
        rctx, {
            LTE(a, 10, -94, false, 310, 470, 25613, 25664526, 687, 1000); /* bad PCI */
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon NB-IOT",
        rctx, {
            NBIOT(a, 10, -108, true, 115, 2, 20263, 15664525, 283, 255); /* bad mcc */
            Sky_errno_t sky_errno;

            a.h.type = SKY_BEACON_MAX;
            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon GSM",
        rctx, {
            GSM(a, 10, -108, true, 515, 2, 20263, 22265, SKY_UNKNOWN_ID5, SKY_UNKNOWN_ID6);
            Sky_errno_t sky_errno;

            a.cell.ta = 100; /* bad ta */
            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon UMTS",
        rctx, {
            UMTS(a, 10, -108, true, 515, 1111, 32768, 16843545, 300, 415); /* bad mnc */
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST(
        "should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon CDMA",
        rctx, {
            CDMA(a, 10, -108, true, 5000, 36683, 25614, 22265, 0, 0); /* bad sid */
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
    TEST("should return SKY_ERROR and set sky_errno to SKY_ERROR_BAD_PARAMETERS with bad beacon NR",
        rctx, {
            NR(a, 10, -108, true, 213, 142, 15614, 25564526, 95, 3279166); /* bad freq */
            Sky_errno_t sky_errno;

            ASSERT(SKY_ERROR == add_beacon(rctx, &sky_errno, &a, TIME_UNAVAILABLE));
            ASSERT(rctx->num_beacons == 0);
            ASSERT(SKY_ERROR_BAD_PARAMETERS == sky_errno);
        });
}

TEST_FUNC(test_distance)
{
    GROUP("Verify distance calculation");
    TEST("48.940511,2.233437 to 48.957394,2.267373=3108.96405", rctx, {
        ASSERT(
            fabs(3108.96405f - distance_A_to_B(48.940511, 2.233437, 48.957394, 2.267373)) < 2.7f);
    });

    TEST("34.26486,-84.5302 to 34.264867,-84.5302 = 0.777078904", rctx, {
        ASSERT(
            fabs(0.777078904f - distance_A_to_B(34.26486, -84.5302, 34.264867, -84.5302)) < 2.7f);
    });

    TEST("34.26004,-84.519028 to 34.26503,-84.529953 = 1147.117852", rctx, {
        ASSERT(fabs(1147.117852f - distance_A_to_B(34.26004, -84.519028, 34.26503, -84.529953)) <
               2.7f);
    });
}

TEST_FUNC(test_insert)
{
    GROUP("insert_beacon rssi order");
    TEST("should insert AP in rctx->beacon[] at index 0", rctx, {
        AP(a, "ABCDEF010203", 1, -108, 5745, true);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon));
    });

    TEST("should insert 3 APs A, B, C in rctx in rssi order B, A, C", rctx, {
        AP(a, "ABCDEF010203", 1, -88, 2412, false);
        AP(b, "ABCDEF010201", 1, -48, 2412, false);
        AP(c, "CBADEF010201", 1, -108, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs C, A, B in rctx in rssi order B, A, C", rctx, {
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs A, C, B in rctx in rssi order B, A, C", rctx, {
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs C, B, A in rctx in rssi order B, A, C", rctx, {
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs B, A, C in rctx in rssi order B, A, C", rctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A in rctx in rssi order B, A, C", rctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with C connected, in rssi order B, A, C", rctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, true);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with C in_cache, in rssi order B, A, C", rctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 2, -108, 2412, false);
        AP(a, "ABCDEF010203", 2, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon + 1));
        rctx->beacon[1].ap.property.in_cache = true; /* mark C in_cache */

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with C younger, in rssi order B, A, C", rctx, {
        AP(b, "ABCDEF010201", 2, -48, 2412, false);
        AP(c, "CBADEF010201", 1, -108, 2412, false); /* youngest */
        AP(a, "ABCDEF010203", 2, -88, 2412, true); /* connected */
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon + 1));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&b, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&c, rctx->beacon + 2));
    });

    TEST("should insert 3 APs B, C, A, with only MAC diff, in mac order C, A, B", rctx, {
        AP(b, "ABCDEF010F01", 2, -48, 2412, false);
        AP(c, "ABCDEF010201", 2, -48, 2412, false);
        AP(a, "ABCDEF010401", 2, -48, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(AP_EQ(&c, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(AP_EQ(&c, rctx->beacon));
        ASSERT(AP_EQ(&a, rctx->beacon + 1));
        ASSERT(AP_EQ(&b, rctx->beacon + 2));
    });

    TEST("should insert 3 Cells B, C, A, with only age diff, in priority order C, A, B", rctx, {
        LTE(b, 12, -94, false, 311, 470, 25613, 25664526, 387, 1000);
        LTE(c, 10, -94, false, 312, 470, 25613, 25664526, 387, 1000);
        LTE(a, 11, -94, false, 310, 470, 25613, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&b, &rctx->beacon[0]));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(CELL_EQ(&c, &rctx->beacon[0]));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(NUM_BEACONS(rctx) == 3);
        ASSERT(CELL_EQ(&c, &rctx->beacon[0]));
        ASSERT(CELL_EQ(&a, &rctx->beacon[1]));
        ASSERT(CELL_EQ(&b, &rctx->beacon[2]));
    });

    GROUP("insert_beacon duplicate handling");

    TEST("should insert 1 for duplicate APs age diff ignore rssi", rctx, {
        AP(a, "ABCDEF010203", 5, -98, 2412, false);
        /* better, younger but weaker */
        AP(b, "ABCDEF010203", 3, -108, 2412, false);
        /* worse older and weaker */
        AP(c, "ABCDEF010203", 7, -118, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(rctx) == 1);
        ASSERT(AP_EQ(&b, rctx->beacon));
    });

    TEST("should insert 1 for duplicate APs age diff ignore connected", rctx, {
        AP(a, "ABCDEF010203", 5, -108, 2412, false);
        /* better, younger and stronger */
        AP(b, "ABCDEF010203", 3, -98, 2412, false);
        /* worse older and stronger */
        AP(c, "ABCDEF010203", 7, -88, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(rctx) == 1);
        ASSERT(AP_EQ(&b, rctx->beacon));
    });

    TEST("should insert 1 for duplicate APs A, B, C with rssi and connected diff", rctx, {
        AP(a, "ABCDEF010203", 1, -108, 2412, false);
        /* rssi better, connected */
        AP(b, "ABCDEF010203", 1, -98, 2412, true);
        /* rssi worse not connected */
        AP(c, "ABCDEF010203", 1, -102, 2412, false);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(AP_EQ(&a, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(AP_EQ(&b, rctx->beacon));

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(rctx) == 1);
        ASSERT(AP_EQ(&b, rctx->beacon));
    });

    TEST("should insert 1 for duplicate Cells B, C, A, with only age diff", rctx, {
        LTE(a, 10, -94, false, 311, 470, 25613, 25664526, 387, 1000);
        /* worse, age */
        LTE(b, 12, -94, false, 311, 470, 25613, 25664526, 387, 1000);
        /* better connected */
        LTE(c, 10, -94, true, 311, 470, 25613, 25664526, 387, 1000);
        Sky_errno_t sky_errno;

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &a));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(NUM_BEACONS(rctx) == 1);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &b));
        ASSERT(CELL_EQ(&a, &rctx->beacon[0]));
        ASSERT(NUM_BEACONS(rctx) == 1);

        ASSERT(SKY_SUCCESS == insert_beacon(rctx, &sky_errno, &c));
        ASSERT(NUM_BEACONS(rctx) == 1);
        ASSERT(CELL_EQ(&c, &rctx->beacon[0]));
    });
}

BEGIN_TESTS(beacon_test)

GROUP_CALL("validate_request_ctx", test_validate_request_ctx);
GROUP_CALL("is_beacon_first", test_beacon_order);
GROUP_CALL("beacon_add", test_add);
GROUP_CALL("beacon_insert", test_insert);
GROUP_CALL("distance_A_to_B", test_distance);

END_TESTS();
