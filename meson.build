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
pipewire_dep = dependency('libpipewire-0.3', required: get_option('server'))

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

subdir('lib')
subdir('src')
subdir('tests')
