PipeWire Instrument Cluster IPC
===============================

This software component provides a communication link between applications
running in the Instrument Cluster container and the host container's PipeWire
daemon. It is useful when IC applications need to suspend sound output from the
host, temporarily, in order to play an emergency sound.

This component is tailored for use in containerized environments. For use in
an environment where the IC is a different host, additional work may be required.

## libicipc

Most of the functionality is implemented as a library, called `icipc`.

This library implements both the server-side and the client-side.
The server-side is meant to be used by the host, while the client-side is
to be used by the applications running in the IC guest.

The library implements a generic unix socket based communication mechanism
where a sender (client) can send any data buffer to the receiver (server).
The server is always required to respond.

```
  client  ------- REQUEST -------->  server
  client  <--- REPLY (OK/ERROR) ---  server
```

The server is also able to track the clients that are connected, individually.
When a client connects or disconnects, the server is notified.

The protocol used for communication is a binary serialization of data structures,
that aims to be compatible with PipeWire's spa_pod serialization mechanism,
which is used in PipeWire's native protocol. The implementation of this protocol
is kept private, for now, as it is not needed for the simple use case of sending
suspend/resume requests (see below).

## module-protocol-ic-ipc

The server-side component is built as a pipewire module, called
`libpipewire-module-protocol-ic-ipc`. It is meant to be executed on
a standalone pipewire instance, using the pipewire configuration file provided:
`pipewire-ic-ipc.conf`

To start the server manually, execute:
```
$ pipewire -c /absolute/path/to/pipewire-ic-ipc.conf
```

For convenience, systemd `.service` and `.socket` units are provided.

The server listens on a socket, which can be specified in the
`pipewire-ic-ipc.conf` file as an argument to the module. When launched via
systemd socket activation, the socket is passed on from systemd.

The path of the socket is determined by the same rules as the ones that PipeWire
uses for its socket. First, the `PIPEWIRE_RUNTIME_DIR` environment variable
is used. Then, `XDG_RUNTIME_DIR`, followed by `HOME`.

This server can accept two requests from its clients:

 * `SUSPEND`: This sets the `suspend.playback` metadata key on pipewire's
    main metadata object, which is then used by the session manager to suspend
    playback.
 * `RESUME`: This unsets the `suspend.playback` metadata key, reverting the
    actions of `SUSPEND`

When multiple clients send `SUSPEND`, the server keeps track of how many clients
have done this and expects an equal amount of `RESUME` requests before unsetting
the `suspend.playback` metadata key.

When a client sends `SUSPEND` and then disconnects without sending `RESUME`,
the server considers the disconnection event to be equivalent to a `RESUME`
request. Therefore, clients do not explicitly need to send `RESUME` before
disconnecting.

## icipc-client

This is an example client, which is provided for testing & demo purposes.
When launched, it presents a command prompt that accepts 2 commands:

 * `quit` - exits the client
 * `send <request_name>` - sends `<request_name>` to the server

To demo it with `libpipewire-module-protocol-ic-ipc` being the server, use:

 * `send SUSPEND` - suspend playback on the host
 * `send RESUME` - resume playback on the host

## Writing IC apps that use libicipc

IC apps that need to use this SUSPEND/RESUME mechanism for sound, should
directly use libicipc.

For API documentation, refer to `lib/client.h`. Clients should use only
the API provided in this file.

To link to libicipc, use the provided `icipc-0.1` pkg-config package:
```
$ pkg-config --cflags --libs icipc-0.1
```

By default, the libicipc library is built statically. If you wish to build
a dynamically linked version, pass the `-Ddefault_library=dynamic` option to
meson when compiling `pipewire-ic-ipc`

## Compilation & Running

To compile:
```
$ meson build
$ ninja -C build
```

To run tests:
```
$ ninja -C build test
... or ...
$ meson test -C build
```

To perform static code analysis using clang-tidy:
```
$ ninja -C build clang-tidy
```

### Depedencies

 * `libsystemd` is needed when building the library for the server-side
   (host-side) in order to integrate with systemd's socket-activation mechanism

No other dependencies are required. The code builds on top of the standard
C library only.
