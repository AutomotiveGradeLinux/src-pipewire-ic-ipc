/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "private.h"
#include "protocol.h"
#include "sender.h"
#include "client.h"

#define BUFFER_SIZE 1024

static void on_lost_connection(
                struct icipc_sender *self,
                int receiver_fd,
                void *data) {
        icipc_log_warn("client: lost connection with server %d", receiver_fd);
}

/* API */

struct icipc_client *icipc_client_new(const char *path, bool connect) {
        struct icipc_sender *base;
        base = icipc_sender_new(path, BUFFER_SIZE, on_lost_connection, NULL, 0);

        if (connect)
                icipc_sender_connect(base);

        return (struct icipc_client *)base;
}

void icipc_client_free(struct icipc_client *self) {
        struct icipc_sender *base = icipc_client_to_sender(self);
        icipc_sender_free(base);
}

bool icipc_client_send_request(
                struct icipc_client *self,
                const char *name,
                const struct spa_pod *args,
                icipc_sender_reply_func_t reply,
                void *data) {
        struct icipc_sender *base = icipc_client_to_sender(self);

        /* check params */
        if (name == NULL)
                return false;

        const size_t size = icipc_protocol_calculate_request_size(name, args);
        uint8_t buffer[size];
        icipc_protocol_build_request(buffer, size, name, args);
        return icipc_sender_send(base, buffer, size, reply, data);
}

const struct spa_pod *icipc_client_send_request_finish(
                struct icipc_sender *self,
                const uint8_t * buffer,
                size_t size,
                const char **error) {
        /* error */
        if (icipc_protocol_is_reply_error(buffer, size)) {
                icipc_protocol_parse_reply_error(buffer, size, error);
                return NULL;
        }

        /* ok */
        if (icipc_protocol_is_reply_ok(buffer, size)) {
                const struct spa_pod *value = NULL;
                if (icipc_protocol_parse_reply_ok(buffer, size, &value))
                        return value;
        }

        return NULL;
}
