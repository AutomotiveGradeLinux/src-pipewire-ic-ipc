/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ICIPC_CLIENT_H__
#define __ICIPC_CLIENT_H__

#include <stddef.h>

#include "sender.h"
#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Casts icipc_client* to icipc_sender*
 * icipc_client is a subclass of icipc_sender and can be cast to it
 * at any time with this macro
 */
#define icipc_client_to_sender(self) ((struct icipc_sender *)(self))

struct icipc_client;
struct icipc_data;

/*!
 * \brief Creates a new client
 * \param path the name of the socket file to use for the connection; this
 *   may be an absolute path, or just a filename that will be looked for in
 *   the standard socket directories: PIPEWIRE_RUNTIME_DIR / XDG_RUNTIME_DIR / HOME
 * \param connect set to \c true to also connect to the server. If connection
 *   fails, there is no indication; use icipc_client_is_connected() to check
 *   the connection status afterwards. Alternatively, pass \c false here and
 *   call icipc_client_connect() manually.
 * \return the new icipc_client
 */
ICIPC_API
struct icipc_client *icipc_client_new(const char *path, bool connect);

/*!
 * \brief Destroys the client
 * The client also gracefully disconnects if it was connected.
 */
ICIPC_API
void icipc_client_free(struct icipc_client *self);

/*!
 * \brief Checks if the client is connected
 * \return a boolean value
 */
#define icipc_client_is_connected(c) \
        icipc_sender_is_connected(icipc_client_to_sender(c))

/*!
 * \brief Connects to the server, if not already connected
 * \return a boolean value: \c true if connection succeeded or if the client
 *   was already connected, \c false if connection failed
 */
#define icipc_client_connect(c) \
        icipc_sender_connect(icipc_client_to_sender(c))

/*!
 * \brief Disconnects from the server
 */
#define icipc_client_disconnect(c) \
        icipc_sender_disconnect(icipc_client_to_sender(c))

/*!
 * \brief Sends a request to the server
 * \param self the client instance
 * \param name the name of the request to send
 * \param args arguments of the request, or NULL
 * \param reply a callback, which is called when the server replies to this request
 * \param data additional user-defined data to pass to the \a reply callback
 * \return \c true if the request was sent, \c false if there was an error
 */
ICIPC_API
bool icipc_client_send_request(
                struct icipc_client *self,
                const char *name,
                const struct icipc_data *args,
                icipc_sender_reply_func_t reply,
                void *data);

/* for reply handlers only */

/*!
 * \brief Finish the request and extract the reply
 *
 * This is meant to be called in the `icipc_sender_reply_func_t` reply callback
 * that was passed on to icipc_client_send_request()
 *
 * If the request succeeded, this extracts the reply and returns it. Even for
 * replies that do not return any value, the returned pointer is guaranteed
 * to be non-NULL on success.
 *
 * If the request failed, the returned pointer is NULL and the \a error is set
 * to a string that can be printed for debugging the problem.
 *
 * \param self the client instance
 * \param buffer the buffer of the reply (argument to the reply callback)
 * \param size the size of the buffer (argument to the reply callback)
 * \param error return location for an error string
 * \return NULL on failure, non-NULL return value of the reply on success
 */
ICIPC_API
const struct icipc_data *icipc_client_send_request_finish(
                struct icipc_sender *self,
                const uint8_t *buffer,
                size_t size,
                const char **error);

#ifdef __cplusplus
}
#endif

#endif
