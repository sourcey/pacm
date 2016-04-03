ask_build_sourcey_module(pacm)
if(BUILD_MODULES AND BUILD_MODULE_pacm)
  include_dependency(OpenSSL REQUIRED)
  #include_dependency(JsonCpp)
  #include_dependency(LibUV)
  #include_dependency(HttpParser)
  #include_dependency(ZLIB)
  #include_dependency(Minizip)

  define_sourcey_module(pacm uv base net json http archo crypto util)

  add_subdirectory(apps)
endif()
