/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <pipewire/impl.h>
#include <pipewire/array.h>
#include <pipewire/extensions/metadata.h>
#include <spa/utils/result.h>
#include <icipc.h>

#define NAME "protocol-ic-ipc"

#define SERVER_SUSPEND_REQUEST_NAME "SUSPEND"
#define SERVER_RESUME_REQUEST_NAME "RESUME"

static const struct spa_dict_item module_props[] = {
        { PW_KEY_MODULE_AUTHOR,
                "George Kiagiadakis <george.kiagiadakis@collabora.com>" },
        { PW_KEY_MODULE_DESCRIPTION,
                "Sends commands to PipeWire from the "
                "AGL Instrument Cluster container" },
        { PW_KEY_MODULE_USAGE, "[ icipc.name=(<name>|<path>), "
                                  PW_KEY_REMOTE_NAME"=<name> ]" },
        { PW_KEY_MODULE_VERSION, PACKAGE_VERSION },
};

struct impl {
        struct pw_context *context;
        struct pw_loop *loop;
        struct icipc_server *server;

        struct pw_array clients;

        char *pipewire_remote;
        struct spa_source *timeout_source;
        struct pw_core *core;
        struct pw_registry *registry;
        struct pw_metadata *metadata;

        struct spa_hook module_listener;
        struct spa_hook core_proxy_listener;
        struct spa_hook core_listener;
        struct spa_hook registry_proxy_listener;
        struct spa_hook registry_listener;
        struct spa_hook metadata_proxy_listener;
};

#ifndef pw_array_add_int
#define pw_array_add_int(a,i) *((int*) pw_array_add(a, sizeof(int))) = (i)
#endif

static void update_timer(struct impl *impl, time_t sec, long nsec)
{
        struct timespec value, interval;
        value.tv_sec = sec;
        value.tv_nsec = nsec;
        interval.tv_sec = 0;
        interval.tv_nsec = 0;
        pw_loop_update_timer(impl->loop, impl->timeout_source,
                             &value, &interval, false);
        pw_loop_update_source(impl->loop, impl->timeout_source);
}

static void disconnect_from_pw(struct impl *impl) {
        if (impl->core)
                pw_core_disconnect(impl->core);
}

static void impl_free(struct impl *impl) {
        pw_log_debug(NAME " %p: destroy", impl);

        disconnect_from_pw(impl);
        if (impl->timeout_source) {
                update_timer(impl, 0, 0);
                pw_loop_destroy_source(impl->loop, impl->timeout_source);
        }
        if (impl->server)
                icipc_server_free(impl->server);
        pw_array_clear(&impl->clients);
        free(impl->pipewire_remote);
        free(impl);
}

static void module_destroy(void *data) {
        struct impl *impl = data;
        impl_free(impl);
}

static const struct pw_impl_module_events module_events = {
        PW_VERSION_IMPL_MODULE_EVENTS,
        .destroy = module_destroy,
};

static void core_destroy(void *d) {
        struct impl *impl = d;
        spa_hook_remove(&impl->core_listener);
        impl->core = NULL;
}

static const struct pw_proxy_events core_proxy_events = {
        PW_VERSION_PROXY_EVENTS,
        .destroy = core_destroy,
};

static void registry_destroy(void *d) {
        struct impl *impl = d;
        spa_hook_remove(&impl->registry_listener);
        impl->registry = NULL;
}

static void registry_removed(void *d) {
        struct impl *impl = d;
        pw_proxy_destroy((struct pw_proxy *)impl->registry);
}

static const struct pw_proxy_events registry_proxy_events = {
        PW_VERSION_PROXY_EVENTS,
        .destroy = registry_destroy,
        .removed = registry_removed,
};

static void metadata_destroy(void *d) {
        struct impl *impl = d;
        impl->metadata = NULL;
}

static void metadata_removed(void *d) {
        struct impl *impl = d;
        pw_proxy_destroy((struct pw_proxy *)impl->metadata);
}

