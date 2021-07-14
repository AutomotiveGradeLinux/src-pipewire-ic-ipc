icipc_lib_sources = files(
  'utils.c',
  'protocol.c',
  'receiver.c',
  'sender.c',
  'client.c',
  'server.c',
)

icipc_lib_headers = files(
  'defs.h',
  'protocol.h',
  'receiver.h',
  'sender.h',
  'client.h',
  'server.h',
  'icipc.h',
)

icipc_api_version = '0.1'
icipc_so_version = '0'

icipc_headers_dir = get_option('includedir') / 'icipc-' + icipc_api_version / 'icipc'

install_headers(icipc_lib_headers,
  install_dir : icipc_headers_dir
)

icipc_lib = library('icipc-' + icipc_api_version,
  icipc_lib_sources,
  install: true,
  dependencies : [threads_dep, libsystemd_dep],
  soversion: icipc_so_version,
  version: meson.project_version(),
)

icipc_dep = declare_dependency(
  link_with: icipc_lib,
  include_directories: include_directories('.'),
)

pkgconfig.generate(icipc_lib,
  subdirs: 'icipc-' + icipc_api_version
)
