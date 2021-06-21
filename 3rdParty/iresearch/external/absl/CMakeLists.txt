cmake_minimum_required(VERSION 3.2)
project(absl)

set(ABSL_sources
  ${PROJECT_SOURCE_DIR}/absl/container/internal/raw_hash_set.cc
  ${PROJECT_SOURCE_DIR}/absl/hash/internal/city.cc
  ${PROJECT_SOURCE_DIR}/absl/hash/internal/hash.cc
)

add_library(
  absl-static
  STATIC
  ${ABSL_sources}
)

target_include_directories(
  absl-static
  SYSTEM
  PUBLIC ${PROJECT_SOURCE_DIR}
)
