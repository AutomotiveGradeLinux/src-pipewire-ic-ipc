common_deps = [icipc_dep, threads_dep]
common_env = environment({
  'XDG_RUNTIME_DIR': '/tmp',
  'ICIPC_DEBUG': '3',
})

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
