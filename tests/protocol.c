/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "test.h"
#include <spa/pod/builder.h>
#include <spa/pod/parser.h>
#include <icipc.h>

static void test_icipc_protocol() {
        uint8_t b[1024];

        /* request null value */
        {
                icipc_protocol_build_request(b, sizeof(b), "name", NULL);
                const char *name = NULL;
                const struct spa_pod *value = NULL;
                test_bool_true(icipc_protocol_parse_request
                               (b, sizeof(b), &name, &value));
                test_str_eq(name, "name");
                test_bool_true(spa_pod_is_none(value));
        }

        /* request */
        {
                struct spa_pod_int i = SPA_POD_INIT_Int(8);
                icipc_protocol_build_request(b, sizeof(b), "name",
                                             (struct spa_pod *)&i);
                const char *name = NULL;
                const struct spa_pod_int *value = NULL;
                test_bool_true(icipc_protocol_parse_request
                               (b, sizeof(b), &name,
                                (const struct spa_pod **)&value));
                test_str_eq(name, "name");
                test_cmpint(value->value, ==, 8);
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
                icipc_protocol_build_reply_ok(b, sizeof(b), NULL);
                test_bool_true(icipc_protocol_is_reply_ok(b, sizeof(b)));
                const struct spa_pod *value = NULL;
                test_bool_true(icipc_protocol_parse_reply_ok
                               (b, sizeof(b), &value));
                test_ptr_notnull(value);
                test_bool_true(spa_pod_is_none(value));
        }

        /* reply ok */
        {
                struct spa_pod_int i = SPA_POD_INIT_Int(3);
                icipc_protocol_build_reply_ok(b, sizeof(b),
                                              (struct spa_pod *)&i);
                test_bool_true(icipc_protocol_is_reply_ok(b, sizeof(b)));
                const struct spa_pod_int *value = NULL;
                test_bool_true(icipc_protocol_parse_reply_ok
                               (b, sizeof(b), (const struct spa_pod **)&value));
                test_cmpint(value->value, ==, 3);
        }
}

int main(int argc, char *argv[]) {
        test_icipc_protocol();
        return TEST_PASS;
}
