/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ICIPC_RECEIVER_H__
#define __ICIPC_RECEIVER_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct icipc_receiver;

enum icipc_receiver_sender_state {
  ICIPC_RECEIVER_SENDER_STATE_CONNECTED = 0,
  ICIPC_RECEIVER_SENDER_STATE_DISCONNECTED
};

struct icipc_receiver_events {
  /* emitted when a sender state changes */
  void (*sender_state) (struct icipc_receiver *self,
                        int sender_fd,
                        enum icipc_receiver_sender_state state,
                        void *data);

  /* emitted when message is received and needs to be handled */
  bool (*handle_message) (struct icipc_receiver *self,
                          int sender_fd,
                          const uint8_t *buffer,
                          size_t size,
                          void *data);
};

ICIPC_API
struct icipc_receiver *
icipc_receiver_new (const char *path,
                    size_t buffer_size,
                    const struct icipc_receiver_events *events,
                    void *events_data,
                    size_t user_size);

ICIPC_API
void
icipc_receiver_free (struct icipc_receiver *self);

ICIPC_API
bool
icipc_receiver_start (struct icipc_receiver *self);

ICIPC_API
void
icipc_receiver_stop (struct icipc_receiver *self);

ICIPC_API
bool
icipc_receiver_is_running (struct icipc_receiver *self);

/* for subclasses only */

ICIPC_API
void *
icipc_receiver_get_user_data (struct icipc_receiver *self);

#ifdef __cplusplus
}
#endif

#endif
