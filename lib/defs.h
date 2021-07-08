/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ICIPC_DEFS_H__
#define __ICIPC_DEFS_H__

#if defined(__GNUC__)
# define ICIPC_API_EXPORT extern __attribute__((visibility("default")))
#else
# define ICIPC_API_EXPORT extern
#endif

#ifndef ICIPC_API
#  define ICIPC_API ICIPC_API_EXPORT
#endif

#ifndef ICIPC_PRIVATE_API
#  define ICIPC_PRIVATE_API __attribute__((deprecated("Private API")))
#endif

#endif
