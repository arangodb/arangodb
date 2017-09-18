# Prerequisites for Windows:
#     This cmake build is for Windows 64-bit only.
#
# Prerequisites:
#     You must have at least Visual Studio 2015 Update 3. Start the Developer Command Prompt window that is a part of Visual Studio installation.
#     Run the build commands from within the Developer Command Prompt window to have paths to the compiler and runtime libraries set.
#     You must have git.exe in your %PATH% environment variable.
#
# To build Rocksdb for Windows is as easy as 1-2-3-4-5:
#
# 1. Update paths to third-party libraries in thirdparty.inc file
# 2. Create a new directory for build artifacts
#        mkdir build
#        cd build
# 3. Run cmake to generate project files for Windows, add more options to enable required third-party libraries.
#    See thirdparty.inc for more information.
#        sample command: cmake -G "Visual Studio 14 Win64" -DGFLAGS=1 -DSNAPPY=1 -DJEMALLOC=1 -DJNI=1 ..
# 4. Then build the project in debug mode (you may want to add /m[:<N>] flag to run msbuild in <N> parallel threads
#                                          or simply /m ot use all avail cores)
#        msbuild rocksdb.sln
#
#        rocksdb.sln build features exclusions of test only code in Release. If you build ALL_BUILD then everything
#        will be attempted but test only code does not build in Release mode.
#
# 5. And release mode (/m[:<N>] is also supported)
#        msbuild rocksdb.sln /p:Configuration=Release
#
# Linux:
#
# 1. Install a recent toolchain such as devtoolset-3 if you're on a older distro. C++11 required.
# 2. mkdir build; cd build
# 3. cmake ..
# 4. make -j

cmake_minimum_required(VERSION 3.2)
project(rocksdb)

#if(POLICY CMP0042)
#  cmake_policy(SET CMP0042 NEW) #MACOS RPATH ENABLED BY DEFAULT - (default since 3.0)
#endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

if(MSVC)
  include(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty.inc)
else()
  option(WITH_JEMALLOC "build with JeMalloc" OFF)
  if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    # FreeBSD has jemaloc as default malloc
    # but it does not have all the jemalloc files in include/...
    set(WITH_JEMALLOC ON)
  else()
    if(WITH_JEMALLOC)
      find_package(JeMalloc REQUIRED)
      add_definitions(-DROCKSDB_JEMALLOC -DJEMALLOC_NO_DEMANGLE)
      include_directories(${JEMALLOC_INCLUDE_DIR})
    endif()
  endif()
endif()

add_definitions(-DSNAPPY)

# set GIT_DATE_TIME - Create US Time String
if(WIN32)
  execute_process(COMMAND powershell -noprofile -Command "Get-Date -format MM_dd_yyyy" OUTPUT_VARIABLE DATE)
  execute_process(COMMAND powershell -noprofile -Command "Get-Date -format HH:mm:ss" OUTPUT_VARIABLE TIME)
  string(REGEX REPLACE "(..)_(..)_..(..).*" "\\1/\\2/\\3" DATE "${DATE}")
  string(REGEX REPLACE "(..):(.....).*" " \\1:\\2" TIME "${TIME}")
  set(GIT_DATE_TIME "${DATE} ${TIME}")
else()
  execute_process(COMMAND date "+%Y/%m/%d %H:%M:%S" OUTPUT_VARIABLE DATETIME)
  string(REGEX REPLACE "\n" "" DATETIME ${DATETIME})
  set(GIT_DATE_TIME "${DATETIME}")
endif()


# set GIT_SHA - sha hash of current commit
find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  if(WIN32)
    execute_process(COMMAND $ENV{COMSPEC} /C ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse HEAD OUTPUT_VARIABLE GIT_SHA)
  else()
    execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse HEAD OUTPUT_VARIABLE GIT_SHA)
  endif()
else()
  set(GIT_SHA 0)
endif()
string(REGEX REPLACE "[^0-9a-f]+" "" GIT_SHA "${GIT_SHA}")

