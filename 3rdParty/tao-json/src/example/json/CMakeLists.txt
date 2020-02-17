cmake_minimum_required (VERSION 3.3.0 FATAL_ERROR)

set (examplesources
  cbor_to_jaxn.cpp
  cbor_to_json.cpp
  cbor_to_msgpack.cpp
  cbor_to_pretty_jaxn.cpp
  cbor_to_pretty_json.cpp
  cbor_to_ubjson.cpp
  jaxn_to_cbor.cpp
  jaxn_to_cplusplus.cpp
  jaxn_to_jaxn.cpp
  jaxn_to_msgpack.cpp
  jaxn_to_pretty_jaxn.cpp
  jaxn_to_ubjson.cpp
  json_to_cbor.cpp
  json_to_json.cpp
  json_to_msgpack.cpp
  json_to_pretty_json.cpp
  json_to_ubjson.cpp
  msgpack_to_cbor.cpp
  msgpack_to_jaxn.cpp
  msgpack_to_json.cpp
  msgpack_to_pretty_jaxn.cpp
  msgpack_to_pretty_json.cpp
  msgpack_to_ubjson.cpp
  printf_doubles.cpp
  ubjson_to_cbor.cpp
  ubjson_to_jaxn.cpp
  ubjson_to_json.cpp
  ubjson_to_msgpack.cpp
  ubjson_to_pretty_jaxn.cpp
  ubjson_to_pretty_json.cpp
  validate_event_order.cpp
  validate_integer.cpp
)

# file (GLOB ...) is used to validate the above list of example_sources
file (GLOB glob_example_sources RELATIVE ${CMAKE_CURRENT_LIST_DIR} *.cpp)

foreach (examplesourcefile ${examplesources})
  if (${examplesourcefile} IN_LIST glob_example_sources)
    list (REMOVE_ITEM glob_example_sources ${examplesourcefile})
  else ()
    message (SEND_ERROR "File ${examplesourcefile} is missing from src/example/json")
  endif ()
  get_filename_component (exename ${examplesourcefile} NAME_WE)
  set (exename "tao-json-example-${exename}")
  add_executable (${exename} ${examplesourcefile})
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
endforeach (examplesourcefile)

if (glob_example_sources)
  foreach (ignored_source_file ${glob_example_sources})
    message (SEND_ERROR "File ${ignored_source_file} in src/example/json is ignored")
  endforeach (ignored_source_file)
endif ()
