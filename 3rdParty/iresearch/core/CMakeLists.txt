# set version components
if (NOT IResearch_version_major) 
  set(IResearch_version_major "1") 
endif()
if (NOT IResearch_version_minor)
  set(IResearch_version_minor "0")
endif()
if (NOT IResearch_version_revision)
  set(IResearch_version_revision "0")
endif()
if (NOT IResearch_version_patch)
  set(IResearch_version_patch "0")
endif()

# calculate version
math(EXPR IResearch_int_version "(${IResearch_version_major} * 1000000) + (${IResearch_version_minor} * 10000) + (${IResearch_version_revision} * 100) + (${IResearch_version_patch} * 1)" )
set(IResearch_version "${IResearch_version_major}.${IResearch_version_minor}.${IResearch_version_revision}.${IResearch_version_patch}")

# generate version_defines.hpp
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/utils/version_defines.template.hpp" 
  "${CMAKE_CURRENT_BINARY_DIR}/utils/version_defines.hpp"
)

set(IResearch_INCLUDE_DIR 
  "${PROJECT_SOURCE_DIR}/core" 
  CACHE INTERNAL
  ""
)

# set openfst directories
set(OpenFST_ROOT 
  ${EXTERNAL_INCLUDE_DIRS}/openfst
)

set(OpenFST_INCLUDE_DIR 
  ${OpenFST_ROOT}
)

set(OpenFST_sources
  ${OpenFST_ROOT}/compat.cc
  ${OpenFST_ROOT}/flags.cc
  ${OpenFST_ROOT}/fst.cc
  ${OpenFST_ROOT}/mapped-file.cc
  ${OpenFST_ROOT}/properties.cc
  ${OpenFST_ROOT}/symbol-table.cc
  ${OpenFST_ROOT}/symbol-table-ops.cc
  ${OpenFST_ROOT}/util.cc
)

if (MSVC)
  add_definitions(/W4)
  # disable "checked iterators" feature
  add_definitions(-D_SCL_SECURE_NO_WARNINGS) 
