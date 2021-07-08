/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ICIPC_PRIVATE_H__
#define __ICIPC_PRIVATE_H__

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdarg.h>

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* log */

#define icipc_log_info(F, ...) \
        icipc_log(ICIPC_LOG_LEVEL_INFO, (F), ##__VA_ARGS__)
#define icipc_log_warn(F, ...) \
        icipc_log(ICIPC_LOG_LEVEL_WARN, (F), ##__VA_ARGS__)
#define icipc_log_error(F, ...) \
        icipc_log(ICIPC_LOG_LEVEL_ERROR, (F), ##__VA_ARGS__)

typedef enum LogLevel {
        ICIPC_LOG_LEVEL_NONE = 0,
        ICIPC_LOG_LEVEL_ERROR,
        ICIPC_LOG_LEVEL_WARN,
        ICIPC_LOG_LEVEL_INFO,
} LogLevel;

void icipc_logv (
                LogLevel level,
                const char *fmt,
                va_list args) __attribute__((format(printf, 2, 0)));

void icipc_log(
                LogLevel level,
                const char *fmt,
                ...) __attribute__((format(printf, 2, 3)));

/* socket path */

int icipc_construct_socket_path(const char *name, char *buf, size_t buf_size);

/* socket */

ssize_t icipc_socket_write(int fd, const uint8_t *buffer, size_t size);

ssize_t icipc_socket_read(int fd, uint8_t **buffer, size_t *max_size);

/* epoll thread */

typedef struct EpollThread EpollThread;

typedef void (*icipc_epoll_thread_event_func_t)(
                struct EpollThread *self,
                int fd,
                void *data);

struct EpollThread {
        int socket_fd;
        int epoll_fd;
        int event_fd;
        pthread_t thread;
        icipc_epoll_thread_event_func_t socket_event_func;
        icipc_epoll_thread_event_func_t other_event_func;
        void *event_data;
};

bool icipc_epoll_thread_init(
                EpollThread *self,
                int socket_fd,
                icipc_epoll_thread_event_func_t sock_func,
                icipc_epoll_thread_event_func_t other_func,
                void *data);

bool icipc_epoll_thread_start(EpollThread *self);

void icipc_epoll_thread_stop(EpollThread *self);

void icipc_epoll_thread_destroy(EpollThread *self);

#ifdef __cplusplus
}
#endif

#endif
