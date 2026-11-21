include(FetchContent)

set(ABSEIL_VERSION "20260817.0")
set(ABSEIL_SHA256 "f7e05179df39c45434cad433f5783840bb3788ef322976f9138bc6b72b3a107d")

set(ABSEIL_FETCH_OPTIONS
  URL https://github.com/abseil/abseil-cpp/releases/download/${ABSEIL_VERSION}/abseil-cpp-${ABSEIL_VERSION}.tar.gz
  URL_HASH SHA256=${ABSEIL_SHA256}
  PATCH_COMMAND patch -p1 -N -i ${CMAKE_CURRENT_SOURCE_DIR}/raw-hash-set-alloc-failure.patch
        COMMAND patch -p1 -N -i ${CMAKE_CURRENT_SOURCE_DIR}/raw-hash-set-insert-failure.patch
)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  list(APPEND ABSEIL_FETCH_OPTIONS DOWNLOAD_EXTRACT_TIMESTAMP OFF)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.25")
  list(APPEND ABSEIL_FETCH_OPTIONS SYSTEM)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
  list(APPEND ABSEIL_FETCH_OPTIONS EXCLUDE_FROM_ALL)
endif()

set(ABSL_PROPAGATE_CXX_STD ON)

FetchContent_Declare(absl ${ABSEIL_FETCH_OPTIONS})
FetchContent_MakeAvailable(absl)

# V8 is built with gyp and cannot consume the CMake targets
set(ABSL_INCLUDE_DIR "${absl_SOURCE_DIR}" PARENT_SCOPE)
