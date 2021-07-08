/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE
#include "test.h"
#include <spa/pod/builder.h>
#include <spa/pod/parser.h>
#include <icipc.h>
#include <unistd.h>
#include <pthread.h>

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
                const struct spa_pod *args,
                void *data) {
        int32_t val = 0;
        test_bool_true(spa_pod_is_int(args));
        test_bool_true(spa_pod_get_int(args, &val) == 0);
        struct spa_pod_int res = SPA_POD_INIT_Int(val + 1);
        return icipc_server_reply_ok(self, client_fd, (struct spa_pod *)&res);
}

static bool error_request_handler(
                struct icipc_server *self,
                int client_fd,
                const char *name,
                const struct spa_pod *args,
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

        const struct spa_pod *pod =
            icipc_client_send_request_finish(self, buffer, size, &data->error);
        if (pod) {
                test_bool_true(spa_pod_is_int(pod));
                test_bool_true(spa_pod_get_int(pod, &data->incremented) == 0);
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
        struct spa_pod_int i = SPA_POD_INIT_Int(3);
        test_bool_true(icipc_client_send_request
                       (c, "INCREMENT", (struct spa_pod *)&i, reply_handler,
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
