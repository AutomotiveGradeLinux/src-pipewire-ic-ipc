/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ICIPC_CLIENT_H__
#define __ICIPC_CLIENT_H__

#include <spa/pod/pod.h>

#include <stddef.h>

#include "sender.h"
#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define icipc_client_to_sender(self) ((struct icipc_sender *)(self))

struct icipc_client;

ICIPC_API
struct icipc_client *
icipc_client_new (const char *path, bool connect);

ICIPC_API
void
icipc_client_free (struct icipc_client *self);

ICIPC_API
bool
icipc_client_send_request (struct icipc_client *self,
                           const char *name,
                           const struct spa_pod *args,
                           icipc_sender_reply_func_t reply,
                           void *data);

/* for reply handlers only */

ICIPC_API
const struct spa_pod *
icipc_client_send_request_finish (struct icipc_sender *self,
                                  const uint8_t *buffer,
                                  size_t size,
                                  const char **error);

#ifdef __cplusplus
}
#endif

#endif
