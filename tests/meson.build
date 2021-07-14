common_deps = [icipc_dep, threads_dep]
common_env = environment({
  'XDG_RUNTIME_DIR': '/tmp',
  'ICIPC_DEBUG': '3',
})

valgrind = find_program('valgrind', required: false)
if valgrind.found()
  add_test_setup('valgrind',
    exe_wrapper: [ valgrind,
      '--leak-check=full',
      '--gen-suppressions=all',
      '--error-exitcode=3',
      '--keep-debuginfo=yes',
    ],
    timeout_multiplier: 2)
endif

test(
  'test-icipc-sender-receiver',
  executable('test-sender-receiver', 'sender-receiver.c', dependencies: common_deps),
  env: common_env,
  workdir : meson.current_source_dir(),
)

test(
  'test-icipc-protocol',
  executable('test-protocol', 'protocol.c', dependencies: common_deps),
  env: common_env,
  workdir : meson.current_source_dir(),
)

test(
  'test-icipc-client-server',
  executable('test-client-server', 'client-server.c', dependencies: common_deps),
  env: common_env,
  workdir : meson.current_source_dir(),
)
