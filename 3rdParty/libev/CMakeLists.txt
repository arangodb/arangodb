project(libev)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

#FIXME TEST AND CONFIGURE
configure_file(config.h.in config.h COPYONLY)

add_library(ar_libev STATIC ev.c event.c)
target_compile_definitions(ar_libev PUBLIC "EV_CHILD_ENABLE=0")
target_compile_options(ar_libev PRIVATE "-O3") ## FIXME Windows
target_include_directories(ar_libev PUBLIC .)
