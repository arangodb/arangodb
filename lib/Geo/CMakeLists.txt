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

add_subdirectory(karney)
add_subdirectory(S2)