else()
  add_definitions(-fvisibility=hidden -Wall)
  set(CMAKE_CXX_FLAGS_RELEASE "-s ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

source_group("analysis" ./analysis/*)
source_group("document" ./document/*)
source_group("error" ./error/*)
source_group("formats" ./formats/*)
source_group("index" ./index/*)
source_group("iql" ./iql/*)
source_group("search" ./search/*)
source_group("store" ./store/*)
source_group("utils" ./utils/*)

set(IResearch_core_sources
  ./utils/string.cpp 
  ./analysis/analyzer.cpp
  ./analysis/analyzers.cpp
  ./analysis/token_attributes.cpp 
  ./analysis/token_stream.cpp 
  ./analysis/token_streams.cpp 
  ./document/field.cpp 
  ./error/error.cpp 
  ./formats/formats.cpp 
  ./formats/format_utils.cpp 
  ./formats/skip_list.cpp 
  ./index/directory_reader.cpp
  ./index/field_data.cpp
  ./index/field_meta.cpp 
  ./index/file_names.cpp 
  ./index/index_meta.cpp 
  ./index/index_writer.cpp 
  ./index/index_reader.cpp
  ./index/iterators.cpp
  ./index/merge_writer.cpp
  ./index/postings.cpp
  ./index/segment_reader.cpp 
  ./index/segment_writer.cpp 
  ./iql/parser.cc
  ./iql/parser_common.cpp
  ./iql/parser_context.cpp
  ./iql/query_builder.cpp
  ./search/all_filter.cpp
  ./search/granular_range_filter.cpp
  ./search/scorers.cpp
  ./search/sort.cpp
  ./search/cost.cpp
  ./search/score_doc_iterators.cpp
  ./search/filter.cpp
  ./search/term_filter.cpp
  ./search/prefix_filter.cpp
  ./search/range_filter.cpp
  ./search/phrase_filter.cpp
  ./search/column_existence_filter.cpp
  ./search/same_position_filter.cpp
  ./search/range_query.cpp
  ./search/term_query.cpp
  ./search/boolean_filter.cpp
  ./store/data_input.cpp 
  ./store/data_output.cpp 
  ./store/directory.cpp 
  ./store/directory_attributes.cpp
  ./store/directory_cleaner.cpp
  ./store/fs_directory.cpp
  ./store/memory_directory.cpp 
  ./store/store_utils.cpp 
  ./utils/async_utils.cpp
  ./utils/attributes.cpp 
  ./utils/bit_packing.cpp 
  ./utils/compression.cpp
  ./utils/directory_utils.cpp
  ./utils/file_utils.cpp 
  ./utils/hash_utils.cpp
  ./utils/index_utils.cpp
  ./utils/math_utils.cpp 
  ./utils/memory.cpp
  ./utils/version_utils.cpp
  ./utils/utf8_path.cpp
  ./utils/locale_utils.cpp
  ./utils/log.cpp
  ./utils/unicode_utils.cpp
  ./utils/so_utils.cpp
  ./utils/process_utils.cpp
  ./utils/network_utils.cpp
  ./utils/cpuinfo.cpp
  ./utils/numeric_utils.cpp
  ${EXTERNAL_INCLUDE_DIRS}/MurmurHash/MurmurHash3.cpp
)

set(IResearch_core_headers
  ./analysis/analyzer.hpp
  ./analysis/analyzer.hpp
  ./analysis/token_attributes.hpp
  ./analysis/token_stream.hpp
  ./analysis/token_streams.hpp
  ./document/field.hpp
  ./error/error.hpp
  ./formats/formats.hpp
  ./formats/format_utils.hpp
  ./formats/skip_list.hpp
  ./index/directory_reader.hpp
  ./index/field_data.hpp
  ./index/field_meta.hpp
  ./index/file_names.hpp
  ./index/index_meta.hpp
  ./index/index_reader.hpp
  ./index/iterators.hpp
  ./index/segment_reader.hpp
  ./index/segment_writer.hpp
  ./index/index_writer.hpp
  ./iql/parser_common.hpp
  ./iql/parser_context.hpp
  ./iql/query_builder.hpp
  ./search/all_filter.hpp
  ./search/granular_range_filter.hpp
  ./search/scorers.hpp
  ./search/sort.hpp
  ./search/cost.hpp
  ./search/filter.hpp
  ./search/score_doc_iterators.hpp
  ./search/term_filter.hpp
  ./search/phrase_filter.hpp
  ./search/same_position_filter.hpp
  ./search/prefix_filter.hpp
  ./search/range_filter.hpp
  ./search/column_existence_filter.hpp
  ./search/range_query.hpp
  ./search/term_query.hpp
  ./search/boolean_filter.hpp
  ./search/disjunction.hpp
  ./search/conjunction.hpp
  ./search/exclusion.hpp
  ./store/data_input.hpp
  ./store/data_output.hpp
  ./store/directory.hpp
  ./store/fs_directory.hpp
  ./store/memory_directory.hpp
  ./store/store_utils.hpp
  ./utils/attributes.hpp
  ./utils/bit_packing.hpp
  ./utils/bit_utils.hpp
  ./utils/block_pool.hpp
  ./utils/checksum.hpp
  ./utils/compression.hpp
  ./utils/file_utils.hpp
  ./utils/fst.hpp
  ./utils/fst_decl.hpp
  ./utils/fst_utils.hpp
  ./utils/integer.hpp
  ./utils/io_utils.hpp
  ./utils/iterator.hpp
  ./utils/math_utils.hpp
  ./utils/memory.hpp
  ./utils/misc.hpp
  ./utils/noncopyable.hpp
  ./utils/singleton.hpp
  ./utils/register.hpp
  ./utils/std.hpp
  ./utils/string.hpp
  ./utils/timer_utils.cpp
  ./utils/log.hpp
  ./utils/thread_utils.hpp
  ./utils/object_pool.hpp
  ./utils/unicode_utils.hpp
  ./utils/so_utils.hpp
  ./utils/process_utils.hpp
  ./utils/network_utils.hpp
  ./utils/cpuinfo.hpp
  ./utils/numeric_utils.hpp
  ./utils/version_utils.hpp
  ./utils/bitset.hpp
  ./utils/type_id.hpp
  ./shared.hpp
  ./types.hpp
  ${EXTERNAL_INCLUDE_DIRS}/MurmurHash/MurmurHash3.h
)

# TODO: use FindLibDL and check linux distr version
if (NOT MSVC)
  set(DL_LIBRARY dl)
endif()

include_directories(
  ${IResearch_INCLUDE_DIR}
  ${EXTERNAL_INCLUDE_DIRS}
  ${BFD_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${Lz4_INCLUDE_DIR}
  ${Unwind_INCLUDE_DIR}
)

include_directories(
  ${OpenFST_INCLUDE_DIR} SYSTEM
)

add_library(
  ${IResearch_TARGET_NAME}-ofst
  OBJECT
  ${OpenFST_sources}
)

# setup CRT
if(MSVC)
  add_library(
    ${IResearch_TARGET_NAME}-ofst-scrt
    OBJECT
    ${OpenFST_sources}
  )
endif()

target_compile_features(
  ${IResearch_TARGET_NAME}-ofst
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(
    ${IResearch_TARGET_NAME}-ofst-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )
endif()

# disable warnings for 3rd-party libs for a cleaner build
if (MSVC)
  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst
    # MSVC requires listing each warning code individually
    # /W0 and /w cannot be used because the produce a single warning themselves
    PRIVATE "/wd4018"
    PRIVATE "/wd4100"
    PRIVATE "/wd4127"
    PRIVATE "/wd4244"
    PRIVATE "/wd4245"
    PRIVATE "/wd4267"
    PRIVATE "/wd4291"
    PRIVATE "/wd4305"
    PRIVATE "/wd4389"
    PRIVATE "/wd4510"
    PRIVATE "/wd4512"
    PRIVATE "/wd4610"
    PRIVATE "/wd4706"
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst-scrt
    # MSVC requires listing each warning code individually
    # /W0 and /w cannot be used because the produce a single warning themselves
    PRIVATE "/wd4018"
    PRIVATE "/wd4100"
    PRIVATE "/wd4127"
    PRIVATE "/wd4244"
    PRIVATE "/wd4245"
    PRIVATE "/wd4267"
    PRIVATE "/wd4291"
    PRIVATE "/wd4305"
    PRIVATE "/wd4389"
    PRIVATE "/wd4510"
    PRIVATE "/wd4512"
    PRIVATE "/wd4610"
    PRIVATE "/wd4706"
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )
else()
  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst
    PRIVATE "-Wno-all"
    PRIVATE "-fPIC"
  )
endif()

set_target_properties(
  ${IResearch_TARGET_NAME}-ofst
  PROPERTIES
  PREFIX lib
  COMPILE_DEFINITIONS "FST_NO_DYNAMIC_LINKING"
)

# setup CRT
if(MSVC)
  set_target_properties(
    ${IResearch_TARGET_NAME}-ofst-scrt
    PROPERTIES
    PREFIX lib
    COMPILE_DEFINITIONS "FST_NO_DYNAMIC_LINKING"
  )
endif()

add_library(${IResearch_TARGET_NAME}-shared
  SHARED
  ${IResearch_core_sources} ${IResearch_core_headers}
)

add_library(${IResearch_TARGET_NAME}-static
  STATIC
  ${IResearch_core_sources} ${IResearch_core_headers}
)

# setup CRT
if(MSVC)
  add_library(${IResearch_TARGET_NAME}-shared-scrt
    SHARED
    ${IResearch_core_sources} ${IResearch_core_headers}
  )

  add_library(${IResearch_TARGET_NAME}-static-scrt
    STATIC
    ${IResearch_core_sources} ${IResearch_core_headers}
  )
endif()

add_custom_command(
  OUTPUT iql/parser.cc
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/iql/parser.yy
  DEPENDS iql ${CMAKE_CURRENT_SOURCE_DIR}/iql/parser.yy
  COMMAND bison --graph --report=all -o parser.cc ${CMAKE_CURRENT_SOURCE_DIR}/iql/parser.yy
  WORKING_DIRECTORY iql
)

add_custom_command(
  OUTPUT iql
  COMMAND ${CMAKE_COMMAND} -E make_directory iql
)

add_custom_command(
  OUTPUT utils
  COMMAND ${CMAKE_COMMAND} -E make_directory utils
)

# trigger generation of BUILD_IDENTIFIER if needed
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/BUILD_IDENTIFIER 
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/BUILD_IDENTIFIER
  DEPENDS utils
  WORKING_DIRECTORY utils
)

# trigger regeneration of utils/version_utils.cpp with build_id from file: BUILD_IDENTIFIER
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/utils/build_identifier.csx
  COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_BINARY_DIR}/BUILD_IDENTIFIER" -DDST="${CMAKE_CURRENT_BINARY_DIR}/utils/build_identifier.csx" -P "${PROJECT_SOURCE_DIR}/cmake/HexEncodeFile.cmake"
  COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${CMAKE_CURRENT_SOURCE_DIR}/version_utils.cpp
  DEPENDS utils ${CMAKE_BINARY_DIR}/BUILD_IDENTIFIER
  WORKING_DIRECTORY utils
)

add_custom_target(
  ${IResearch_TARGET_NAME}-build_identifier
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/utils/build_identifier.csx
)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/BUILD_VERSION
  COMMAND ${CMAKE_COMMAND} -E echo_append "${IResearch_version}" > ${CMAKE_BINARY_DIR}/BUILD_VERSION
  COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_BINARY_DIR}/BUILD_VERSION" -DDST="${CMAKE_CURRENT_BINARY_DIR}/utils/build_version.csx" -P "${PROJECT_SOURCE_DIR}/cmake/HexEncodeFile.cmake"
  COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${CMAKE_CURRENT_SOURCE_DIR}/version_utils.cpp
  DEPENDS utils
  WORKING_DIRECTORY utils
)

add_custom_target(
  ${IResearch_TARGET_NAME}-build_version
  DEPENDS ${CMAKE_BINARY_DIR}/BUILD_VERSION
)

add_dependencies(${IResearch_TARGET_NAME}-shared
  ${IResearch_TARGET_NAME}-build_identifier
  ${IResearch_TARGET_NAME}-build_version
)

add_dependencies(${IResearch_TARGET_NAME}-static
  ${IResearch_TARGET_NAME}-build_identifier
  ${IResearch_TARGET_NAME}-build_version
  ${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-scorer-tfidf-static
  ${IResearch_TARGET_NAME}-scorer-bm25-static
)

# setup CRT
if(MSVC)
  add_dependencies(${IResearch_TARGET_NAME}-shared-scrt
    ${IResearch_TARGET_NAME}-build_identifier
    ${IResearch_TARGET_NAME}-build_version
  )

  add_dependencies(${IResearch_TARGET_NAME}-static-scrt
    ${IResearch_TARGET_NAME}-build_identifier
    ${IResearch_TARGET_NAME}-build_version
    ${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    ${IResearch_TARGET_NAME}-format-1_0-static-scrt
    ${IResearch_TARGET_NAME}-scorer-tfidf-static-scrt
    ${IResearch_TARGET_NAME}-scorer-bm25-static-scrt
 )
endif()

target_include_directories(${IResearch_TARGET_NAME}-shared
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(${IResearch_TARGET_NAME}-static
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

# setup CRT
if(MSVC)
  target_include_directories(${IResearch_TARGET_NAME}-shared-scrt
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )

  target_include_directories(${IResearch_TARGET_NAME}-static-scrt
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
 )
endif()

target_compile_features(${IResearch_TARGET_NAME}-shared
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

target_compile_features(${IResearch_TARGET_NAME}-static
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(${IResearch_TARGET_NAME}-shared-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )

  target_compile_features(${IResearch_TARGET_NAME}-static-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
 )
endif()

# setup MSVC specific compiler flags
if(MSVC)
  target_compile_options(${IResearch_TARGET_NAME}-shared
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
    PRIVATE "/EHa"
  )

  target_compile_options(${IResearch_TARGET_NAME}-static
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
    PRIVATE "/EHa"
  )

  target_compile_options(${IResearch_TARGET_NAME}-shared-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
    PRIVATE "/EHa"
  )

  target_compile_options(${IResearch_TARGET_NAME}-static-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
    PRIVATE "/EHa"
  )
endif()

set_target_properties(${IResearch_TARGET_NAME}-shared
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME iresearch
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_EXPORTS;BOOST_ALL_DYN_LINK;$<$<CONFIG:Debug>:YYDEBUG=1>"
)

set_target_properties(${IResearch_TARGET_NAME}-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME iresearch-s
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

# setup CRT
if(MSVC)
  set_target_properties(${IResearch_TARGET_NAME}-shared-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME iresearch-scrt
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_EXPORTS;BOOST_ALL_DYN_LINK"
  )

  set_target_properties(${IResearch_TARGET_NAME}-static-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME iresearch-scrt-s
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
 )
endif()

if (MSVC)
  set(MSVC_ONLY_LIBRARIES "Dbghelp.lib")
else()
  set(MSVC_ONLY_LIBRARIES "")
endif()

target_link_libraries(${IResearch_TARGET_NAME}-shared
  ${GCOV_LIBRARY}
  ${BFD_SHARED_LIBS}
  ${Boost_SHARED_sharedRT_LIBRARIES}
  ${Lz4_SHARED_LIB}
  ${Unwind_SHARED_LIBS}
  ${DL_LIBRARY}
  ${MSVC_ONLY_LIBRARIES}
)

target_link_libraries(${IResearch_TARGET_NAME}-static
  ${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-scorer-bm25-static
  ${IResearch_TARGET_NAME}-scorer-tfidf-static
  ${GCOV_LIBRARY}
  ${BFD_STATIC_LIBS}
  ${Lz4_STATIC_LIB}
  ${Unwind_STATIC_LIBS}
  ${DL_LIBRARY}
  ${MSVC_ONLY_LIBRARIES}
)

# setup CRT
if(MSVC)
  target_link_libraries(${IResearch_TARGET_NAME}-shared-scrt
    ${GCOV_LIBRARY}
    ${BFD_SHARED_LIBS}
    ${Boost_SHARED_sharedRT_LIBRARIES}
    ${Lz4_SHARED_LIB}
    ${Unwind_SHARED_LIBS}
    ${DL_LIBRARY}
    ${MSVC_ONLY_LIBRARIES}
  )

  target_link_libraries(${IResearch_TARGET_NAME}-static-scrt
    ${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    ${IResearch_TARGET_NAME}-format-1_0-static-scrt
    ${IResearch_TARGET_NAME}-scorer-bm25-static-scrt
    ${IResearch_TARGET_NAME}-scorer-tfidf-static-scrt
    ${GCOV_LIBRARY}
    ${BFD_STATIC_LIBS}
    ${Lz4_STATIC_LIB}
    ${Unwind_STATIC_LIBS}
    ${DL_LIBRARY}
    ${MSVC_ONLY_LIBRARIES}
  )
endif()

if (IRESEARCH_EXCLUDE_STATIC_THIRD_PARTY_LIBS)
  #NOOP, do not link in 3rd party libraries
elseif (MSVC)
  unset(IResearch_STATIC_LIBRARIES)

  foreach(ELEMENT
    ${BFD_STATIC_LIBS}
    "$<$<CONFIG:Debug>:${Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_RELEASE}>"
    "$<$<CONFIG:Debug>:${Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${Boost_STATIC_sharedRT_LOCALE_LIBRARY_RELEASE}>"
    "$<$<CONFIG:Debug>:${Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${Boost_STATIC_sharedRT_SYSTEM_LIBRARY_RELEASE}>"
    ${ICU_STATIC_LIBS}
    "${Lz4_STATIC_LIB}"
    "${Snowball_STATIC_LIB}"
    ${Unwind_STATIC_LIBS}
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-analyzer-text-static>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-format-1_0-static>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-scorer-tfidf-static>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-scorer-bm25-static>"
  )
    set(IResearch_STATIC_LIBRARIES "${IResearch_STATIC_LIBRARIES} \"${ELEMENT}\"")
  endforeach()

  set(IResearch_STATIC_LIBRARIES_CMD "lib.exe \"/OUT:$<TARGET_FILE:${IResearch_TARGET_NAME}-static>\" \"$<TARGET_FILE:${IResearch_TARGET_NAME}-static>\" ${IResearch_STATIC_LIBRARIES}")
  string(REPLACE " " ";" IResearch_STATIC_LIBRARIES_CMD "${IResearch_STATIC_LIBRARIES_CMD}") # COMMAND requires ';' instead of ' '
  add_custom_command(
    TARGET ${IResearch_TARGET_NAME}-static POST_BUILD
    COMMAND ${IResearch_STATIC_LIBRARIES_CMD}
  )

  unset(IResearch_STATIC_SCRT_LIBRARIES)

  foreach(ELEMENT
    ${BFD_STATIC_LIBS}
    "$<$<CONFIG:Debug>:${Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_RELEASE}>"
    "$<$<CONFIG:Debug>:${Boost_STATIC_staticRT_LOCALE_LIBRARY_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${Boost_STATIC_staticRT_LOCALE_LIBRARY_RELEASE}>"
    "$<$<CONFIG:Debug>:${Boost_STATIC_staticRT_SYSTEM_LIBRARY_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${Boost_STATIC_staticRT_SYSTEM_LIBRARY_RELEASE}>"
    ${ICU_STATIC_LIBS}
    "${Lz4_STATIC_LIB}"
    "${Snowball_STATIC_LIB}"
    ${Unwind_STATIC_LIBS}
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-analyzer-text-static-scrt>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-format-1_0-static-scrt>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-scorer-tfidf-static-scrt>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-scorer-bm25-static-scrt>"
  )
    set(IResearch_STATIC_SCRT_LIBRARIES "${IResearch_STATIC_SCRT_LIBRARIES} \"${ELEMENT}\"")
  endforeach()

  set(IResearch_STATIC_SCRT_LIBRARIES_CMD "lib.exe \"/OUT:$<TARGET_FILE:${IResearch_TARGET_NAME}-static-scrt>\" \"$<TARGET_FILE:${IResearch_TARGET_NAME}-static-scrt>\" ${IResearch_STATIC_SCRT_LIBRARIES}")
  string(REPLACE " " ";" IResearch_STATIC_SCRT_LIBRARIES_CMD "${IResearch_STATIC_SCRT_LIBRARIES_CMD}") # COMMAND requires ';' instead of ' '
  add_custom_command(
    TARGET ${IResearch_TARGET_NAME}-static-scrt POST_BUILD
    COMMAND ${IResearch_STATIC_SCRT_LIBRARIES_CMD}
  )
else()
  unset(IResearch_STATIC_LIBRARIES)

  foreach(ELEMENT
    ${BFD_STATIC_LIBS}
    ${Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_DEBUG}
    ${Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG}
    ${Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG}
    ${Boost_STATIC_sharedRT_THREAD_LIBRARY_DEBUG}
    ${ICU_STATIC_LIBS}
    ${Lz4_STATIC_LIB}
    ${Snowball_STATIC_LIB}
    ${Unwind_STATIC_LIBS}
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-analyzer-text-static>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-format-1_0-static>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-scorer-tfidf-static>"
    "$<TARGET_FILE:${IResearch_TARGET_NAME}-scorer-bm25-static>"
  )
    set(IResearch_STATIC_LIBRARIES "${IResearch_STATIC_LIBRARIES} addlib '${ELEMENT}'\\n")
  endforeach ()

  set(IResearch_STATIC_LIBRARIES_CMD "`which echo` -e \"create '$<TARGET_FILE:${IResearch_TARGET_NAME}-static>'\\n addlib '$<TARGET_FILE:${IResearch_TARGET_NAME}-static>'\\n ${IResearch_STATIC_LIBRARIES} save\\n end\" | ar -M")
  add_custom_command(
    TARGET ${IResearch_TARGET_NAME}-static POST_BUILD
    COMMAND sh -c ${IResearch_STATIC_LIBRARIES_CMD}
    VERBATIM
  )
endif()

################################################################################
### analysis plugins
################################################################################

add_library(${IResearch_TARGET_NAME}-analyzer-text-shared
  SHARED
  ./analysis/text_token_stream.cpp
  ./analysis/text_token_stream.hpp
)

add_library(${IResearch_TARGET_NAME}-analyzer-text-static
  STATIC
  ./analysis/text_token_stream.cpp
)

# setup CRT
if(MSVC)
  add_library(${IResearch_TARGET_NAME}-analyzer-text-shared-scrt
    SHARED
    ./analysis/text_token_stream.cpp
  )

  add_library(${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    STATIC
    ./analysis/text_token_stream.cpp
  )
endif()

target_include_directories(${IResearch_TARGET_NAME}-analyzer-text-shared
  PRIVATE ${ICU_INCLUDE_DIR}
  PRIVATE ${Snowball_INCLUDE_DIR}
)

target_include_directories(${IResearch_TARGET_NAME}-analyzer-text-static
  PRIVATE ${ICU_INCLUDE_DIR}
  PRIVATE ${Snowball_INCLUDE_DIR}
)

# setup CRT
if(MSVC)
  target_include_directories(${IResearch_TARGET_NAME}-analyzer-text-shared-scrt
    PRIVATE ${ICU_INCLUDE_DIR}
    PRIVATE ${Snowball_INCLUDE_DIR}
  )

  target_include_directories(${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    PRIVATE ${ICU_INCLUDE_DIR}
    PRIVATE ${Snowball_INCLUDE_DIR}
  )
endif()

target_compile_features(${IResearch_TARGET_NAME}-analyzer-text-shared
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

target_compile_features(${IResearch_TARGET_NAME}-analyzer-text-static
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(${IResearch_TARGET_NAME}-analyzer-text-shared-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )

  target_compile_features(${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )
endif()

# setup CRT
if(MSVC)
  target_compile_options(${IResearch_TARGET_NAME}-analyzer-text-shared
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-analyzer-text-static
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-analyzer-text-shared-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )
endif()

set_target_properties(${IResearch_TARGET_NAME}-analyzer-text-shared
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-text
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_EXPORTS;IRESEARCH_DLL_PLUGIN;BOOST_ALL_DYN_LINK"
)

set_target_properties(${IResearch_TARGET_NAME}-analyzer-text-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-text-s
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

# setup CRT
if(MSVC)
  set_target_properties(${IResearch_TARGET_NAME}-analyzer-text-shared-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME analyzer-text-scrt
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_EXPORTS;IRESEARCH_DLL_PLUGIN;BOOST_ALL_DYN_LINK"
  )

  set_target_properties(${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME analyzer-text-scrt-s
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
  )
endif()

target_link_libraries(${IResearch_TARGET_NAME}-analyzer-text-shared
  ${IResearch_TARGET_NAME}-shared
  ${Boost_SHARED_sharedRT_LIBRARIES}
  ${ICU_SHARED_LIBS}
  ${Snowball_SHARED_LIB}
)

target_link_libraries(${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-static
  ${ICU_STATIC_LIBS}
  ${Snowball_STATIC_LIB}
)

# setup CRT
if(MSVC)
  target_link_libraries(${IResearch_TARGET_NAME}-analyzer-text-shared-scrt
    ${IResearch_TARGET_NAME}-shared-scrt
    ${Boost_SHARED_sharedRT_LIBRARIES}
    ${ICU_SHARED_LIBS}
    ${Snowball_SHARED_LIB}
  )

  target_link_libraries(${IResearch_TARGET_NAME}-analyzer-text-static-scrt
    ${IResearch_TARGET_NAME}-static-scrt
    ${ICU_STATIC_LIBS}
    ${Snowball_STATIC_LIB}
  )
endif()

################################################################################
### format plugins
################################################################################

set(IResearch_format-1_0_source
  ./formats/formats_10.cpp
  ./formats/formats_10_attributes.cpp
  ./formats/formats_burst_trie.cpp
  ./formats/formats_10.hpp
  ./formats/formats_10_attributes.hpp
  ./formats/formats_burst_trie.hpp
)

add_library(${IResearch_TARGET_NAME}-format-1_0-shared
  SHARED
  ${IResearch_format-1_0_source}
  $<TARGET_OBJECTS:${IResearch_TARGET_NAME}-ofst> # used by formats_burst_trie
)

add_library(${IResearch_TARGET_NAME}-format-1_0-static
  STATIC
  ${IResearch_format-1_0_source}
  $<TARGET_OBJECTS:${IResearch_TARGET_NAME}-ofst> # used by formats_burst_trie
)

# setup CRT
if(MSVC)
  add_library(${IResearch_TARGET_NAME}-format-1_0-shared-scrt
    SHARED
    ${IResearch_format-1_0_source}
    $<TARGET_OBJECTS:${IResearch_TARGET_NAME}-ofst-scrt> # used by formats_burst_trie
  )

  add_library(${IResearch_TARGET_NAME}-format-1_0-static-scrt
    STATIC
    ${IResearch_format-1_0_source}
    $<TARGET_OBJECTS:${IResearch_TARGET_NAME}-ofst-scrt> # used by formats_burst_trie
  )
endif()

target_compile_features(${IResearch_TARGET_NAME}-format-1_0-shared
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

target_compile_features(${IResearch_TARGET_NAME}-format-1_0-static
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(${IResearch_TARGET_NAME}-format-1_0-shared-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )

  target_compile_features(${IResearch_TARGET_NAME}-format-1_0-static-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )
endif()

# setup CRT
if(MSVC)
  target_compile_options(${IResearch_TARGET_NAME}-format-1_0-shared
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-format-1_0-static
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-format-1_0-shared-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-format-1_0-static-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )
endif()

set_target_properties(${IResearch_TARGET_NAME}-format-1_0-shared
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME format-1_0
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_EXPORTS;IRESEARCH_DLL_PLUGIN;FST_NO_DYNAMIC_LINKING"
)

set_target_properties(${IResearch_TARGET_NAME}-format-1_0-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME format-1_0-s
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;FST_NO_DYNAMIC_LINKING"
)

# setup CRT
if(MSVC)
  set_target_properties(${IResearch_TARGET_NAME}-format-1_0-shared-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME format-1_0-scrt
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_EXPORTS;IRESEARCH_DLL_PLUGIN;FST_NO_DYNAMIC_LINKING"
  )

  set_target_properties(${IResearch_TARGET_NAME}-format-1_0-static-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME format-1_0-scrt-s
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;FST_NO_DYNAMIC_LINKING"
  )
endif()

target_link_libraries(${IResearch_TARGET_NAME}-format-1_0-shared
  ${IResearch_TARGET_NAME}-shared
)

target_link_libraries(${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-static
)

# setup CRT
if(MSVC)
  target_link_libraries(${IResearch_TARGET_NAME}-format-1_0-shared-scrt
    ${IResearch_TARGET_NAME}-shared-scrt
  )

  target_link_libraries(${IResearch_TARGET_NAME}-format-1_0-static-scrt
    ${IResearch_TARGET_NAME}-static-scrt
  )
endif()

################################################################################
### scorer plugin : TF-IDF
################################################################################

set(IResearch_tfidf_source
  ./search/tfidf.cpp
  ./search/tfidf.hpp
)

add_library(${IResearch_TARGET_NAME}-scorer-tfidf-shared
  SHARED
  ${IResearch_tfidf_source}
)

add_library(${IResearch_TARGET_NAME}-scorer-tfidf-static
  STATIC
  ${IResearch_tfidf_source}
)

# setup CRT
if(MSVC)
  add_library(${IResearch_TARGET_NAME}-scorer-tfidf-shared-scrt
    SHARED
    ${IResearch_tfidf_source}
  )

  add_library(${IResearch_TARGET_NAME}-scorer-tfidf-static-scrt
    STATIC
    ${IResearch_tfidf_source}
  )
endif()

target_compile_features(${IResearch_TARGET_NAME}-scorer-tfidf-shared
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

target_compile_features(${IResearch_TARGET_NAME}-scorer-tfidf-static
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(${IResearch_TARGET_NAME}-scorer-tfidf-shared-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )

  target_compile_features(${IResearch_TARGET_NAME}-scorer-tfidf-static-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )
endif()

# setup CRT
if(MSVC)
  target_compile_options(${IResearch_TARGET_NAME}-scorer-tfidf-shared
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-scorer-tfidf-static
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-scorer-tfidf-shared-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-scorer-tfidf-static-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )
endif()

set_target_properties(${IResearch_TARGET_NAME}-scorer-tfidf-shared
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME scorer-tfidf
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_IMPORTS;IRESEARCH_DLL_PLUGIN"
)

set_target_properties(${IResearch_TARGET_NAME}-scorer-tfidf-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME scorer-tfidf-s
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

# setup CRT
if(MSVC)
  set_target_properties(${IResearch_TARGET_NAME}-scorer-tfidf-shared-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME scorer-tfidf-scrt
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_IMPORTS;IRESEARCH_DLL_PLUGIN"
  )

  set_target_properties(${IResearch_TARGET_NAME}-scorer-tfidf-static-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME scorer-tfidf-scrt-s
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
  )
endif()

target_link_libraries(${IResearch_TARGET_NAME}-scorer-tfidf-shared
  ${IResearch_TARGET_NAME}-shared
)

# setup CRT
if(MSVC)
  target_link_libraries(${IResearch_TARGET_NAME}-scorer-tfidf-shared-scrt
    ${IResearch_TARGET_NAME}-shared-scrt
  )
endif()

################################################################################
### scorer plugin : BM25
################################################################################
    
set(IResearch_bm25_source
  ./search/bm25.cpp
  ./search/bm25.hpp
)


add_library(${IResearch_TARGET_NAME}-scorer-bm25-shared
  SHARED
  ${IResearch_bm25_source}
)

add_library(${IResearch_TARGET_NAME}-scorer-bm25-static
  STATIC
  ${IResearch_bm25_source}
)

# setup CRT
if(MSVC)
  add_library(${IResearch_TARGET_NAME}-scorer-bm25-shared-scrt
    SHARED
    ${IResearch_bm25_source}
  )

  add_library(${IResearch_TARGET_NAME}-scorer-bm25-static-scrt
    STATIC
    ${IResearch_bm25_source}
  )
endif()

target_compile_features(${IResearch_TARGET_NAME}-scorer-bm25-shared
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

target_compile_features(${IResearch_TARGET_NAME}-scorer-bm25-static
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(${IResearch_TARGET_NAME}-scorer-bm25-shared-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )

  target_compile_features(${IResearch_TARGET_NAME}-scorer-bm25-static-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )
endif()

# setup CRT
if(MSVC)
  target_compile_options(${IResearch_TARGET_NAME}-scorer-bm25-shared
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-scorer-bm25-static
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-scorer-bm25-shared-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )

  target_compile_options(${IResearch_TARGET_NAME}-scorer-bm25-static-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
  )
endif()

set_target_properties(${IResearch_TARGET_NAME}-scorer-bm25-shared
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME scorer-bm25
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_IMPORTS;IRESEARCH_DLL_PLUGIN"
)

set_target_properties(${IResearch_TARGET_NAME}-scorer-bm25-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME scorer-bm25-s
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

# setup CRT
if(MSVC)
  set_target_properties(${IResearch_TARGET_NAME}-scorer-bm25-shared-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME scorer-bm25-scrt
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;IRESEARCH_DLL_IMPORTS;IRESEARCH_DLL_PLUGIN"
  )

  set_target_properties(${IResearch_TARGET_NAME}-scorer-bm25-static-scrt
    PROPERTIES
    PREFIX lib
    IMPORT_PREFIX lib
    OUTPUT_NAME scorer-bm25-scrt-s
    COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
  )
endif()

target_link_libraries(${IResearch_TARGET_NAME}-scorer-bm25-shared
  ${IResearch_TARGET_NAME}-shared
)

# setup CRT
if(MSVC)
  target_link_libraries(${IResearch_TARGET_NAME}-scorer-bm25-shared-scrt
    ${IResearch_TARGET_NAME}-shared-scrt
  )
endif()

##########################  
# installation
########################## 

install(TARGETS ${IResearch_TARGET_NAME}-shared
  DESTINATION lib
)

install(TARGETS ${IResearch_TARGET_NAME}-static
  DESTINATION lib
)

install(DIRECTORY analysis document error
  DESTINATION include
  PATTERN "*.cpp" EXCLUDE
  PATTERN "*.mk" EXCLUDE
)