static const struct pw_proxy_events metadata_proxy_events = {
        PW_VERSION_PROXY_EVENTS,
        .destroy = metadata_destroy,
        .removed = metadata_removed,
};

static void on_core_error(
                void *d, uint32_t id, int seq, int res, const char *message) {
        struct impl *impl = d;

        pw_log_error("error id:%u seq:%d res:%d (%s): %s",
                      id, seq, res, spa_strerror(res), message);

        /* pipewire disconnected */
        if (id == PW_ID_CORE && res == -EPIPE)
                update_timer(impl, 0, 1);
}

static const struct pw_core_events core_events = {
        PW_VERSION_CORE_EVENTS,
        .error = on_core_error,
};

static void on_global_added (
                void *object, uint32_t id,
                uint32_t permissions, const char *type, uint32_t version,
                const struct spa_dict *props) {
        struct impl *impl = object;
        const char *str;

        if (type && props && !strcmp(type, PW_TYPE_INTERFACE_Metadata) &&
            (str = spa_dict_lookup(props, "metadata.name")) &&
            !strcmp(str, "default")) {
                impl->metadata = pw_registry_bind(impl->registry, id, type,
                                                  version, 0);
                pw_proxy_add_listener((struct pw_proxy*)impl->metadata,
                                      &impl->metadata_proxy_listener,
                                      &metadata_proxy_events, impl);
        }
}

static const struct pw_registry_events registry_events = {
        PW_VERSION_REGISTRY_EVENTS,
        .global = on_global_added,
};

static void connect_to_pipewire(struct impl *impl) {
        if (!impl->core) {
                struct pw_properties *p = pw_properties_new(
                                PW_KEY_REMOTE_NAME, impl->pipewire_remote,
                                NULL);
                impl->core = pw_context_connect(impl->context, p, 0);
                if (!impl->core) {
                        pw_log_warn(NAME " %p: failed to connect to pipewire:"
                                    " %m", impl);
                        update_timer(impl, 5, 0);
                        return;
                }
                pw_log_info(NAME " %p: connected to pw", impl);
        }

        update_timer(impl, 0, 0);
        impl->registry =
                pw_core_get_registry(impl->core, PW_VERSION_REGISTRY, 0);

        pw_proxy_add_listener((struct pw_proxy*)impl->core,
                              &impl->core_proxy_listener,
                              &core_proxy_events, impl);
        pw_core_add_listener(impl->core,
                             &impl->core_listener,
                             &core_events, impl);

        pw_proxy_add_listener((struct pw_proxy*)impl->registry,
                              &impl->registry_proxy_listener,
                              &registry_proxy_events, impl);
        pw_registry_add_listener(impl->registry,
                                 &impl->registry_listener,
                                 &registry_events, impl);
}

static void on_timeout(void *data, uint64_t expirations) {
        struct impl *impl = data;
        pw_log_debug(NAME " %p: timeout", impl);

        if (impl->core) {
                pw_log_info(NAME " %p: disconnect", impl);
                disconnect_from_pw(impl);
                update_timer(impl, 5, 0);
        } else
                connect_to_pipewire(impl);
}

static int do_suspend_resume(
                struct spa_loop *loop,
                bool async,
                uint32_t seq,
                const void *data,
                size_t size,
                void *user_data) {
        struct impl *impl = user_data;
        const int suspend = *(const int *)data;

        if (impl->metadata) {
                pw_metadata_set_property(impl->metadata, 0, "suspend.playback",
                        "Spa:Bool", suspend ? "1" : NULL);
        }
        return 0;
}

static bool suspend_handler(
                struct icipc_server *s,
                int client_fd,
                const char *name,
                const struct icipc_data *args,
                void *data) {
        struct impl *impl = data;
        int *c;

        pw_array_for_each(c, &impl->clients) {
                if (*c == client_fd)
                        return icipc_server_reply_ok (s, client_fd, NULL);
        }
        pw_array_add_int(&impl->clients, client_fd);

        /* do suspend if this is the first client asking for suspend */
        if (impl->clients.size == sizeof(int)) {
                const int suspend = 1;
                pw_loop_invoke(impl->loop, do_suspend_resume, 0, &suspend,
                               sizeof(suspend), true, impl);
        }
        return icipc_server_reply_ok (s, client_fd, NULL);
}

