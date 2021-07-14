project('icipc', ['c'],
  version : '0.1',
  license : 'MIT',
  meson_version : '>= 0.52.0',
  default_options : [
    'warning_level=1',
    'buildtype=debugoptimized',
  ]
)

threads_dep = dependency('threads', required: true)
libsystemd_dep = dependency('libsystemd', required: get_option('systemd'))

if get_option('server')
  pipewire_dep = dependency('libpipewire-0.3', required: true)
  systemd_dep = dependency('systemd', required: get_option('systemd'))
endif

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')

common_flags = [
  '-fvisibility=hidden',
  '-Wsuggest-attribute=format',
  '-Wsign-compare',
  '-Wpointer-arith',
  '-Wpointer-sign',
  '-Wformat',
  '-Wformat-security',
  '-Wimplicit-fallthrough',
  '-Wmissing-braces',
  '-Wtype-limits',
  '-Wvariadic-macros',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
  '-Wno-pedantic',
  '-Wold-style-declaration',
  '-Wunused-result',
]
add_project_arguments(cc.get_supported_arguments(common_flags), language: 'c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')

if libsystemd_dep.found()
  add_project_arguments('-DHAVE_SYSTEMD', language: 'c')
endif

subdir('lib')
subdir('src')
subdir('tests')
