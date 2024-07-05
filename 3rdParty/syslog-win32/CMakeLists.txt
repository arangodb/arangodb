add_library(syslog-win32 STATIC
  syslog-client.c
  syslog.h
)
target_include_directories(syslog-win32 SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
