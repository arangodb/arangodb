if (MSVC)
  # disable "checked iterators" feature
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
endif ()

include_directories(
  ${PROJECT_BINARY_DIR}/core
)

include_directories(SYSTEM
  ${EXTERNAL_INCLUDE_DIRS}
  ${FROZEN_INCLUDE_DIR}
)

################################################################################
### iresearch-util
################################################################################

add_executable(iresearch-index-util
  ./common.cpp
  ./index-dump.cpp
  ./index-convert.cpp
  ./index-util.cpp
  ./main.cpp
  )

target_link_libraries(iresearch-index-util
  iresearch-static
  iresearch-cmdline
  ${PTHREAD_LIBRARY}
  ${ATOMIC_LIBRARY}
  )

include_directories(iresearch-index-util
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  )

################################################################################
### iresearch-benchmarks
################################################################################

add_executable(iresearch-benchmarks
  ./common.cpp
  ./index-put.cpp
  ./index-search.cpp
  ./index-benchmarks.cpp
  ./main.cpp
  )

add_dependencies(iresearch-benchmarks
  iresearch-static
  )

target_link_libraries(iresearch-benchmarks
  iresearch-static
  iresearch-cmdline
  ${PTHREAD_LIBRARY}
  ${ATOMIC_LIBRARY}
  )

include_directories(iresearch-benchmarks
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  )

################################################################################
### parametric-description-dump
################################################################################

add_executable(iresearch-pdd
  ./common.cpp
  ./pdd.cpp
  ./main.cpp
  )

target_link_libraries(iresearch-pdd
  iresearch-static
  iresearch-cmdline
  ${PTHREAD_LIBRARY}
  ${ATOMIC_LIBRARY}
  )

include_directories(iresearch-pdd
  ${IResearch_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  )
