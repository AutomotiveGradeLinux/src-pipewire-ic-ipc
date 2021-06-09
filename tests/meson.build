common_deps = [icipc_dep, glib_dep]
common_env = environment({
  'G_TEST_SRCDIR': meson.current_source_dir(),
  'G_TEST_BUILDDIR': meson.current_build_dir(),
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
