# set version components
if (NOT IResearch_version_major)
  set(IResearch_version_major "1")
endif ()
if (NOT IResearch_version_minor)
  set(IResearch_version_minor "3")
endif ()
if (NOT IResearch_version_revision)
  set(IResearch_version_revision "0")
endif ()
if (NOT IResearch_version_patch)
  set(IResearch_version_patch "0")
endif ()

set(SIMD_LIBRARY_STATIC "simdcomp-static")
set(FASTTEXT_LIBRARY_STATIC "fasttext-static")

# set libatomic library
# (not available on Windows and Mac)
if (NOT MSVC AND NOT APPLE)
  set(ATOMIC_LIBRARY atomic)
endif ()

# calculate version
math(EXPR IResearch_int_version "(${IResearch_version_major} * 1000000) + (${IResearch_version_minor} * 10000) + (${IResearch_version_revision} * 100) + (${IResearch_version_patch} * 1)")
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


#for some reason TARGET_PROPERTY generator expression does not work for this target
get_target_property(VPACK_INCLUDE_PATH velocypack INTERFACE_INCLUDE_DIRECTORIES)


if (MSVC)
  add_definitions(/W4)
  # disable "checked iterators" feature
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)

  # set OS specific sources
  set(IResearch_core_os_specific_sources
    ./utils/mman_win32.cpp
    )

  # set OS specific headers
  set(IResearch_core_os_specific_headers
    ./utils/mman_win32.hpp
    )
