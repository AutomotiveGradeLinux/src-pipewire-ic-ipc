/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ICIPC_SENDER_H__
#define __ICIPC_SENDER_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct icipc_sender;

typedef void (*icipc_sender_lost_conn_func_t)(
                struct icipc_sender *self,
                int receiver_fd,
                void *data);

typedef void (*icipc_sender_reply_func_t)(
                struct icipc_sender *self,
                const uint8_t *buffer,
                size_t size,
                void *data);

ICIPC_API
struct icipc_sender *icipc_sender_new(
                const char *path,
                size_t buffer_size,
                icipc_sender_lost_conn_func_t lost_func,
                void *lost_data,
                size_t user_size);

ICIPC_API
void icipc_sender_free(struct icipc_sender *self);

ICIPC_API
bool icipc_sender_connect(struct icipc_sender *self);

ICIPC_API
void icipc_sender_disconnect(struct icipc_sender *self);

ICIPC_API
bool icipc_sender_is_connected(struct icipc_sender *self);

ICIPC_API
bool icipc_sender_send(
                struct icipc_sender *self,
                const uint8_t *buffer,
                size_t size,
                icipc_sender_reply_func_t reply,
                void *data);

/* for subclasses only */

ICIPC_API
void *icipc_sender_get_user_data(struct icipc_sender *self);

#ifdef __cplusplus
}
#endif

#endif
