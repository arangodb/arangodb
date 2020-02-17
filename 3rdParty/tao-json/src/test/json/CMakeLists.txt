cmake_minimum_required (VERSION 3.3.0 FATAL_ERROR)

set(testsources
  big_list_of_naughty_strings.cpp
  binding_array.cpp
  binding_factory.cpp
  binding_object.cpp
  binding_versions.cpp
  cbor.cpp
  cbor_parts_parser.cpp
  contrib_diff.cpp
  contrib_get.cpp
  contrib_patch_add.cpp
  contrib_patch_copy.cpp
  contrib_patch_move.cpp
  contrib_patch_remove.cpp
  contrib_patch_replace.cpp
  contrib_patch_test.cpp
  contrib_position.cpp
  contrib_reference.cpp
  contrib_schema.cpp
  contrib_traits.cpp
  double.cpp
  enable_implicit_constructor.cpp
  escape.cpp
  events_binary_to.cpp
  events_compare.cpp
  events_debug.cpp
  events_hash.cpp
  events_to_stream.cpp
  events_to_string.cpp
  include_json.cpp
  integer.cpp
  jaxn_ostream.cpp
  jaxn_parse.cpp
  jaxn_parts_parser.cpp
  json_ostream.cpp
  json_parse.cpp
  json_parts_parser.cpp
  json_pointer.cpp
  key_camel_case_to_snake_case.cpp
  key_snake_case_to_camel_case.cpp
  literal.cpp
  msgpack.cpp
  object_construction.cpp
  opaque_pointer.cpp
  operators.cpp
  optional.cpp
  public_base.cpp
  self_contained.cpp
  sha256.cpp
  string_view.cpp
  temporary_parsing.cpp
  type.cpp
  ubjson.cpp
  uri_fragment.cpp
  validate_event_interfaces.cpp
  validate_utf8.cpp
  value_access.cpp
  value_basics.cpp
  value_create.cpp
  value_ptr.cpp
  value_subscript.cpp
  with_arguments.cpp
)

# file (GLOB ...) is used to validate the above list of test_sources
file (GLOB glob_test_sources RELATIVE ${CMAKE_CURRENT_LIST_DIR} *.cpp)

foreach (testsourcefile ${testsources})
  if (${testsourcefile} IN_LIST glob_test_sources)
    list (REMOVE_ITEM glob_test_sources ${testsourcefile})
  else ()
    message (SEND_ERROR "File ${testsourcefile} is missing from src/test/json")
  endif ()
  get_filename_component (exename ${testsourcefile} NAME_WE)
  set (exename "tao-json-test-${exename}")
  add_executable (${exename} ${testsourcefile})
  target_link_libraries (${exename} PRIVATE taocpp::json)
  set_target_properties (${exename} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
  if (MSVC)
    target_compile_options (${exename} PRIVATE /W4 /WX /utf-8)
  else ()
    target_compile_options (${exename} PRIVATE -pedantic -Wall -Wextra -Wshadow)
  endif ()
  add_test (NAME ${exename} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../.. COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${exename})
endforeach (testsourcefile)

if (glob_test_sources)
  foreach (ignored_source_file ${glob_test_sources})
    message (SEND_ERROR "File ${ignored_source_file} in src/test/json is ignored")
  endforeach (ignored_source_file)
endif ()
