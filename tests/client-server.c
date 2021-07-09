/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE
#include "test.h"
#include "data.h"
#include <icipc.h>
#include <unistd.h>
#include <pthread.h>

typedef struct DataInt {
        struct icipc_data hdr;
        int value;
} DataInt;

static inline char *new_address() {
        char *address = NULL;
        (void)asprintf(&address, "icipc-test-%d-%d", getpid(), rand());
        test_ptr_notnull(address);
        return address;
}

static bool increment_request_handler(
                struct icipc_server *self,
                int client_fd,
                const char *name,
                const struct icipc_data *args,
                void *data) {
        int32_t val = 0;
        test_cmpint(args->type, ==, (uint32_t) DATA_TYPE_INT);
        test_cmpint(ICIPC_DATA_BODY_SIZE(args), ==,
                    ROUND_UP_TO_ALIGN(sizeof(int)));
        val = *((const int*)ICIPC_DATA_BODY_CONST(args));

        DataInt res = {
                .hdr.size = sizeof(int),
                .hdr.type = DATA_TYPE_INT,
                .value = val + 1,
        };
        return icipc_server_reply_ok(self, client_fd, (struct icipc_data *)&res);
}

static bool error_request_handler(
                struct icipc_server *self,
                int client_fd,
                const char *name,
                const struct icipc_data *args,
                void *data) {
        return icipc_server_reply_error(self, client_fd, "error message");
}

typedef struct ReplyData {
        int32_t incremented;
        const char *error;
        int n_replies;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
} ReplyData;

static void wait_for_reply(ReplyData *data, int n_replies) {
        pthread_mutex_lock(&data->mutex);
        while (data->n_replies < n_replies)
                pthread_cond_wait(&data->cond, &data->mutex);
        pthread_mutex_unlock(&data->mutex);
}

static void reply_handler(
                struct icipc_sender *self,
                const uint8_t * buffer,
                size_t size,
                void *p) {
        ReplyData *data = p;
        test_ptr_notnull(data);

        pthread_mutex_lock(&data->mutex);

        const struct icipc_data *args =
            icipc_client_send_request_finish(self, buffer, size, &data->error);
        if (args) {
                test_cmpint(args->type, ==, (uint32_t) DATA_TYPE_INT);
                test_cmpint(ICIPC_DATA_BODY_SIZE(args), ==,
                            ROUND_UP_TO_ALIGN(sizeof(int)));
                data->incremented = *((const int*)ICIPC_DATA_BODY_CONST(args));
        }
        data->n_replies++;
        pthread_cond_signal(&data->cond);

        pthread_mutex_unlock(&data->mutex);
}

static void test_icipc_server_client() {
        char *address = new_address();
        struct icipc_server *s = icipc_server_new(address, true);
        test_ptr_notnull(s);
        struct icipc_client *c = icipc_client_new(address, true);
        test_ptr_notnull(c);
        ReplyData data;
        pthread_mutex_init(&data.mutex, NULL);
        pthread_cond_init(&data.cond, NULL);

        /* add request handlers */
        test_bool_true(icipc_server_set_request_handler
                       (s, "INCREMENT", increment_request_handler, NULL));
        test_bool_true(icipc_server_set_request_handler
                       (s, "ERROR", error_request_handler, NULL));

        /* send an INCREMENT request of 3, and make sure the returned value is 4 */
        data.incremented = -1;
        data.error = NULL;
        data.n_replies = 0;
        DataInt i = {
                .hdr.size = sizeof(int),
                .hdr.type = DATA_TYPE_INT,
                .value = 3,
        };
        test_bool_true(icipc_client_send_request
                       (c, "INCREMENT", (struct icipc_data *)&i, reply_handler,
                        &data));
        wait_for_reply(&data, 1);
        test_ptr_null(data.error);
        test_cmpint(data.incremented, ==, 4);

        /* send an ERROR request, and make sure the returned value is an error */
        data.error = NULL;
        data.n_replies = 0;
        test_bool_true(icipc_client_send_request
                       (c, "ERROR", NULL, reply_handler, &data));
        wait_for_reply(&data, 1);
        test_str_eq(data.error, "error message");

        /* send an unhandled request, and make sure the server replies with an error */
        data.error = NULL;
        data.n_replies = 0;
        test_bool_true(icipc_client_send_request
                       (c, "UNHANDLED-REQUEST", NULL, reply_handler, &data));
        wait_for_reply(&data, 1);
        test_str_eq(data.error, "request handler not found");

        /* clean up */
        pthread_cond_destroy(&data.cond);
        pthread_mutex_destroy(&data.mutex);
        icipc_client_free(c);
        icipc_server_free(s);
        free(address);
}

int main(int argc, char *argv[]) {
        test_icipc_server_client();
        return TEST_PASS;
}