else ()
  add_definitions(-Wall)

  if (NOT APPLE AND NOT (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Debug"))
    add_link_options(-s)
  endif ()

  # set OS specific headers
  set(IResearch_core_os_specific_headers
    ./utils/mman_posix.hpp
    )
endif ()

source_group("analysis" ./analysis/*)
source_group("document" ./document/*)
source_group("error" ./error/*)
source_group("formats" ./formats/*)
source_group("index" ./index/*)
source_group("search" ./search/*)
source_group("store" ./store/*)
source_group("utils" ./utils/*)

set(IResearch_core_sources
  ./utils/assert.cpp
  ./analysis/analyzers.cpp
  ./analysis/token_attributes.cpp
  ./analysis/token_streams.cpp
  ./error/error.cpp
  ./formats/columnstore.cpp
  ./formats/columnstore2.cpp
  ./formats/formats.cpp
  ./formats/format_utils.cpp
  ./formats/skip_list.cpp
  ./formats/sparse_bitmap.cpp
  ./index/buffered_column.cpp
  ./index/directory_reader.cpp
  ./index/directory_reader_impl.cpp
  ./index/field_data.cpp
  ./index/field_meta.cpp
  ./index/file_names.cpp
  ./index/index_writer.cpp
  ./index/index_reader.cpp
  ./index/iterators.cpp
  ./index/merge_writer.cpp
  ./index/norm.cpp
  ./index/postings.cpp
  ./index/segment_reader.cpp
  ./index/segment_reader_impl.cpp
  ./index/segment_writer.cpp
  ./search/all_docs_provider.cpp
  ./search/all_filter.cpp
  ./search/all_iterator.cpp
  ./search/boost_scorer.cpp
  ./search/granular_range_filter.cpp
  ./search/scorers.cpp
  ./search/scorer.cpp
  ./search/score_function.cpp
  ./search/cost.cpp
  ./search/collectors.cpp
  ./search/score.cpp
  ./search/bitset_doc_iterator.cpp
  ./search/filter.cpp
  ./search/term_filter.cpp
  ./search/nested_filter.cpp
  ./search/terms_filter.cpp
  ./search/prefix_filter.cpp
  ./search/range_filter.cpp
  ./search/phrase_filter.cpp
  ./search/phrase_query.cpp
  ./search/column_existence_filter.cpp
  ./search/same_position_filter.cpp
  ./search/wildcard_filter.cpp
  ./search/levenshtein_filter.cpp
  ./search/multiterm_query.cpp
  ./search/term_query.cpp
  ./search/boolean_filter.cpp
  ./search/boolean_query.cpp
  ./search/ngram_similarity_filter.cpp
  ./search/ngram_similarity_query.cpp
  ./search/proxy_filter.cpp
  ./store/data_input.cpp
  ./store/data_output.cpp
  ./store/directory.cpp
  ./store/directory_attributes.cpp
  ./store/directory_cleaner.cpp
  ./store/fs_directory.cpp
  ./store/mmap_directory.cpp
  ./store/memory_directory.cpp
  ./store/store_utils.cpp
  ./utils/async_utils.cpp
  ./utils/thread_utils.cpp
  ./utils/attributes.cpp
  ./utils/automaton_utils.cpp
  ./utils/bit_packing.cpp
  ./utils/encryption.cpp
  ./utils/ctr_encryption.cpp
  ./utils/compression.cpp
  ./utils/delta_compression.cpp
  ./utils/lz4compression.cpp
  ./utils/directory_utils.cpp
  ./utils/file_utils.cpp
  ./utils/mmap_utils.cpp
  ./utils/index_utils.cpp
  ./utils/levenshtein_utils.cpp
  ./utils/wildcard_utils.cpp
  ./utils/levenshtein_default_pdp.cpp
  ./utils/memory.cpp
  ./utils/timer_utils.cpp
  ./utils/version_utils.cpp
  ./utils/log.cpp
  ./utils/so_utils.cpp
  ./utils/process_utils.cpp
  ./utils/network_utils.cpp
  ./utils/numeric_utils.cpp
  ${IResearch_core_os_specific_sources}
  ${IResearch_core_optimized_sources}
  )

set(IResearch_core_headers
  ./utils/assert.hpp
  ./analysis/analyzer.hpp
  ./analysis/analyzer.hpp
  ./analysis/token_attributes.hpp
  ./analysis/token_stream.hpp
  ./analysis/token_streams.hpp
  ./error/error.hpp
  ./formats/formats.hpp
  ./formats/format_utils.hpp
  ./formats/skip_list.hpp
  ./index/directory_reader.hpp
  ./index/directory_reader_impl.hpp
  ./index/field_data.hpp
  ./index/field_meta.hpp
  ./index/file_names.hpp
  ./index/index_meta.hpp
  ./index/index_reader.hpp
  ./index/index_reader_options.hpp
  ./index/iterators.hpp
  ./index/segment_reader.hpp
  ./index/segment_reader_impl.hpp
  ./index/segment_writer.hpp
  ./index/index_writer.hpp
  ./search/all_filter.hpp
  ./search/all_iterator.hpp
  ./search/boost_scorer.hpp
  ./search/granular_range_filter.hpp
  ./search/scorers.hpp
  ./search/states_cache.hpp
  ./search/scorer.hpp
  ./search/cost.hpp
  ./search/filter.hpp
  ./search/term_filter.hpp
  ./search/nested_filter.hpp
  ./search/phrase_filter.hpp
  ./search/same_position_filter.hpp
  ./search/prefix_filter.hpp
  ./search/range_filter.hpp
  ./search/column_existence_filter.hpp
  ./search/multiterm_query.hpp
  ./search/term_query.hpp
  ./search/boolean_filter.hpp
  ./search/disjunction.hpp
  ./search/conjunction.hpp
  ./search/exclusion.hpp
  ./search/ngram_similarity_filter.hpp
  ./search/filter_visitor.hpp
  ./search/proxy_filter.hpp
  ./store/data_input.hpp
  ./store/data_output.hpp
  ./store/directory.hpp
  ./store/fs_directory.hpp
  ./store/memory_directory.hpp
  ./store/store_utils.hpp
  ./utils/attributes.hpp
  ./utils/automaton.hpp
  ./utils/automaton_utils.hpp
  ./utils/wildcard_utils.hpp
  ./utils/bit_packing.hpp
  ./utils/bit_utils.hpp
  ./utils/block_pool.hpp
  ./utils/compression.hpp
  ./utils/directory_utils.hpp
  ./utils/lz4compression.hpp
  ./utils/file_utils.hpp
  ./utils/fstext/fst_builder.hpp
  ./utils/fstext/fst_decl.hpp
  ./utils/fstext/fst_table_matcher.hpp
  ./utils/fstext/fst_string_weight.hpp
  ./utils/mmap_utils.hpp
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
  ./utils/log.hpp
  ./utils/thread_utils.hpp
  ./utils/object_pool.hpp
  ./utils/so_utils.hpp
  ./utils/process_utils.hpp
  ./utils/network_utils.hpp
  ./utils/numeric_utils.hpp
  ./utils/version_utils.hpp
  ./utils/bitset.hpp
  ./utils/type_id.hpp
  ./shared.hpp
  ./types.hpp
  )

if (uring_FOUND)
  list(APPEND IResearch_core_sources ./store/async_directory.cpp)
endif ()

if (MSVC)
  set(DL_LIBRARY "Dbghelp.lib")  # TODO Try to remove it
else ()
  set(DL_LIBRARY dl)
endif ()

include_directories(
  ${IResearch_INCLUDE_DIR}
)

set(IRESEARCH_EXTERNAL_INCLUDES
  ${EXTERNAL_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS} # ensure Boost paths take precedence over other system libraries as Boost may be defined elsewhere
  ${Lz4_INCLUDE_DIR}
  ${Fasttext_INCLUDE_DIR}
  ${FROZEN_INCLUDE_DIR})

include_directories(SYSTEM ${IRESEARCH_EXTERNAL_INCLUDES})

add_library(iresearch-static
  STATIC
  ${IResearch_core_sources} ${IResearch_core_headers}
  )

# trigger generation of BUILD_IDENTIFIER if needed
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/BUILD_IDENTIFIER
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/BUILD_IDENTIFIER
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
  iresearch-build_identifier
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/utils/build_identifier.csx
)

# trigger generation of BUILD_VERSION if needed
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/BUILD_VERSION
  COMMAND ${CMAKE_COMMAND} -E echo_append "${IResearch_version}" > ${CMAKE_BINARY_DIR}/BUILD_VERSION
)

# trigger regeneration of utils/version_utils.cpp with build_id from file: BUILD_VERSION
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/utils/build_version.csx
  COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_BINARY_DIR}/BUILD_VERSION" -DDST="${CMAKE_CURRENT_BINARY_DIR}/utils/build_version.csx" -P "${PROJECT_SOURCE_DIR}/cmake/HexEncodeFile.cmake"
  COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${CMAKE_CURRENT_SOURCE_DIR}/version_utils.cpp
  DEPENDS utils ${CMAKE_BINARY_DIR}/BUILD_VERSION
  WORKING_DIRECTORY utils
)

add_custom_target(
  iresearch-build_version
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/utils/build_version.csx
)

add_dependencies(iresearch-static
  iresearch-build_identifier
  iresearch-build_version
  iresearch-analyzer-delimiter-static
  iresearch-analyzer-multi-delimiter-static
  iresearch-analyzer-ngram-static
  iresearch-analyzer-text-static
  iresearch-analyzer-norm-static
  iresearch-analyzer-collation-static
  iresearch-analyzer-stem-static
  iresearch-analyzer-stopwords-static
  iresearch-analyzer-pipeline-static
  iresearch-analyzer-segmentation-static
  iresearch-analyzer-nearest-neighbors-static
  iresearch-analyzer-classification-static
  iresearch-analyzer-minhash-static
  iresearch-format-1_0-static
  iresearch-scorer-tfidf-static
  iresearch-scorer-bm25-static
  hwy
  velocypack
  )

target_include_directories(iresearch-static
  SYSTEM
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  PRIVATE ${ICU_INCLUDE_DIR}
  PRIVATE $<TARGET_PROPERTY:iresearch-ofst,INTERFACE_INCLUDE_DIRECTORIES>
  PUBLIC ${VPACK_INCLUDE_PATH}
  PUBLIC $<TARGET_PROPERTY:hwy,INTERFACE_INCLUDE_DIRECTORIES>
  PUBLIC $<TARGET_PROPERTY:${FASTTEXT_LIBRARY_STATIC},INTERFACE_INCLUDE_DIRECTORIES>
  )

target_compile_definitions(iresearch-static PRIVATE -DDISABLE_EXECINFO)

# setup MSVC specific compiler flags
if (MSVC)
  target_compile_options(iresearch-static
    PRIVATE "$<$<CONFIG:Debug>:/bigobj>"
    PRIVATE "/EHa"
    )
else ()
  # Suppress missing field initializer warnings (treated as errors with -Werror)
  target_compile_options(iresearch-static
    PRIVATE "-Wno-missing-field-initializers"
  )
endif ()

set_target_properties(iresearch-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME iresearch-s
  )

if (USE_IWYU)
  set_property(TARGET iresearch-static PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH})
endif ()

target_link_libraries(iresearch-static
  PUBLIC iresearch-analyzer-delimiter-static
  PUBLIC iresearch-analyzer-multi-delimiter-static
  PUBLIC iresearch-analyzer-ngram-static
  PUBLIC iresearch-analyzer-text-static
  PUBLIC iresearch-analyzer-norm-static
  PUBLIC iresearch-analyzer-minhash-static
  PUBLIC iresearch-analyzer-collation-static
  PUBLIC iresearch-analyzer-stem-static
  PUBLIC iresearch-analyzer-stopwords-static
  PUBLIC iresearch-analyzer-pipeline-static
  PUBLIC iresearch-analyzer-segmentation-static
  PUBLIC iresearch-analyzer-classification-static
  PUBLIC iresearch-analyzer-nearest-neighbors-static
  PUBLIC iresearch-format-1_0-static
  PUBLIC iresearch-scorer-bm25-static
  PUBLIC iresearch-scorer-tfidf-static
  PUBLIC ${ICU_STATIC_LIBS}
  PUBLIC absl::flat_hash_map
  PUBLIC absl::flat_hash_set
  PUBLIC absl::crc32c
  ${GCOV_LIBRARY}
  ${Lz4_STATIC_LIB}
  ${DL_LIBRARY}
  ${SIMD_LIBRARY_STATIC}
  velocypack
  ${LIBURING}
  )

if (CLANG_TIDY_EXE)
  set_target_properties(
    iresearch-static
    PROPERTIES
    CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-header-filter=.;-checks=clang-analyzer-cplusplus*,google-*,performance-*,readability-*"
  )
endif ()

if (NOT ("${IRESEARCH_EXTERNAL_DEPENDENCIES}" STREQUAL ""))
  add_dependencies(iresearch-static ${IRESEARCH_EXTERNAL_DEPENDENCIES})
endif()

################################################################################
### analysis plugin : delimiter
################################################################################

add_library(iresearch-analyzer-delimiter-static
  STATIC
  ./analysis/delimited_token_stream.cpp
)

set_target_properties(iresearch-analyzer-delimiter-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-delimiter-s
)

target_link_libraries(iresearch-analyzer-delimiter-static
  iresearch-static
)

################################################################################
### analysis plugin : multi-delimiter
################################################################################

add_library(iresearch-analyzer-multi-delimiter-static
  STATIC
  ./analysis/multi_delimited_token_stream.cpp
)

set_target_properties(iresearch-analyzer-multi-delimiter-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-multi-delimiter-s
)

target_link_libraries(iresearch-analyzer-multi-delimiter-static
  iresearch-static
  iresearch-ofst
)

################################################################################
### analysis plugin : text
################################################################################

add_library(iresearch-analyzer-text-static
  STATIC
  ./analysis/text_token_stream.cpp
  ./utils/snowball_stemmer.cpp
  )

target_include_directories(iresearch-analyzer-text-static
  SYSTEM
  PRIVATE ${ICU_INCLUDE_DIR} # cmake on MSVC does not properly expand dependencies for linked libraries
  PRIVATE ${Snowball_INCLUDE_DIR}
  )

set_target_properties(iresearch-analyzer-text-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-text-s
  )

target_link_libraries(iresearch-analyzer-text-static
  iresearch-static
  stemmer-static
  )

if (NOT ("${IRESEARCH_EXTERNAL_DEPENDENCIES}" STREQUAL ""))
  add_dependencies(iresearch-analyzer-text-static ${IRESEARCH_EXTERNAL_DEPENDENCIES})
endif()

################################################################################
### analysis plugin : collation token stream
################################################################################

add_library(iresearch-analyzer-collation-static
  STATIC
  ./analysis/collation_token_stream.cpp
  )

target_include_directories(iresearch-analyzer-collation-static
  SYSTEM
  PRIVATE ${ICU_INCLUDE_DIR} # cmake on MSVC does not properly expand dependencies for linked libraries
  )

set_target_properties(iresearch-analyzer-collation-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-collation-s
  )

target_link_libraries(iresearch-analyzer-collation-static
  iresearch-static
  )

if (NOT ("${IRESEARCH_EXTERNAL_DEPENDENCIES}" STREQUAL ""))
  add_dependencies(iresearch-analyzer-collation-static ${IRESEARCH_EXTERNAL_DEPENDENCIES})
endif()

################################################################################
### analysis plugin : minhash
################################################################################

add_library(iresearch-analyzer-minhash-static
  STATIC
  ./analysis/minhash_token_stream.cpp
  )

set_target_properties(iresearch-analyzer-minhash-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-minhash-s
  )

target_link_libraries(iresearch-analyzer-minhash-static
  iresearch-static
  )

################################################################################
### analysis plugin : text token normalizing
################################################################################

add_library(iresearch-analyzer-norm-static
  STATIC
  ./analysis/text_token_normalizing_stream.cpp
  )

target_include_directories(iresearch-analyzer-norm-static
  SYSTEM
  PRIVATE ${ICU_INCLUDE_DIR} # cmake on MSVC does not properly expand dependencies for linked libraries
  )

set_target_properties(iresearch-analyzer-norm-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-norm-s
  )

target_link_libraries(iresearch-analyzer-norm-static
  iresearch-static
  )

if (NOT ("${IRESEARCH_EXTERNAL_DEPENDENCIES}" STREQUAL ""))
  add_dependencies(iresearch-analyzer-norm-static ${IRESEARCH_EXTERNAL_DEPENDENCIES})
endif()

################################################################################
### analysis plugin : text token stemming
################################################################################

add_library(iresearch-analyzer-stem-static
  STATIC
  ./analysis/text_token_stemming_stream.cpp
  ./utils/snowball_stemmer.cpp
  )

target_include_directories(iresearch-analyzer-stem-static
  SYSTEM
  PRIVATE ${Snowball_INCLUDE_DIR}
  PRIVATE ${ICU_INCLUDE_DIR}
  )

set_target_properties(iresearch-analyzer-stem-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-stem-s
  )

target_link_libraries(iresearch-analyzer-stem-static
  iresearch-static
  stemmer-static
  )

if (NOT ("${IRESEARCH_EXTERNAL_DEPENDENCIES}" STREQUAL ""))
  add_dependencies(iresearch-analyzer-stem-static ${IRESEARCH_EXTERNAL_DEPENDENCIES})
endif()

################################################################################
### analysis plugin : token stopwords
################################################################################

add_library(iresearch-analyzer-stopwords-static
  STATIC
  ./analysis/token_stopwords_stream.cpp
  )

set_target_properties(iresearch-analyzer-stopwords-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-stopwords-s
  )

target_link_libraries(iresearch-analyzer-stopwords-static
  iresearch-static
  )

################################################################################
### analysis plugin : ngram
################################################################################

add_library(iresearch-analyzer-ngram-static
  STATIC
  ./analysis/ngram_token_stream.cpp
  )

set_target_properties(iresearch-analyzer-ngram-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-ngram-s
  )

target_link_libraries(iresearch-analyzer-ngram-static
  iresearch-static
  )

################################################################################
### analysis plugin : pipeline
################################################################################

add_library(iresearch-analyzer-pipeline-static
  STATIC
  ./analysis/pipeline_token_stream.cpp
  )

set_target_properties(iresearch-analyzer-pipeline-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-pipeline-s
  )

target_link_libraries(iresearch-analyzer-pipeline-static
  iresearch-static
  )

################################################################################
### analysis plugin : text segmentation
################################################################################

add_library(iresearch-analyzer-segmentation-static
  STATIC
  ./analysis/segmentation_token_stream.cpp
  )

target_link_libraries(
  iresearch-analyzer-segmentation-static
  text
)

set_target_properties(iresearch-analyzer-segmentation-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-segmentation-s
  )

target_include_directories(iresearch-analyzer-segmentation-static
  PRIVATE $<TARGET_PROPERTY:text,INTERFACE_INCLUDE_DIRECTORIES>
  )

target_link_libraries(iresearch-analyzer-segmentation-static
  iresearch-static
  )

################################################################################
### analysis plugin: embedding classification
################################################################################

add_library(iresearch-analyzer-classification-static
  STATIC
  analysis/classification_stream.cpp
  )

set_target_properties(iresearch-analyzer-classification-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-classification-s
  )

target_link_libraries(iresearch-analyzer-classification-static
  iresearch-static
  ${FASTTEXT_LIBRARY_STATIC}
  )

target_include_directories(iresearch-analyzer-classification-static
  SYSTEM
  PRIVATE $<TARGET_PROPERTY:${FASTTEXT_LIBRARY_STATIC},INCLUDE_DIRECTORIES>
  )

################################################################################
### analysis plugin: nearest neighbors
################################################################################

add_library(iresearch-analyzer-nearest-neighbors-static
  STATIC
  analysis/nearest_neighbors_stream.cpp
  )

set_target_properties(iresearch-analyzer-nearest-neighbors-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME analyzer-nearest-neighbors-s
  )

target_link_libraries(iresearch-analyzer-nearest-neighbors-static
  iresearch-static
  ${FASTTEXT_LIBRARY_STATIC}
  )

target_include_directories(iresearch-analyzer-nearest-neighbors-static
  SYSTEM
  PRIVATE $<TARGET_PROPERTY:${FASTTEXT_LIBRARY_STATIC},INCLUDE_DIRECTORIES>
  )

################################################################################
### format plugin: format-1_0
################################################################################

set(IResearch_format-1_0_source
  ./formats/formats_10.cpp
  ./formats/formats_burst_trie.cpp
  ./formats/formats_10.hpp
  ./formats/formats_10_attributes.hpp
  ./formats/formats_burst_trie.hpp
  )

add_library(iresearch-format-1_0-static
  STATIC
  ${IResearch_format-1_0_source}
  $<TARGET_OBJECTS:iresearch-ofst> # used by formats_burst_trie
  )

set_target_properties(iresearch-format-1_0-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME format-1_0-s
  COMPILE_DEFINITIONS "${FORMAT_10_EXTRA_DEFS}"
  )

target_include_directories(iresearch-format-1_0-static
  SYSTEM
  PRIVATE $<TARGET_PROPERTY:iresearch-ofst,INTERFACE_INCLUDE_DIRECTORIES> # used by formats_burst_trie
  )

target_link_libraries(iresearch-format-1_0-static
  iresearch-static
  ${ATOMIC_LIBRARY}
  ${FORMAT_10_EXTRA_LIBS_STATIC}
  )

################################################################################
### scorer plugin : TF-IDF
################################################################################

set(IResearch_tfidf_source
  ./search/tfidf.cpp
  ./search/tfidf.hpp
  )

add_library(iresearch-scorer-tfidf-static
  STATIC
  ${IResearch_tfidf_source}
  )

set_target_properties(iresearch-scorer-tfidf-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME scorer-tfidf-s
  )

target_link_libraries(iresearch-scorer-tfidf-static
  iresearch-static
  )
################################################################################
### scorer plugin : BM25
################################################################################

set(IResearch_bm25_source
  ./search/bm25.cpp
  ./search/bm25.hpp
  )

add_library(iresearch-scorer-bm25-static
  STATIC
  ${IResearch_bm25_source}
  )

set_target_properties(iresearch-scorer-bm25-static
  PROPERTIES
  PREFIX lib
  IMPORT_PREFIX lib
  OUTPUT_NAME scorer-bm25-s
  )

target_link_libraries(iresearch-scorer-bm25-static
  iresearch-static
  )