# run version script on UNIX like systems
if(NOT WIN32)
  execute_process(COMMAND
      "./build_tools/version.sh" "full"
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      OUTPUT_VARIABLE ROCKSDB_VERSION
  )
  string(STRIP "${ROCKSDB_VERSION}" ROCKSDB_VERSION)
  execute_process(COMMAND
      "./build_tools/version.sh" "major"
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      OUTPUT_VARIABLE ROCKSDB_VERSION_MAJOR
  )
  string(STRIP "${ROCKSDB_VERSION_MAJOR}" ROCKSDB_VERSION_MAJOR)
endif()

# select windows runteime (dynamic / static)
option(WITH_MD_LIBRARY "build with MD" ON)
if(WIN32 AND MSVC)
  if(WITH_MD_LIBRARY)
      set(RUNTIME_LIBRARY "MD") #DYNAMIC
  else()
      set(RUNTIME_LIBRARY "MT") #STATIC
  endif()
endif()

# portable does not set -m<arch>
# but we are still able to force SSE
option(PORTABLE "build a portable binary" OFF)
option(FORCE_SSE42 "force building with SSE4.2, even when PORTABLE=ON" OFF)
if(PORTABLE)
  # MSVC does not need a separate compiler flag to enable SSE4.2; if nmmintrin.h
  # is available, it is available by default.
  if(FORCE_SSE42 AND NOT MSVC)
    set(RDB_ARCH_FLAGS "-msse4.2")
  endif()
else()
  if(MSVC) #still append to global configuration?
    set(RDB_ARCH_FLAGS "/arch:AVX2")
  else()
    set(RDB_ARCH_FLAGS "-march=native")
  endif()
endif()

