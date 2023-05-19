add_library(arango_geo STATIC
  GeoJson.cpp
  GeoParams.cpp
  ShapeContainer.cpp
  Utils.cpp)

if (USE_ENTERPRISE)
  target_sources(arango_geo PRIVATE ${PROJECT_SOURCE_DIR}/enterprise/Enterprise/IResearch/CodingEE.cpp)
else ()
  target_sources(arango_geo PRIVATE Coding.cpp)
endif ()

target_link_libraries(arango_geo
  arango
  boost_system
  boost_boost
  s2)

if (MSVC)
  target_compile_options(arango_geo PUBLIC "-J")
endif ()

add_subdirectory(karney)
add_subdirectory(S2)
