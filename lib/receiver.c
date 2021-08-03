/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "private.h"
#include "receiver.h"
#include "icipc.h"

#define MAX_SENDERS 128

struct icipc_receiver {
        struct sockaddr_un addr;
        int socket_fd;
        bool activated;

        uint8_t *buffer_read;
        size_t buffer_size;

        EpollThread epoll_thread;
        bool thread_running;

        const struct icipc_receiver_events *events;
        void *events_data;

        /* for subclasses */
        void *user_data;
};

static bool reply_message(
                struct icipc_receiver *self,
                int sender_fd,
                uint8_t * buffer,
                size_t size) {
        return self->events && self->events->handle_message ?
            self->events->handle_message(self, sender_fd, buffer, size,
                                         self->events_data) :
            icipc_socket_write(sender_fd, buffer, size) == (ssize_t) size;
}

static void socket_event_received(EpollThread *t, int fd, void *data) {
        /* sender wants to connect, accept connection */
        struct icipc_receiver *self = data;
        socklen_t addr_size = sizeof(self->addr);
        int sender_fd = accept4(fd, (struct sockaddr *)&self->addr, &addr_size,
                                SOCK_CLOEXEC | SOCK_NONBLOCK);
        struct epoll_event event = {0};
        event.events = EPOLLIN;
        event.data.fd = sender_fd;
        epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, sender_fd, &event);
        if (self->events && self->events->sender_state)
                self->events->sender_state(self, sender_fd,
                                           ICIPC_RECEIVER_SENDER_STATE_CONNECTED,
                                           self->events_data);
}

static void other_event_received(EpollThread *t, int fd, void *data) {
        struct icipc_receiver *self = data;

        /* sender sends a message, read it and reply */
        ssize_t size =
            icipc_socket_read(fd, &self->buffer_read, &self->buffer_size);
        if (size <= 0) {
                if (size < 0)
                        icipc_log_error("receiver: could not read message: %s",
                                        strerror(errno));
                /* client disconnected */
                epoll_ctl(t->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                if (self->events && self->events->sender_state)
                        self->events->sender_state(self, fd,
                                                   ICIPC_RECEIVER_SENDER_STATE_DISCONNECTED,
                                                   self->events_data);
                return;
        }

        /* reply */
        if (!reply_message(self, fd, self->buffer_read, size))
                icipc_log_error("receiver: could not reply message: %s",
                                strerror(errno));

        return;
}

/* API */

struct icipc_receiver *icipc_receiver_new(
                const char *path,
                size_t buffer_size,
                const struct icipc_receiver_events *events,
                void *events_data,
                size_t user_size) {
        struct icipc_receiver *self;
        int res;

        /* check params */
        if (path == NULL || buffer_size == 0)
                return NULL;

        self = calloc(1, sizeof(struct icipc_receiver) + user_size);
        if (self == NULL)
                return NULL;

        self->socket_fd = -1;

        /* set address */
        self->addr.sun_family = AF_LOCAL;
        res = icipc_construct_socket_path(path, self->addr.sun_path,
                                          sizeof(self->addr.sun_path));
        if (res < 0)
                goto error;

#ifdef HAVE_SYSTEMD
        {
                int i, n = sd_listen_fds(0);
                for (i = 0; i < n; ++i) {
                        if (sd_is_socket_unix(SD_LISTEN_FDS_START + i,
                                              SOCK_STREAM, 1,
                                              self->addr.sun_path, 0) > 0) {
                                self->socket_fd = SD_LISTEN_FDS_START + i;
                                self->activated = true;
                                icipc_log_info("receiver %p: Found socket "
                                               "activation socket for '%s'",
                                               self, self->addr.sun_path);
                                break;
                        }
                }
        }
#endif

        if (self->socket_fd < 0) {
                struct stat socket_stat;

                /* create socket */
                self->socket_fd =
                        socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
                if (self->socket_fd < 0)
                        goto error;

                if (stat(self->addr.sun_path, &socket_stat) < 0) {
                        if (errno != ENOENT) {
                                icipc_log_error("receiver %p: stat %s failed "
                                                "with error: %m",
                                                self, self->addr.sun_path);
                                goto error;
                        }
                } else if (socket_stat.st_mode & S_IWUSR ||
                           socket_stat.st_mode & S_IWGRP) {
                        unlink(self->addr.sun_path);
                }

                /* bind socket */
                if (bind(self->socket_fd, (struct sockaddr *)&self->addr,
                         sizeof(self->addr)) != 0)
                        goto error;

                /* listen socket */
                if (listen(self->socket_fd, MAX_SENDERS) != 0)
                        goto error;
        }

        /* alloc buffer read */
        self->buffer_size = buffer_size;
        self->buffer_read = calloc(buffer_size, sizeof(uint8_t));
        if (self->buffer_read == NULL)
                goto error;

        /* init epoll thread */
        if (!icipc_epoll_thread_init(&self->epoll_thread, self->socket_fd,
                                     socket_event_received,
                                     other_event_received, self))
                goto error;

        self->events = events;
        self->events_data = events_data;
        if (user_size > 0)
                self->user_data =
                    (void *)((uint8_t *) self + sizeof(struct icipc_receiver));

        return self;

 error:
        if (self->buffer_read)
                free(self->buffer_read);
        if (self->socket_fd != -1)
                close(self->socket_fd);
        free(self);
        return NULL;
}

void icipc_receiver_free(struct icipc_receiver *self) {
        icipc_receiver_stop(self);

        icipc_epoll_thread_destroy(&self->epoll_thread);
        free(self->buffer_read);
        close(self->socket_fd);
        if (!self->activated)
                unlink(self->addr.sun_path);
        free(self);
}

bool icipc_receiver_start(struct icipc_receiver *self) {
        if (icipc_receiver_is_running(self))
                return true;

        self->thread_running = icipc_epoll_thread_start(&self->epoll_thread);
        return self->thread_running;
}

void icipc_receiver_stop(struct icipc_receiver *self) {
        if (icipc_receiver_is_running(self)) {
                icipc_epoll_thread_stop(&self->epoll_thread);
                self->thread_running = false;
        }
}

bool icipc_receiver_is_running(struct icipc_receiver *self) {
        return self->thread_running;
}

void *icipc_receiver_get_user_data(struct icipc_receiver *self) {
        return self->user_data;
}