# check compiler
## verify that sse is available in the compiler
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${RDB_ARCH_FLAGS}") # sets flags for CheckCXXSourceCompiles
include(CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES("
#include <cstdint>
#include <nmmintrin.h>
int main() {
  volatile uint32_t x = _mm_crc32_u32(0, 0);
}
" HAVE_SSE42)

## fail if force sse is enabled but the compiler is not capable
if(FORCE_SSE42 AND NOT HAVE_SSE42)
  message(FATAL_ERROR "FORCE_SSE42=ON but unable to compile with SSE4.2 enabled")
endif()

## check compiler if thread local is available
CHECK_CXX_SOURCE_COMPILES("
#if defined(_MSC_VER) && !defined(__thread)
#define __thread __declspec(thread)
#endif
int main() {
  static __thread int tls;
}
" HAVE_THREAD_LOCAL)

if(HAVE_THREAD_LOCAL)
  add_definitions(-DROCKSDB_SUPPORT_THREAD_LOCAL)
endif()

# create build version lib
set(BUILD_VERSION_CC ${CMAKE_BINARY_DIR}/build_version.cc)
configure_file(util/build_version.cc.in ${BUILD_VERSION_CC} @ONLY)
add_library(build_version OBJECT ${BUILD_VERSION_CC})
target_include_directories(build_version PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/util)

# modify compiler flags
## set compiler / os specific flags
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi /nologo /EHsc /GS /Gd /GR /GF /fp:precise /Zc:wchar_t /Zc:forScope /errorReport:queue")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FC /d2Zi+ /W3 /wd4127 /wd4800 /wd4996 /wd4351")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wextra -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-compare -Wshadow -Wno-unused-parameter -Wno-unused-variable -Woverloaded-virtual -Wnon-virtual-dtor -Wno-missing-field-initializers")
  if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DNDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fno-omit-frame-pointer")
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-momit-leaf-frame-pointer" HAVE_OMIT_LEAF_FRAME_POINTER)
    if(HAVE_OMIT_LEAF_FRAME_POINTER)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -momit-leaf-frame-pointer")
    endif()
  endif()
endif()

## fail on warnings
option(FAIL_ON_WARNINGS "Treat compile warnings as errors" ON)
if(FAIL_ON_WARNINGS)
  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
  else() # assume GCC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  endif()
endif()

## build with ASAN
option(WITH_ASAN "build with ASAN" OFF)
if(WITH_ASAN)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  if(WITH_JEMALLOC)
    message(FATAL "ASAN does not work well with JeMalloc")
  endif()
endif()

## build with TSAN
option(WITH_TSAN "build with TSAN" OFF)
if(WITH_TSAN)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread -pie")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fPIC")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -fPIC")
  if(WITH_JEMALLOC)
    message(FATAL "TSAN does not work well with JeMalloc")
  endif()
endif()

## build with UBSAN
option(WITH_UBSAN "build with UBSAN" OFF)
if(WITH_UBSAN)
  add_definitions(-DROCKSDB_UBSAN_RUN)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
  if(WITH_JEMALLOC)
    message(FATAL "UBSAN does not work well with JeMalloc")
  endif()
endif()

# Used to run CI build and tests so we can run faster
set(OPTIMIZE_DEBUG_DEFAULT 0)        # Debug build is unoptimized by default use -DOPTDBG=1 to optimize

# set debug level
if(DEFINED OPTDBG)
   set(OPTIMIZE_DEBUG ${OPTDBG})
else()
   set(OPTIMIZE_DEBUG ${OPTIMIZE_DEBUG_DEFAULT})
endif()

if(MSVC)
  if((${OPTIMIZE_DEBUG} EQUAL 1))
    message(STATUS "Debug optimization is enabled")
    set(CMAKE_CXX_FLAGS_DEBUG "/Oxt /${RUNTIME_LIBRARY}d")
  else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /RTC1 /Gm /${RUNTIME_LIBRARY}d")
  endif()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oxt /Zp8 /Gm- /Gy /${RUNTIME_LIBRARY}")

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-memcmp")
endif()

option(ROCKSDB_LITE "Build RocksDBLite version" OFF)
if(ROCKSDB_LITE)
  add_definitions(-DROCKSDB_LITE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
endif()

# add definitions for various operating systems
if(CMAKE_SYSTEM_NAME MATCHES "Cygwin")
  add_definitions(-fno-builtin-memcmp -DCYGWIN)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_definitions(-DOS_MACOSX)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES arm)
    add_definitions(-DIOS_CROSS_COMPILE -DROCKSDB_LITE)
    # no debug info for IOS, that will make our library big
    add_definitions(-DNDEBUG)
  endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_definitions(-DOS_LINUX)
elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
  add_definitions(-DOS_SOLARIS)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  add_definitions(-DOS_FREEBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
  add_definitions(-DOS_NETBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
  add_definitions(-DOS_OPENBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly")
  add_definitions(-DOS_DRAGONFLYBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
  add_definitions(-DOS_ANDROID)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32 -DOS_WIN -D_MBCS -DWIN64 -DNOMINMAX)
  if(MINGW)
    add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_VISTA)
  endif()
endif()

if(NOT WIN32)
  add_definitions(-DROCKSDB_PLATFORM_POSIX -DROCKSDB_LIB_IO_POSIX)
endif()

option(WITH_FALLOCATE "build with fallocate" ON)
if(WITH_FALLOCATE)
  include(CheckCSourceCompiles)
  CHECK_C_SOURCE_COMPILES("
#include <fcntl.h>
#include <linux/falloc.h>
int main() {
 int fd = open(\"/dev/null\", 0);
 fallocate(fd, FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE, 0, 1024);
}
" HAVE_FALLOCATE)
  if(HAVE_FALLOCATE)
    add_definitions(-DROCKSDB_FALLOCATE_PRESENT)
  endif()
endif()

include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(malloc_usable_size HAVE_MALLOC_USABLE_SIZE)
if(HAVE_MALLOC_USABLE_SIZE)
  add_definitions(-DROCKSDB_MALLOC_USABLE_SIZE)
endif()

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/third-party/gtest-1.7.0/fused-src)
find_package(Threads REQUIRED)

add_subdirectory(third-party/gtest-1.7.0/fused-src/gtest)

# Main library source code

set(SOURCES
        cache/clock_cache.cc
        cache/lru_cache.cc
        cache/sharded_cache.cc
        db/builder.cc
        db/c.cc
        db/column_family.cc
        db/compacted_db_impl.cc
        db/compaction.cc
        db/compaction_iterator.cc
        db/compaction_job.cc
        db/compaction_picker.cc
        db/compaction_picker_universal.cc
        db/convenience.cc
        db/db_filesnapshot.cc
        db/db_impl.cc
        db/db_impl_write.cc
        db/db_impl_compaction_flush.cc
        db/db_impl_files.cc
        db/db_impl_open.cc
        db/db_impl_debug.cc
        db/db_impl_experimental.cc
        db/db_impl_readonly.cc
        db/db_info_dumper.cc
        db/db_iter.cc
        db/dbformat.cc
        db/event_helpers.cc
        db/experimental.cc
        db/external_sst_file_ingestion_job.cc
        db/file_indexer.cc
        db/flush_job.cc
        db/flush_scheduler.cc
        db/forward_iterator.cc
        db/internal_stats.cc
        db/log_reader.cc
        db/log_writer.cc
        db/malloc_stats.cc
        db/managed_iterator.cc
        db/memtable.cc
        db/memtable_list.cc
        db/merge_helper.cc
        db/merge_operator.cc
        db/range_del_aggregator.cc
        db/repair.cc
        db/snapshot_impl.cc
        db/table_cache.cc
        db/table_properties_collector.cc
        db/transaction_log_impl.cc
        db/version_builder.cc
        db/version_edit.cc
        db/version_set.cc
        db/wal_manager.cc
        db/write_batch.cc
        db/write_batch_base.cc
        db/write_controller.cc
        db/write_thread.cc
        env/env.cc
        env/env_chroot.cc
        env/env_encryption.cc
        env/env_hdfs.cc
        env/mock_env.cc
        memtable/alloc_tracker.cc
        memtable/hash_cuckoo_rep.cc
        memtable/hash_linklist_rep.cc
        memtable/hash_skiplist_rep.cc
        memtable/skiplistrep.cc
        memtable/vectorrep.cc
        memtable/write_buffer_manager.cc
        monitoring/histogram.cc
        monitoring/histogram_windowing.cc
        monitoring/instrumented_mutex.cc
        monitoring/iostats_context.cc
        monitoring/perf_context.cc
        monitoring/perf_level.cc
        monitoring/statistics.cc
        monitoring/thread_status_impl.cc
        monitoring/thread_status_updater.cc
        monitoring/thread_status_util.cc
        monitoring/thread_status_util_debug.cc
        options/cf_options.cc
        options/db_options.cc
        options/options.cc
        options/options_helper.cc
        options/options_parser.cc
        options/options_sanity_check.cc
        port/stack_trace.cc
        table/adaptive_table_factory.cc
        table/block.cc
        table/block_based_filter_block.cc
        table/block_based_table_builder.cc
        table/block_based_table_factory.cc
        table/block_based_table_reader.cc
        table/block_builder.cc
        table/block_prefix_index.cc
        table/bloom_block.cc
        table/cuckoo_table_builder.cc
        table/cuckoo_table_factory.cc
        table/cuckoo_table_reader.cc
        table/flush_block_policy.cc
        table/format.cc
        table/full_filter_block.cc
        table/get_context.cc
        table/index_builder.cc
        table/iterator.cc
        table/merging_iterator.cc
        table/meta_blocks.cc
        table/partitioned_filter_block.cc
        table/persistent_cache_helper.cc
        table/plain_table_builder.cc
        table/plain_table_factory.cc
        table/plain_table_index.cc
        table/plain_table_key_coding.cc
        table/plain_table_reader.cc
        table/sst_file_writer.cc
        table/table_properties.cc
        table/two_level_iterator.cc
        tools/db_bench_tool.cc
        tools/dump/db_dump_tool.cc
        tools/ldb_cmd.cc
        tools/ldb_tool.cc
        tools/sst_dump_tool.cc
        util/arena.cc
        util/auto_roll_logger.cc
        util/bloom.cc
        util/coding.cc
        util/compaction_job_stats_impl.cc
        util/comparator.cc
        util/concurrent_arena.cc
        util/crc32c.cc
        util/crc32c_sse4.cc
        util/delete_scheduler.cc
        util/dynamic_bloom.cc
        util/event_logger.cc
        util/file_reader_writer.cc
        util/file_util.cc
        util/filename.cc
        util/filter_policy.cc
        util/hash.cc
        util/log_buffer.cc
        util/murmurhash.cc
        util/random.cc
        util/rate_limiter.cc
        util/slice.cc
        util/sst_file_manager_impl.cc
        util/status.cc
        util/status_message.cc
        util/string_util.cc
        util/sync_point.cc
        util/testutil.cc
        util/thread_local.cc
        util/threadpool_imp.cc
        util/transaction_test_util.cc
        util/xxhash.cc
        utilities/backupable/backupable_db.cc
        utilities/blob_db/blob_db.cc
        utilities/blob_db/blob_db_impl.cc
        utilities/blob_db/blob_db_options_impl.cc
        utilities/blob_db/blob_dump_tool.cc
        utilities/blob_db/blob_file.cc
        utilities/blob_db/blob_log_reader.cc
        utilities/blob_db/blob_log_writer.cc
        utilities/blob_db/blob_log_format.cc
        utilities/checkpoint/checkpoint_impl.cc
        utilities/col_buf_decoder.cc
        utilities/col_buf_encoder.cc
        utilities/column_aware_encoding_util.cc
        utilities/compaction_filters/remove_emptyvalue_compactionfilter.cc
        utilities/date_tiered/date_tiered_db_impl.cc
        utilities/debug.cc
        utilities/document/document_db.cc
        utilities/document/json_document.cc
        utilities/document/json_document_builder.cc
        utilities/env_mirror.cc
        utilities/env_timed.cc
        utilities/geodb/geodb_impl.cc
        utilities/leveldb_options/leveldb_options.cc
        utilities/lua/rocks_lua_compaction_filter.cc
        utilities/memory/memory_util.cc
        utilities/merge_operators/max.cc
        utilities/merge_operators/put.cc
        utilities/merge_operators/string_append/stringappend.cc
        utilities/merge_operators/string_append/stringappend2.cc
        utilities/merge_operators/uint64add.cc
        utilities/option_change_migration/option_change_migration.cc
        utilities/options/options_util.cc
        utilities/persistent_cache/block_cache_tier.cc
        utilities/persistent_cache/block_cache_tier_file.cc
        utilities/persistent_cache/block_cache_tier_metadata.cc
        utilities/persistent_cache/persistent_cache_tier.cc
        utilities/persistent_cache/volatile_tier_impl.cc
        utilities/redis/redis_lists.cc
        utilities/simulator_cache/sim_cache.cc
        utilities/spatialdb/spatial_db.cc
        utilities/table_properties_collectors/compact_on_deletion_collector.cc
        utilities/transactions/optimistic_transaction_db_impl.cc
        utilities/transactions/optimistic_transaction_impl.cc
        utilities/transactions/transaction_base.cc
        utilities/transactions/transaction_db_impl.cc
        utilities/transactions/transaction_db_mutex_impl.cc
        utilities/transactions/transaction_impl.cc
        utilities/transactions/transaction_lock_mgr.cc
        utilities/transactions/transaction_util.cc
        utilities/ttl/db_ttl_impl.cc
        utilities/write_batch_with_index/write_batch_with_index.cc
        utilities/write_batch_with_index/write_batch_with_index_internal.cc
        $<TARGET_OBJECTS:build_version>)

if(WIN32)
  list(APPEND SOURCES
    port/win/io_win.cc
    port/win/env_win.cc
    port/win/env_default.cc
    port/win/port_win.cc
    port/win/win_logger.cc
    port/win/win_thread.cc
    port/win/xpress_win.cc)
else()
  list(APPEND SOURCES
    port/port_posix.cc
    env/env_posix.cc
    env/io_posix.cc)
endif()

set(ROCKSDB_STATIC_LIB rocksdb${ARTIFACT_SUFFIX})
#set(ROCKSDB_SHARED_LIB rocksdb-shared${ARTIFACT_SUFFIX})
#set(ROCKSDB_IMPORT_LIB ${ROCKSDB_SHARED_LIB})

if(WIN32)
  set(SYSTEM_LIBS ${SYSTEM_LIBS} Shlwapi.lib Rpcrt4.lib)
#  set(LIBS ${ROCKSDB_STATIC_LIB} ${THIRDPARTY_LIBS} ${SYSTEM_LIBS})
else()
  set(SYSTEM_LIBS ${CMAKE_THREAD_LIBS_INIT})
#  set(LIBS ${ROCKSDB_SHARED_LIB} ${THIRDPARTY_LIBS} ${SYSTEM_LIBS})
#
#  add_library(${ROCKSDB_SHARED_LIB} SHARED ${SOURCES})
#  target_link_libraries(${ROCKSDB_SHARED_LIB}
#    ${THIRDPARTY_LIBS} ${SYSTEM_LIBS})
#  set_target_properties(${ROCKSDB_SHARED_LIB} PROPERTIES
#                        LINKER_LANGUAGE CXX
#                        VERSION ${ROCKSDB_VERSION}
#                        SOVERSION ${ROCKSDB_VERSION_MAJOR}
#                        CXX_STANDARD 11
#                        OUTPUT_NAME "rocksdb")
endif()

option(WITH_LIBRADOS "Build with librados" OFF)
if(WITH_LIBRADOS)
  list(APPEND SOURCES
    utilities/env_librados.cc)
  list(APPEND THIRDPARTY_LIBS rados)
endif()

add_library(${ROCKSDB_STATIC_LIB} STATIC ${SOURCES})
target_link_libraries(${ROCKSDB_STATIC_LIB}
  ${THIRDPARTY_LIBS} ${SYSTEM_LIBS} snappystatic)

#if(WIN32)
#  add_library(${ROCKSDB_IMPORT_LIB} SHARED ${SOURCES})
#  target_link_libraries(${ROCKSDB_IMPORT_LIB}
#    ${THIRDPARTY_LIBS} ${SYSTEM_LIBS})
#  set_target_properties(${ROCKSDB_IMPORT_LIB} PROPERTIES
#    COMPILE_DEFINITIONS "ROCKSDB_DLL;ROCKSDB_LIBRARY_EXPORTS")
#  if(MSVC)
#    set_target_properties(${ROCKSDB_STATIC_LIB} PROPERTIES
#      COMPILE_FLAGS "/Fd${CMAKE_CFG_INTDIR}/${ROCKSDB_STATIC_LIB}.pdb")
#    set_target_properties(${ROCKSDB_IMPORT_LIB} PROPERTIES
#      COMPILE_FLAGS "/Fd${CMAKE_CFG_INTDIR}/${ROCKSDB_IMPORT_LIB}.pdb")
#  endif()
#endif()

## TODO -- set property on sse version of crc file
#get_property(info_out SOURCE util/crc32_sse4.cc PROPERTY COMPILE_FLAGS)
#message(INFO "compile flags ${info_out}")
#get_property(info_out SOURCE util/crc32_sse4.cc PROPERTY COMPILE_DEFINITIONS)
#message(INFO "compile definitions ${info_out}")

set_property( SOURCE util/crc32c.cc APPEND PROPERTY COMPILE_FLAGS "${RDB_ARCH_FLAGS}")
#get_property(info_out SOURCE util/crc32_sse4.cc PROPERTY COMPILE_FLAGS)
#message(INFO "compile flags ${info_out}")

if(HAVE_SSE42)
  set_property( SOURCE util/crc32c_sse4.cc APPEND PROPERTY COMPILE_FLAGS "${RDB_ARCH_FLAGS}")
  set_property( SOURCE util/crc32c_sse4.cc APPEND PROPERTY COMPILE_DEFINITIONS "HAVE_SSE42=1")
  #get_property(info_out SOURCE util/crc32_sse4.cc PROPERTY COMPILE_DEFINITIONS)
  #message(INFO "compile definitions ${info_out}")
endif()

option(WITH_JNI "build with JNI" OFF)
if(WITH_JNI OR JNI)
  message(STATUS "JNI library is enabled")
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/java)
else()
  message(STATUS "JNI library is disabled")
endif()
