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
spa_dep = dependency('libspa-0.2', version: '>= 0.2', required: true)

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

subdir('lib')
subdir('src')
subdir('tests')
