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

enum icipc_log_level {
  ICIPC_LOG_LEVEL_NONE = 0,
  ICIPC_LOG_LEVEL_ERROR,
  ICIPC_LOG_LEVEL_WARN,
  ICIPC_LOG_LEVEL_INFO,
};

void
icipc_logv (enum icipc_log_level level,
            const char *fmt,
            va_list args) __attribute__ ((format (printf, 2, 0)));

void
icipc_log (enum icipc_log_level level,
           const char *fmt,
           ...) __attribute__ ((format (printf, 2, 3)));

/* socket */

ssize_t
icipc_socket_write (int fd, const uint8_t *buffer, size_t size);

ssize_t
icipc_socket_read (int fd, uint8_t **buffer, size_t *max_size);

/* epoll thread */

struct epoll_thread;

typedef void (*icipc_epoll_thread_event_funct_t) (struct epoll_thread *self,
                                                  int fd,
                                                  void *data);

struct epoll_thread {
  int socket_fd;
  int epoll_fd;
  int event_fd;
  pthread_t thread;
  icipc_epoll_thread_event_funct_t socket_event_func;
  icipc_epoll_thread_event_funct_t other_event_func;
  void *event_data;
};

bool
icipc_epoll_thread_init (struct epoll_thread *self,
                         int socket_fd,
                         icipc_epoll_thread_event_funct_t sock_func,
                         icipc_epoll_thread_event_funct_t other_func,
                         void *data);

bool
icipc_epoll_thread_start (struct epoll_thread *self);

void
icipc_epoll_thread_stop (struct epoll_thread *self);

void
icipc_epoll_thread_destroy (struct epoll_thread *self);

#ifdef __cplusplus
}
#endif

#endif
