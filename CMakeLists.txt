if(HAVE_OPENSSL)
  scy_add_module(pacm
    DEPENDS base net json http archo crypto
    PACKAGES OpenSSL::SSL OpenSSL::Crypto
  )

  if(BUILD_APPLICATIONS AND TARGET pacm)
    add_subdirectory(apps)
  endif()
endif()
