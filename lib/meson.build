if get_option('icipc').disabled()
  icipc_dep = disabler()
  subdir_done()
endif

icipc_lib_sources = files(
  'utils.c',
  'protocol.c',
  'receiver.c',
  'sender.c',
  'client.c',
  'server.c',
)

icipc_lib_headers = files(
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
  c_args : [
    '-D_GNU_SOURCE',
    '-DG_LOG_USE_STRUCTURED',
    '-DG_LOG_DOMAIN="icipc"',
  ],
  install: true,
  dependencies : [threads_dep, spa_dep],
  soversion: icipc_so_version,
  version: meson.project_version(),
)

icipc_dep = declare_dependency(
  link_with: icipc_lib,
  include_directories: wp_lib_include_dir,
  dependencies: [spa_dep],
)
