add_library(arango_geo STATIC
  Ellipsoid.cpp
  GeoJson.cpp
  GeoParams.cpp
  ShapeContainer.cpp
  Shapes.cpp
  Utils.cpp)

target_link_libraries(arango_geo
  arango
  boost_system
  boost_boost
  s2)

if (MSVC)
  target_compile_options(arango_geo PUBLIC "-J")
endif()

if (USE_FAIL_ON_WARNINGS)
  if (MSVC)
    target_compile_options(arango_geo PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS96)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # clang-cl.exe
      # There's a warning about /MP being unused if compiling only a single file
      target_compile_options(arango_geo PRIVATE -Wno-unused-command-line-argument)
    endif()
  else ()
    target_compile_options(arango_geo PRIVATE -Werror -Wno-error=deprecated-declarations)
  endif ()
endif ()

add_subdirectory(karney)
add_subdirectory(S2)
