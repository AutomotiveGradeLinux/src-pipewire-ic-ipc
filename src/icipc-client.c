/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>
#include <icipc.h>

typedef struct ClientData {
        struct icipc_client *c;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        bool reply_received;
} ClientData;

static void reply_handler(
                struct icipc_sender *self,
                const uint8_t * buffer,
                size_t size,
                void *p) {
        ClientData *data = p;
        const char *error = NULL;

        if (buffer) {
                const struct icipc_data *data =
                    icipc_client_send_request_finish(self, buffer, size, &error);
                if (!data)
                        printf("error: %s\n", error ? error : "unknown");
                else
                        printf("success!\n");
        } else {
                printf("error: lost connection with server\n");
        }

        /* signal reply received */
        pthread_mutex_lock(&data->mutex);
        data->reply_received = true;
        pthread_cond_signal(&data->cond);
        pthread_mutex_unlock(&data->mutex);
}

int main(int argc, char *argv[]) {
        ClientData data;
        char str[LINE_MAX];

        if (argc < 2) {
                printf("usage: <server-path>\n");
                return -1;
        }

        /* init */
        data.c = icipc_client_new(argv[1], true);
        pthread_mutex_init(&data.mutex, NULL);
        pthread_cond_init(&data.cond, NULL);
        data.reply_received = false;

        while (true) {
                memset(str, 0, sizeof(str));
                printf("> ");
                fgets(str, sizeof(str), stdin);

                if (strncmp(str, "quit", 4) == 0) {
                        printf("exiting...\n");
                        break;
                } else if (strncmp(str, "send ", 5) == 0) {
                        const char *request_name = str + 5;
                        if (*request_name == '\0')
                                continue;

                        /* send request */
                        icipc_client_send_request(data.c, request_name,
                                                  NULL, reply_handler,
                                                  &data);

                        /* wait for reply */
                        pthread_mutex_lock(&data.mutex);
                        while (!data.reply_received)
                                pthread_cond_wait(&data.cond, &data.mutex);
                        pthread_mutex_unlock(&data.mutex);
                } else {
                        printf("help\tprints this message\n");
                        printf("quit\texits the client\n");
                        printf("send\tsends a request, usage: send [SUSPEND|RESUME]\n");
                }
        }

        /* clean up */
        pthread_cond_destroy(&data.cond);
        pthread_mutex_destroy(&data.mutex);
        icipc_client_free(data.c);
        return 0;
}
