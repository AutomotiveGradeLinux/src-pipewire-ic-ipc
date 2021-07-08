/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <pthread.h>

#include "private.h"
#include "protocol.h"
#include "receiver.h"
#include "server.h"

#define BUFFER_SIZE 1024
#define MAX_REQUEST_HANDLERS 128

typedef struct ClientHandler {
        icipc_server_client_handler_func_t handler;
        void *data;
} ClientHandler;

typedef struct RequestHandler {
        const char *name;
        icipc_server_request_handler_func_t handler;
        void *data;
} RequestHandler;

typedef struct ServerPriv {
        pthread_mutex_t mutex;
        ClientHandler client_handler;
        size_t n_request_handlers;
        RequestHandler request_handlers[MAX_REQUEST_HANDLERS];
} ServerPriv;

static void sender_state(
                struct icipc_receiver *base,
                int sender_fd,
                enum icipc_receiver_sender_state sender_state,
                void *data) {
        ServerPriv *priv = icipc_receiver_get_user_data(base);

        icipc_log_info("server: new state %d on client %d", sender_state,
                       sender_fd);

        pthread_mutex_lock(&priv->mutex);
        if (priv->client_handler.handler)
                priv->client_handler.handler((struct icipc_server *)base,
                                             sender_fd, sender_state,
                                             priv->client_handler.data);
        pthread_mutex_unlock(&priv->mutex);
}

static bool handle_message(
                struct icipc_receiver *base,
                int sender_fd,
                const uint8_t * buffer,
                size_t size,
                void *data) {
        ServerPriv *priv = icipc_receiver_get_user_data(base);
        const char *name = NULL;
        const struct spa_pod *args = NULL;

        icipc_log_info("server: message from client %d received", sender_fd);

        /* parse */
        if (!icipc_protocol_parse_request(buffer, size, &name, &args)) {
                const char *msg = "could not parse request";
                const size_t s = icipc_protocol_calculate_reply_error_size(msg);
                uint8_t b[s];
                icipc_protocol_build_reply_error(b, s, msg);
                return icipc_socket_write(sender_fd, b, s) == (ssize_t) s;
        }

        /* handle */
        size_t i;
        bool res = false;
        pthread_mutex_lock(&priv->mutex);

        for (i = 0; i < MAX_REQUEST_HANDLERS; i++) {
                RequestHandler *rh = priv->request_handlers + i;
                if (rh->name != NULL && strcmp(rh->name, name) == 0
                    && rh->handler != NULL) {
                        res =
                            rh->handler((struct icipc_server *)base, sender_fd,
                                        name, args, rh->data);
                        pthread_mutex_unlock(&priv->mutex);
                        return res;
                }
        }

        /* handler was not found, reply with error */
        res = icipc_server_reply_error((struct icipc_server *)base, sender_fd,
                                       "request handler not found");

        pthread_mutex_unlock(&priv->mutex);
        return res;
}

static struct icipc_receiver_events events = {
        .sender_state = sender_state,
        .handle_message = handle_message,
};

/* API */

struct icipc_server *icipc_server_new(const char *path, bool start) {
        ServerPriv *priv = NULL;
        struct icipc_receiver *base = NULL;

        base = icipc_receiver_new(path, BUFFER_SIZE, &events, NULL,
                                  sizeof(ServerPriv));
        if (base == NULL)
                return NULL;

        priv = icipc_receiver_get_user_data(base);
        pthread_mutex_init(&priv->mutex, NULL);
        priv->n_request_handlers = 0;

        if (start && !icipc_receiver_start(base)) {
                icipc_log_error("failed to start receiver");
                icipc_server_free((struct icipc_server *)base);
                return NULL;
        }

        return (struct icipc_server *)base;
}

void icipc_server_free(struct icipc_server *self) {
        struct icipc_receiver *base = icipc_server_to_receiver(self);
        ServerPriv *priv = icipc_receiver_get_user_data(base);

        pthread_mutex_destroy(&priv->mutex);

        icipc_receiver_free(base);
}

void icipc_server_set_client_handler(
                struct icipc_server *self,
                icipc_server_client_handler_func_t handler,
                void *data) {
        struct icipc_receiver *base = icipc_server_to_receiver(self);
        ServerPriv *priv = icipc_receiver_get_user_data(base);

        pthread_mutex_lock(&priv->mutex);
        priv->client_handler.handler = handler;
        priv->client_handler.data = data;
        pthread_mutex_unlock(&priv->mutex);
}

void icipc_server_clear_client_handler(struct icipc_server *self) {
        struct icipc_receiver *base = icipc_server_to_receiver(self);
        ServerPriv *priv = icipc_receiver_get_user_data(base);

        pthread_mutex_lock(&priv->mutex);
        priv->client_handler.handler = NULL;
        priv->client_handler.data = NULL;
        pthread_mutex_unlock(&priv->mutex);
}

bool icipc_server_set_request_handler(
                struct icipc_server *self,
                const char *name,
                icipc_server_request_handler_func_t handler,
                void *data) {
        struct icipc_receiver *base = icipc_server_to_receiver(self);
        ServerPriv *priv = icipc_receiver_get_user_data(base);
        size_t i;

        /* check params */
        if (name == NULL)
                return false;

        pthread_mutex_lock(&priv->mutex);

        /* make sure handler does not exist */
        for (i = 0; i < MAX_REQUEST_HANDLERS; i++) {
                RequestHandler *rh =
                    priv->request_handlers + i;
                if (rh->name != NULL && strcmp(rh->name, name) == 0) {
                        pthread_mutex_unlock(&priv->mutex);
                        return false;
                }
        }

        /* set handler */
        for (i = 0; i < MAX_REQUEST_HANDLERS; i++) {
                RequestHandler *rh =
                    priv->request_handlers + i;
                if (rh->name == NULL) {
                        rh->name = name;
                        rh->handler = handler;
                        rh->data = data;
                        pthread_mutex_unlock(&priv->mutex);
                        return true;
                }
        }

        pthread_mutex_unlock(&priv->mutex);

        return false;
}

void icipc_server_clear_request_handler(
                struct icipc_server *self,
                const char *name) {
        struct icipc_receiver *base = icipc_server_to_receiver(self);
        ServerPriv *priv = icipc_receiver_get_user_data(base);
        size_t i;

        /* check params */
        if (name == NULL)
                return;

        pthread_mutex_lock(&priv->mutex);

        /* clear handler */
        for (i = 0; i < MAX_REQUEST_HANDLERS; i++) {
                RequestHandler *rh =
                    priv->request_handlers + i;
                if (rh->name != NULL && strcmp(rh->name, name) == 0) {
                        rh->name = NULL;
                        break;
                }
        }

        pthread_mutex_unlock(&priv->mutex);
}

bool icipc_server_reply_ok(
                struct icipc_server *self,
                int client_fd,
                const struct spa_pod *value) {
        const size_t s = icipc_protocol_calculate_reply_ok_size(value);
        uint8_t b[s];
        icipc_protocol_build_reply_ok(b, s, value);
        return icipc_socket_write(client_fd, b, s) == (ssize_t) s;
}

bool icipc_server_reply_error(
                struct icipc_server *self,
                int client_fd,
                const char *msg) {
        if (msg == NULL)
                return false;

        const size_t s = icipc_protocol_calculate_reply_error_size(msg);
        uint8_t b[s];
        icipc_protocol_build_reply_error(b, s, msg);
        return icipc_socket_write(client_fd, b, s) == (ssize_t) s;
}
