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

  install_data(
    sources: 'pipewire-ic-ipc.conf',
    install_dir: pipewire_dep.get_variable(pkgconfig: 'prefix') / 'share' / 'pipewire'
  )

  if systemd_dep.found() and libsystemd_dep.found()
    systemd_system_services_dir = systemd_dep.get_pkgconfig_variable(
      'systemdsystemunitdir',
      define_variable: ['prefix', get_option('prefix')])

    install_data(
      sources: ['pipewire-ic-ipc.socket', 'pipewire-ic-ipc.service'],
      install_dir: systemd_system_services_dir)
  endif
endif
