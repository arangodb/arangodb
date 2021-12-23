####
# Unit tests for the Unit Test Framework library

if(NOT DEFINED BOOST_TEST_ROOT_DIR)
    message(FATAL_ERROR "Please use this file from the main CMakeLists.txt from the build/ folder")
endif()

if(NOT TARGET boost_test_framework OR NOT TARGET boost_test_framework_shared)
    message(FATAL_ERROR "Please use this file from the main CMakeLists.txt from the build/ folder")
endif()

# unit tests folder
set(BOOST_TEST_UNITTESTS_FOLDER ${BOOST_TEST_ROOT_DIR}/test)
set(BOOST_TEST_EXAMPLES_FOLDER ${BOOST_TEST_ROOT_DIR}/example)


# documentation tests
file(GLOB_RECURSE
     BOOST_UTF_DOC_EXAMPLES
     CONFIGURE_DEPENDS
     ${BOOST_TEST_ROOT_DIR}/doc/examples/*.cpp)

foreach(_h IN LISTS BOOST_UTF_DOC_EXAMPLES)
  get_filename_component(_hh ${_h} NAME_WE)
  add_executable(doc-${_hh} 
    ${_h} 
    ${BOOST_TEST_ROOT_DIR}/doc/examples/${_hh}.output)
  set_target_properties(doc-${_hh}
    PROPERTIES
        FOLDER "Doc examples"
        VS_DEBUGGER_WORKING_DIRECTORY ${BOOST_TEST_UNITTESTS_FOLDER})
        target_include_directories(doc-${_hh}
    PUBLIC
        ${BOOST_TEST_ROOT_DIR}/include/
        ${BOOST_ROOT_DIR_ABS}/)

  add_test(NAME doc-${_hh}-test
           COMMAND doc-${_hh})
  get_filename_component(_ext ${_h} EXT)
  string(FIND ${_ext} "fail" _index_fail)
  if(${_index_fail} GREATER -1)
    set_tests_properties(doc-${_hh}-test
        PROPERTIES
            WILL_FAIL TRUE)
  endif()
endforeach()


# datasets
file(GLOB
     BOOST_TEST_UNITTESTS_DATASET
     CONFIGURE_DEPENDS
     ${BOOST_TEST_UNITTESTS_FOLDER}/test-organization-ts/datasets-test/*.cpp
     ${BOOST_TEST_UNITTESTS_FOLDER}/test-organization-ts/datasets-test/*.hpp)
add_executable(boost_test_datasets ${BOOST_TEST_UNITTESTS_DATASET})
set_target_properties(boost_test_datasets
    PROPERTIES
        FOLDER "Unit tests"
        VS_DEBUGGER_WORKING_DIRECTORY ${BOOST_TEST_UNITTESTS_FOLDER})
target_include_directories(boost_test_datasets
    PUBLIC
        ${BOOST_TEST_ROOT_DIR}/include/
        ${BOOST_ROOT_DIR_ABS}/)
target_link_libraries(boost_test_datasets boost_test_framework)
add_test(NAME bt-unittest-dataset
         COMMAND boost_test_datasets)


####
# TS writing-test-ts

set(BOOST_UTF_TESTS_FIND_FILES
    writing-test-ts
    execution_monitor-ts
    framework-ts
    usage-variants-ts
    utils-ts
    test-organization-ts
    smoke-ts
  )


foreach(_ts IN LISTS BOOST_UTF_TESTS_FIND_FILES)

  file(GLOB
       _boost_utf_current_tsuite
       ${BOOST_TEST_UNITTESTS_FOLDER}/${_ts}/*.cpp)

  foreach(_h IN LISTS _boost_utf_current_tsuite)
    get_filename_component(_hh ${_h} ABSOLUTE)
    get_filename_component(_name ${_h} NAME_WE)
    file(RELATIVE_PATH _v ${BOOST_TEST_UNITTESTS_FOLDER} ${_hh})

    add_executable(${_name} ${_hh})
    set_target_properties(${_name}
        PROPERTIES
            FOLDER "Unit tests/${_ts}"
            VS_DEBUGGER_WORKING_DIRECTORY ${BOOST_TEST_UNITTESTS_FOLDER})
    target_link_libraries(${_name}
        PRIVATE
            boost_test_framework) # inaccurate

    add_test(NAME bt-unittest-${_name}
             COMMAND ${_name})
  endforeach()

  unset(_boost_utf_current_tsuite)

endforeach() # test suite


#
# Example code
#

set(LIST_EXAMPLES
  unit_test_example_01.cpp,shared,fail
  unit_test_example_02.cpp,static,fail
  unit_test_example_03.cpp,static,fail
  unit_test_example_04.cpp,shared,fail
  unit_test_example_05.cpp,shared,fail
  unit_test_example_06.cpp,shared,fail
  unit_test_example_07.cpp,shared,run
  unit_test_example_08.cpp,shared,run
  unit_test_example_09_1.cpp,unit_test_example_09_2.cpp,shared,run

  unit_test_example_10.cpp,static,fail
  unit_test_example_11.cpp,static,fail
  unit_test_example_12.cpp,static,link
  unit_test_example_13.cpp,shared,run
  unit_test_example_15.cpp,shared,fail
  unit_test_example_16.cpp,shared,run

  const_string_test.cpp,none,run
  named_param_example.cpp,none,run

  external_main_example_1.cpp,shared,fail
  external_main_example_2.cpp,shared,fail
  external_main_example_3.cpp,none,fail
  filtering_example.cpp,static,fail
)

foreach(_var IN LISTS LIST_EXAMPLES)
    string(REPLACE "," ";" _var_to_list "${_var}")
    list(REVERSE _var_to_list)
    list(GET _var_to_list 0 action)
    list(GET _var_to_list 1 boost_test_type)
    list(REMOVE_AT _var_to_list 0)
    list(REMOVE_AT _var_to_list 0)

    list(GET _var_to_list 0 first_file)
    get_filename_component(_name_example "${first_file}" NAME_WE)

    set(_list_files)
    foreach(_file IN LISTS _var_to_list)
      set(_list_files ${_list_files} ${BOOST_TEST_EXAMPLES_FOLDER}/${_file})
    endforeach()

    add_executable(${_name_example} ${_list_files})
    set_target_properties(${_name_example}
        PROPERTIES
            FOLDER "Examples"
            VS_DEBUGGER_WORKING_DIRECTORY ${BOOST_TEST_UNITTESTS_FOLDER})

    if("${boost_test_type}" STREQUAL "shared")
      target_link_libraries(${_name_example}
        PRIVATE
            boost_test_framework_shared)
    elseif("${boost_test_type}" STREQUAL "static")
      target_link_libraries(${_name_example}
        PRIVATE
            boost_test_framework)
    elseif(NOT "${boost_test_type}" STREQUAL "none")
      message(FATAL_ERROR "Wrong action for example target '${_name_example}'")
    endif()

    if("${action}" STREQUAL "run" OR "${action}" STREQUAL "run-fail")
      add_test(NAME bt-exampletest-${_name_example}
               COMMAND ${_name_example})

      if("${action}" STREQUAL "run-fail")
        set_tests_properties(bt-exampletest-${_name_example}
            PROPERTIES
                WILL_FAIL TRUE)
      endif()
    endif()
endforeach()
