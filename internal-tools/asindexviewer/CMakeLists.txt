cmake_minimum_required(VERSION 3.20)
project(asindexviewer LANGUAGES CXX)

# ------------------------------------------------------------------------------
# Compiler setup
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer clang++-19 explicitly (optional but recommended)
set(CMAKE_CXX_COMPILER clang++-19)

# ------------------------------------------------------------------------------
# Executable
# ------------------------------------------------------------------------------
add_executable(asindexviewer
    main.cpp
    util.cpp
)

# ------------------------------------------------------------------------------
# Compile definitions
# ------------------------------------------------------------------------------
target_compile_definitions(asindexviewer PRIVATE
    BOOST_TEXT_DISABLE_CONCEPTS
    UNICODE
    _UNICODE
    NDEBUG
)

# ------------------------------------------------------------------------------
# Include directories
# ------------------------------------------------------------------------------
target_include_directories(asindexviewer PRIVATE
    ../../lib/iresearch/core
    ../../lib
)

target_include_directories(asindexviewer SYSTEM PRIVATE
    ../../3rdParty/boost/1.78.0
    ../../lib/iresearch/external/frozen/include
    ../../lib/iresearch/external/highway
    ../../lib/iresearch/external/openfst
    ../../lib/iresearch/external/kaldi/src
    ../../3rdParty
)

# ------------------------------------------------------------------------------
# Compile options
# ------------------------------------------------------------------------------
target_compile_options(asindexviewer PRIVATE
    -O2
    -g
    -msse4.2
    -mavx
    -fsized-deallocation
)

# ------------------------------------------------------------------------------
# Link directories
# ------------------------------------------------------------------------------
target_link_directories(asindexviewer PRIVATE
    ../../build/bin
)

# ------------------------------------------------------------------------------
# Libraries
# ------------------------------------------------------------------------------
target_link_libraries(asindexviewer PRIVATE
    -Wl,--whole-archive

    iresearch-static
    format-1_0-s
    absl_crc32c
    absl_base
    absl_time

    # end whole-archive
    -Wl,--no-whole-archive

    lz4
    simdcomp-static

    absl_crc_internal
    absl_crc_cpu_detect
    absl_raw_hash_set
    absl_hash
    absl_city
    absl_low_level_hash
    absl_strings
    absl_int128
    absl_spinlock_wait
    absl_raw_logging_internal
    absl_synchronization
    absl_time_zone
    absl_kernel_timeout_internal
    absl_stacktrace
    absl_symbolize
    absl_malloc_internal
    absl_debugging_internal
    absl_demangle_internal

    scorer-bm25-s
    scorer-tfidf-s

    velocypack
)
