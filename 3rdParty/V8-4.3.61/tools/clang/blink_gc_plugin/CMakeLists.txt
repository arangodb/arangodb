# This line is read by update.sh and other scripts in tools/clang/scripts
# Note: The spaces are significant.
set(LIBRARYNAME BlinkGCPlugin)

add_llvm_loadable_module("lib${LIBRARYNAME}"
                         BlinkGCPlugin.cpp
                         Edge.cpp
                         RecordInfo.cpp
                        )

add_dependencies("lib${LIBRARYNAME}" clang)

cr_install(TARGETS "lib${LIBRARYNAME}" LIBRARY DESTINATION lib)

cr_add_test(blink_gc_plugin_test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.sh
  ${CMAKE_BINARY_DIR}/bin/clang
  $<TARGET_FILE:lib${LIBRARYNAME}>
  )
