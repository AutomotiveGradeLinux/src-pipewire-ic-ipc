/* PipeWire
 *
 * Copyright © 2021 Red Hat, Inc.
 * Copyright © 2021 Collabora Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#ifdef __GNUC__
#define TEST_NORETURN __attribute__ ((noreturn))
#define TEST_LIKELY(x) (__builtin_expect(!!(x),1))
#else
#define TEST_NORETURN
#define TEST_LIKELY(x) (x)
#endif

enum {
        TEST_PASS = EXIT_SUCCESS,
        TEST_FAIL = EXIT_FAILURE,
        TEST_SKIP = 77,
};

#define test_log(...) fprintf(stderr, __VA_ARGS__)
#define test_vlog(format_, args_) vfprintf(stderr, format_, args_)

static inline bool test_priv_streq(const char *s1, const char *s2) {
        return TEST_LIKELY(s1 && s2) ? strcmp(s1, s2) == 0 : s1 == s2;
}

TEST_NORETURN
static inline void test_priv_fail(
                const char *file, int line, const char *func,
                const char *message) {
        test_log("FAILED: %s\n", message);
        test_log("in %s() (%s:%d)\n", func, file, line);
        exit(TEST_FAIL);
}

TEST_NORETURN
static inline void test_priv_fail_comparison_bool(
                const char *file, int line, const char *func,
                const char *operator, bool a, bool b,
                const char *astr, const char *bstr) {
        test_log("FAILED COMPARISON: %s %s %s\n", astr, operator, bstr);
        test_log("Resolved to: %s %s %s\n", a ? "true" : "false", operator,
                        b ? "true" : "false");
        test_log("in %s() (%s:%d)\n", func, file, line);
        exit(TEST_FAIL);
}

TEST_NORETURN
static inline void test_priv_fail_comparison_int(
                const char *file, int line, const char *func,
                const char *operator, int a, int b,
                const char *astr, const char *bstr) {
        test_log("FAILED COMPARISON: %s %s %s\n", astr, operator, bstr);
        test_log("Resolved to: %d %s %d\n", a, operator, b);
        test_log("in %s() (%s:%d)\n", func, file, line);
        exit(TEST_FAIL);
}

TEST_NORETURN
static inline void test_priv_fail_comparison_double(
                const char *file, int line, const char *func,
                const char *operator, double a, double b,
                const char *astr, const char *bstr) {
        test_log("FAILED COMPARISON: %s %s %s\n", astr, operator, bstr);
        test_log("Resolved to: %.3f %s %.3f\n", a, operator, b);
        test_log("in %s() (%s:%d)\n", func, file, line);
        exit(TEST_FAIL);
}

TEST_NORETURN
static inline void test_priv_fail_comparison_ptr(
                const char *file, int line, const char *func,
                const char *comparison) {
        test_log("FAILED COMPARISON: %s\n", comparison);
        test_log("in %s() (%s:%d)\n", func, file, line);
        exit(TEST_FAIL);
}

TEST_NORETURN
static inline void test_priv_fail_comparison_str(
                const char *file, int line, const char *func,
                const char *comparison, const char *a, const char *b) {
        test_log("FAILED COMPARISON: %s, expanded (\"%s\" vs \"%s\")\n",
                        comparison, a, b);
        test_log("in %s() (%s:%d)\n", func, file, line);
        exit(TEST_FAIL);
}

#define test_fail_if_reached() \
        test_priv_fail(__FILE__, __LINE__, __func__, \
                        "This line is supposed to be unreachable")

#define test_cmpbool(a_, op_, b_) \
        do { \
                bool _a = !!(a_); \
                bool _b = !!(b_); \
                if (!(_a op_ _b)) \
                        test_priv_fail_comparison_bool(__FILE__, __LINE__, __func__,\
                                        #op_, _a, _b, #a_, #b_); \
        } while(0)

#define test_bool_true(cond_) \
        test_cmpbool(cond_, ==, true)

#define test_bool_false(cond_) \
        test_cmpbool(cond_, ==, false)

#define test_cmpint(a_, op_, b_) \
        do { \
                __typeof__(a_) _a = a_; \
                __typeof__(b_) _b = b_; \
                if (trunc(_a) != _a || trunc(_b) != _b) \
                        test_priv_fail(__FILE__, __LINE__, __func__, \
                                "test_int_* used for non-integer value"); \
                if (!((_a) op_ (_b))) \
                        test_priv_fail_comparison_int(__FILE__, __LINE__, __func__,\
                                        #op_, _a, _b, #a_, #b_); \
        } while(0)

#define test_cmpptr(a_, op_, b_) \
        do { \
                __typeof__(a_) _a = a_; \
                __typeof__(b_) _b = b_; \
                if (!((_a) op_ (_b))) \
                        test_priv_fail_comparison_ptr(__FILE__, __LINE__, __func__,\
                                        #a_ " " #op_ " " #b_); \
        } while(0)

#define test_ptr_null(a_) \
        test_cmpptr(a_, ==, NULL)

#define test_ptr_notnull(a_) \
        test_cmpptr(a_, !=, NULL)

#define test_cmpdouble(a_, op_, b_) \
        do { \
                const double EPSILON = 1.0/256; \
                __typeof__(a_) _a = a_; \
                __typeof__(b_) _b = b_; \
                if (!((_a) op_ (_b)) && fabs((_a) - (_b)) > EPSILON)  \
                        test_priv_fail_comparison_double(__FILE__, __LINE__, __func__,\
                                        #op_, _a, _b, #a_, #b_); \
        } while(0)

#define test_str_eq(a_, b_) \
        do { \
                const char *_a = a_; \
                const char *_b = b_; \
                if (!test_priv_streq(_a, _b)) \
                        test_priv_fail_comparison_str(__FILE__, __LINE__, __func__, \
                                        #a_ " equals " #b_, _a, _b); \
        } while(0)

#define test_str_ne(a_, b_) \
        do { \
                const char *_a = a_; \
                const char *_b = b_; \
                if (test_priv_streq(_a, _b)) \
                        test_priv_fail_comparison_str(__FILE__, __LINE__, __func__, \
                                        #a_ " not equal to " #b_, _a, _b); \
        } while(0)