static bool resume_handler(
                struct icipc_server *s,
                int client_fd,
                const char *name,
                const struct icipc_data *args,
                void *data) {
        struct impl *impl = data;
        size_t old_size = impl->clients.size;
        int *c;

        pw_array_for_each(c, &impl->clients) {
                if (*c == client_fd) {
                        pw_array_remove(&impl->clients, c);
                        break;
                }
        }
        /* resume if there are no more clients suspending */
        if (old_size > 0 && impl->clients.size == 0) {
                const int suspend = 0;
                pw_loop_invoke(impl->loop, do_suspend_resume, 0, &suspend,
                               sizeof(suspend), true, impl);
        }
        /* don't reply if called with name == NULL from client_handler() */
        return name ? icipc_server_reply_ok (s, client_fd, NULL) : true;
}

static void client_handler (
                struct icipc_server *s,
                int client_fd,
                enum icipc_receiver_sender_state client_state,
                void *data) {
        struct impl *impl = data;

        switch (client_state) {
        case ICIPC_RECEIVER_SENDER_STATE_CONNECTED:
                pw_log_info (NAME " %p: client connected %d", impl, client_fd);
                break;
        case ICIPC_RECEIVER_SENDER_STATE_DISCONNECTED: {
                pw_log_info (NAME " %p: client disconnected %d", impl, client_fd);
                resume_handler(s, client_fd, NULL, NULL, impl);
                break;
        }
        default:
                break;
        }
}

SPA_EXPORT
int pipewire__module_init(struct pw_impl_module *module, const char *args) {
        struct impl *impl;
        struct pw_properties *props = NULL;
        const char *str = NULL;
        int res = 0;

        impl = calloc(1, sizeof(struct impl));
        if (impl == NULL)
                return -errno;

        pw_log_debug(NAME " %p: new %s", impl, args);

        impl->context = pw_impl_module_get_context(module);
        impl->loop = pw_context_get_main_loop(impl->context);

        /* setup icipc server */

        if (args) {
                props = pw_properties_new_string(args);
                str = pw_properties_get(props, "icipc.name");
        }
        if (!str)
                str = "icipc-0";

        impl->server = icipc_server_new(str, false);
        if (!impl->server) {
                res = -ENOMEM;
                goto out;
        }

        pw_array_init(&impl->clients, 4*sizeof(int));
        icipc_server_set_client_handler(impl->server, client_handler, impl);
        icipc_server_set_request_handler(impl->server,
                                         SERVER_SUSPEND_REQUEST_NAME,
                                         suspend_handler, impl);
        icipc_server_set_request_handler(impl->server,
                                         SERVER_RESUME_REQUEST_NAME,
                                         resume_handler, impl);

        /* connect to pipewire */

        str = props ? pw_properties_get(props, PW_KEY_REMOTE_NAME) : NULL;
        if (str)
                impl->pipewire_remote = strdup(str);

        impl->timeout_source = pw_loop_add_timer(impl->loop, on_timeout, impl);
        update_timer(impl, 0, 1);

        /* start icipc */

        if (!icipc_receiver_start((struct icipc_receiver *) impl->server)) {
                res = -errno;
                pw_log_error("failed to start icipc server: %m");
                goto out;
        }

        /* finish module setup */

        pw_impl_module_add_listener(module, &impl->module_listener,
                                    &module_events, impl);
        pw_impl_module_update_properties(module,
                                         &SPA_DICT_INIT_ARRAY(module_props));

out:
        if (props)
                pw_properties_free(props);
        if (res < 0)
                impl_free(impl);
        return res;
}
