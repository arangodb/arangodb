add_library(arango_futures STATIC
  Future.cpp)

target_include_directories(arango_futures
  PUBLIC
  ${PROJECT_SOURCE_DIR}/3rdParty/iresearch/external/function2
  .)

target_link_libraries(arango_futures arango_assertions)

if (MSVC)
  target_compile_options(arango_futures PUBLIC "-J")
endif()
