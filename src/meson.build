executable('icipc-client',
  'icipc-client.c',
  c_args : [
    '-D_GNU_SOURCE',
    '-DG_LOG_USE_STRUCTURED',
    '-DG_LOG_DOMAIN="icipc-client"',
  ],
  dependencies : [icipc_dep, threads_dep],
)
