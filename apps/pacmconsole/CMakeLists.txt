ask_build_sourcey_application(pacmconsole)
if(BUILD_APPLICATIONS AND BUILD_APPLICATION_pacmconsole) 
  set(pacmconsole_EXECUTABLE_NAME "pacm")
  set(pacmconsole_DEBUG_POSTFIX "")

  define_sourcey_application(pacmconsole uv base net json http crypto archo pacm)
endif()
