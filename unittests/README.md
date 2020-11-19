# EC Unit Tests

## Development

### Initializing

Unit test code may be placed either inline with the source being tested, or in
an external file included from said source code. In either case, this code
should be added to the end of the file whose source is being tested:

```C
#ifdef UNITTESTS

/* in case of external tests */
#include "source.ut.c"

/* in case of inline */
BEGIN_TESTS(inline_tests)
    /* test code... */
END_TESTS();

#endif
```

For external tests, the convention is to add `.ut.c` as the extension, but it
could be any filename within the `unittests/` directory.

Each source file with tests has a single entry point defined by
`BEGIN_TESTS(name)`. `name` is referenced and executed in `runtests.c` via:
```C
RUN_TEST(name)
```

At the top of `libel/runtests.c`:
```C
/**************************
 *     ADD TESTS HERE     *
 **************************/
static Test_rs runtests() {
    Test_rs rs = { 0, 0 };

    /* START TEST LIST */
    RUN_TEST(beacon_test);
    RUN_TEST(ap_plugin_vap_used);
    RUN_TEST(test_utilities);
    RUN_TEST(plugin_test);
    /* END TEST LIST */

    return rs;
}
```

Add the named test to this list so it will be executed when the test binary is
run.

### Defining

As noted above, each test group is defined with the `BEGIN_TESTS(name)` and
`END_TESTS()` macros, and all test code is contained within these.

Before defining tests, set `GROUP` to something describing the section being
tested, such as the function name:
```C
GROUP("beacon_compare");
```

Each test begins with a `TEST_DEF()` call:
```C
TEST_DEF("test description", {
    /* pre-test initalization */
    MOCK_SKY_CTX(ctx);

    /* test code */
    EXE({
        ASSERT( validate_workspace(ctx) );
    });

    /* post-test cleanup */
    /* (will be executed regardless of test pass/fail!) */
    CLOSE_SKY_CTX(ctx);
});
```

Because the use-case of needing to setup and cleanup a `Sky_ctx_t` struct before
and after tests is common, a convenience macro `TEST()` has been defined:
```C
TEST("should create workspace and validate sucesfully", ctx, {
    ASSERT( validate_workspace(ctx) );
});
```

Which is equivalent to the above. The name of the `Sky_ctx_t` variable is
defined in the second argument.

### Writing

Within `EXE()`, execution only continues so long as each `ASSERT()` call passes.
If one fails it will immediately jump out of the test scope and report the
failure. If all asserts pass (or there are no asserts), the test is considered
successful.

Putting it all together (with an added test designed to demonstrate failure
logging):
```C
/* file: unittests/new_tests.ut.c */
BEGIN_TESTS(new_tests)
    
    GROUP("validate_workspace");
    TEST_DEF("create and validate workspace", {
        MOCK_SKY_CTX(ctx);

        EXE({
            ASSERT( validate_workspace(ctx) );
        });

        CLOSE_SKY_CTX(ctx);
    });

    TEST("do it again but more succintly", ctx, {
        ASSERT( validate_workspace(ctx) );
    });

    TEST("show test failure", ctx, {
        ctx->len = TOTAL_BEACONS + 2;
        ASSERT( validate_workspace(ctx) );
    });

END_TESTS();
```

## Building
```Bash
make unittest
```

> Note that plugin tests will be compiled according to the path set in
the environment variable `PLUGIN_DIR`.

## Running
```
$ ./bin/tests
 >>> Skyhook Embedded Library (Version: 2.0.0-129-g96b426a-dirty)
 >>> libel.c:sky_new_request() 1 cachelines present
 >>> libel.c:sky_new_request() cache: 0 of 1 - empty len:0 ap_len:0 time:0
 >>> libel.c:sky_new_request() Dump WorkSpace: Got 0 beacons, WiFi 0, connected -1
 >>> libel.c:sky_new_request() Config: Total:20 AP:15 VAP:4(12) Thresholds:50(All) 65(Used) 24(Age) 3(Beacon) -90(RSSI) Update:Pending
Running ASSERT() in utilities.c:1274
 >>> Skyhook Embedded Library (Version: 2.0.0-129-g96b426a-dirty)
 >>> libel.c:sky_new_request() 1 cachelines present
 >>> libel.c:sky_new_request() cache: 0 of 1 - empty len:0 ap_len:0 time:0
 >>> libel.c:sky_new_request() Dump WorkSpace: Got 0 beacons, WiFi 0, connected -1
 >>> libel.c:sky_new_request() Config: Total:20 AP:15 VAP:4(12) Thresholds:50(All) 65(Used) 24(Age) 3(Beacon) -90(RSSI) Update:Pending
Running ASSERT() in utilities.c:1283
 >>> Skyhook Embedded Library (Version: 2.0.0-129-g96b426a-dirty)
 >>> libel.c:sky_new_request() 1 cachelines present
 >>> libel.c:sky_new_request() cache: 0 of 1 - empty len:0 ap_len:0 time:0
 >>> libel.c:sky_new_request() Dump WorkSpace: Got 0 beacons, WiFi 0, connected -1
 >>> libel.c:sky_new_request() Config: Total:20 AP:15 VAP:4(12) Thresholds:50(All) 65(Used) 24(Age) 3(Beacon) -90(RSSI) Update:Pending
Running ASSERT() in utilities.c:1288
 >>> utilities.c:validate_workspace() Too many beacons
new_tests:validate_workspace:utilities.c:1286 [ FAIL ] show test failure
3 tests run, 1 failed
```

All logging by libel functions is sent to `stderr` prepended with
`>>>`. In addition the message:
```
Running ASSERT() in utilities.c:1283
```
is printed to `stderr` before each ASSERT specifying the exact line number which
the call is run on.

By default only failed tests are printed, to get the output of all tests
regardless of pass/fail pass the verbose flag. In addition we can pipe stderr to
`/dev/null` to get cleaner output:
```
./bin/tests -v 2>/dev/null
new_tests:validate_workspace:utilities.c:1273 [ PASS ] create and validate workspace
new_tests:validate_workspace:utilities.c:1282 [ PASS ] do it again but more succintly
new_tests:validate_workspace:utilities.c:1286 [ FAIL ] show test failure
```

## Helper Macros

These macros assist in creating different types of the `Beacon_t` struct and
comparing them:

```C
BEACON(name, Sky_beacon_type_t type, time_t timestamp, int16_t rssi, bool connected)
BEACON(a, SKY_BEACON_LTE, 1605633264, -108, true);

AP(name, const char *mac, time_t timestamp, int16_t rssi, int32_t frequency, bool connected)
AP(b, "ABCDEF010203", 1605633264, -108, 2, true);

CELL(name, time_t timestamp, int16_t rssi, bool connected)
CELL(c, 1605633264, -108, true);

BEACON_EQ(A, B) _test_beacon_eq((A), (B))
AP_EQ(A, B) _test_ap_eq((A), (B))

ASSERT( ! BEACON_EQ(a, b) )
```

These macros assist in breaking up large test groups into smaller sections:
```C
TEST_FUNC(group_a) {
    TEST("test 1")...
}

TEST_FUNC(group_b) {
    TEST("test 2")...
}

BEGIN_TESTS(tests)
    GROUP_CALL("group A", group_a);
    GROUP_CALL("group B", group_b);
END_TESTS();
```
