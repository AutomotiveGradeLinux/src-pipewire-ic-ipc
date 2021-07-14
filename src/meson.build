if get_option('client')
  executable('icipc-client',
    'icipc-client.c',
    dependencies : [icipc_dep, threads_dep],
  )
endif

if get_option('server')
  pipewire_module_access = shared_library(
    'pipewire-module-protocol-ic-ipc',
    [ 'module-protocol-ic-ipc.c' ],
    c_args : [
      '-DPACKAGE_VERSION="' + meson.project_version() + '"',
    ],
    install : true,
    install_dir : pipewire_dep.get_variable(pkgconfig: 'moduledir'),
    dependencies : [icipc_dep, pipewire_dep],
  )
endif
