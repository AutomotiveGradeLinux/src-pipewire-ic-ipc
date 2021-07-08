/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ICIPC_SERVER_H__
#define __ICIPC_SERVER_H__

#include <spa/pod/pod.h>

#include "defs.h"
#include "receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define icipc_server_to_receiver(self) ((struct icipc_receiver *)(self))

struct icipc_server;

typedef void (*icipc_server_client_handler_func_t)(
                struct icipc_server *self,
                int client_fd,
                enum icipc_receiver_sender_state client_state,
                void *data);

typedef bool (*icipc_server_request_handler_func_t)(
                struct icipc_server *self,
                int client_fd,
                const char *name,
                const struct spa_pod *args,
                void *data);

ICIPC_API
struct icipc_server *icipc_server_new(const char *path, bool start);

ICIPC_API
void icipc_server_free(struct icipc_server *self);

ICIPC_API
void icipc_server_set_client_handler(
                struct icipc_server *self,
                icipc_server_client_handler_func_t handler,
                void *data);

ICIPC_API
void icipc_server_clear_client_handler(struct icipc_server *self);

ICIPC_API
bool icipc_server_set_request_handler(
                struct icipc_server *self,
                const char *name,
                icipc_server_request_handler_func_t handler,
                void *data);

ICIPC_API
void icipc_server_clear_request_handler(
                struct icipc_server *self,
                const char *name);

/* for request handlers only */

ICIPC_API
bool icipc_server_reply_ok(
                struct icipc_server *self,
                int client_fd,
                const struct spa_pod *value);

ICIPC_API
bool icipc_server_reply_error(
                struct icipc_server *self,
                int client_fd,
                const char *msg);

#ifdef __cplusplus
}
#endif

#endif
