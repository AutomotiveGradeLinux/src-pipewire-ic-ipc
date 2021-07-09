/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "test.h"
#include "data.h"
#include <icipc.h>

typedef struct DataInt {
        struct icipc_data hdr;
        int value;
} DataInt;

static void test_icipc_protocol() {
        uint8_t b[1024];

        /* request null value */
        {
                const char *name = NULL;
                const struct icipc_data *value = NULL;

                icipc_protocol_build_request(b, sizeof(b), "name", NULL);
                test_bool_true(icipc_protocol_parse_request
                               (b, sizeof(b), &name, &value));
                test_str_eq(name, "name");
                test_cmpint(value->type, ==, (uint32_t) DATA_TYPE_NONE);
        }

        /* request */
        {
                DataInt i = {
                        .hdr.size = sizeof(int),
                        .hdr.type = DATA_TYPE_INT,
                        .value = 8,
                };
                const char *name = NULL;
                const struct icipc_data *value = NULL;

                icipc_protocol_build_request(b, sizeof(b), "name",
                                             (struct icipc_data *)&i);
                test_bool_true(icipc_protocol_parse_request
                               (b, sizeof(b), &name,
                                (const struct icipc_data **)&value));
                test_str_eq(name, "name");
                test_cmpint(ICIPC_DATA_BODY_SIZE(value), ==,
                            ROUND_UP_TO_ALIGN(sizeof(int)));
                test_cmpint(value->type, ==, (uint32_t) DATA_TYPE_INT);
                test_cmpint(*((const int*)ICIPC_DATA_BODY_CONST(value)), ==, 8);
        }

        /* reply error */
        {
                icipc_protocol_build_reply_error(b, sizeof(b), "error message");
                test_bool_true(icipc_protocol_is_reply_error(b, sizeof(b)));
                const char *msg = NULL;
                test_bool_true(icipc_protocol_parse_reply_error
                               (b, sizeof(b), &msg));
                test_str_eq(msg, "error message");
        }

        /* reply ok null value */
        {
                const struct icipc_data *value = NULL;

                icipc_protocol_build_reply_ok(b, sizeof(b), NULL);
                test_bool_true(icipc_protocol_is_reply_ok(b, sizeof(b)));
                test_bool_true(icipc_protocol_parse_reply_ok
                               (b, sizeof(b), &value));
                test_ptr_notnull(value);
                test_cmpint(value->type, ==, (uint32_t) DATA_TYPE_NONE);
        }

        /* reply ok */
        {
                DataInt i = {
                        .hdr.size = sizeof(int),
                        .hdr.type = DATA_TYPE_INT,
                        .value = 3,
                };
                const struct icipc_data *value = NULL;

                icipc_protocol_build_reply_ok(b, sizeof(b),
                                              (struct icipc_data *)&i);
                test_bool_true(icipc_protocol_is_reply_ok(b, sizeof(b)));
                test_bool_true(icipc_protocol_parse_reply_ok
                               (b, sizeof(b), (const struct icipc_data **)&value));
                test_cmpint(ICIPC_DATA_BODY_SIZE(value), ==,
                            ROUND_UP_TO_ALIGN(sizeof(int)));
                test_cmpint(value->type, ==, (uint32_t) DATA_TYPE_INT);
                test_cmpint(*((const int*)ICIPC_DATA_BODY_CONST(value)), ==, 3);
        }
}

int main(int argc, char *argv[]) {
        test_icipc_protocol();
        return TEST_PASS;
}
