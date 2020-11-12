#ifndef UNITTEST_H
#define UNITTEST_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SKY_LIBEL
#include "libel.h"

#define UNITTESTS

#define TEST_DEVICE_ID "123456123456112233445566"
#define TEST_PARTNER_ID 2
#define TEST_KEY "000102030405060708090a0b0c0d0e0f"

// ANSI Colors
#define ESC "\033"
#define BRIGHT ESC "[1;37m"
#define GREEN ESC "[0;32m"
#define RED ESC "[0;31m"
#define RESET ESC "[0m"

// equivalent to basename(__FILE__)
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* Test entry point. Should be paired with END_TESTS() in use */
#define BEGIN_TESTS(N) static Test_rs _ ## N ## _test_main(Test_opts *opts);\
    Test_rs (*N)(Test_opts *opts) = _ ## N ## _test_main;\
    static Test_rs _ ## N ## _test_main(Test_opts *opts) {\
        Test_ctx __ctx;\
        Test_ctx *_ctx = &__ctx;\
        _test_init(_ctx, opts, #N);

// TODO: This uses "compound literal" operator, a C99 feature.
//       Probably should just do things the old fashioned way...
#define END_TESTS() return (Test_rs) { __ctx.ran, __ctx.failed }; }

#define TEST(S) _test_set_desc(_ctx, (S))

/* Define function to be used with TEST_CALL
 * Usage: TEST_FUNC(test_function_name) {
 *            // test code
 *        }
 */
#define TEST_FUNC(N) void N(Test_ctx *_ctx)

#define TEST_CALL(S, F) _test_set_desc(_ctx, (S)); F(_ctx);

#define GROUP(S) _test_set_group(_ctx, (S)) 

//#define ASSERT(X) _test_assert(_ctx, __FILE__, __LINE__, (X))
#define ASSERT(X) { int _line = __LINE__;\
    pid_t _p = fork();\
    if (_p == -1) {\
        perror("fork failed");\
        exit(-1);\
    } else if (_p == 0) {\
        exit((X) == false);\
    }\
    int _status, _res;\
    if (waitpid(_p, &_status, 0) == -1 || !WIFEXITED(_status))\
        _res = 0;\
    else\
        _res = !WEXITSTATUS(_status);\
    _test_assert(_ctx, __FILE__, _line, _res);\
}

#define EXTERN_TEST(N) extern Test_rs (*N)(Test_opts *)

#define RUN_TEST(N) {\
    EXTERN_TEST(N);\
    Test_rs _rs = N(&opts);\
    rs.ran += _rs.ran;\
    rs.failed += _rs.failed;\
    }

#define PRINT_RESULT(X) _test_print_rs((X))

#define MOCK_SKY_LOG_CTX(N) Sky_ctx_t ctx = { .logf = _test_log }

#define MOCK_SKY_CTX(N) Sky_ctx_t *N = _test_sky_ctx()

#define CLOSE_SKY_CTX(C) Sky_errno_t err;\
    if (sky_close(&err, NULL) != SKY_SUCCESS) {\
        fprintf(stderr, "error closing mock sky context\n");\
        exit(-1);\
    }\
    free(C);

#define BEACON(N, TYPE, TIME, RSSI, CON) Beacon_t N;\
    _test_beacon(&(N), (TYPE), (TIME), (RSSI), (CON));

#define AP(N, MAC, TIME, RSSI, FREQ, CON) Beacon_t N;\
    _test_ap(&(N), (MAC), (TIME), (RSSI), (FREQ), (CON));

#define CELL(N, TIME, RSSI, CON) Beacon_t N;\
    _test_cell(&(N), (TIME), (RSSI), (CON));

typedef struct {
    int verbose;
} Test_opts;

typedef struct {
    const Test_opts *opts;
    const char *name;
    const char *file;
    const char *group;
    const char *desc;

    unsigned ran;
    unsigned failed;
} Test_ctx;

typedef struct {
    unsigned ran; // number of tests ran
    unsigned failed; // number of tests failed
} Test_rs;

void _test_init(Test_ctx *ctx, Test_opts *opts, const char *str);
void _test_set_group(Test_ctx *ctx, const char *str);
void _test_set_desc(Test_ctx *ctx, const char *str);
void _test_assert(Test_ctx *ctx, const char *file, int line, int res);
void _test_print_rs(Test_opts *opts, Test_rs rs);
int _test_log(Sky_log_level_t level, char *s);
Sky_ctx_t *_test_sky_ctx();
int _test_beacon(Beacon_t *b, Sky_beacon_type_t type, time_t timestamp, int16_t, bool is_connected);
int _test_ap(Beacon_t *b, const char *mac,
//int _test_cell(Beacon_t *b, time_t timestamp, int16_t rssi, int32_t frequency, bool is_connected);
time_t timestamp, int16_t rssi, int32_t frequency, bool is_connected);

/*static void _test_init(Test_ctx *ctx, Test_opts *opts, const char *str) {
    ctx->opts = opts;
    ctx->name = str;
    ctx->file = __FILENAME__;
    ctx->group = NULL;
    ctx->desc = NULL;

    ctx->ran = 0;
    ctx->failed = 0;
}

static void _test_set_group(Test_ctx *ctx, const char *str) {
    ctx->group = str;
}

static void _test_set_desc(Test_ctx *ctx, const char *str) {
    ctx->desc = str;
}

static void _test_assert(Test_ctx *ctx, int line, int res) {
    ctx->ran++;
    if (!res) ctx->failed++;

    if (!res || ctx->opts->verbose) {
        fprintf(stderr, BRIGHT "%s" RESET ":%s:%d [ %s" RESET " ] %s\n",
            ctx->name, ctx->group, line,
            res ? GREEN "PASS" : RED "FAIL",
            ctx->desc);
    }
}

static void _test_print_rs(Test_opts *opts, Test_rs rs) {
    if (rs.failed || opts->verbose) {
        fprintf(stderr, "%d Tests, %d Failures\n", rs.ran, rs.failed);
    }
}

static int _test_log(Sky_log_level_t level, char *s) {
    fprintf(stderr, " >>> %s\n", s);
    return 0;
}*/
#endif
