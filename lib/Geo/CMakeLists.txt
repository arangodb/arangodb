add_library(arango_geo STATIC
  GeoJson.cpp
  GeoParams.cpp
  ShapeContainer.cpp
  Utils.cpp)

if (NOT USE_ENTERPRISE)
  target_sources(arango_geo PRIVATE Coding.cpp)
endif ()

target_link_libraries(arango_geo
  arango
  boost_system
  boost_boost
  s2)

add_subdirectory(karney)
add_subdirectory(S2)
